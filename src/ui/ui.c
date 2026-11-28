/*
 * 画面の土台：下部タブ、画面の切り替え、画面どうしで共有する部品
 */
#include "ui_internal.h"

#include <stdint.h>
#include <string.h>

#define TAB_COUNT 4
#define TAB_H     64

static const char *const k_tab_names[TAB_COUNT] = {"ホーム", "睡眠記録", "睡眠モード", "設定"};
static const nk_ui_screen k_tab_screens[TAB_COUNT] = {NK_UI_HOME, NK_UI_RECORD_DAY, NK_UI_NIGHT,
                                                      NK_UI_SETTINGS};

static struct {
    lv_obj_t *page; /* タブの上の、スクロールする部分 */
    lv_obj_t *tab_labels[TAB_COUNT];
    lv_obj_t *tab_marks[TAB_COUNT];
    nk_ui_screen current;
} s_ui;

/* 寝床に入った 23:20 から順に。合計 7 時間 32 分（覚醒 54 分、深い眠り 72 分） */
const nk_segment nk_demo_segments[] = {
    {NK_STAGE_WAKE, 28}, {NK_STAGE_LIGHT, 20}, {NK_STAGE_DEEP, 40}, {NK_STAGE_LIGHT, 40},
    {NK_STAGE_WAKE, 6},  {NK_STAGE_LIGHT, 30}, {NK_STAGE_DEEP, 22}, {NK_STAGE_LIGHT, 100},
    {NK_STAGE_WAKE, 16}, {NK_STAGE_LIGHT, 70}, {NK_STAGE_DEEP, 10}, {NK_STAGE_LIGHT, 66},
    {NK_STAGE_WAKE, 4},
};
const int nk_demo_segment_count = (int)(sizeof(nk_demo_segments) / sizeof(nk_demo_segments[0]));

lv_color_t nk_stage_color(nk_stage stage) {
    switch (stage) {
    case NK_STAGE_WAKE:
        return NK_WAKE;
    case NK_STAGE_DEEP:
        return NK_DEEP;
    default:
        return NK_LIGHT;
    }
}

/* ---------------------------------------------------------------- 画面の切り替え */

static int tab_of(nk_ui_screen screen) {
    switch (screen) {
    case NK_UI_HOME:
        return 0;
    case NK_UI_NIGHT:
        return 2;
    case NK_UI_SETTINGS:
    case NK_UI_BEDROOM:
    case NK_UI_CURTAIN_CONNECT:
        return 3;
    default:
        return 1;
    }
}

static void update_tabs(void) {
    int active = tab_of(s_ui.current);
    for (int i = 0; i < TAB_COUNT; i++) {
        bool on = i == active;
        lv_obj_set_hidden(s_ui.tab_marks[i], !on);
        lv_obj_set_style_text_font(s_ui.tab_labels[i], nk_font(NK_FS_SMALL + 1, on), 0);
        lv_obj_set_style_text_color(s_ui.tab_labels[i], on ? NK_BRAND : NK_INK_3, 0);
    }
}

void nk_ui_open(nk_ui_screen screen) {
    static void (*const k_build[NK_UI_SCREEN_COUNT])(lv_obj_t *) = {
        nk_build_home,  nk_build_record_day, nk_build_record_week, nk_build_record_detail,
        nk_build_night, nk_build_settings,   nk_build_bedroom,     nk_build_curtain_connect,
    };
    if ((int)screen < 0 || (int)screen >= (int)NK_UI_SCREEN_COUNT || s_ui.page == NULL) {
        return;
    }
    s_ui.current = screen;
    lv_obj_clean(s_ui.page);
    lv_obj_scroll_to_y(s_ui.page, 0, LV_ANIM_OFF);
    k_build[screen](s_ui.page);
    update_tabs();
}

nk_ui_screen nk_ui_current(void) {
    return s_ui.current;
}

bool nk_ui_back(void) {
    if (s_ui.current == NK_UI_RECORD_DETAIL) {
        nk_ui_open(NK_UI_RECORD_DAY);
        return true;
    }
    if (s_ui.current == NK_UI_CURTAIN_CONNECT) {
        nk_ui_open(NK_UI_BEDROOM);
        return true;
    }
    if (s_ui.current == NK_UI_BEDROOM) {
        nk_ui_open(NK_UI_SETTINGS);
        return true;
    }
    return false;
}

void nk_ui_scroll_by(int32_t dy) {
    if (s_ui.page != NULL) {
        lv_obj_scroll_by_bounded(s_ui.page, 0, dy, LV_ANIM_OFF);
    }
}

/* 押された部品の中で画面を作り直さないよう、切り替えは次の周回で行う */
static void open_later(void *screen) {
    nk_ui_open((nk_ui_screen)(intptr_t)screen);
}

static void back_later(void *unused) {
    (void)unused;
    (void)nk_ui_back();
}

static void on_open_clicked(lv_event_t *e) {
    (void)lv_async_call(open_later, lv_event_get_user_data(e));
}

static void on_back_clicked(lv_event_t *e) {
    (void)e;
    (void)lv_async_call(back_later, NULL);
}

void nk_on_click_open(lv_obj_t *obj, nk_ui_screen screen) {
    lv_obj_set_clickable(obj, true);
    lv_obj_add_event_cb(obj, on_open_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)screen);
}

static void build_tabbar(lv_obj_t *root) {
    lv_obj_t *bar = nk_box(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_height(bar, TAB_H);
    lv_obj_set_style_pad_column(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, NK_TABBAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, NK_CARD_2, 0);
    for (int i = 0; i < TAB_COUNT; i++) {
        lv_obj_t *tab = nk_rect(bar, 0, LV_PCT(100), NK_TABBAR, 0);
        lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_grow(tab, 1);
        nk_on_click_open(tab, k_tab_screens[i]);
        s_ui.tab_marks[i] = nk_rect(tab, 48, 3, NK_BRAND, 2);
        lv_obj_align(s_ui.tab_marks[i], LV_ALIGN_TOP_MID, 0, 0);
        s_ui.tab_labels[i] = nk_text(tab, k_tab_names[i], NK_FS_SMALL + 1, false, NK_INK_3);
        lv_obj_center(s_ui.tab_labels[i]);
    }
}

void nk_ui_init(void) {
    memset(&s_ui, 0, sizeof(s_ui));
    lv_obj_t *root = lv_screen_active();
    lv_obj_remove_style_all(root);
    lv_obj_set_style_bg_color(root, NK_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(root, false);

    s_ui.page = lv_obj_create(root);
    lv_obj_remove_style_all(s_ui.page);
    lv_obj_set_width(s_ui.page, LV_PCT(100));
    lv_obj_set_flex_grow(s_ui.page, 1);
    lv_obj_set_flex_flow(s_ui.page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(s_ui.page, NK_PAGE_PAD, 0);
    lv_obj_set_style_pad_top(s_ui.page, 20, 0);
    lv_obj_set_style_pad_bottom(s_ui.page, 24, 0);
    lv_obj_set_style_pad_row(s_ui.page, 12, 0);
    lv_obj_set_scroll_dir(s_ui.page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_ui.page, LV_SCROLLBAR_MODE_OFF);

    build_tabbar(root);
    nk_ui_open(NK_UI_HOME);
}

void nk_ui_deinit(void) {
    if (s_ui.page != NULL) {
        lv_obj_clean(lv_screen_active());
    }
    memset(&s_ui, 0, sizeof(s_ui));
}

/* ---------------------------------------------------------------- 部品 */

lv_obj_t *nk_box(lv_obj_t *parent, lv_flex_flow_t flow) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, flow);
    lv_obj_set_clickable(box, false);
    lv_obj_set_scrollable(box, false);
    if (flow == LV_FLEX_FLOW_COLUMN) {
        lv_obj_set_style_pad_row(box, 4, 0);
    } else {
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(box, 8, 0);
    }
    return box;
}

lv_obj_t *nk_rect(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t color, int32_t radius) {
    lv_obj_t *rect = lv_obj_create(parent);
    lv_obj_remove_style_all(rect);
    lv_obj_set_size(rect, w, h);
    lv_obj_set_style_bg_color(rect, color, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, radius, 0);
    lv_obj_set_clickable(rect, false);
    lv_obj_set_scrollable(rect, false);
    return rect;
}

lv_obj_t *nk_card(lv_obj_t *parent) {
    lv_obj_t *card = nk_box(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(card, NK_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, NK_CARD_PAD, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    return card;
}

lv_obj_t *nk_text(lv_obj_t *parent, const char *text, int32_t px, bool bold, lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, nk_font(px, bold), 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t *nk_wrap_text(lv_obj_t *parent, const char *text, int32_t px, bool bold,
                       lv_color_t color) {
    lv_obj_t *label = nk_text(parent, text, px, bold, color);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_line_space(label, 4, 0);
    return label;
}

lv_obj_t *nk_card_title(lv_obj_t *card, const char *title, const char *note) {
    lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
    nk_text(row, title, NK_FS_TITLE, true, NK_INK);
    if (note != NULL) {
        nk_text(row, note, NK_FS_CAPTION, false, NK_INK_3);
    }
    return row;
}

lv_obj_t *nk_button(lv_obj_t *parent, const char *text, bool primary) {
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, LV_PCT(100), 48);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, primary ? NK_BRAND : NK_CARD, 0);
    lv_obj_set_style_bg_color(button, primary ? NK_INK : NK_SELECT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, primary ? 0 : 2, 0);
    lv_obj_set_style_border_color(button, NK_INK_2, 0);
    lv_obj_t *label = nk_text(button, text, NK_FS_BODY + 1, true, primary ? NK_BRAND_INK : NK_INK);
    lv_obj_center(label);
    return button;
}

lv_obj_t *nk_pill(lv_obj_t *parent, const char *text, lv_color_t color, bool dot) {
    lv_obj_t *pill = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(pill, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(pill, 12, 0);
    lv_obj_set_style_pad_ver(pill, 4, 0);
    lv_obj_set_style_pad_column(pill, 6, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, color, 0);
    lv_obj_set_style_bg_color(pill, color, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_20, 0);
    if (dot) {
        nk_rect(pill, 8, 8, color, LV_RADIUS_CIRCLE);
    }
    nk_text(pill, text, NK_FS_SMALL, true, color);
    return pill;
}

lv_obj_t *nk_chip(lv_obj_t *parent, const char *text, bool selected) {
    lv_obj_t *chip = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(chip, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(chip, 14, 0);
    lv_obj_set_style_pad_ver(chip, 8, 0);
    lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(chip, selected ? NK_SELECT : NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, selected ? 2 : 0, 0);
    lv_obj_set_style_border_color(chip, NK_BRAND, 0);
    nk_text(chip, text, NK_FS_SMALL + 1, selected, selected ? NK_INK : NK_INK_2);
    return chip;
}

lv_obj_t *nk_stat(lv_obj_t *parent, const char *caption, const char *value, bool center) {
    lv_obj_t *cell = nk_box(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(cell, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(cell, 2, 0);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_START,
                          center ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    nk_text(cell, caption, NK_FS_SMALL, false, NK_INK_3);
    nk_text(cell, value, NK_FS_TITLE, true, NK_INK);
    return cell;
}

lv_obj_t *nk_ring(lv_obj_t *parent, int32_t size, int32_t score) {
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, score);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(arc, false);
    lv_obj_set_style_pad_all(arc, 0, 0);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, score >= 70 ? NK_GOOD : NK_WARN, LV_PART_INDICATOR);
    char text[8];
    lv_snprintf(text, sizeof(text), "%d", (int)score);
    lv_obj_center(nk_text(arc, text, size / 3, true, NK_INK));
    return arc;
}

lv_obj_t *nk_switch(lv_obj_t *parent, bool on) {
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 52, 30);
    lv_obj_set_style_bg_color(sw, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, NK_GOOD, NK_SEL(LV_PART_INDICATOR, LV_STATE_CHECKED));
    lv_obj_set_style_bg_color(sw, NK_INK, LV_PART_KNOB);
    lv_obj_set_checked(sw, on);
    return sw;
}

lv_obj_t *nk_pager(lv_obj_t *parent, const char *text) {
    lv_obj_t *row = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(row, NK_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_pad_hor(row, 16, 0);
    lv_obj_set_style_pad_ver(row, 10, 0);
    nk_text(row, "‹", NK_FS_TITLE + 3, false, NK_INK_2);
    nk_text(row, text, NK_FS_BODY, true, NK_INK);
    /* 試作の日付がいちばん新しい日なので、未来へは進めない */
    lv_obj_set_style_text_opa(nk_text(row, "›", NK_FS_TITLE + 3, false, NK_INK_3), LV_OPA_40, 0);
    return row;
}

void nk_header(lv_obj_t *parent, const char *back, const char *title) {
    lv_obj_t *bar = nk_rect(parent, LV_PCT(100), 40, NK_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_center(nk_text(bar, title, NK_FS_TITLE + 2, true, NK_INK));
    char text[64];
    lv_snprintf(text, sizeof(text), "‹ %s", back);
    lv_obj_t *back_link = nk_text(bar, text, NK_FS_BODY, false, NK_INK_2);
    lv_obj_align(back_link, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_clickable(back_link, true);
    lv_obj_set_ext_click_area(back_link, 12);
    lv_obj_add_event_cb(back_link, on_back_clicked, LV_EVENT_CLICKED, NULL);
}

void nk_record_toggle(lv_obj_t *parent, bool week) {
    lv_obj_t *row = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(row, NK_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_style_pad_column(row, 4, 0);
    for (int i = 0; i < 2; i++) {
        bool selected = (i == 1) == week;
        lv_obj_t *half = nk_rect(row, 0, 36, selected ? NK_SELECT : NK_CARD, LV_RADIUS_CIRCLE);
        lv_obj_set_flex_grow(half, 1);
        lv_obj_set_style_border_width(half, selected ? 2 : 0, 0);
        lv_obj_set_style_border_color(half, NK_SERIES, 0);
        lv_obj_center(nk_text(half, i == 0 ? "日" : "週", NK_FS_BODY, selected,
                              selected ? NK_INK : NK_INK_2));
        if (!selected) {
            nk_on_click_open(half, i == 0 ? NK_UI_RECORD_DAY : NK_UI_RECORD_WEEK);
        }
    }
}
