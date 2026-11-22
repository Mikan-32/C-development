/*
 * テーマとフォント生成（Tiny TTF + UX-8 文字サイズ倍率対応）
 */
#include "nk_theme.h"

#define MAX_CACHED_FONTS 64

typedef struct {
    int32_t px;
    bool bold;
    lv_font_t *font;
} nk_font_cache_entry_t;

static const void *s_reg_data = NULL;
static size_t s_reg_size = 0;
static const void *s_bold_data = NULL;
static size_t s_bold_size = 0;
static nk_font_scale s_font_scale = NK_FONT_SCALE_100;
static nk_font_cache_entry_t s_cache[MAX_CACHED_FONTS];
static int s_cache_count = 0;

void nk_theme_init(const void *regular, size_t regular_size, const void *bold, size_t bold_size) {
    nk_theme_deinit();
    s_reg_data = regular;
    s_reg_size = regular_size;
    s_bold_data = bold;
    s_bold_size = bold_size;
    s_font_scale = NK_FONT_SCALE_100;
}

void nk_theme_deinit(void) {
    for (int i = 0; i < s_cache_count; i++) {
        if (s_cache[i].font != NULL) {
            lv_tiny_ttf_destroy(s_cache[i].font);
            s_cache[i].font = NULL;
        }
    }
    s_cache_count = 0;
    s_reg_data = NULL;
    s_reg_size = 0;
    s_bold_data = NULL;
    s_bold_size = 0;
}

void nk_theme_set_font_scale(nk_font_scale scale) {
    if (scale >= NK_FONT_SCALE_100 && scale <= NK_FONT_SCALE_140) {
        s_font_scale = scale;
    }
}

nk_font_scale nk_theme_get_font_scale(void) {
    return s_font_scale;
}

int32_t nk_theme_scale_px(int32_t base_px) {
    if (s_font_scale == NK_FONT_SCALE_120) {
        return (base_px * 12 + 5) / 10;
    }
    if (s_font_scale == NK_FONT_SCALE_140) {
        return (base_px * 14 + 5) / 10;
    }
    return base_px;
}

const lv_font_t *nk_font(int32_t px, bool bold) {
    int32_t scaled_px = nk_theme_scale_px(px);
    if (scaled_px <= 0) {
        scaled_px = 12;
    }

    for (int i = 0; i < s_cache_count; i++) {
        if (s_cache[i].px == scaled_px && s_cache[i].bold == bold && s_cache[i].font != NULL) {
            return s_cache[i].font;
        }
    }

    const void *data = (bold && s_bold_data != NULL) ? s_bold_data : s_reg_data;
    size_t size = (bold && s_bold_data != NULL) ? s_bold_size : s_reg_size;
    if (data == NULL || size == 0) {
        return lv_font_get_default();
    }

    lv_font_t *created = lv_tiny_ttf_create_data(data, size, scaled_px);
    if (created == NULL) {
        return lv_font_get_default();
    }

    if (s_cache_count < MAX_CACHED_FONTS) {
        s_cache[s_cache_count].px = scaled_px;
        s_cache[s_cache_count].bold = bold;
        s_cache[s_cache_count].font = created;
        s_cache_count++;
    }
    return created;
}
