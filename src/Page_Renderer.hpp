#ifndef PAGE_RENDERER_H
#define PAGE_RENDERER_H

extern "C" {
    #include "microMD.h"
    #include "epdDraw.h"
}

#define LINES_ON_PAGE 7
#define PIXELS_IN_LINE 296

static void convert_style_sheet(text_style_t *text_style, style_set_t *style_set) {
    text_style->bold = is_style_active(style_set, STYLE_BOLD);
    text_style->italic = is_style_active(style_set, STYLE_ITALIC);
    text_style->underlined = is_style_active(style_set, STYLE_UNDERLINED);
    text_style->dotted = is_style_active(style_set, STYLE_DOTTED);
    text_style->strikethrough = is_style_active(style_set, STYLE_STRIKETHROUGH);
}

class PageRenderer {
    public:
        explicit PageRenderer(canvas_config_t *canvas);
        void OpenFile(FIL *file);
        void NextPage();
    private:
        static bool WrapperRenderWord(style_set_t *style, uint16_t *word, uint8_t wordLen) { return PageRenderer::instance->RenderWord(style, word, wordLen); }
        static bool WrapperHasPageSpace();

        bool RenderWord(style_set_t *style, uint16_t *word, uint8_t wordLen);
        void LineBreak();

        static PageRenderer* instance;

        canvas_config_t *canvas;
        microMD_parser_t parser;
        text_style_t text_style;

        uint32_t remaining_pixels_in_line;
        uint32_t remaining_lines_on_page;
        uint32_t current_line, current_index;
};

#endif