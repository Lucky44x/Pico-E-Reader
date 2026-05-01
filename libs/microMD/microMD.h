#ifndef _MICRO_MD_H
#define _MICRO_MD_H

#include "pico/stdlib.h"
#include "stdio.h"
#include "tf_card.h"
#include "ff.h"

#define MAX_PAGE_SIZE 512
#define MAX_WORD_LENGTH 32

#define STYLE_BOLD          (1 << 0)
#define STYLE_ITALIC        (1 << 1)
#define STYLE_UNDERLINED    (1 << 2)
#define STYLE_DOTTED        (1 << 3)
#define STYLE_STRIKETHROUGH (1 << 4)
#define STYLE_HEADER        (1 << 5)

typedef struct {
    uint8_t flags;
} style_set_t;

typedef bool (*render_callback_t)(style_set_t* style_set, uint16_t* word, uint8_t word_length);
typedef bool (*page_space_callback_t)();

typedef struct {
    uint8_t* page_ring_buffer;
    uint16_t p_head;
    uint16_t p_tail;
    uint16_t p_length;

    uint16_t word_buffer[MAX_WORD_LENGTH];
    uint8_t word_length;

    uint16_t tag_buffer[4];
    uint8_t tag_length;

    uint8_t style_set; // Text-Styling bitset

    bool escape_flag;
    bool has_space_remaining;

    render_callback_t render_word;
    page_space_callback_t page_has_space;

    FIL *currentFile;
    FSIZE_t currentFileOffset;
} microMD_parser_t;

static inline bool is_style_active(style_set_t *style_set, int style) {
    return style_set->flags & style;
}

static inline void set_style(style_set_t *style_set, int style) {
    style_set->flags |= style;
}

static inline void reset_style(style_set_t *style_set, int style) {
    style_set->flags &= ~style;
}

void make_microMD_parser(microMD_parser_t *parser, render_callback_t render_callback, page_space_callback_t page_space_callback);
void destroy_microMD_parser(microMD_parser_t *parser);

void open_microMD_file(microMD_parser_t *parser, FIL *file, FSIZE_t offset);
void parse_page(microMD_parser_t *parser);

#endif