#include "ui_font.h"

#include <stdio.h>
#include <stdlib.h>

static lv_font_t *s_jp_font = NULL;
static uint8_t *s_font_buf = NULL;

bool nk_ui_font_init(lv_display_t *disp, const char *ttf_path, int32_t font_size) {
    if (disp == NULL || ttf_path == NULL) {
        return false;
    }

    FILE *fp = fopen(ttf_path, "rb");
    if (fp == NULL) {
        printf("[Font Error] File not found: %s\n", ttf_path);
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        printf("[Font Error] Invalid file size: %ld\n", size);
        fclose(fp);
        return false;
    }

    s_font_buf = (uint8_t *)malloc((size_t)size);
    if (s_font_buf == NULL) {
        printf("[Font Error] malloc failed (%ld bytes)\n", size);
        fclose(fp);
        return false;
    }

    size_t read_size = fread(s_font_buf, 1, (size_t)size, fp);
    fclose(fp);

    if (read_size != (size_t)size) {
        printf("[Font Error] fread failed\n");
        free(s_font_buf);
        s_font_buf = NULL;
        return false;
    }

    s_jp_font = lv_tiny_ttf_create_data(s_font_buf, (size_t)size, font_size);
    if (s_jp_font == NULL) {
        printf(
            "[Font Error] lv_tiny_ttf_create_data failed (Check LV_USE_TINY_TTF & LV_MEM_SIZE)\n");
        free(s_font_buf);
        s_font_buf = NULL;
        return false;
    }

    printf("[Font OK] Loaded %s (%ld bytes)\n", ttf_path, size);

    lv_theme_t *th = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                           lv_palette_main(LV_PALETTE_GREY), false, s_jp_font);
    lv_display_set_theme(disp, th);

    return true;
}

void nk_ui_font_deinit(void) {
    if (s_jp_font != NULL) {
        lv_tiny_ttf_destroy(s_jp_font);
        s_jp_font = NULL;
    }
    if (s_font_buf != NULL) {
        free(s_font_buf);
        s_font_buf = NULL;
    }
}
