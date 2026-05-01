#ifndef _MICRO_MD_IMPL_H
#define _MICRO_MD_IMPL_H

#include "microMD.h"

static inline bool check_bit(uint8_t value, uint8_t offset) {
    uint8_t mask = 1 << (7 - offset);
    return (value & mask) & 1;
}

void setup_ring_buffer(uint8_t **buffer, size_t length, uint16_t *tail, uint16_t *head);
void teardown_ring_buffer(uint8_t *buffer);

void refill_text_rbuffer(microMD_parser_t *parser);
size_t enqueue_rbuffer(microMD_parser_t *parser, uint8_t *data, size_t len);
uint8_t read_rbuffer(microMD_parser_t *parser);

bool decode_next_utf8_from_buffer(microMD_parser_t *parser, uint16_t *codepoint_out);

#endif