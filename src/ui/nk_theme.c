/*
 * 色と文字（UX-8 のデザイントークン）
 */
#include "nk_theme.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define FONT_SLOTS 24

typedef struct font_slot {
    int32_t px;
    bool bold;
    lv_font_t *font;
} font_slot;

static struct {
    const void *data[2]; /* 0 本文、1 太字 */
    size_t size[2];
    int ascent[2]; /* hhea の値（font unit） */
    int descent[2];
    int units_per_em[2];
    font_slot slots[FONT_SLOTS];
    int count;
} s_theme;

static int read_u16(const uint8_t *p) {
    return ((int)p[0] << 8) | p[1];
}

static uint32_t read_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/*
 * TTF・TTC（先頭のフォント）の hhea から ascent・descent を読む。
 * Tiny TTF は行間（lineGap）を文字の下に足すため、日本語が上に寄って見える。
 * 行の高さを ascent − descent にして、文字を行の真ん中に置く
 */
static void read_metrics(int which) {
    const uint8_t *p = s_theme.data[which];
    size_t size = s_theme.size[which];
    s_theme.units_per_em[which] = 0;
    if (p == NULL || size < 16) {
        return;
    }
    uint32_t base = memcmp(p, "ttcf", 4) == 0 ? read_u32(p + 12) : 0;
    if ((size_t)base + 12 > size) {
        return;
    }
    uint32_t head = 0;
    uint32_t hhea = 0;
    int tables = read_u16(p + base + 4);
    for (int i = 0; i < tables; i++) {
        size_t rec = (size_t)base + 12 + (size_t)i * 16;
        if (rec + 16 > size) {
            return;
        }
        if (memcmp(p + rec, "head", 4) == 0) {
            head = read_u32(p + rec + 8);
        } else if (memcmp(p + rec, "hhea", 4) == 0) {
            hhea = read_u32(p + rec + 8);
        }
    }
    if (head == 0 || hhea == 0 || (size_t)head + 20 > size || (size_t)hhea + 8 > size) {
        return;
    }
    s_theme.units_per_em[which] = read_u16(p + head + 18);
    s_theme.ascent[which] = (int16_t)read_u16(p + hhea + 4);
    s_theme.descent[which] = (int16_t)read_u16(p + hhea + 6);
}

static const lv_font_t *builtin_font(int32_t px) {
    if (px <= 12) {
        return &lv_font_montserrat_12;
    }
    if (px <= 15) {
        return &lv_font_montserrat_14;
    }
    if (px <= 18) {
        return &lv_font_montserrat_16;
    }
    if (px <= 24) {
        return &lv_font_montserrat_20;
    }
    if (px <= 34) {
        return &lv_font_montserrat_28;
    }
    return &lv_font_montserrat_40;
}

void nk_theme_init(const void *regular, size_t regular_size, const void *bold, size_t bold_size) {
    memset(&s_theme, 0, sizeof(s_theme));
    /* 片方しかないときは、もう片方にも同じフォントを使う */
    s_theme.data[0] = regular != NULL ? regular : bold;
    s_theme.size[0] = regular != NULL ? regular_size : bold_size;
    s_theme.data[1] = bold != NULL ? bold : regular;
    s_theme.size[1] = bold != NULL ? bold_size : regular_size;
    read_metrics(0);
    read_metrics(1);
}

void nk_theme_deinit(void) {
    for (int i = 0; i < s_theme.count; i++) {
        lv_tiny_ttf_destroy(s_theme.slots[i].font);
    }
    memset(&s_theme, 0, sizeof(s_theme));
}

const lv_font_t *nk_font(int32_t px, bool bold) {
    for (int i = 0; i < s_theme.count; i++) {
        if (s_theme.slots[i].px == px && s_theme.slots[i].bold == bold) {
            return s_theme.slots[i].font;
        }
    }
    int which = bold ? 1 : 0;
    if (s_theme.data[which] == NULL || s_theme.count >= FONT_SLOTS) {
        return builtin_font(px);
    }
    lv_font_t *font = lv_tiny_ttf_create_data_ex(s_theme.data[which], s_theme.size[which], px,
                                                 LV_FONT_KERNING_NONE, LV_TINY_TTF_CACHE_GLYPH_CNT);
    if (font == NULL) {
        return builtin_font(px);
    }
    font->fallback = builtin_font(px); /* TTF にない記号（LV_SYMBOL_*）は Montserrat で描く */
    if (s_theme.units_per_em[which] > 0) {
        float em = (float)px / (float)s_theme.units_per_em[which];
        font->line_height =
            (int32_t)lroundf(em * (float)(s_theme.ascent[which] - s_theme.descent[which]));
        font->base_line = (int32_t)lroundf(em * (float)(-s_theme.descent[which]));
    }
    s_theme.slots[s_theme.count].px = px;
    s_theme.slots[s_theme.count].bold = bold;
    s_theme.slots[s_theme.count].font = font;
    s_theme.count++;
    return font;
}
