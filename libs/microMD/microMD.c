#include "microMD_impl.h"
#include <malloc.h>
#include "string.h"

#define UTF8_1b_MASK (0xFF >> 1)
#define UTF8_2b_MASK (0xFF >> 3)
#define UTF8_3b_MASK (0xFF >> 4)
#define UTF8_4b_MASK (0xFF >> 5)
#define UTF8_EXTRA_MASK (0xFF >> 2)

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

    parser->render_word = render_callback;
    parser->has_space_remaining = page_space_callback;
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
    uint8_t expectedBytes = 0, codepointOffset = 0;

    uint16_t codepoint = 0;

    /*
        Let this run until it either finds the beginning of a new byte, or the buffer runs out of space
        In the case it runs out of space, the return value will be false, signaling that no byte was found, thus requiring the buffer to be refilled
    */
    while(true) {
        if (parser->p_length == 0) return false; // No valid character found

        // Read next byte in buffer
        currentByte = read_rbuffer(parser);

        for (uint bitIdx = 0; bitIdx < 5; bitIdx ++) {
            if (!check_bit(currentByte, bitIdx)) break;
            typeBits += 1;
        }

        if (typeBits == 1) {
            // Malformed input, return a replacement byte
            *codepoint_out = 0xFFFD;
            return true;
        }
        break;
    }

    switch(typeBits) {
        case 0: 
            codepoint |= (currentByte & UTF8_1b_MASK);
            codepointOffset = 7;
            expectedBytes = 0;
            break;
        case 2:
            codepoint |= (currentByte & UTF8_2b_MASK);
            codepointOffset = 5;
            expectedBytes = 1;
            break;
        case 3: 
            codepoint |= (currentByte & UTF8_3b_MASK);
            codepointOffset = 4;
            expectedBytes = 2;
            break;
        case 4: 
            /* No Support for 4 byte UTF-8    
            codepoint |= (currentByte & UTF8_4b_MASK);
            codepointOffset = 3;
            */
            expectedBytes = 3;
            codepoint = 0xFFFD; // Replacement Character
            break; // Break not return -> Let for loop below consume all 4 bytes so that buffer is at proper position
    }

    for (uint characterByte = 0; characterByte < expectedBytes; characterByte ++) {
        if (parser->p_length == 0) refill_text_rbuffer(parser); // Since we know a sequence of UTF8 should be here, we refill the buffer if we are missing bytes
        if (parser->p_length == 0) return false;                // Still empty after forcing refill, assume corrupted or EOF, so return false to let caller handle

        currentByte = read_rbuffer(parser); // Read next byte
        if (expectedBytes == 3) continue;   // No Support for 4 byte UTF-8

        if (!check_bit(currentByte, 0) || check_bit(currentByte, 1)) {
            codepoint = 0xFFFD;
            break;
        } // New char begins before we reached our expected byte count, corrupted input, give replacement character

        uint16_t value = (currentByte & UTF8_EXTRA_MASK) << codepointOffset;
        codepoint |= value;
        codepointOffset += 6;
    }

    *codepoint_out = codepoint;
    return true;
}

void parse_page(microMD_parser_t *parser) {
    // Run a Loop -> Read UTF8 and interpret character according to state machine
    // When Word is completed, try to render
    // - If rendering fails: break loop
    // - If rendering succeeds: restart state machine and continue loop
}