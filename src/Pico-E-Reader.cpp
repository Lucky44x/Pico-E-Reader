#include <stdio.h>
#include "pico/stdlib.h"

extern "C" {
    #include "epdDraw.h"
}

int main()
{
    stdio_init_all();

    canvas_config_t cfg = canvas_build(2, CANVAS_ROTATE_90, CANVAS_COLOR_BW_WHITE);
    /*
        Bold, Italic, Underlined, Strikethrough, Truncate, Wrap
    */
    text_style_t style = {
        true, true, true, false, false, false
    };
    canvas_init(&cfg);
    sleep_ms(100);

    canvas_clear(&cfg, CANVAS_COLOR_BW_WHITE);
    const uint16_t message[11] = {'H','E','L','L','O',' ','W','O','R','L','D'};

    canvas_draw_text(&cfg, &style, message, 11, 8, 8, CANVAS_COLOR_BW_BLACK, 0, 0);

    canvas_refresh_screen(&cfg);

    while (true) {
        printf("Alive\n");
        sleep_ms(1000);
    }
}
