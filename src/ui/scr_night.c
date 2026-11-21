/*
 * NT-01 睡眠モード（UX-6 の内容：今夜の確認、サウンド、再生の操作、アラーム、主ボタン）
 */
#include "ui_internal.h"

static void check_line(lv_obj_t *card, const char *mark, lv_color_t color, const char *text) {
    lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_width(nk_text(row, mark, NK_FS_BODY, false, color), 18);
    lv_obj_set_flex_grow(nk_wrap_text(row, text, NK_FS_SMALL + 1, false, NK_INK_2), 1);
}

static void sound_tiles(lv_obj_t *card) {
    static const char *const k_sounds[] = {"ホワイトノイズ", "雨", "ピンクノイズ", "波", "音楽",
                                           "ブラウンノイズ"};
    const int playing = 1;
    lv_obj_t *grid = nk_box(card, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 8, 0);
    for (int i = 0; i < 6; i++) {
        bool on = i == playing;
        lv_obj_t *tile = nk_box(grid, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_size(tile, (NK_CARD_INNER_W - 8) / 2, 56);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_hor(tile, 14, 0);
        lv_obj_set_style_pad_row(tile, 0, 0);
        lv_obj_set_style_radius(tile, 12, 0);
        lv_obj_set_style_bg_color(tile, on ? NK_SELECT : NK_CARD_2, 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tile, on ? 2 : 0, 0);
        lv_obj_set_style_border_color(tile, NK_BRAND, 0);
        nk_text(tile, k_sounds[i], NK_FS_BODY, true, NK_INK);
        if (on) {
            nk_text(tile, "再生中", NK_FS_CAPTION, false, NK_BRAND);
        }
    }
}

static void player(lv_obj_t *card) {
    lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 14, 0);
    lv_obj_t *pause = nk_rect(row, 52, 52, NK_BRAND, LV_RADIUS_CIRCLE);
    lv_obj_center(nk_text(pause, LV_SYMBOL_PAUSE, NK_FS_TITLE, true, NK_BRAND_INK));
    nk_text(row, LV_SYMBOL_VOLUME_MID, NK_FS_BODY, false, NK_INK_2);
    lv_obj_t *volume = lv_slider_create(row);
    lv_obj_set_height(volume, 6);
    lv_obj_set_flex_grow(volume, 1);
    lv_slider_set_value(volume, 45, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume, NK_BRAND, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume, NK_INK, LV_PART_KNOB);

    lv_obj_t *timers = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(timers, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_chip(timers, "15分", false);
    nk_chip(timers, "30分", true);
    nk_chip(timers, "60分", false);
    nk_chip(timers, "朝まで", false);
}

void nk_build_night(lv_obj_t *page) {
    lv_obj_t *head = nk_box(page, LV_FLEX_FLOW_ROW);
    nk_text(head, "睡眠モード", NK_FS_HEAD + 4, true, NK_INK);
    nk_text(head, "10月31日（土）22:41", NK_FS_SMALL, false, NK_INK_2);

    /* 今夜の確認 */
    lv_obj_t *check = nk_card(page);
    check_line(check, LV_SYMBOL_OK, NK_GOOD, "電池 64%・充電中");
    check_line(check, LV_SYMBOL_OK, NK_GOOD, "マイク：いびきも記録します（録音は保存しません）");
    check_line(check, LV_SYMBOL_WARNING, NK_WARN, "枕元のマットレスの上に置いてください");

    /* 入眠サウンド */
    lv_obj_t *sound = nk_card(page);
    lv_obj_set_style_pad_row(sound, 12, 0);
    nk_card_title(sound, "入眠サウンド", "タイマー 30分");
    sound_tiles(sound);
    player(sound);

    /* アラーム */
    lv_obj_t *alarm = nk_card(page);
    nk_card_title(alarm, "アラーム", "スヌーズ 5分・3回まで");
    lv_obj_t *row = nk_box(alarm, LV_FLEX_FLOW_ROW);
    lv_obj_t *when = nk_box(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_grow(when, 1);
    lv_obj_set_flex_align(when, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    nk_text(when, "8:00", NK_FS_BIG + 4, true, NK_INK);
    nk_text(when, "あす（日）", NK_FS_BODY, false, NK_INK_2);
    nk_switch(row, true);

    /* 主ボタン */
    nk_button(page, "計測をはじめる", true);
}
