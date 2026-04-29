#ifndef EPDDRAW_H
#define EPDDRAW_H

#include "pico/stdlib.h"
#include "stdio.h"
#include "epd2in9.h"

#define CANVAS_ROTATE_0 0
#define CANVAS_ROTATE_90 90
#define CANVAS_ROTATE_180 180
#define CANVAS_ROTATE_270 270

#define CANVAS_COLOR_BW_WHITE 0xFF
#define CANVAS_COLOR_BW_BLACK 0x00

#define CANVAS_COLOR_GRAY_G1 0x03  // Blackest
#define CANVAS_COLOR_GRAY_G2 0x02
#define CANVAS_COLOR_GRAY_G3 0x01
#define CANVAS_COLOR_GRAY_G4 0x00  // Whitest

typedef enum {
    MIRROR_NONE = 0x00,
    MIRROR_HORIZONTAL = 0x01,
    MIRROR_VERTICAL = 0x02,
    MIRROR_ORIGIN = 0x03,
} CANVAS_MIRROR;
#define CANVAS_MIRROR_DFT MIRROR_NONE

typedef enum {
    DOT_SIZE_1X1 = 1,
    DOT_SIZE_2X2,
    DOT_SIZE_3X3,
    DOT_SIZE_4X4,
    DOT_SIZE_5X5,
    DOT_SIZE_6X6,
    DOT_SIZE_7X7,
    DOT_SIZE_8X8,
} CANVAS_DOT_SIZE;
#define CANVAS_DOT_SIZE_DFT DOT_SIZE_1X1

typedef enum {
    DOT_FILL_AROUND = 1,
    DOT_FILL_RIGHTUP,
} CANVAS_DOT_STYLE;
#define CANVAS_DOT_STYLE_DFT DOT_FILL_AROUND

typedef enum {
    LINE_STYLE_SOLID = 0,
    LINE_STYLE_DOTTED,
} CANVAS_LINE_STYLE;

typedef enum {
    DRAW_FILL_EMPTY = 0,
    DRAW_FILL_FULL,
} CANVAS_DRAW_FILL;

typedef struct {
    bool bold, italic, underlined, strikethrough, truncate, wrap;
} text_style_t;

static text_style_t STYLE_DEFAULT = {
    false, false, false, false, false, false
};

typedef struct {
    epd_config_t driverConfig;
    uint8_t *frameBuffer;
    uint16_t width;
    uint16_t height;
    uint16_t widthMem;
    uint16_t heightMem;
    uint16_t widthBytes;
    uint16_t heightBytes;
    uint16_t rotation;
    uint8_t color;
    uint8_t mirror;
    uint8_t colorscale;
} canvas_config_t;

// Life-Cycle functions
canvas_config_t canvas_build(uint8_t colorLevels, uint16_t rotation, uint8_t color);
void canvas_init(canvas_config_t *cfg);
void canvas_destroy(canvas_config_t *cfg);
void canvas_update_color_depth(canvas_config_t *cfg);

// Properties
void canvas_set_mirror(canvas_config_t *cfg, uint8_t mirror);
void canvas_set_rotation(canvas_config_t *cfg, uint16_t rotation);
void canvas_set_colorscale(canvas_config_t *cfg, uint8_t colorscale);

// Clearing
void canvas_clear(canvas_config_t *cfg, uint8_t color);
void canvas_clear_partial(canvas_config_t *cfg, uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint8_t color);

// Drawing
void canvas_draw_point(canvas_config_t *cfg, uint16_t xPoint, uint16_t yPoint, uint8_t color, CANVAS_DOT_SIZE pixelStyle, CANVAS_DOT_STYLE fillStyle);
void canvas_draw_line(canvas_config_t *cfg, uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint8_t color, CANVAS_DOT_SIZE lineWidth, CANVAS_LINE_STYLE lineStyle);
void canvas_draw_rect(canvas_config_t *cfg, uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint8_t color, CANVAS_DOT_SIZE lineWidth, CANVAS_DRAW_FILL fillStyle);
void canvas_draw_circle(canvas_config_t *cfg, uint16_t xCenter, uint16_t yCenter, uint16_t radius, uint8_t color, CANVAS_DOT_SIZE lineWidth, CANVAS_DRAW_FILL fillStyle);
uint8_t canvas_draw_char(canvas_config_t *cfg, text_style_t *style, uint16_t character, uint16_t xPoint, uint16_t yPoint, uint8_t color);
void canvas_draw_text(canvas_config_t *cfg, text_style_t *style, const uint16_t *text, size_t len, uint16_t xPoint, uint16_t yPoint, uint8_t color, uint8_t spacing, uint16_t maxTextArea);
uint8_t canvas_get_char_width(uint16_t character, text_style_t *style);
void canvas_draw_bitmap(canvas_config_t *cfg, const uint8_t *imageBuffer, uint16_t xPoint, uint16_t yPoint, uint16_t width, uint16_t height, uint8_t bpp, bool invert);

// General
void canvas_refresh_screen(canvas_config_t *cfg);
void canvas_refresh_partial(canvas_config_t *cfg, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void canvas_init_partial(canvas_config_t *cfg);
void canvas_refresh_screen_fast(canvas_config_t *cfg);

#endif //EPDDRAW_H