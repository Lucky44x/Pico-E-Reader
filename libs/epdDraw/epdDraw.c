#include "epdDraw-impl.h"
#include <stdlib.h>

#define ITALIC_DIVISOR 4

extern const uint8_t _binary_fonts_unifont_bin_start[];
extern const uint8_t _binary_fonts_unifont_bin_end[];

static inline void canvas_point_to_panel(const canvas_config_t *cfg, uint16_t sx, uint16_t sy, uint16_t *px, uint16_t *py)
{
    uint16_t x = sx, y = sy;

    switch (cfg->rotation) {
        case CANVAS_ROTATE_0:
            break;
        case CANVAS_ROTATE_90:
            x = (uint16_t)(cfg->widthMem  - 1 - sy);
            y = sx;
            break;
        case CANVAS_ROTATE_180:
            x = (uint16_t)(cfg->widthMem  - 1 - sx);
            y = (uint16_t)(cfg->heightMem - 1 - sy);
            break;
        case CANVAS_ROTATE_270:
            x = sy;
            y = (uint16_t)(cfg->heightMem - 1 - sx);
            break;
    }

    switch (cfg->mirror) {
        case MIRROR_HORIZONTAL: x = (uint16_t)(cfg->widthMem  - 1 - x); break;
        case MIRROR_VERTICAL:   y = (uint16_t)(cfg->heightMem - 1 - y); break;
        case MIRROR_ORIGIN:
            x = (uint16_t)(cfg->widthMem  - 1 - x);
            y = (uint16_t)(cfg->heightMem - 1 - y);
            break;
        default: break;
    }

    *px = x;
    *py = y;
}

/******************************************************************************
function :	Build config for Canvas
returns:    The constructed Canvas-Config
parameter:  
******************************************************************************/
canvas_config_t canvas_build(uint8_t colorLevels, uint16_t rotation, uint8_t color) {
    uint16_t canvasWidth = (rotation == CANVAS_ROTATE_270 || rotation == CANVAS_ROTATE_90) ? EPD_HEIGHT : EPD_WIDTH;
    uint16_t canvasHeight = (rotation == CANVAS_ROTATE_270 || rotation == CANVAS_ROTATE_90) ? EPD_WIDTH : EPD_HEIGHT;

    canvas_config_t c = {
        .driverConfig=epd_spi0_default_config,                // Driver config
        .frameBuffer=NULL,                                   // Framebuffer ptr - uninitialized, so NULL
        .width=canvasWidth, .height=canvasHeight,              // Canvas Size
        .widthMem=EPD_WIDTH,.heightMem=EPD_HEIGHT,                  // Memory Size
        .widthBytes=0,.heightBytes=EPD_HEIGHT,                           // Width and Height Bytes
        .rotation=rotation, 
        .color=color, 
        .mirror=MIRROR_ORIGIN,
        .colorscale=0                                       // Scale
    };
    canvas_set_colorscale(&c, colorLevels);
    return c;
}

/******************************************************************************
function :	Initializes the Canvas and EPD Driver
parameter:  cfg
******************************************************************************/
void canvas_init(canvas_config_t *cfg) {
    if (epd_driver_init(&cfg->driverConfig) != 0) {
        panic("Could not intialize Driver for EPD-Draw Canvas");
        return;
    }  

    if ( cfg->colorscale == 4 ) epd_init_gray(&cfg->driverConfig);
    else epd_init(&cfg->driverConfig);
    //epd_clear(&cfg->driverConfig);

    // Allocate Frame-Buffer
    uint8_t *frameBuffer;
    uint16_t bufferLen = (EPD_WIDTH / 8) * EPD_HEIGHT * 2;
    if ( (frameBuffer = (uint8_t *)malloc(bufferLen)) == NULL ) {
        panic("could not allocate %u Bytes for Canvas-Framebuffer...\r\n", bufferLen);
        return;
    }

    cfg->frameBuffer = frameBuffer;
}

/******************************************************************************
function :  Reinitializes the EPD-Driver based on changed color-depth
parameter:  cfg
******************************************************************************/
void canvas_update_color_depth(canvas_config_t *cfg) {
    canvas_clear(cfg, cfg->colorscale==4 ? CANVAS_COLOR_GRAY_G4 : CANVAS_COLOR_BW_WHITE);

    if ( cfg->colorscale == 4 ) epd_init_gray(&cfg->driverConfig);
    else epd_init(&cfg->driverConfig);
    //epd_clear(&cfg->driverConfig);
}

/******************************************************************************
function :	Destroys the canvas
parameter:  cfg
******************************************************************************/
void canvas_destroy(canvas_config_t *cfg) {
    free(cfg->frameBuffer);
    cfg->frameBuffer = NULL;
}

/******************************************************************************
function :	Sets the canvas's color-scale (2 - B/W, 4 - grayscale)
parameter:  cfg, color-scale
******************************************************************************/
void canvas_set_colorscale(canvas_config_t *cfg, uint8_t colorScale) {
    if ( colorScale == 2 ) {
        cfg->widthBytes = (cfg->widthMem % 8 == 0) ? (cfg->widthMem / 8) : (cfg->widthMem / 8 + 1);
    } else if ( colorScale == 4 ) {
        cfg->widthBytes = (cfg->widthMem % 4 == 0) ? (cfg->widthMem / 4) : (cfg->widthMem / 4 + 1);
    }
    cfg->colorscale = colorScale;
}

/******************************************************************************
function :	Sets the canvas's rotation
parameter:  cfg, rotation
******************************************************************************/
void canvas_set_rotation(canvas_config_t *cfg, uint16_t rotation) { cfg->rotation = rotation; }

/******************************************************************************
function :	Sets the canvas's mirroring
parameter:  cfg, mirror
******************************************************************************/
void canvas_set_mirror(canvas_config_t *cfg, uint8_t mirror) { cfg->mirror = mirror; }

/******************************************************************************
function :	Clears the canvas
parameter:  cfg, clearColor
******************************************************************************/
void canvas_clear(canvas_config_t *cfg, uint8_t clearColor) {
    if ( cfg->colorscale == 2 ) {
        for (uint16_t y = 0; y < cfg->heightBytes; y++ ) {
            for (uint16_t x = 0; x < cfg->widthBytes; x ++ ) {
                uint32_t addr = x + y * cfg->widthBytes;
                cfg->frameBuffer[addr] = (clearColor == CANVAS_COLOR_BW_WHITE) ? 0xFF : 0x00;
            }
        }
    } else {    // Scale == 4, 2bpp
        uint8_t c = (uint8_t)(clearColor & 0x03);
        uint8_t pat = (uint8_t)((c << 6) | (c << 4) | (c << 2) | c);    // replicate c across 4 pixels
        for (uint16_t y = 0; y < cfg->heightBytes; y++ ) {
            for (uint16_t x = 0; x < cfg->widthBytes; x ++ ) {
                uint32_t addr = x + y * cfg->widthBytes;
                cfg->frameBuffer[addr] = pat;
            }
        }
    }
}

/******************************************************************************
function :	Clears partial section of the canvas
parameter:  cfg, xStart, yStart, xEnd, yEnd, clearColor
******************************************************************************/
void canvas_clear_partial(canvas_config_t *cfg, uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint8_t clearColor) {
    uint16_t x,y;
    for( y = yStart; y < yEnd; y++ ) {
        for( x = xStart; x < xEnd; x++ ) {
            canvas_set_pixel(cfg, x, y, clearColor);
        }
    }
}

/******************************************************************************
function :	Draws a point at the given position
parameter:  cfg, xPoint, yPoint, color, dotSize, dotStyle
******************************************************************************/
void canvas_draw_point(canvas_config_t *cfg, uint16_t xPoint, uint16_t yPoint, uint8_t color, CANVAS_DOT_SIZE pixelStyle, CANVAS_DOT_STYLE fillStyle) {
    if ( xPoint >= cfg->width || yPoint >= cfg->height ) return;

    int16_t xDirNum, yDirNum;
    if ( fillStyle == DOT_FILL_AROUND ) {
        for ( xDirNum = 0; xDirNum < 2 * pixelStyle - 1; xDirNum++ ) {
            for ( yDirNum = 0; yDirNum < 2 * pixelStyle - 1; yDirNum++ ) {
                if ( xPoint + xDirNum - pixelStyle < 0 || yPoint + yDirNum - pixelStyle < 0 ) break;
                canvas_set_pixel(cfg, xPoint + xDirNum - pixelStyle, yPoint + yDirNum - pixelStyle, color);
            }
        }
    } else {
        for ( xDirNum = 0; xDirNum < pixelStyle; xDirNum++ ) {
            for ( yDirNum = 0; yDirNum < pixelStyle; yDirNum++ ) {
                canvas_set_pixel(cfg, xPoint + xDirNum - 1, yPoint + yDirNum - 1, color);
            }
        }
    }
}

/******************************************************************************
function :	Draws a Line on the canvas
parameter:  cfg, xStart, yStart, xEnd, yEnd, color, dotSize (lineWidth), lineStyle
******************************************************************************/
void canvas_draw_line(canvas_config_t *cfg, uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint8_t color, CANVAS_DOT_SIZE lineWidth, CANVAS_LINE_STYLE lineStyle) {
    if ( xStart > cfg->width|| yStart > cfg->height || xEnd > cfg->width || yEnd > cfg->height ) return;

    uint16_t xPoint = xStart;
    uint16_t yPoint = yStart;
    int dx = (int)xEnd - (int)xStart >= 0 ? xEnd - xStart : xStart - xEnd;
    int dy = (int)yEnd - (int)yStart <= 0 ? yEnd - yStart : yStart - yEnd;

    // Increment direction, 1 is positive, -1 is counter;
    int xAddway = xStart < xEnd ? 1 : -1;
    int yAddway = yStart < yEnd ? 1 : -1;

    //Cumulative error
    int esp = dx + dy;
    char dottedLen = 0;

    for (;;) {
        dottedLen++;
        //Painted dotted line, 2 point is really virtual
        if (lineStyle == LINE_STYLE_DOTTED && dottedLen % 3 == 0) {
            //Debug("LINE_DOTTED\r\n");
            canvas_draw_point(cfg, xPoint, yPoint, cfg->color, lineWidth, CANVAS_DOT_STYLE_DFT);
            dottedLen = 0;
        } else {
            canvas_draw_point(cfg, xPoint, yPoint, color, lineWidth, CANVAS_DOT_STYLE_DFT);
        }
        if (2 * esp >= dy) {
            if (xPoint == xEnd)
                break;
            esp += dy;
            xPoint += xAddway;
        }
        if (2 * esp <= dx) {
            if (yPoint == yEnd)
                break;
            esp += dx;
            yPoint += yAddway;
        }
    }
}

/******************************************************************************
function :  Draws a rect on the canvas
parameter:  cfg, xStart, yStart, xEnd, yEnd, color, dotSize (lineWidth), fillStyle
******************************************************************************/
void canvas_draw_rect(canvas_config_t *cfg, uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint8_t color, CANVAS_DOT_SIZE lineWidth, CANVAS_DRAW_FILL fillStyle) {
    if ( xStart > cfg->width || yStart > cfg->height || xEnd > cfg->width || yEnd > cfg->height ) return;

    if (fillStyle) {
        uint16_t yPoint;
        for(yPoint = yStart; yPoint < yEnd; yPoint++) {
            canvas_draw_line(cfg, xStart, yPoint, xEnd, yPoint, color , lineWidth, LINE_STYLE_SOLID);
        }
    } else {
        canvas_draw_line(cfg, xStart, yStart, xEnd, yStart, color, lineWidth, LINE_STYLE_SOLID);
        canvas_draw_line(cfg, xStart, yStart, xStart, yEnd, color, lineWidth, LINE_STYLE_SOLID);
        canvas_draw_line(cfg, xEnd, yEnd, xEnd, yStart, color, lineWidth, LINE_STYLE_SOLID);
        canvas_draw_line(cfg, xEnd, yEnd, xStart, yEnd, color, lineWidth, LINE_STYLE_SOLID);
    }
}

/******************************************************************************
function :  Draws a circle on the canvas
parameter:  cfg, xCenter, yCenter, radius, color, dotSize (lineWidth), fillStyle
******************************************************************************/
void canvas_draw_circle(canvas_config_t *cfg, uint16_t xCenter, uint16_t yCenter, uint16_t radius, uint8_t color, CANVAS_DOT_SIZE lineWidth, CANVAS_DRAW_FILL fillStyle) {
    if ( xCenter > cfg->width || yCenter >= cfg->height ) return;
    
    //Draw a circle from(0, R) as a starting point
    int16_t xCurrent, yCurrent;
    xCurrent = 0;
    yCurrent = radius;

    //Cumulative error,judge the next point of the logo
    int16_t esp = 3 - (radius << 1 );

    int16_t sCountY;
    if (fillStyle == DRAW_FILL_FULL) {
        while (xCurrent <= yCurrent ) { //Realistic circles
            for (sCountY = xCurrent; sCountY <= yCurrent; sCountY ++ ) {
                canvas_draw_point(cfg, xCenter + xCurrent, yCenter + sCountY, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);//1
                canvas_draw_point(cfg, xCenter - xCurrent, yCenter + sCountY, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);//2
                canvas_draw_point(cfg, xCenter - sCountY, yCenter + xCurrent, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);//3
                canvas_draw_point(cfg, xCenter - sCountY, yCenter - xCurrent, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);//4
                canvas_draw_point(cfg, xCenter - xCurrent, yCenter - sCountY, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);//5
                canvas_draw_point(cfg, xCenter + xCurrent, yCenter - sCountY, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);//6
                canvas_draw_point(cfg, xCenter + sCountY, yCenter - xCurrent, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);//7
                canvas_draw_point(cfg, xCenter + sCountY, yCenter + xCurrent, color, CANVAS_DOT_SIZE_DFT, CANVAS_DOT_STYLE_DFT);
            }
            if (esp < 0 )
                esp += 4 * xCurrent + 6;
            else {
                esp += 10 + 4 * (xCurrent - yCurrent );
                yCurrent --;
            }
            xCurrent ++;
        }
    } else { //Draw a hollow circle
        while (xCurrent <= yCurrent ) {
            canvas_draw_point(cfg, xCenter + xCurrent, yCenter + yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//1
            canvas_draw_point(cfg, xCenter - xCurrent, yCenter + yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//2
            canvas_draw_point(cfg, xCenter - xCurrent, yCenter + yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//3
            canvas_draw_point(cfg, xCenter - xCurrent, yCenter - yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//4
            canvas_draw_point(cfg, xCenter - xCurrent, yCenter - yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//5
            canvas_draw_point(cfg, xCenter + xCurrent, yCenter - yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//6
            canvas_draw_point(cfg, xCenter + xCurrent, yCenter - yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//7
            canvas_draw_point(cfg, xCenter + xCurrent, yCenter + yCurrent, color, lineWidth, CANVAS_DOT_STYLE_DFT);//0

            if (esp < 0 )
                esp += 4 * xCurrent + 6;
            else {
                esp += 10 + 4 * (xCurrent - yCurrent );
                yCurrent --;
            }
            xCurrent ++;
        }
    }
}

/******************************************************************************
function :  Draws the given UniCode codepoint on the canvas
parameter:  cfg, codepoint (UTF-8 codepoint), xPoint, yPoint, color
******************************************************************************/
uint8_t canvas_draw_char(canvas_config_t *cfg, text_style_t *style, uint16_t codepoint, uint16_t xPoint, uint16_t yPoint, uint8_t color ) {
    const uint8_t* font = _binary_fonts_unifont_bin_start;
    
    //Sanity check: char must be in BMP (U+0000 - U+FFFF)
    if (codepoint >= 0xFFFF)
        return 0;

    //Get 3byte offset from header
    uint32_t header_offset = codepoint * 3;
    uint32_t glyph_offset = font[header_offset] |
                            (font[header_offset + 1] << 8) |
                            (font[header_offset + 2] << 16);
    
    if (glyph_offset == 0)
        return 0;
    
    const uint8_t* glyph = font + glyph_offset;

    // First byte = WH byte
    uint8_t wh = glyph[0];
    uint8_t width = ((wh >> 4) & 0xF) + 1;
    uint8_t height = (wh & 0xF) + 1;

    // 4th byte = Offset to next character
    uint8_t advance = glyph[3];

    // Second byte = Offset byte
    uint8_t off = glyph[1];
    int8_t offX = (int8_t)(((off >> 4) & 0xF) << 4) >> 4;
    int8_t offY = (int8_t)((off & 0xF) << 4) >> 4;

    //1bpp, so total bits = width * height
    uint32_t bitmap_size = ((width * height) + 7) / 8;
    const uint8_t* bitmap = glyph + 4;

    //Render Pixel by Pixel
    for (uint8_t y = 0; y < height; ++y) {
        uint8_t italicShift = 0;
        if (style->italic) italicShift = y / ITALIC_DIVISOR;

        for (uint8_t x = 0; x < width; ++x) {
            uint32_t bit_index = y * width + x;
            uint8_t byte = bitmap[bit_index / 8];
            uint8_t bit = 7 - (bit_index % 8);
            if (byte & (1 << bit)) {
                // When bold, synthetically bolden the character by duplicating it's pixels to the right
                if (style->bold) {
                    canvas_set_pixel(cfg, xPoint + x + 1 - italicShift, yPoint + y, color);
                }
                canvas_set_pixel(cfg, xPoint + x - italicShift, yPoint + y, color);
            }
        }
    }

    if (style->italic) advance += height / ITALIC_DIVISOR; // Apply italic divisor to advance value
    if (style->bold) advance += 1;

    // Render Underline if style is set BUT ignore space
    if (style->underlined && codepoint != 32) {
        int8_t underLineOffset = glyph[2];
        canvas_draw_line(cfg, xPoint + offX, yPoint + height + 1 + underLineOffset, xPoint + advance, yPoint + height + 1 + underLineOffset, color, DOT_SIZE_1X1, LINE_STYLE_SOLID);
    }

    return advance;
}

/******************************************************************************
function :  Draws the given text (array of codepoints) on the canvas
parameter:  cfg, text-ptr, text-length, xPoint, yPoint, color, spacing, maxTextArea
******************************************************************************/
void canvas_draw_text(canvas_config_t *cfg, text_style_t *style, const uint16_t *text, size_t len, uint16_t xPoint, uint16_t yPoint, uint8_t color, uint8_t spacing, uint16_t maxTextArea) {
    uint16_t x = xPoint;
    uint16_t used_width = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t advance;
        /* TODO: Implement Truncation on demand and wrapping on demand
         = canvas_get_char_width(text[i]);
        
        bool needsTruncation = (i < len - 1) &&
                                (maxTextArea != 0) &&
                                (used_width + char_width + ellipsis_width > maxTextArea);
        if (needsTruncation) {
            //Draw "."
            for(int j = 0; j < 3; j ++) {
                x += canvas_draw_char(cfg, dot, x, yPoint, color) + spacing; //No NULL check needed, dots are always defined... otherwise its a shit font
            }
            return;
        }
        */

        //Redundency but what the hell, why not
        advance = canvas_draw_char(cfg, style, text[i], x, yPoint, color);
        x += advance;
        
        used_width += advance;
        /*
        if (i < len - 1) {
            if (advance > 0) { x += spacing; used_width += spacing; }
        }
        */
    }
}

/******************************************************************************
function :  Gets the screen-size of the given codepoint
returns:    The width of the codepoint when drawn on screen
parameter:  codepoint (UTF-8 Codepoint between U+0000 and U+FFFF)
******************************************************************************/
uint8_t canvas_get_char_width(uint16_t codepoint, text_style_t *style) {
    const uint8_t *font = _binary_fonts_unifont_bin_start;
    if (codepoint >= 0xFFFF) return 0;

    //Get 3 byte offset from header
    uint32_t header_offset = codepoint * 3;
    uint32_t glyph_offset = font[header_offset] |
                            ((font[header_offset + 1]) << 8 ) |
                            ((font[header_offset + 2]) << 16 );
    
    if (glyph_offset == 0) return 0;

    const uint8_t* glyph = font + glyph_offset;

    // First byte WH byte
    uint8_t wh = glyph[0];
    uint8_t width = ((wh >> 4) & 0xF) + 1;
    uint8_t height = (wh & 0xF) + 1;

    //4th byte = advance byte
    uint8_t advance = glyph[3];
    if (style->bold) advance += 1;
    if (style->italic) advance += height / ITALIC_DIVISOR;

    return advance;
   return 0;
}

/******************************************************************************
function :  Draws the given Bitmap on the canvas
parameter:  cfg, image-data ptr, xPoint, yPoint, width, height
******************************************************************************/
void canvas_draw_bitmap(canvas_config_t *cfg, const uint8_t *imageBuffer, uint16_t xPoint, uint16_t yPoint, uint16_t width, uint16_t height, uint8_t bpp, bool invert) {
    if (!imageBuffer) return;

    for ( uint16_t y = 0; y < height; ++y ) {
        for ( uint16_t x = 0; x < width; ++x ) {
            uint32_t i = (uint32_t)y * width + x;     // pixel index in the source
            uint8_t v = 0;

            if (bpp == 2) {
                uint8_t byte = imageBuffer[i >> 2];     // 4 pixels per byte
                uint8_t shift = 6 - ((i & 3) << 1);     // 6,4,2,0
                v = (byte >> shift) & 0x03;             // 0..3
            } else if (bpp == 1) {
                uint8_t byte = imageBuffer[i >> 3];     // 8 pixels per byte
                uint8_t shift = 7 - (i & 7);            // 6,4,2,0
                v = (byte >> shift) & 0x01;             // 0..1
                if (cfg->colorscale==4) v *= 0x03;      // Push values to 0..3 if we are in grayscale mode
            }

            if (cfg->colorscale == 4) {
                if (invert) v = 3 - v;
                // grayscale framebuffer: SetPixel expects 0..3 (0=whitest, 3=blackest)
                canvas_set_pixel(cfg, xPoint + x, yPoint + y, v);
            } else {
                if (bpp == 2) {
                    if (invert) v = 3 - v;
                    canvas_set_pixel(cfg, xPoint + x, yPoint + y, (v >= 2) ? CANVAS_COLOR_BW_BLACK : CANVAS_COLOR_BW_WHITE);
                }
                else {
                    if (invert) v = !v;
                    canvas_set_pixel(cfg, xPoint + x, yPoint + y, v ? CANVAS_COLOR_BW_BLACK : CANVAS_COLOR_BW_WHITE);
                }
            }
        }
    }
}

/******************************************************************************
function :	Pushes the current frame-buffer to the screen
parameter:  cfg
******************************************************************************/
void canvas_push_framebuffer(canvas_config_t *cfg) {
    if ( cfg->colorscale == 4 ) epd_display_gray(&cfg->driverConfig, cfg->frameBuffer);
    else epd_display(&cfg->driverConfig, cfg->frameBuffer);
    printf("Scale: %u\n", cfg->colorscale);
}

void canvas_refresh_screen(canvas_config_t *cfg) {
    canvas_push_framebuffer(cfg);
}

/******************************************************************************
function :	initializes the partial mode for the epd
parameter:  cfg
******************************************************************************/
void canvas_init_partial(canvas_config_t *cfg) {
    epd_prepare_partial(&cfg->driverConfig);
}

/******************************************************************************
function :	Pushes the provided portion of the frameBuffer to the screen
parameter:  cfg
******************************************************************************/
void canvas_refresh_partial(canvas_config_t *cfg, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Transform both corners to panel-space
    uint16_t px0, py0, px1, py1;
    canvas_point_to_panel(cfg, x0, y0, &px0, &py0);
    canvas_point_to_panel(cfg, x1, y1, &px1, &py1);
    
    if (px0 > px1) { uint16_t t = px0; px0 = px1; px1 = t; }
    if (py0 > py1) { uint16_t t = py0; py0 = py1; py1 = t; }

    if (px1 >= cfg->widthMem) px1 = cfg->widthMem - 1;
    if (py1 >= cfg->heightMem) py1 = cfg->heightMem - 1;

    epd_send_partial(&cfg->driverConfig, cfg->frameBuffer, cfg->widthBytes, px0, py0, px1, py1);
}

/******************************************************************************
function :	Updates the screen in fullscreen-mode but fast
parameter:  cfg
******************************************************************************/
void canvas_refresh_screen_fast(canvas_config_t *cfg) {

    if (cfg->colorscale==4) {
        static const uint16_t msg[16] = {'O','n','l','y', ' ', 'B', '/','W', ' ', 'a', 'l', 'l', 'o', 'w', 'e', 'd'};
        canvas_clear(cfg, CANVAS_COLOR_GRAY_G4);
        canvas_draw_text(cfg, &STYLE_DEFAULT, msg, 16, 8, 8, CANVAS_COLOR_GRAY_G1, 2, 0);
        canvas_refresh_screen(cfg);
        return;
    }

    epd_display_partial(&cfg->driverConfig, cfg->frameBuffer);
}

/******************************************************************************
function :	Sets the given pixel
parameter:  cfg, x, y, color
******************************************************************************/
void canvas_set_pixel(canvas_config_t *cfg, uint16_t xPoint, uint16_t yPoint, uint8_t color) {
    if ( cfg->frameBuffer == NULL ) { printf("Cannot write to frameBuffer NULL"); return; }                     // Uninitialized
    if ( xPoint >= cfg->width || yPoint >= cfg->height) return;   // Out of bounds
    uint16_t x,y;
    
    switch ( cfg->rotation )
    {
        case 0:
            x = xPoint;
            y = yPoint;
            break;
        case 90:
            x = cfg->widthMem - yPoint - 1;
            y = xPoint;
            break;
        case 180:
            x = cfg->widthMem - xPoint - 1;
            y = cfg->heightMem  - yPoint - 1;
            break;
        case 270:
            x = yPoint;
            y = cfg->heightMem - xPoint - 1;
        default:
            return;
    }

    switch ( cfg->mirror ) {
        case MIRROR_HORIZONTAL:
            x = cfg->widthMem - x - 1;
            break;
        case MIRROR_VERTICAL:
            y = cfg->heightMem - y - 1;
            break;
        case MIRROR_ORIGIN:
            x = cfg->widthMem - x - 1;
            y = cfg->heightMem - y - 1;
            break;
        case MIRROR_NONE:
            break;
        default:
            return;
    }

    if ( x > cfg->widthMem || y > cfg->heightMem ) { return; }

    // Set Colors
    if ( cfg->colorscale == 2 ) {
        uint32_t addr = x / 8 + y * cfg->widthBytes;
        uint8_t rData = cfg->frameBuffer[addr];
        if ( color == CANVAS_COLOR_BW_BLACK )
            cfg->frameBuffer[addr] = rData & ~(0x80 >> (x % 8));
        else
            cfg->frameBuffer[addr] = rData | (0x80 >> (x % 8));

    } else if ( cfg->colorscale == 4 ) {
        uint32_t addr = x / 4 + y * cfg->widthBytes;
        color = color % 4; // color scale is 0..3
        uint8_t rData = cfg->frameBuffer[addr];
        rData = rData & (~(0xC0 >> ((x % 4) * 2))); // Clear first, then set value
        cfg->frameBuffer[addr] = rData | ((color << 6) >> ((x % 4) * 2));
    }
}