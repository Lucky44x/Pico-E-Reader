#ifndef EPD_2IN9_H
#define EPD_2IN9_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "stdio.h"

#define EPD_WIDTH   128
#define EPD_HEIGHT  296

typedef struct {
    int pin_rst;
    int pin_dc;
    int pin_cs;
    int pin_busy;
    int pin_clk;
    int pin_mosi;
    spi_inst_t *epd_port_spi;
} epd_config_t;

static epd_config_t epd_spi0_default_config = {
    20, 16, 17, 21, 18, 19, (spi_inst_t*)spi0
};

epd_config_t epd_build(int rst, int dc, int cs, int busy, int clk, int mosi, spi_inst_t *port);
void epd_init(epd_config_t *cfg);
void epd_init_gray(epd_config_t *cfg);
void epd_clear(epd_config_t *cfg);
void epd_display(epd_config_t *cfg, uint8_t *image_data);
void epd_display_base(epd_config_t *cfg, uint8_t *image_data);
void epd_display_gray(epd_config_t *cfg, uint8_t *image_data);
void epd_display_partial(epd_config_t *cfg, uint8_t *image_data);
void epd_sleep(epd_config_t *cfg);
void epd_reset(epd_config_t *cfg);

uint8_t epd_driver_init(epd_config_t *cfg);
void epd_refresh_full(epd_config_t *cfg);
void epd_refresh_partial(epd_config_t *cfg);
void epd_set_partial(epd_config_t *cfg, uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd);
void epd_set_cursor(epd_config_t *cfg, uint16_t xStart, uint16_t yStart);

void epd_send_partial(epd_config_t *cfg, const uint8_t *buffer, uint16_t fb_width_bytes, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void epd_prepare_partial(epd_config_t *cfg);

#endif //EPD_2IN9_H