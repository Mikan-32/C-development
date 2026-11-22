/*
 * HM-01 ホーム（UX-3 改良版の図）
 * 上から、結果、助言、今夜の準備、操作の順に置く
 */
#include "nk_widgets.h"
#include "ui_internal.h"

static bool s_store_ready = false;

static void ensure_store(void) {
    if (!s_store_ready) {
        nk_ui_store_init();
        s_store_ready = true;
    }
}

/* 「睡眠時間 6時間38分」のような、見出しと値の 1 行 */
static void fact(lv_obj_t *parent, const char *caption, const char *value) {
    lv_obj_t *row = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_text(row, caption, NK_FS_SMALL, false, NK_INK_2);
    nk_text(row, value, NK_FS_BODY + 1, true, NK_INK);
}

static void on_help_clicked(lv_event_t *e) {
    (void)e;
    nk_open_help_st03();
}

static void on_banner_dismiss_clicked(lv_event_t *e) {
    (void)e;
    nk_ui_store_clear_banner();
    nk_ui_open(NK_UI_HOME);
}

static void on_curtain_open_clicked(lv_event_t *e) {
    (void)e;
    nk_ui_store_operate_curtain(true);
    nk_ui_open(NK_UI_HOME);
}

static void on_curtain_close_clicked(lv_event_t *e) {
    (void)e;
    nk_ui_store_operate_curtain(false);
    nk_ui_open(NK_UI_HOME);
}

void nk_build_home(lv_obj_t *page) {
    ensure_store();
    const nk_sleep_record *latest = nk_ui_store_latest();

    /* 1・2 ヘッダー（ロゴと日付）とヘルプ */
    lv_obj_t *head = nk_box(page, LV_FLEX_FLOW_ROW);
    lv_obj_t *logo = nk_box(head, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(logo, LV_SIZE_CONTENT);
    nk_text(logo, "ねむのき", NK_FS_HEAD + 4, true, NK_INK);
    nk_text(logo, latest != NULL ? latest->morning_title : "まだ記録がありません", NK_FS_SMALL,
            false, NK_INK_2);

    lv_obj_t *help = nk_rect(head, 36, 36, NK_BG, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(help, 2, 0);
    lv_obj_set_style_border_color(help, NK_INK_2, 0);
    lv_obj_set_clickable(help, true);
    lv_obj_set_ext_click_area(help, 8);
    lv_obj_add_event_cb(help, on_help_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_center(nk_text(help, "?", NK_FS_TITLE, true, NK_INK_2));

    /* 3 状態の帯スロット（あらかじめ高さを確保し、表示・非表示で全体がズレないようにする） */
    int32_t slot_h = nk_theme_scale_px(44);
    lv_obj_t *banner_slot = nk_box(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(banner_slot, LV_PCT(100), slot_h);
    lv_obj_set_flex_align(banner_slot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    const char *banner_msg = nk_ui_store_banner_text();
    if (banner_msg != NULL && banner_msg[0] != '\0') {
        lv_obj_t *b = nk_banner(banner_slot, banner_msg, nk_ui_store_banner_is_warn());
        lv_obj_set_height(b, slot_h);
        lv_obj_set_style_pad_ver(b, 6, 0);
        lv_obj_t *close_lbl = nk_text(b, "×", NK_FS_BODY, true, NK_INK_2);
        lv_obj_set_clickable(close_lbl, true);
        lv_obj_set_ext_click_area(close_lbl, 12);
        lv_obj_add_event_cb(close_lbl, on_banner_dismiss_clicked, LV_EVENT_CLICKED, NULL);
    } else if (latest != NULL && latest->quality == NK_QUALITY_INSUFFICIENT) {
        lv_obj_t *b =
            nk_banner(banner_slot, "計測時間が短かったため参考値です（タップで見方）", true);
        lv_obj_set_height(b, slot_h);
        lv_obj_set_style_pad_ver(b, 6, 0);
        lv_obj_set_clickable(b, true);
        lv_obj_add_event_cb(b, on_help_clicked, LV_EVENT_CLICKED, NULL);
    }

    /* 4 スコアのカード */
    lv_obj_t *score = nk_card(page);
    lv_obj_set_style_border_width(score, 2, 0);
    lv_obj_set_style_border_color(score, NK_SERIES, 0);

    if (latest == NULL) {
        nk_text(score, "今夜から記録できます", NK_FS_TITLE, true, NK_INK);
        nk_wrap_text(score,
                     "寝る前に「おやすみを始める」を押すと、翌朝から睡眠スコアが表示されます。",
                     NK_FS_SMALL, false, NK_INK_2);
        nk_on_click_open(nk_button(page, "おやすみを始める", true), NK_UI_NIGHT);
    } else {
        lv_obj_t *top = nk_box(score, LV_FLEX_FLOW_ROW);
        nk_text(top, "睡眠スコア", NK_FS_BODY, false, NK_INK_2);
        lv_color_t pill_col = (latest->quality == NK_QUALITY_INSUFFICIENT) ? NK_WARN : NK_GOOD;
        nk_pill(top, latest->score_label, pill_col, false);

        lv_obj_t *body = nk_box(score, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(body, 20, 0);
        nk_ring(body, 96, latest->score >= 0 ? latest->score : 0);

        lv_obj_t *facts = nk_box(body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_grow(facts, 1);
        lv_obj_set_style_pad_row(facts, 6, 0);

        char sleep_buf[32];
        nk_ui_format_duration(latest->sleep_min, sleep_buf, sizeof(sleep_buf));
        fact(facts, "睡眠時間", sleep_buf);

        char span_buf[32];
        lv_snprintf(span_buf, sizeof(span_buf), "%s → %s", latest->bedtime, latest->wake_time);
        nk_text(facts, span_buf, NK_FS_BODY, false, NK_INK_2);

        char diff_buf[32];
        if (latest->score_diff == 999) {
            lv_snprintf(diff_buf, sizeof(diff_buf), "比較なし");
        } else if (latest->score_diff > -5 && latest->score_diff < 5) {
            lv_snprintf(diff_buf, sizeof(diff_buf), "ほぼ変わらず (%+d点)",
                        (int)latest->score_diff);
        } else {
            lv_snprintf(diff_buf, sizeof(diff_buf), "%+d点", (int)latest->score_diff);
        }
        fact(facts, "前日比", diff_buf);

        /* 5 詳しく見る（日別の詳細へ） */
        nk_on_click_open(nk_button(page, "詳しく見る", false), NK_UI_RECORD_DETAIL);
    }

    /* 6 AI フィードバック */
    const char *fb_text =
        (latest != NULL)
            ? latest->short_advice
            : "スマホを枕元のマットレスの上に置くと、体の動きと音を正確に計測できます。";
    nk_advice_card(page, "AIフィードバック", fb_text, true);

    /* 7 今夜の目標と機器の接続状態 */
    lv_obj_t *tonight = nk_card(page);
    lv_obj_set_flex_flow(tonight, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tonight, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    char goal_buf[32];
    lv_snprintf(goal_buf, sizeof(goal_buf), "就寝 %s", nk_ui_store_goal_bedtime());
    nk_stat(tonight, "今夜の目標", goal_buf, true);
    nk_rect(tonight, 1, 40, NK_CARD_2, 0);
    lv_obj_t *device = nk_stat(tonight, "機器の接続状態", "接続中", true);
    lv_obj_set_style_text_color(lv_obj_get_child(device, 1), NK_GOOD, 0);

    /* 8 カーテン操作 */
    lv_obj_t *curtain = nk_box(page, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(curtain, 12, 0);
    lv_obj_t *open_btn = nk_button(curtain, "開ける", false);
    lv_obj_set_flex_grow(open_btn, 1);
    lv_obj_add_event_cb(open_btn, on_curtain_open_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_btn = nk_button(curtain, "閉じる", false);
    lv_obj_set_flex_grow(close_btn, 1);
    lv_obj_add_event_cb(close_btn, on_curtain_close_clicked, LV_EVENT_CLICKED, NULL);
}
