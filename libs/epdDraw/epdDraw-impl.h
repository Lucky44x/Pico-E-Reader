#ifndef EPDDRAW_IMPL_H
#define EPDDRAW_IMPL_H

#include "epdDraw.h"

void canvas_set_pixel(canvas_config_t *cfg, uint16_t xPoint, uint16_t yPoint, uint8_t color);
void canvas_push_framebuffer(canvas_config_t *cfg);

#endif //EPDDRAW_H