#ifndef _MICRO_MD_H
#define _MICRO_MD_H

#include "pico/stdlib.h"
#include "stdio.h"
#include "tf_card.h"
#include "ff.h"

#define MAX_PAGE_SIZE 1024
#define MAX_WORD_LENGTH 64

#define STYLE_BOLD          (1 << 0)
#define STYLE_ITALIC        (1 << 1)
#define STYLE_UNDERLINED    (1 << 2)
#define STYLE_DOTTED        (1 << 3)
#define STYLE_STRIKETHROUGH (1 << 4)
#define STYLE_HEADER        (1 << 5)
#define STYLE_PAGE_BREAK    (1 << 6)
#define STYLE_CHAPTER_BREAK (1 << 7)

typedef struct {
    uint8_t flags;
    bool newLine;
} style_set_t;

typedef bool (*render_callback_t)(style_set_t* style_set, uint16_t* word, uint8_t word_length);
typedef bool (*page_space_callback_t)();

static inline bool is_style_active(style_set_t *style_set, int style) {
    return style_set->flags & style;
}

static inline void put_style(style_set_t *style_set, int style) {
    style_set->flags |= style;
}

static inline void reset_style(style_set_t *style_set, int style) {
    style_set->flags &= ~style;
}

static inline void set_style(style_set_t *style_set, int style, bool state) {
    if (state) put_style(style_set, style);
    else reset_style(style_set, style);
}

typedef struct {
    uint8_t* page_ring_buffer;
    uint16_t p_head;
    uint16_t p_tail;
    uint16_t p_length;

    uint16_t word_buffer[MAX_WORD_LENGTH+1]; // Null Termination
    uint8_t word_length;

    uint16_t tag_buffer[5];
    uint8_t tag_length;

    style_set_t applied_style_set;
    style_set_t temporary_style_set;

    bool escape_flag;
    bool has_space_remaining;

    render_callback_t render_word;
    page_space_callback_t page_has_space;

    FIL *currentFile;
    FSIZE_t currentFileOffset;
} microMD_parser_t;

void make_microMD_parser(microMD_parser_t *parser, render_callback_t render_callback, page_space_callback_t page_space_callback);
void destroy_microMD_parser(microMD_parser_t *parser);

void open_microMD_file(microMD_parser_t *parser, FIL *file, FSIZE_t offset);
void parse_page(microMD_parser_t *parser);

// Utility functions for printing uint16_t strings
const char* str16_to_utf8(const uint16_t *str16, size_t len);
void print_str16(const uint16_t *str16, size_t len);
void printf_str16(const char *format, const uint16_t *str16, size_t len);

#endif