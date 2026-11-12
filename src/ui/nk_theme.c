/*
 * 色と文字（デザイントークン）
 */
#include "nk_theme.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define FONT_SLOTS   32
#define FONT_REGULAR 0
#define FONT_BOLD    1

typedef struct font_slot {
    int32_t px;
    bool bold;
    lv_font_t *font;
} font_slot;

/* フォントの縦の寸法（hhea 表。単位は font unit） */
typedef struct font_metrics {
    bool valid;
    int units_per_em;
    int ascent;
    int descent; /* 負の値 */
} font_metrics;

static struct {
    const void *data[2];
    size_t size[2];
    font_metrics metrics[2];
    float scale;
    font_slot slots[FONT_SLOTS];
    int count;
    font_slot retired[FONT_SLOTS];
    int retired_count;
} s_theme;

static uint32_t read_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int read_i16(const uint8_t *p) {
    return (int)(int16_t)(uint16_t)(((unsigned)p[0] << 8) | p[1]);
}

/*
 * TTF・TTC（先頭のフォント）から、hhea の ascent・descent と head の unitsPerEm を読む。
 * Tiny TTF は行間（lineGap）を文字の下に足すため、日本語が上に寄って見える。
 * 行の高さを ascent − descent にして、行間は部品ごとに text_line_space で付ける
 */
static font_metrics read_metrics(const void *data, size_t size) {
    font_metrics m = {false, 0, 0, 0};
    const uint8_t *p = data;
    if (p == NULL || size < 12) {
        return m;
    }
    uint32_t base = 0;
    if (memcmp(p, "ttcf", 4) == 0) {
        if (size < 16) {
            return m;
        }
        base = read_u32(p + 12);
    }
    if ((size_t)base + 12 > size) {
        return m;
    }
    unsigned tables = ((unsigned)p[base + 4] << 8) | p[base + 5];
    uint32_t head = 0;
    uint32_t hhea = 0;
    for (unsigned i = 0; i < tables; i++) {
        size_t rec = (size_t)base + 12 + (size_t)i * 16;
        if (rec + 16 > size) {
            return m;
        }
        if (memcmp(p + rec, "head", 4) == 0) {
            head = read_u32(p + rec + 8);
        } else if (memcmp(p + rec, "hhea", 4) == 0) {
            hhea = read_u32(p + rec + 8);
        }
    }
    if (head == 0 || hhea == 0 || (size_t)head + 20 > size || (size_t)hhea + 10 > size) {
        return m;
    }
    m.units_per_em = ((int)p[head + 18] << 8) | p[head + 19];
    m.ascent = read_i16(p + hhea + 4);
    m.descent = read_i16(p + hhea + 6);
    m.valid = m.units_per_em > 0 && m.ascent > 0 && m.descent <= 0;
    return m;
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

static void destroy_slots(font_slot *slots, int *count) {
    for (int i = 0; i < *count; i++) {
        lv_tiny_ttf_destroy(slots[i].font);
        slots[i].font = NULL;
    }
    *count = 0;
}

static void apply_default_theme(void) {
    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        return;
    }
    lv_theme_t *theme = lv_theme_default_init(disp, NK_BRAND, NK_SERIES, true, NK_FR(15));
    lv_display_set_theme(disp, theme);
}

void nk_theme_init(const void *regular, size_t regular_size, const void *bold, size_t bold_size,
                   float scale) {
    /* 片方しかないときは、もう片方にも同じフォントを使う */
    s_theme.data[FONT_REGULAR] = regular != NULL ? regular : bold;
    s_theme.size[FONT_REGULAR] = regular != NULL ? regular_size : bold_size;
    s_theme.data[FONT_BOLD] = bold != NULL ? bold : regular;
    s_theme.size[FONT_BOLD] = bold != NULL ? bold_size : regular_size;
    s_theme.metrics[FONT_REGULAR] =
        read_metrics(s_theme.data[FONT_REGULAR], s_theme.size[FONT_REGULAR]);
    s_theme.metrics[FONT_BOLD] = read_metrics(s_theme.data[FONT_BOLD], s_theme.size[FONT_BOLD]);
    s_theme.scale = scale > 0.5f ? scale : 1.0f;
    s_theme.count = 0;
    s_theme.retired_count = 0;
    apply_default_theme();
}

void nk_theme_set_font_scale(float scale) {
    /* 古いフォントは、使っている画面を消すまで残す */
    nk_theme_release_retired();
    for (int i = 0; i < s_theme.count; i++) {
        s_theme.retired[i] = s_theme.slots[i];
    }
    s_theme.retired_count = s_theme.count;
    s_theme.count = 0;
    s_theme.scale = scale > 0.5f ? scale : 1.0f;
    apply_default_theme();
}

float nk_theme_font_scale(void) {
    return s_theme.scale;
}

void nk_theme_release_retired(void) {
    destroy_slots(s_theme.retired, &s_theme.retired_count);
}

void nk_theme_deinit(void) {
    nk_theme_release_retired();
    destroy_slots(s_theme.slots, &s_theme.count);
    s_theme.data[FONT_REGULAR] = NULL;
    s_theme.data[FONT_BOLD] = NULL;
}

const lv_font_t *nk_font(int32_t px, bool bold) {
    for (int i = 0; i < s_theme.count; i++) {
        if (s_theme.slots[i].px == px && s_theme.slots[i].bold == bold) {
            return s_theme.slots[i].font;
        }
    }

    int which = bold ? FONT_BOLD : FONT_REGULAR;
    int32_t real_px = (int32_t)lroundf((float)px * (s_theme.scale > 0.5f ? s_theme.scale : 1.0f));
    if (s_theme.data[which] == NULL || s_theme.count >= FONT_SLOTS) {
        return builtin_font(real_px);
    }

    lv_font_t *font = lv_tiny_ttf_create_data_ex(s_theme.data[which], s_theme.size[which], real_px,
                                                 LV_FONT_KERNING_NONE, LV_TINY_TTF_CACHE_GLYPH_CNT);
    if (font == NULL) {
        return builtin_font(real_px);
    }
    /* TTF にない記号（LV_SYMBOL_*）は Montserrat で描く */
    font->fallback = builtin_font(real_px);
    const font_metrics *m = &s_theme.metrics[which];
    if (m->valid) {
        float em = (float)real_px / (float)m->units_per_em;
        font->line_height = (int32_t)lroundf(em * (float)(m->ascent - m->descent));
        font->base_line = (int32_t)lroundf(em * (float)(-m->descent));
    }

    s_theme.slots[s_theme.count].px = px;
    s_theme.slots[s_theme.count].bold = bold;
    s_theme.slots[s_theme.count].font = font;
    s_theme.count++;
    return font;
}

lv_color_t nk_theme_score_color(int score, bool reference) {
    if (reference) {
        return NK_INK_3;
    }
    if (score >= 70) {
        return NK_GOOD;
    }
    if (score >= 55) {
        return NK_WARN;
    }
    return NK_SERIOUS;
}

const char *nk_theme_score_label(int score) {
    if (score >= 85) {
        return "とても良い";
    }
    if (score >= 70) {
        return "良い";
    }
    if (score >= 55) {
        return "まずまず";
    }
    return "回復を優先";
}
