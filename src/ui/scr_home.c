/*
 * HM-01 ホーム（UX-3 改良版の図）
 * 上から、結果、助言、今夜の準備、操作の順に置く
 */
#include "ui_internal.h"

/* 「睡眠時間 6時間38分」のような、見出しと値の 1 行 */
static void fact(lv_obj_t *parent, const char *caption, const char *value) {
    lv_obj_t *row = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_text(row, caption, NK_FS_SMALL, false, NK_INK_2);
    nk_text(row, value, NK_FS_BODY + 1, true, NK_INK);
}

void nk_build_home(lv_obj_t *page) {
    /* 1・2 ヘッダー（ロゴと日付）とヘルプ */
    lv_obj_t *head = nk_box(page, LV_FLEX_FLOW_ROW);
    lv_obj_t *logo = nk_box(head, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(logo, LV_SIZE_CONTENT);
    nk_text(logo, "ねむのき", NK_FS_HEAD + 4, true, NK_INK);
    nk_text(logo, "10月31日（土）の朝", NK_FS_SMALL, false, NK_INK_2);
    lv_obj_t *help = nk_rect(head, 36, 36, NK_BG, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(help, 2, 0);
    lv_obj_set_style_border_color(help, NK_INK_2, 0);
    lv_obj_center(nk_text(help, "?", NK_FS_TITLE, true, NK_INK_2));

    /* 4 スコアのカード */
    lv_obj_t *score = nk_card(page);
    lv_obj_set_style_border_width(score, 2, 0);
    lv_obj_set_style_border_color(score, NK_SERIES, 0);
    lv_obj_t *top = nk_box(score, LV_FLEX_FLOW_ROW);
    nk_text(top, "睡眠スコア", NK_FS_BODY, false, NK_INK_2);
    nk_pill(top, NK_DEMO_LABEL, NK_GOOD, false);
    lv_obj_t *body = nk_box(score, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(body, 20, 0);
    nk_ring(body, 96, NK_DEMO_SCORE);
    lv_obj_t *facts = nk_box(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(facts, 1);
    lv_obj_set_style_pad_row(facts, 6, 0);
    fact(facts, "睡眠時間", NK_DEMO_SLEEP);
    nk_text(facts, NK_DEMO_BEDTIME " → " NK_DEMO_WAKE, NK_FS_BODY, false, NK_INK_2);
    fact(facts, "前日比", NK_DEMO_DIFF);

    /* 5 詳しく見る（日別の詳細へ） */
    nk_on_click_open(nk_button(page, "詳しく見る", false), NK_UI_RECORD_DETAIL);

    /* 6 AI フィードバック */
    lv_obj_t *ai = nk_card(page);
    nk_text(ai, "AIフィードバック", NK_FS_SMALL, false, NK_INK_3);
    nk_wrap_text(ai, NK_DEMO_FEEDBACK, NK_FS_BODY + 1, true, NK_INK);

    /* 7 今夜の目標と機器の接続状態 */
    lv_obj_t *tonight = nk_card(page);
    lv_obj_set_flex_flow(tonight, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tonight, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_stat(tonight, "今夜の目標", "就寝 " NK_DEMO_GOAL, true);
    nk_rect(tonight, 1, 40, NK_CARD_2, 0);
    lv_obj_t *device = nk_stat(tonight, "機器の接続状態", "接続中", true);
    lv_obj_set_style_text_color(lv_obj_get_child(device, 1), NK_GOOD, 0);

    /* 8 カーテン操作 */
    lv_obj_t *curtain = nk_box(page, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(curtain, 12, 0);
    lv_obj_set_flex_grow(nk_button(curtain, "開ける", false), 1);
    lv_obj_set_flex_grow(nk_button(curtain, "閉じる", false), 1);
}
