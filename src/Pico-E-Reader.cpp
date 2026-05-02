#include <stdio.h>
#include "pico/stdlib.h"

#include "Page_Renderer.hpp"

#define ONBOARD_LED 25

extern "C" {
    #include "ff.h"
    #include "tf_card.h"
}

canvas_config_t cfg;
text_style_t style = STYLE_DEFAULT;

FATFS fs;
FIL txtFile;

int init_file_system() {
    pico_fatfs_spi_config_t sdConfig = {
        .spi_inst = spi1,
        .clk_slow = 400000,
        .clk_fast = 12000000,
        .pin_miso = 12,
        .pin_cs = 13,
        .pin_sck = 10,
        .pin_mosi = 11,
        .pullup = true
    };

    gpio_init(13);
    gpio_set_dir(13, true);
    gpio_put(13, 1);
    sleep_ms(5);

    if(!pico_fatfs_set_config(&sdConfig)) {
        style.bold = true;
        canvas_draw_text_u8(&cfg, &style, "ERROR", 0, 0, CANVAS_COLOR_BW_BLACK, 0, 0);
        style.bold = false;
        canvas_draw_text_u8(&cfg, &style, "ERR: SPI Config failed", 0, 18, CANVAS_COLOR_BW_BLACK, 0, 0);
        printf("SPI config failed\n");
        return -1;
    }

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        style.bold = true;
        canvas_draw_text_u8(&cfg, &style, "ERROR", 0, 0, CANVAS_COLOR_BW_BLACK, 0, 0);
        style.bold = false;
        canvas_draw_text_u8(&cfg, &style, "Failed to mount filesystem", 0, 18, CANVAS_COLOR_BW_BLACK, 0, 0);
        printf("Failed to mount filesystem: %d\n", fr);
        return -1;
    }

    DIR dir;
    fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        style.bold = true;
        canvas_draw_text_u8(&cfg, &style, "ERROR", 0, 0, CANVAS_COLOR_BW_BLACK, 0, 0);
        style.bold = false;
        canvas_draw_text_u8(&cfg, &style, "Failed to open root directory", 0, 18, CANVAS_COLOR_BW_BLACK, 0, 0);
        printf("Failed to open root directory: %d\n", fr);
        return -1;
    }

    fr = f_open(&txtFile, "IDK.txt", FA_READ);
    if (fr != FR_OK) {
        style.bold = true;
        canvas_draw_text_u8(&cfg, &style, "ERROR", 0, 0, CANVAS_COLOR_BW_BLACK, 0, 0);
        style.bold = false;
        canvas_draw_text_u8(&cfg, &style, "Failed to open IDK.txt", 0, 18, CANVAS_COLOR_BW_BLACK, 0, 0);
        printf("Failed to open root directory: %d\n", fr);
        return -1;
    }

    return 0;
}

int main()
{
    stdio_init_all();

    gpio_init(ONBOARD_LED);
    gpio_set_dir(ONBOARD_LED, GPIO_OUT);
    gpio_put(ONBOARD_LED, 1);

    cfg = canvas_build(2, CANVAS_ROTATE_90, CANVAS_COLOR_BW_WHITE);
    style.bold = true;

    canvas_init(&cfg);
    sleep_ms(100);

    canvas_clear(&cfg, CANVAS_COLOR_BW_WHITE);

    // Initialize File-System
    bool FS_OK = init_file_system() == 0;
    if (!FS_OK) {
        canvas_refresh_screen(&cfg);
        return -1;
    }

    // Initialize the PageRenderer
    PageRenderer pageRend = PageRenderer(&cfg);

    // Open first page and render
    pageRend.OpenFile(&txtFile);

    printf("Finished parsing");

    while (true) {
        gpio_put(ONBOARD_LED, 0);
        sleep_ms(250);
        gpio_put(ONBOARD_LED, 1);
        sleep_ms(250);
    }
}
