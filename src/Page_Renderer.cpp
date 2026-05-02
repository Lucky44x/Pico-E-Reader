#include "Page_Renderer.hpp"

PageRenderer* PageRenderer::instance = nullptr;

PageRenderer::PageRenderer(canvas_config_t *canvas) {
    PageRenderer::instance = this;
    this->canvas = canvas;
    make_microMD_parser(&parser, WrapperRenderWord, WrapperHasPageSpace);
}

bool PageRenderer::WrapperHasPageSpace() {
    if (PageRenderer::instance->remaining_lines_on_page > 0) return true;
    if (PageRenderer::instance->remaining_pixels_in_line > 0) return true;
    return false;
}

void PageRenderer::OpenFile(FIL *file) {
    //TODO: Somehow cache offset on last close etc...
    open_microMD_file(&parser, file, 0); // Start at the top of the file
    NextPage();
}

void PageRenderer::NextPage() {
    canvas_clear(canvas, CANVAS_COLOR_BW_WHITE);
    remaining_lines_on_page = LINES_ON_PAGE;
    remaining_pixels_in_line = PIXELS_IN_LINE;
    current_line = 0;
    current_index = 0;

    parse_page(&parser); // Should block and render until no space left on page
    canvas_refresh_screen(canvas); // Refresh screen at end
}

bool PageRenderer::RenderWord(style_set_t *style, uint16_t *word, uint8_t wordLen) {
    convert_style_sheet(&text_style, style);

    // Debug print the word (convert to UTF-8 first)
    printf("Received Draw Command: \n");
    print_str16(word, wordLen);
    printf("\n");

    // If page-break is set, immediatly return false
    if (is_style_active(style, STYLE_PAGE_BREAK)) {
        reset_style(style, STYLE_PAGE_BREAK);
        return false;
    }

    uint16_t word_width = canvas_get_word_width(word, wordLen, &text_style);

    // TODO: handle special cases, like spaces at line-end (could theoretically be truncated)
    if (word_width > remaining_pixels_in_line) {
        if (remaining_lines_on_page == 0) {
            printf("No more remaining lines");
            return false; // Reached end of page
        }
        LineBreak();
    }

    // Render Word at new position
    canvas_draw_text(canvas, &text_style, word, wordLen, current_index, current_line * 18, CANVAS_COLOR_BW_BLACK, 0, 0);
    // Update counters
    current_index += word_width;
    if (current_index >= EPD_HEIGHT) {
        printf("index %d overrun %d", current_index, EPD_HEIGHT);
        return false; // If our index exceeds 295, we can assume something went wrong
    }
    remaining_pixels_in_line -= word_width > remaining_pixels_in_line ? remaining_pixels_in_line : word_width;

    if (style->newLine) {
        LineBreak();
        style->newLine = false;
    }

    return true;
}

void PageRenderer::LineBreak() {
    if (remaining_lines_on_page == 0) return;
    current_line++;
    remaining_lines_on_page -= 1;
    current_index = 0;
    remaining_pixels_in_line = PIXELS_IN_LINE;
}