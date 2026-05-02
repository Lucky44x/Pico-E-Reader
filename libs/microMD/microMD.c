#include "microMD_impl.h"
#include <malloc.h>
#include "string.h"

#define UTF8_1b_MASK (0xFF >> 1)
#define UTF8_2b_MASK (0xFF >> 3)
#define UTF8_3b_MASK (0xFF >> 4)
#define UTF8_4b_MASK (0xFF >> 5)
#define UTF8_EXTRA_MASK (0xFF >> 2)

/*
    Useful control characters:
        - 0x0A : Line Feed -> Line Break
        - 0x0D : Carriage Return -> Line Break
        - 0x0C : Form Feed -> Page Break
*/

// Static buffer for UTF-8 conversion (avoids dynamic allocation)
#define UTF8_BUFFER_SIZE 256
static char utf8_buffer[UTF8_BUFFER_SIZE];

// Encode a single Unicode codepoint to UTF-8 bytes
// Returns the number of bytes written to the buffer
static size_t encode_utf8(uint16_t codepoint, char *buffer, size_t buffer_size) {
    if (codepoint <= 0x7F) {
        // 1-byte sequence
        if (buffer_size < 1) return 0;
        buffer[0] = (char)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        // 2-byte sequence
        if (buffer_size < 2) return 0;
        buffer[0] = (char)(0xC0 | (codepoint >> 6));
        buffer[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    } else {
        // 3-byte sequence (covers BMP, which is what uint16_t can hold)
        if (buffer_size < 3) return 0;
        buffer[0] = (char)(0xE0 | (codepoint >> 12));
        buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }
}

// Convert uint16_t array to UTF-8 string
// Returns pointer to static buffer, or NULL if buffer overflow
const char* str16_to_utf8(const uint16_t *str16, size_t len) {
    size_t utf8_len = 0;
    
    for (size_t i = 0; i < len; i++) {
        size_t bytes = encode_utf8(str16[i], utf8_buffer + utf8_len, UTF8_BUFFER_SIZE - utf8_len);
        if (bytes == 0) {
            // Buffer overflow
            utf8_buffer[UTF8_BUFFER_SIZE - 1] = '\0';
            return NULL;
        }
        utf8_len += bytes;
    }
    
    if (utf8_len >= UTF8_BUFFER_SIZE) {
        utf8_buffer[UTF8_BUFFER_SIZE - 1] = '\0';
        return NULL;
    }
    
    utf8_buffer[utf8_len] = '\0';
    return utf8_buffer;
}

// Print a uint16_t string array
void print_str16(const uint16_t *str16, size_t len) {
    const char *utf8_str = str16_to_utf8(str16, len);
    if (utf8_str) {
        printf("%s", utf8_str);
    } else {
        printf("[UTF-8 conversion failed - buffer overflow]");
    }
}

// Print a uint16_t string with formatting (like printf)
void printf_str16(const char *format, const uint16_t *str16, size_t len) {
    const char *utf8_str = str16_to_utf8(str16, len);
    if (utf8_str) {
        printf(format, utf8_str);
    } else {
        printf(format, "[UTF-8 conversion failed - buffer overflow]");
    }
}

void setup_ring_buffer(uint8_t **buffer, size_t length, uint16_t *tail, uint16_t *head) {
    *buffer = malloc(length);
    *tail = 0;
    *head = 0;
}

void teardown_ring_buffer(uint8_t *buffer) {
    free(buffer);
}

void make_microMD_parser(microMD_parser_t *parser, render_callback_t render_callback, page_space_callback_t page_space_callback) {
    setup_ring_buffer(&parser->page_ring_buffer, MAX_PAGE_SIZE, &parser->p_tail, &parser->p_head);
    parser->tag_length = 0;
    parser->word_length = 0;
    parser->p_length = 0;
    parser->has_space_remaining = true;

    parser->escape_flag = false;
    parser->applied_style_set.flags = 0x00;
    parser->temporary_style_set.flags = 0x00;

    parser->render_word = render_callback;
    parser->page_has_space = page_space_callback;
}

void destroy_microMD_parser(microMD_parser_t *parser) {
    teardown_ring_buffer(parser->page_ring_buffer);
}

void open_microMD_file(microMD_parser_t *parser, FIL *file, FSIZE_t offset) {
    f_lseek(file, offset);
    parser->currentFile = file;
    parser->currentFileOffset = offset;
}

void refill_text_rbuffer(microMD_parser_t *parser) {
    f_lseek(parser->currentFile, parser->currentFileOffset);

    if (parser->p_length >= MAX_PAGE_SIZE) return; // Buffer is already full... ignore

    // Figure out the number of bytes that remain in the ring-buffer
    size_t remaining_bytes = MAX_PAGE_SIZE - parser->p_length;
    size_t read_bytes, written_bytes;
    uint8_t byte_buffer[MAX_PAGE_SIZE];

    // Read this number of bytes from the open file
    FRESULT read_result = f_read(parser->currentFile, byte_buffer, remaining_bytes, &read_bytes);
    if (read_result != FR_OK) {
        printf("Error during File-Read: %d", read_result);
        return;
    }

    // Enqueue this data into the parser's buffer, and offset file pointer back, if not all bytes were enqueued
    written_bytes = enqueue_rbuffer(parser, byte_buffer, read_bytes);
    if (written_bytes < read_bytes) {
        f_lseek(parser->currentFile, parser->currentFile->fptr - (read_bytes - written_bytes)); // Offset file pointer back by missing bytes that were not written to buffer
    }
    parser->currentFileOffset = parser->currentFile->fptr;
}

/*
    Tries to enqueue the given data into the parser's buffer, and returns the number of bytes actually enqueued
*/
size_t enqueue_rbuffer(microMD_parser_t *parser, uint8_t *data, size_t len) {
    size_t written_bytes = 0;

    for (size_t i = 0; i < len; i++) {
        if (parser->p_length >= MAX_PAGE_SIZE) break;

        parser->page_ring_buffer[parser->p_head] = data[i];
        parser->p_head ++;                  // increment head position
        parser->p_head %= MAX_PAGE_SIZE;    // Wrap around
        parser->p_length ++;                // Increment length
        written_bytes ++;                   // Increment written byte count
    }
    return written_bytes;
}

/*
    Reads a single byte from the parser's buffer
    will return a 0, if buffer is empty
*/
uint8_t read_rbuffer(microMD_parser_t *parser) {
    if (parser->p_length == 0) return 0x00;

    uint8_t data = parser->page_ring_buffer[parser->p_tail];
    parser->p_tail += 1;
    parser->p_tail %= MAX_PAGE_SIZE; // Wrap around
    parser->p_length -= 1;
    return data;
}

bool decode_next_utf8_from_buffer(microMD_parser_t *parser, uint16_t *codepoint_out) {
    uint8_t currentByte = 0, typeBits = 0;
    uint8_t expectedBytes = 0;

    uint16_t codepoint = 0;

    /*
        Let this run until it either finds the beginning of a new byte, or the buffer runs out of space
        In the case it runs out of space, the return value will be false, signaling that no byte was found, thus requiring the buffer to be refilled
    */
    while(true) {
        if (parser->p_length == 0) return false; // No valid character found

        // Read next byte in buffer
        currentByte = read_rbuffer(parser);

        // Count leading 1s from MSB (bit 7)
        typeBits = 0;
        for (int bitIdx = 7; bitIdx >= 0; bitIdx--) {
            if ((currentByte & (1 << bitIdx)) == 0) break;
            typeBits++;
        }

        // Validate typeBits
        if (typeBits == 1 || typeBits > 4) {
            // Malformed input: single 1 or more than 4 leading 1s
            *codepoint_out = 0xFFFD;
            return true;
        }
        break;
    }

    switch(typeBits) {
        case 0: 
            codepoint = currentByte;
            expectedBytes = 0;
            break;
        case 2:
            codepoint = (currentByte & UTF8_2b_MASK) << 6;
            expectedBytes = 1;
            break;
        case 3: 
            codepoint = (currentByte & UTF8_3b_MASK) << 12;
            expectedBytes = 2;
            break;
        case 4: 
            /* No Support for 4 byte UTF-8 */
            expectedBytes = 3;
            codepoint = 0xFFFD; // Replacement Character
            break; // Break not return -> Let for loop below consume all 4 bytes so that buffer is at proper position
    }

    for (uint characterByte = 0; characterByte < expectedBytes; characterByte ++) {
        if (parser->p_length == 0) refill_text_rbuffer(parser); // Since we know a sequence of UTF8 should be here, we refill the buffer if we are missing bytes
        if (parser->p_length == 0) return false;                // Still empty after forcing refill, assume corrupted or EOF, so return false to let caller handle

        currentByte = read_rbuffer(parser); // Read next byte
        if (expectedBytes == 3) continue;   // No Support for 4 byte UTF-8

        // Check if it's a valid continuation byte (starts with 10xxxxxx)
        if ((currentByte & 0xC0) != 0x80) {
            codepoint = 0xFFFD;
            break;
        } // Invalid continuation byte, give replacement character

        uint8_t shift = 6 * (expectedBytes - characterByte - 1);
        codepoint |= (currentByte & UTF8_EXTRA_MASK) << shift;
    }

    *codepoint_out = codepoint;
    return true;
}

void parse_page(microMD_parser_t *parser) {
    uint16_t current_char = 0xFFFD;
    parser->has_space_remaining = parser->page_has_space();

    refill_text_rbuffer(parser); // Ensure ring buffer is full
    if (parser->p_length == 0) return; // Text Buffer is empty even after refilling -> Error during read?

    // If we still have a word buffered, try to render it and set flags
    if (parser->word_length > 0) {
        printf("Rendering buffered word...\n");
        perform_word_render(parser);
    }

    // For as long as we have space, execute the state machine
    while(parser->has_space_remaining) {
        bool char_ok = decode_next_utf8_from_buffer(parser, &current_char);
        if (!char_ok) break; // If we encounter an invalid char -> false return meaining EOF or Problems filling buffer from file

        printf("Recieved char: %c\n", current_char);

        switch(current_char) {
            case '\\':
                if (parser->escape_flag){
                    push_character_to_word(parser, current_char);
                } else {
                    parser->escape_flag = true;
                    continue;
                }
            break;
            case '<':
                if (parser->escape_flag) {
                    printf("%c added to word, as escape is set\n");
                    push_character_to_word(parser, '<');
                    break; // Unsetting the escape flag is done automatically after breaking
                }

                push_character_to_tag(parser, current_char); // Push the starting character into our tag-buffer
                if (tag_state_machine(parser)) {
                    // Perform the render to ensure word before style change is rendered with appropriate style. Rendering function also automatically updates stylesheet when successfull
                    if (parser->word_length > 0) perform_word_render(parser);
                    else parser->applied_style_set.flags = parser->temporary_style_set.flags;
                } else {
                    // Push tag buffer to word-buffer (when word exceeds MAX_WORD_LENGTH, automatically discards any overrun chars)
                    for (uint idx = 0; idx < parser->tag_length; idx++) {
                        push_character_to_word(parser, parser->tag_buffer[idx]);
                    }
                }
                // After we are done with processing the tag, reset the tag buffer
                parser->tag_length = 0;
            break;
            default:
                push_character_to_word(parser, current_char);
                // Check for ending character
                switch(current_char) {
                    case 0x0A:
                        parser->applied_style_set.newLine = true;
                    case ' ':
                    case '.':
                    case ',':
                    case ';':
                    case ':':
                    case '-':
                    case '_':
                    case '!':
                    case '?':
                    case '/':
                    case '+':
                    case '*':
                    case '=':
                        perform_word_render(parser);
                        break;
                    default:
                        //NOOP -> Character is not an ending char
                    break;
                }
            break;
        }

        parser->escape_flag = false; // Reset escape flag after character is read
        // Continue to next iteration, if no space remains, the next iteration will immediatly close, thus ending the loop
    }

    if (parser->word_length > 0) {
        perform_word_render(parser);
    }
}

void push_character_to_word(microMD_parser_t *parser, uint16_t codepoint) {
    printf("    Pushing %04x - %c to word buffer %d\n", codepoint, codepoint, parser->word_length);
    if (parser->word_length == MAX_WORD_LENGTH) return;
    parser->word_buffer[parser->word_length] = codepoint;
    parser->word_length ++;
    parser->word_buffer[parser->word_length] = 0x00;
}

void push_character_to_tag(microMD_parser_t *parser, uint16_t codepoint) {
    printf("    Pushing %04x - %c to tag buffer %d\n", codepoint, codepoint, parser->tag_length);
    if (parser->tag_length == 4) return;
    parser->tag_buffer[parser->tag_length] = codepoint;
    parser->tag_length ++;
    parser->tag_buffer[parser->tag_length] = 0x00;
}

bool tag_state_machine(microMD_parser_t *parser) {
    style_set_t tmp_style_set = {};
    bool is_inverted = false;
    
    // Read next character
    uint16_t next_char = 0x00;
    if (!decode_next_utf8_from_buffer(parser, &next_char)) return false; // Something went wrong while reading the next character, return invalid tag
    push_character_to_tag(parser, next_char);

    if (next_char == '/') is_inverted = true;
    if (is_inverted) {
        if (!decode_next_utf8_from_buffer(parser, &next_char)) return false; // Get Next char after inversion char
        push_character_to_tag(parser, next_char);
        if (!perform_style_tag_processing(next_char, is_inverted, &tmp_style_set)) return false; // Invalid tag format
    } else {
        if (!perform_style_tag_processing(next_char, is_inverted, &tmp_style_set)) return false; // Invalid tag format
    }

    // Check end-char
    if (!decode_next_utf8_from_buffer(parser, &next_char)) return false; // Get final char
    push_character_to_tag(parser, next_char);

    if (next_char != '>') return false; // Invalid tag format

    printf("Got tag: \n");
    print_str16(parser->tag_buffer, parser->tag_length);

    // If all chars match tags, set temporary style sheet for word
    parser->temporary_style_set.flags = tmp_style_set.flags;
    return true;
}

bool perform_style_tag_processing(uint16_t style_char, bool unset, style_set_t *style_set) {
    int style = 0;

    switch (style_char)
    {
        // Bold
        case 'b': style = STYLE_BOLD; break;
        // Italic
        case 'i': style = STYLE_ITALIC; break;
        // Underlined
        case 'u': style = STYLE_UNDERLINED; break;
        // Dotted underline
        case 'd': style = STYLE_DOTTED; break;
        // Strikethrough
        case 's': style = STYLE_STRIKETHROUGH; break;
        // Heading
        case 'h': style = STYLE_HEADER; break;
        // Chapter break
        case 'c': style = STYLE_CHAPTER_BREAK; break;
        // Page break
        case 'p': style = STYLE_PAGE_BREAK; break;
        default:
            return false; // anything else is considered an error
    }
    set_style(style_set, style, !unset);
    return true;
}

bool perform_word_render(microMD_parser_t *parser) {
    printf("Sending Render command\n");

    // Try to render
    if (!parser->render_word(&parser->applied_style_set, parser->word_buffer, parser->word_length)) {
        // When render is unsuccessfull, return out
        parser->has_space_remaining = false;
        return false;
    }

    // Reset page-break -> Ensures once a page break was consumed by the rendere, it is cleared and not applied on the next render call unless explicitly called
    reset_style(&parser->temporary_style_set, STYLE_PAGE_BREAK);

    // Set Space-Remaining flag
    parser->has_space_remaining = true;

    /* Apply temporary style-sheet:
        - No New Tags read in word -> Temp_Sheet == Applied_Sheet
        - New Tags read, but word could not render -> Applied_Sheet is not set to Temp_Sheet, thus it will be updated when the next render call comes in (when the buffered word gets rendered)
        - New Tags read in word and word rendered -> Applied_Sheet is set to Temp_Sheet, thus applying the style for the next word and ensuring condition 1
    */
    parser->applied_style_set.flags = parser->temporary_style_set.flags;

    // Clear word buffer
    parser->word_length = 0;
}