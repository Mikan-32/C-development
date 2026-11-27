/*
 * ST-01 設定と BR-01 寝室（UX-5 改訂版の図。寝室は設定の中に置いた仮の形）
 */
#include "nk_widgets.h"
#include "ui_internal.h"

/* 設定の 1 行：名前（と説明）、右に値と「›」 */
static lv_obj_t *item(lv_obj_t *card, const char *name, const char *note, const char *value) {
    lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_ver(row, 12, 0);
    if (lv_obj_get_child_count(card) > 1) {
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, NK_CARD_2, 0);
    }
    lv_obj_t *left = nk_box(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(left, 1);
    nk_text(left, name, NK_FS_BODY, false, NK_INK);
    if (note != NULL) {
        nk_text(left, note, NK_FS_CAPTION, false, NK_INK_3);
    }
    if (value != NULL) {
        nk_text(row, value, NK_FS_SMALL + 1, false, NK_INK_2);
    }
    nk_text(row, "›", NK_FS_TITLE + 3, false, NK_INK_3);
    return row;
}

static lv_obj_t *group(lv_obj_t *page, const char *title) {
    lv_obj_set_style_pad_top(nk_text(page, title, NK_FS_SMALL, true, NK_INK_3), 4, 0);
    lv_obj_t *card = nk_card(page);
    lv_obj_set_style_pad_ver(card, 4, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    return card;
}

static const char *font_scale_label(void) {
    switch (nk_theme_get_font_scale()) {
    default:
        return "1.0倍（標準）";
    case NK_FONT_SCALE_120:
        return "1.2倍（大）";
    case NK_FONT_SCALE_140:
        return "1.4倍（特大）";
    }
}

static void on_font_scale_clicked(lv_event_t *e) {
    (void)e;
    nk_font_scale next = (nk_font_scale)(((int)nk_theme_get_font_scale() + 1) % 3);
    nk_theme_set_font_scale(next);
    nk_ui_open(NK_UI_SETTINGS);
}

static void on_privacy_clicked(lv_event_t *e) {
    (void)e;
    nk_open_privacy_modal();
}

static void on_about_analysis_clicked(lv_event_t *e) {
    (void)e;
    nk_open_help_st03();
}

static void on_license_clicked(lv_event_t *e) {
    (void)e;
    nk_open_license_modal();
}

void nk_build_settings(lv_obj_t *page) {
    nk_text(page, "設定", NK_FS_HEAD + 4, true, NK_INK);

    lv_obj_t *room = group(page, "寝室");
    nk_on_click_open(item(room, "寝室", "カーテンと自動モード", "接続済み"), NK_UI_BEDROOM);

    lv_obj_t *sleep_goal = group(page, "睡眠");
    item(sleep_goal, "目標の就寝時刻", NULL, NK_DEMO_GOAL);
    item(sleep_goal, "起きる時刻", NULL, "平日 6:30・休日 8:00");

    lv_obj_t *other = group(page, "そのほか");
    lv_obj_t *row_font = item(other, "文字の大きさ", "1.0／1.2／1.4 倍", font_scale_label());
    lv_obj_set_clickable(row_font, true);
    lv_obj_add_event_cb(row_font, on_font_scale_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row_privacy =
        item(other, "データとプライバシー", "同意、マイクの許可、記録の削除", NULL);
    lv_obj_set_clickable(row_privacy, true);
    lv_obj_add_event_cb(row_privacy, on_privacy_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row_about = item(other, "この分析について", NULL, NULL);
    lv_obj_set_clickable(row_about, true);
    lv_obj_add_event_cb(row_about, on_about_analysis_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row_license = item(other, "ライセンス表示", NULL, NULL);
    lv_obj_set_clickable(row_license, true);
    lv_obj_add_event_cb(row_license, on_license_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *version =
        nk_text(page, "ねむのき 0.1.0（画面の試作）", NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_set_width(version, LV_PCT(100));
    lv_obj_set_style_text_align(version, LV_TEXT_ALIGN_CENTER, 0);
}

/* ---------------------------------------------------------------- 寝室 */

/* カーテンの開き具合の図：窓の両側から、閉じている分だけ布をかける */
static void curtain_picture(lv_obj_t *parent, int32_t open_pct) {
    const int32_t w = 112;
    const int32_t h = 84;
    lv_obj_t *window = nk_rect(parent, w, h, NK_SELECT, 6);
    lv_obj_set_pos(nk_rect(window, 2, h, NK_CARD, 0), w / 2 - 1, 0);
    int32_t cloth = (100 - open_pct) * w / 200;
    lv_obj_set_pos(nk_rect(window, cloth, h, NK_INK_3, 0), 0, 0);
    lv_obj_set_pos(nk_rect(window, cloth, h, NK_INK_3, 0), w - cloth, 0);
}

static void plan_row(lv_obj_t *card, const char *name, const char *time_text, bool on) {
    lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_grow(nk_text(row, name, NK_FS_BODY, false, NK_INK), 1);
    nk_text(row, time_text, NK_FS_TITLE + 1, true, NK_INK);
    nk_text(row, on ? "オン" : "オフ", NK_FS_SMALL, true, on ? NK_GOOD : NK_INK_3);
    nk_switch(row, on);
}

void nk_build_bedroom(lv_obj_t *page) {
    /* 見出し */
    nk_header(page, "設定", "寝室");

    /* 1 接続の札 */
    lv_obj_t *link_card = nk_card(page);
    lv_obj_set_flex_flow(link_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(link_card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(link_card, 10, 0);
    nk_pill(link_card, "接続済み", NK_GOOD, true);
    nk_text(link_card, "つなぎ直す ›", NK_FS_SMALL, false, NK_INK_2);

    /* 2 カーテンの状態 */
    lv_obj_t *state = nk_card(page);
    lv_obj_set_flex_flow(state, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(state, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(state, 16, 0);
    curtain_picture(state, NK_DEMO_CURTAIN_PCT);
    lv_obj_t *words = nk_box(state, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(words, 1);
    nk_text(words, "60% 開いています", NK_FS_TITLE + 1, true, NK_INK);
    nk_text(words, "最終確認 6:21", NK_FS_SMALL, false, NK_INK_2);
    nk_wrap_text(words, "（0%：全部閉／100%：全開）", NK_FS_CAPTION, false, NK_INK_3);

    /* 3 操作 */
    lv_obj_t *ops = nk_card(page);
    lv_obj_set_style_pad_row(ops, 12, 0);
    nk_card_title(ops, "操作", NULL);
    lv_obj_t *buttons = nk_box(ops, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(buttons, 12, 0);
    lv_obj_set_flex_grow(nk_button(buttons, "開ける", false), 1);
    lv_obj_set_flex_grow(nk_button(buttons, "閉める", false), 1);
    lv_obj_t *slider = lv_slider_create(ops);
    lv_obj_set_size(slider, LV_PCT(100), 6);
    lv_slider_set_value(slider, NK_DEMO_CURTAIN_PCT, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, NK_SERIES, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, NK_INK, LV_PART_KNOB);
    lv_obj_t *scale = nk_box(ops, LV_FLEX_FLOW_ROW);
    nk_text(scale, "0%", NK_FS_CAPTION, false, NK_INK_3);
    nk_text(scale, "離したときに命令を送ります", NK_FS_CAPTION, false, NK_INK_3);
    nk_text(scale, "100%", NK_FS_CAPTION, false, NK_INK_3);

    /* 4 結果の行 */
    lv_obj_t *result = nk_card(page);
    nk_text(result, "直近の結果", NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_t *done = nk_box(result, LV_FLEX_FLOW_ROW);
    nk_text(done, "開けました（6:21 確認）", NK_FS_BODY + 1, true, NK_INK);
    nk_pill(done, "確認済み", NK_GOOD, false);

    /* 5 予約 */
    lv_obj_t *plan = nk_card(page);
    lv_obj_set_style_pad_row(plan, 10, 0);
    nk_card_title(plan, "予約", NULL);
    plan_row(plan, "開ける時刻", "6:20", true);
    plan_row(plan, "閉める時刻", "22:30", false);
    lv_obj_t *repeat = nk_box(plan, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(repeat, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_chip(repeat, "毎日", true);
    nk_chip(repeat, "平日のみ", false);

    /* 6 自動モード（オフのときは説明と「有効にする」） */
    lv_obj_t *mode = nk_card(page);
    lv_obj_t *mode_head = nk_box(mode, LV_FLEX_FLOW_ROW);
    nk_text(mode_head, "自動モード", NK_FS_TITLE, true, NK_INK);
    nk_pill(mode_head, "オフ", NK_INK_3, false);
    lv_obj_t *mode_body = nk_box(mode, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mode_body, 12, 0);
    lv_obj_set_flex_grow(nk_wrap_text(mode_body,
                                      "決めた時刻に、カーテンとアラームを自動で動かします",
                                      NK_FS_SMALL, false, NK_INK_2),
                         1);
    lv_obj_t *enable = nk_button(mode_body, "有効にする", false);
    lv_obj_set_size(enable, 120, 44);

    /* 7 動作の記録（直近 5 件。失敗も残す） */
    static const char *const k_log[][2] = {
        {"今朝 6:21", "予約で開けました（確認済み）"},
        {"昨夜 23:52", "手動で閉めました（確認済み）"},
        {"昨朝 6:20", "開けられませんでした（失敗）"},
        {"29日 23:40", "手動で閉めました（確認済み）"},
        {"29日 6:21", "予約で開けました（確認済み）"},
    };
    lv_obj_t *history = nk_card(page);
    nk_card_title(history, "動作の記録", "直近5件");
    for (int i = 0; i < 5; i++) {
        lv_obj_t *row = nk_box(history, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(nk_text(row, k_log[i][0], NK_FS_SMALL, false, NK_INK_3), 84);
        nk_text(row, k_log[i][1], NK_FS_SMALL + 1, false, i == 2 ? NK_SERIOUS : NK_INK);
    }
}
