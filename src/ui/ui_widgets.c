/*
 * 画面の部品（カード、ボタン、札、状態の表示、入力、シート、トースト）。
 * 色と形は画面モック v2 の CSS（UX-8 のデザイントークン）
 */
#include "ui_internal.h"

#include <stdint.h>

lv_color_t nk_tone_color(nk_tone tone) {
    switch (tone) {
    case NK_TONE_INFO:
        return NK_SPIN;
    case NK_TONE_GOOD:
        return NK_GOOD;
    case NK_TONE_WARN:
        return NK_WARN;
    case NK_TONE_SERIOUS:
        return NK_SERIOUS;
    case NK_TONE_NEUTRAL:
    default:
        return NK_INK_2;
    }
}

/* ---------------------------------------------------------------- 入れ物 */

lv_obj_t *nk_ui_box(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_clickable(obj, false);
    return obj;
}

lv_obj_t *nk_ui_row(lv_obj_t *parent, int32_t gap) {
    lv_obj_t *row = nk_ui_box(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, gap, 0);
    return row;
}

lv_obj_t *nk_ui_col(lv_obj_t *parent, int32_t gap) {
    lv_obj_t *col = nk_ui_box(parent);
    lv_obj_set_width(col, LV_PCT(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, gap, 0);
    return col;
}

lv_obj_t *nk_ui_spacer(lv_obj_t *row) {
    lv_obj_t *obj = nk_ui_box(row);
    lv_obj_set_height(obj, 1);
    lv_obj_set_flex_grow(obj, 1);
    return obj;
}

lv_obj_t *nk_ui_gap(lv_obj_t *parent, int32_t height) {
    lv_obj_t *obj = nk_ui_box(parent);
    lv_obj_set_size(obj, 1, height);
    return obj;
}

/* ---------------------------------------------------------------- 文字 */

lv_obj_t *nk_ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t *nk_ui_text(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color) {
    lv_obj_t *label = nk_ui_label(parent, text, font, color);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    nk_ui_line_space(label, lv_font_get_line_height(font) / 3);
    return label;
}

void nk_ui_line_space(lv_obj_t *label, int32_t space) {
    lv_obj_set_style_text_line_space(label, space, 0);
}

/* ---------------------------------------------------------------- カード */

static void style_line_border(lv_obj_t *obj, lv_opa_t opa) {
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, NK_WHITE, 0);
    lv_obj_set_style_border_opa(obj, opa, 0);
}

lv_obj_t *nk_ui_card(lv_obj_t *parent) {
    lv_obj_t *card = nk_ui_col(parent, 10);
    lv_obj_set_style_bg_color(card, NK_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, NK_RADIUS, 0);
    lv_obj_set_style_pad_all(card, NK_PAD, 0);
    style_line_border(card, NK_OPA_LINE);
    return card;
}

lv_obj_t *nk_ui_card_head(lv_obj_t *card, const char *title) {
    lv_obj_t *head = nk_ui_row(card, 8);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t *left = nk_ui_box(head);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 6, 0);
    nk_ui_label(left, title, NK_FB(15), NK_INK);
    return head;
}

lv_obj_t *nk_ui_hint(lv_obj_t *parent, const char *text) {
    return nk_ui_label(parent, text, NK_FR(12), NK_INK_3);
}

lv_obj_t *nk_ui_tile(lv_obj_t *parent) {
    lv_obj_t *tile = nk_ui_col(parent, 2);
    lv_obj_set_style_bg_color(tile, NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, NK_RADIUS_S, 0);
    lv_obj_set_style_pad_hor(tile, 11, 0);
    lv_obj_set_style_pad_ver(tile, 10, 0);
    return tile;
}

/* ---------------------------------------------------------------- 見出し */

lv_obj_t *nk_ui_title(lv_obj_t *body, const char *title) {
    lv_obj_t *row = nk_ui_row(body, 10);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, 8, 0);
    lv_obj_set_style_pad_bottom(row, 4, 0);
    nk_ui_label(row, title, NK_FB(24), NK_INK);
    return row;
}

static void back_cb(lv_event_t *e) {
    (void)e;
    (void)nk_ui_back();
}

lv_obj_t *nk_ui_subhead(lv_obj_t *body, const char *title) {
    /* 左に「‹ 前の画面」、真ん中に見出し（UX-4・UX-5 改訂版の図） */
    lv_obj_t *bar = nk_ui_box(body);
    lv_obj_set_size(bar, LV_PCT(100), 44);

    lv_obj_t *back = nk_ui_box(bar);
    lv_obj_set_flex_flow(back, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(back, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(back, 2, 0);
    lv_obj_set_height(back, NK_TOUCH_MIN);
    lv_obj_set_clickable(back, true);
    lv_obj_set_ext_click_area(back, 6);
    lv_obj_set_style_opa(back, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    nk_ui_icon(back, NK_ICON_LEFT, 18, NK_INK_3);
    nk_ui_label(back, nk_ui_back_label(), NK_FR(13), NK_INK_3);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, -4, 0);

    lv_obj_t *label = nk_ui_label(bar, title, NK_FB(18), NK_INK);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    return bar;
}

/* ---------------------------------------------------------------- ボタン */

static lv_obj_t *button_base(lv_obj_t *parent, int32_t height, int32_t radius) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, LV_PCT(100), height);
    lv_obj_set_style_radius(btn, radius, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn, 8, 0);
    lv_obj_set_style_pad_hor(btn, 12, 0);
    lv_obj_set_scrollable(btn, false);
    lv_obj_set_style_opa(btn, LV_OPA_50, LV_STATE_DISABLED);
    return btn;
}

static lv_obj_t *button_label(lv_obj_t *btn, const char *text, const lv_font_t *font,
                              lv_color_t color) {
    lv_obj_t *label = nk_ui_label(btn, text, font, color);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

lv_obj_t *nk_ui_btn_primary(lv_obj_t *parent, const char *text, nk_icon icon, lv_event_cb_t cb,
                            void *user_data) {
    lv_obj_t *btn = button_base(parent, 54, 18);
    lv_obj_set_style_bg_color(btn, NK_BRAND, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xE36F91), LV_STATE_PRESSED);
    if (icon != NK_ICON_NONE) {
        nk_ui_icon(btn, icon, 20, NK_BRAND_INK);
    }
    button_label(btn, text, NK_FB(16), NK_BRAND_INK);
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

static lv_obj_t *outline_button(lv_obj_t *parent, const char *text, lv_color_t border,
                                lv_opa_t border_opa, lv_color_t text_color, lv_event_cb_t cb,
                                void *user_data) {
    lv_obj_t *btn = button_base(parent, 48, 16);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, border, 0);
    lv_obj_set_style_border_opa(btn, border_opa, 0);
    lv_obj_set_style_bg_color(btn, NK_WHITE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, 15, LV_STATE_PRESSED);
    button_label(btn, text, NK_FB(15), text_color);
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

lv_obj_t *nk_ui_btn_ghost(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data) {
    return outline_button(parent, text, NK_WHITE, NK_OPA_LINE_2, NK_INK_2, cb, user_data);
}

lv_obj_t *nk_ui_btn_serious(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data) {
    return outline_button(parent, text, NK_SERIOUS, LV_OPA_COVER, NK_SERIOUS, cb, user_data);
}

lv_obj_t *nk_ui_btn_pair(lv_obj_t *parent) {
    return nk_ui_row(parent, 10);
}

void nk_ui_btn_set_text(lv_obj_t *btn, const char *text) {
    lv_obj_t *label = lv_obj_get_child_by_type(btn, 0, &lv_label_class);
    if (label != NULL) {
        lv_label_set_text(label, text);
    }
}

lv_obj_t *nk_ui_icon_btn(lv_obj_t *parent, nk_icon icon, int32_t size, lv_event_cb_t cb,
                         void *user_data) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, NK_CARD, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, NK_CARD_2, LV_STATE_PRESSED);
    style_line_border(btn, NK_OPA_LINE);
    lv_obj_set_ext_click_area(btn, (NK_TOUCH_MIN - size) / 2 > 0 ? (NK_TOUCH_MIN - size) / 2 : 0);
    lv_obj_t *ic = nk_ui_icon(btn, icon, size / 2, NK_INK_2);
    lv_obj_center(ic);
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

lv_obj_t *nk_ui_link(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data) {
    lv_obj_t *label = nk_ui_label(parent, text, NK_FB(13), NK_BRAND);
    lv_obj_set_clickable(label, true);
    lv_obj_set_ext_click_area(label, 10);
    lv_obj_set_style_text_opa(label, LV_OPA_70, LV_STATE_PRESSED);
    if (cb != NULL) {
        lv_obj_add_event_cb(label, cb, LV_EVENT_CLICKED, user_data);
    }
    return label;
}

/* ---------------------------------------------------------------- 札 */

lv_obj_t *nk_ui_pill(lv_obj_t *parent, const char *text, nk_tone tone, bool dot) {
    lv_color_t color = tone == NK_TONE_INFO ? NK_SERIES : nk_tone_color(tone);
    lv_obj_t *pill = nk_ui_box(parent);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pill, 5, 0);
    lv_obj_set_style_pad_hor(pill, 9, 0);
    lv_obj_set_style_pad_ver(pill, 3, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pill, tone == NK_TONE_NEUTRAL ? NK_WHITE : color, 0);
    lv_obj_set_style_bg_opa(pill, tone == NK_TONE_NEUTRAL ? 20 : 41, 0);
    if (dot) {
        lv_obj_t *d = nk_ui_box(pill);
        lv_obj_set_size(d, 8, 8);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(d, color, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    }
    nk_ui_label(pill, text, NK_FB(12), color);
    return pill;
}

lv_obj_t *nk_ui_tag_est(lv_obj_t *parent) {
    lv_obj_t *tag = nk_ui_box(parent);
    lv_obj_set_style_pad_hor(tag, 6, 0);
    lv_obj_set_style_pad_ver(tag, 1, 0);
    lv_obj_set_style_radius(tag, 6, 0);
    lv_obj_set_style_bg_color(tag, NK_WHITE, 0);
    lv_obj_set_style_bg_opa(tag, 18, 0);
    nk_ui_label(tag, "推定", NK_FB(11), NK_INK_3);
    return tag;
}

lv_obj_t *nk_ui_tag_ref(lv_obj_t *parent) {
    lv_obj_t *tag = nk_ui_box(parent);
    lv_obj_set_style_pad_hor(tag, 8, 0);
    lv_obj_set_style_pad_ver(tag, 2, 0);
    lv_obj_set_style_radius(tag, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(tag, 1, 0);
    lv_obj_set_style_border_color(tag, NK_INK_3, 0);
    nk_ui_label(tag, "参考値", NK_FB(11), NK_INK_2);
    return tag;
}

/* ---------------------------------------------------------------- 状態の表示 */

lv_obj_t *nk_ui_spinner(lv_obj_t *parent, int32_t size, lv_color_t color) {
    lv_obj_t *sp = lv_spinner_create(parent);
    lv_obj_set_size(sp, size, size);
    lv_spinner_set_anim_params(sp, 1000, 90);
    int32_t w = size >= 24 ? 3 : 2;
    lv_obj_set_style_arc_width(sp, w, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sp, NK_WHITE, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(sp, 46, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, color, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(sp, 0, LV_PART_MAIN);
    lv_obj_set_clickable(sp, false);
    return sp;
}

/* 左に印、右に文字の列を持つ行 */
static lv_obj_t *text_column(lv_obj_t *row, const char *text, const char *sub, const char *link,
                             lv_event_cb_t cb, void *user_data) {
    lv_obj_t *col = nk_ui_col(row, 2);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    nk_ui_text(col, text, NK_FR(13), NK_INK);
    if (sub != NULL) {
        nk_ui_text(col, sub, NK_FR(12), NK_INK_2);
    }
    if (link != NULL) {
        lv_obj_t *l = nk_ui_link(col, link, cb, user_data);
        lv_obj_set_style_pad_top(l, 4, 0);
    }
    return col;
}

lv_obj_t *nk_ui_banner(lv_obj_t *parent, nk_tone tone, nk_icon icon, bool spinner, const char *text,
                       const char *sub, const char *link, lv_event_cb_t cb, void *user_data) {
    lv_color_t base =
        tone == NK_TONE_INFO || tone == NK_TONE_NEUTRAL ? NK_SERIES : nk_tone_color(tone);
    lv_obj_t *row = nk_ui_row(parent, 10);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(row, base, 0);
    lv_obj_set_style_bg_opa(row, tone == NK_TONE_SERIOUS ? 33 : 31, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, base, 0);
    lv_obj_set_style_border_opa(row, tone == NK_TONE_SERIOUS ? 102 : 86, 0);
    lv_obj_set_style_radius(row, 16, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 11, 0);
    if (spinner) {
        nk_ui_spinner(row, 18, NK_SPIN);
    } else if (icon != NK_ICON_NONE) {
        nk_ui_icon(row, icon, 22, nk_tone_color(tone == NK_TONE_NEUTRAL ? NK_TONE_INFO : tone));
    }
    text_column(row, text, sub, link, cb, user_data);
    return row;
}

lv_obj_t *nk_ui_op_result(lv_obj_t *parent, nk_op op, const char *text, const char *sub,
                          const char *link, lv_event_cb_t cb, void *user_data) {
    lv_obj_t *row = nk_ui_row(parent, 10);
    lv_obj_set_style_bg_color(row, NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 10, 0);
    switch (op) {
    case NK_OP_PENDING:
        nk_ui_spinner(row, 18, NK_SPIN);
        break;
    case NK_OP_OK:
        nk_ui_icon(row, NK_ICON_CHECK_CIRCLE, 18, NK_GOOD);
        break;
    case NK_OP_FAILED:
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, NK_SERIOUS, 0);
        lv_obj_set_style_border_opa(row, 115, 0);
        nk_ui_icon(row, NK_ICON_INFO, 18, NK_SERIOUS);
        break;
    case NK_OP_UNCONFIRMED:
    default:
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, NK_WARN, 0);
        lv_obj_set_style_border_opa(row, 102, 0);
        nk_ui_icon(row, NK_ICON_INFO, 18, NK_WARN);
        break;
    }
    lv_obj_t *col = text_column(row, text, NULL, link, cb, user_data);
    if (sub != NULL) {
        lv_obj_t *s = nk_ui_text(col, sub, NK_FR(12), NK_INK_3);
        if (link != NULL) {
            lv_obj_move_to_index(s, 1);
        }
    }
    return row;
}

lv_obj_t *nk_ui_notice_row(lv_obj_t *parent, nk_icon icon, lv_color_t icon_color,
                           const char *text) {
    lv_obj_t *row = nk_ui_row(parent, 8);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *ic = nk_ui_icon(row, icon, 16, icon_color);
    lv_obj_set_style_margin_top(ic, 2, 0);
    lv_obj_t *label = nk_ui_text(row, text, NK_FR(13), NK_INK_2);
    lv_obj_set_width(label, 0);
    lv_obj_set_flex_grow(label, 1);
    return row;
}

lv_obj_t *nk_ui_empty(lv_obj_t *parent, const char *title, const char *text, const char *btn,
                      lv_event_cb_t cb, void *user_data) {
    lv_obj_t *card = nk_ui_card(parent);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(card, 22, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    nk_ui_icon(card, NK_ICON_MOON, 56, lv_color_hex(0x5C66A8));
    lv_obj_t *t = nk_ui_label(card, title, NK_FB(15), NK_INK);
    lv_obj_set_style_margin_top(t, 4, 0);
    lv_obj_t *s = nk_ui_text(card, text, NK_FR(13), NK_INK_2);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    if (btn != NULL) {
        lv_obj_t *b = nk_ui_btn_primary(card, btn, NK_ICON_MOON, cb, user_data);
        lv_obj_set_style_margin_top(b, 8, 0);
    }
    return card;
}

/* ---------------------------------------------------------------- 入力 */

lv_obj_t *nk_ui_switch(lv_obj_t *parent, bool on, lv_event_cb_t cb, void *user_data) {
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 52, 32);
    lv_obj_set_style_bg_color(sw, NK_CARD_3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, NK_GOOD, NK_SEL(LV_PART_INDICATOR, LV_STATE_CHECKED));
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xC9CCE3), LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, NK_WHITE, NK_SEL(LV_PART_KNOB, LV_STATE_CHECKED));
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(sw, 0, LV_PART_KNOB);
    lv_obj_set_style_outline_width(sw, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_ext_click_area(sw, 8);
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    if (cb != NULL) {
        lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    return sw;
}

lv_obj_t *nk_ui_slider(lv_obj_t *parent, int32_t value, lv_event_cb_t cb, void *user_data) {
    lv_obj_t *sl = lv_slider_create(parent);
    lv_obj_set_height(sl, 6);
    lv_obj_set_width(sl, LV_PCT(100));
    lv_slider_set_range(sl, 0, 100);
    lv_slider_set_value(sl, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, NK_CARD_3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, NK_BRAND, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, NK_WHITE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 8, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(sl, 8, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(sl, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(sl, LV_OPA_40, LV_PART_KNOB);
    lv_obj_set_style_outline_width(sl, 0, NK_SEL(LV_PART_MAIN, LV_STATE_FOCUS_KEY));
    lv_obj_set_ext_click_area(sl, 14);
    if (cb != NULL) {
        lv_obj_add_event_cb(sl, cb, LV_EVENT_ALL, user_data);
    }
    return sl;
}

lv_obj_t *nk_ui_chip(lv_obj_t *parent, const char *text, bool on, lv_event_cb_t cb,
                     void *user_data) {
    lv_obj_t *chip = lv_button_create(parent);
    lv_obj_remove_style_all(chip);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(chip, 13, 0);
    lv_obj_set_style_pad_ver(chip, 8, 0);
    lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(chip, NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, NK_CARD_2, 0);
    lv_obj_set_style_bg_color(chip, lv_color_mix(NK_BRAND, NK_CARD_2, 41), LV_STATE_CHECKED);
    lv_obj_set_style_border_color(chip, NK_BRAND, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(chip, NK_INK_2, 0);
    lv_obj_set_style_text_color(chip, NK_INK, LV_STATE_CHECKED);
    lv_obj_set_ext_click_area(chip, 6);
    lv_obj_t *label = lv_label_create(chip);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, NK_FB(13), 0);
    if (on) {
        lv_obj_add_state(chip, LV_STATE_CHECKED);
    }
    if (cb != NULL) {
        lv_obj_add_event_cb(chip, cb, LV_EVENT_CLICKED, user_data);
    }
    return chip;
}

lv_obj_t *nk_ui_segmented(lv_obj_t *parent, const char *const *labels, int count, int selected,
                          lv_event_cb_t cb) {
    lv_obj_t *seg = nk_ui_box(parent);
    lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(seg, 3, 0);
    lv_obj_set_style_pad_column(seg, 3, 0);
    lv_obj_set_style_radius(seg, 12, 0);
    lv_obj_set_style_bg_color(seg, NK_CARD, 0);
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    style_line_border(seg, NK_OPA_LINE);
    for (int i = 0; i < count; i++) {
        lv_obj_t *b = lv_button_create(seg);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 54, 32);
        lv_obj_set_style_radius(b, 9, 0);
        lv_obj_set_style_bg_color(b, NK_SELECT, 0);
        lv_obj_set_style_bg_opa(b, i == selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_t *l = nk_ui_label(b, labels[i], NK_FB(13), i == selected ? NK_WHITE : NK_INK_2);
        lv_obj_center(l);
        if (cb != NULL) {
            lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
    }
    return seg;
}

lv_obj_t *nk_ui_roller(lv_obj_t *parent, const char *options, uint32_t selected, int32_t width) {
    lv_obj_t *roller = lv_roller_create(parent);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, width);
    lv_obj_set_style_text_font(roller, NK_FB(22), LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, NK_INK_3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 14, LV_PART_MAIN);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(roller, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, NK_SELECT, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, NK_INK, LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, NK_FB(22), LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, 10, LV_PART_SELECTED);
    lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
    return roller;
}

/* ---------------------------------------------------------------- 一覧の行 */

/* 2 行目以降の行に、上の区切り線を付ける */
static void list_row_border(lv_obj_t *row) {
    lv_obj_t *parent = lv_obj_get_parent(row);
    if (lv_obj_get_index(row) > 0 && lv_obj_get_child_count(parent) > 1) {
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, NK_WHITE, 0);
        lv_obj_set_style_border_opa(row, NK_OPA_LINE, 0);
    }
}

lv_obj_t *nk_ui_divider(lv_obj_t *parent) {
    lv_obj_t *line = nk_ui_box(parent);
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(line, NK_WHITE, 0);
    lv_obj_set_style_bg_opa(line, NK_OPA_LINE, 0);
    return line;
}

lv_obj_t *nk_ui_kv(lv_obj_t *parent, const char *key, const char *value, bool estimated) {
    lv_obj_t *row = nk_ui_row(parent, 6);
    lv_obj_set_style_pad_ver(row, 8, 0);
    list_row_border(row);
    nk_ui_label(row, key, NK_FR(14), NK_INK_2);
    if (estimated) {
        nk_ui_tag_est(row);
    }
    nk_ui_spacer(row);
    nk_ui_label(row, value, NK_FB(14), NK_INK);
    return row;
}

lv_obj_t *nk_ui_icon_tile(lv_obj_t *parent, nk_icon icon, int32_t size, lv_color_t bg,
                          lv_opa_t bg_opa, lv_color_t fg) {
    lv_obj_t *tile = nk_ui_box(parent);
    lv_obj_set_size(tile, size, size);
    lv_obj_set_style_radius(tile, size * 11 / 34, 0);
    lv_obj_set_style_bg_color(tile, bg, 0);
    lv_obj_set_style_bg_opa(tile, bg_opa, 0);
    lv_obj_t *ic = nk_ui_icon(tile, icon, size * 19 / 34, fg);
    lv_obj_center(ic);
    return tile;
}

lv_obj_t *nk_ui_list_item(lv_obj_t *parent, nk_icon icon, const char *title, const char *text) {
    lv_obj_t *row = nk_ui_row(parent, 12);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_ver(row, 11, 0);
    list_row_border(row);
    nk_ui_icon_tile(row, icon, 34, NK_SERIES, 56, NK_SERIES_LT);
    lv_obj_t *col = nk_ui_col(row, 2);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    nk_ui_label(col, title, NK_FB(14), NK_INK);
    if (text != NULL) {
        nk_ui_text(col, text, NK_FR(13), NK_INK_2);
    }
    return row;
}

lv_obj_t *nk_ui_setrow(lv_obj_t *parent, const char *title, const char *sub, const char *value) {
    lv_obj_t *row = nk_ui_row(parent, 10);
    lv_obj_set_style_pad_ver(row, 12, 0);
    list_row_border(row);
    lv_obj_t *col = nk_ui_col(row, 2);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    nk_ui_text(col, title, NK_FR(14), NK_INK);
    if (sub != NULL) {
        nk_ui_text(col, sub, NK_FR(12), NK_INK_3);
    }
    if (value != NULL) {
        nk_ui_label(row, value, NK_FB(13), NK_INK_2);
    }
    return row;
}

/* ---------------------------------------------------------------- シート */

static lv_obj_t *s_sheet;

static void sheet_dim_cb(lv_event_t *e) {
    if (lv_event_get_target_obj(e) == lv_event_get_current_target_obj(e)) {
        nk_ui_sheet_close();
    }
}

void nk_ui_sheet_close_cb(lv_event_t *e) {
    (void)e;
    nk_ui_sheet_close();
}

lv_obj_t *nk_ui_sheet_open(const char *title, const char *sub) {
    nk_ui_sheet_close();
    lv_obj_t *dim = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(dim);
    lv_obj_set_size(dim, NK_SCREEN_W, NK_SCREEN_H);
    lv_obj_set_style_bg_color(dim, NK_NIGHT, 0);
    lv_obj_set_style_bg_opa(dim, 158, 0);
    lv_obj_set_scrollable(dim, false);
    lv_obj_set_clickable(dim, true);
    lv_obj_add_event_cb(dim, sheet_dim_cb, LV_EVENT_CLICKED, NULL);
    s_sheet = dim;

    /* 下の角が画面の外に出るようにして、上の角だけを丸く見せる */
    lv_obj_t *panel = nk_ui_col(dim, 10);
    lv_obj_set_clickable(panel, true);
    lv_obj_set_style_bg_color(panel, NK_BG_2, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 26, 0);
    lv_obj_set_style_pad_hor(panel, 18, 0);
    lv_obj_set_style_pad_top(panel, 10, 0);
    lv_obj_set_style_pad_bottom(panel, 26 + 30, 0);
    style_line_border(panel, NK_OPA_LINE_2);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 26);

    lv_obj_t *grab_row = nk_ui_row(panel, 0);
    lv_obj_set_flex_align(grab_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(grab_row, 4, 0);
    lv_obj_t *grab = nk_ui_box(grab_row);
    lv_obj_set_size(grab, 40, 5);
    lv_obj_set_style_radius(grab, 3, 0);
    lv_obj_set_style_bg_color(grab, NK_WHITE, 0);
    lv_obj_set_style_bg_opa(grab, NK_OPA_LINE_2, 0);

    lv_obj_t *head = nk_ui_col(panel, 4);
    nk_ui_label(head, title, NK_FB(19), NK_INK);
    if (sub != NULL) {
        nk_ui_text(head, sub, NK_FR(13), NK_INK_2);
    }
    return panel;
}

void nk_ui_sheet_close(void) {
    if (s_sheet != NULL) {
        lv_obj_delete(s_sheet);
        s_sheet = NULL;
    }
}

bool nk_ui_sheet_is_open(void) {
    return s_sheet != NULL;
}

/* ---------------------------------------------------------------- トースト */

static lv_obj_t *s_toast;
static lv_timer_t *s_toast_timer;

static void toast_remove(void) {
    if (s_toast_timer != NULL) {
        lv_timer_delete(s_toast_timer);
        s_toast_timer = NULL;
    }
    if (s_toast != NULL) {
        lv_obj_delete(s_toast);
        s_toast = NULL;
    }
}

static void toast_done_cb(lv_timer_t *timer) {
    (void)timer;
    s_toast_timer = NULL; /* 1 回だけのタイマーは、この後 LVGL が消す */
    if (s_toast != NULL) {
        lv_obj_delete(s_toast);
        s_toast = NULL;
    }
}

void nk_ui_toast(const char *text) {
    toast_remove();
    lv_obj_t *toast = nk_ui_box(lv_layer_top());
    lv_obj_set_flex_flow(toast, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toast, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(toast, 8, 0);
    lv_obj_set_style_bg_color(toast, NK_INK, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toast, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(toast, 16, 0);
    lv_obj_set_style_pad_ver(toast, 10, 0);
    lv_obj_set_style_shadow_width(toast, 24, 0);
    lv_obj_set_style_shadow_opa(toast, LV_OPA_50, 0);
    lv_obj_set_style_shadow_color(toast, lv_color_hex(0x000000), 0);
    nk_ui_icon(toast, NK_ICON_CHECK, 18, lv_color_hex(0x1D9E6E));
    nk_ui_label(toast, text, NK_FB(14), lv_color_hex(0x141A3D));
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -(NK_TABBAR_H + 20));
    s_toast = toast;
    s_toast_timer = lv_timer_create(toast_done_cb, 2200, NULL);
    lv_timer_set_repeat_count(s_toast_timer, 1);
}

void nk_ui_widgets_reset(void) {
    nk_ui_sheet_close();
    toast_remove();
}
