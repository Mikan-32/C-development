/*
 * HM-01 ホーム（UX-3 改訂案）と AD-01 アドバイスの一覧
 *
 * 上から、結果（スコア）、助言（AI フィードバック）、今夜の準備（目標・機器・カーテン）の順。
 * どの状態でも部品の並びは変えず、状態と次の行動はスコアのカードと状態の帯に出す。
 */
#include "ui_internal.h"

#include <stdint.h>
#include <stdio.h>

#define NOW_TEXT "7:03" /* デモの「今」（カーテンの確認時刻に使う） */

/* ---------------------------------------------------------------- 移動 */

static void start_night_cb(lv_event_t *e) {
    (void)e;
    nk_ui_go_tab(NK_UI_NT01);
}

static void open_detail(const nk_demo_night *night) {
    nk_demo_state *st = nk_demo_state_get();
    for (size_t i = 0; i < nk_demo_night_count(); i++) {
        const nk_demo_night *n = nk_demo_night_at(i);
        if (night != NULL && n->month == night->month && n->day == night->day) {
            st->detail_index = (int)i;
        }
    }
    nk_ui_state state = NK_UI_ST_NONE;
    if (night != NULL && night->quality == NK_Q_INSUFFICIENT) {
        state = NK_UI_ST_INSUFFICIENT;
    }
    nk_ui_push(NK_UI_LG02, state);
}

static void detail_cb(lv_event_t *e) {
    open_detail(lv_event_get_user_data(e));
}

/* スコアのカードを押すと、睡眠記録の「日」の表示（その夜）へ */
static void day_view_cb(lv_event_t *e) {
    const nk_demo_night *n = lv_event_get_user_data(e);
    if (n != NULL) {
        nk_demo_state_get()->history_day = n->day;
    }
    nk_ui_go_tab(NK_UI_LG01);
}

/* ---------------------------------------------------------------- ヘルプ */

static void help_cb(lv_event_t *e) {
    (void)e;
    lv_obj_t *sheet =
        nk_ui_sheet_open("ねむのきの使い方", "下のタブで、4 つの場所を行き来します。");
    lv_obj_t *list = nk_ui_col(sheet, 0);
    nk_ui_list_item(list, NK_ICON_HOME, "ホーム",
                    "昨夜の結果と、今夜の準備。カーテンの開け閉めもここで");
    nk_ui_list_item(list, NK_ICON_CHART, "睡眠記録",
                    "日ごとの結果と、週の傾向。「くわしく見る」で理由まで");
    nk_ui_list_item(list, NK_ICON_MOON, "睡眠モード",
                    "寝る前に「おやすみ」で計測を始めます。サウンドとアラームも");
    nk_ui_list_item(list, NK_ICON_GEAR, "設定", "寝室のカーテン、自動モード、データの扱い");
    nk_ui_btn_ghost(sheet, "閉じる", nk_ui_sheet_close_cb, NULL);
}

/* ---------------------------------------------------------------- 1 ヘッダー・2 ヘルプマーク */

static void header(lv_obj_t *body, const char *date_text) {
    lv_obj_t *top = nk_ui_row(body, 8);
    lv_obj_set_style_pad_top(top, 2, 0);
    lv_obj_t *col = nk_ui_col(top, 2);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_t *logo = nk_ui_row(col, 6);
    nk_ui_icon(logo, NK_ICON_LOGO, 26, NK_BRAND);
    nk_ui_label(logo, "ねむのき", NK_FB(22), NK_INK);
    nk_ui_label(col, date_text, NK_FR(13), NK_INK_2);

    /* 歯車は置かない（設定は下部タブから）。空いた場所にヘルプマーク */
    lv_obj_t *help = lv_button_create(top);
    lv_obj_remove_style_all(help);
    lv_obj_set_size(help, 40, 40);
    lv_obj_set_style_radius(help, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(help, NK_CARD, 0);
    lv_obj_set_style_bg_opa(help, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(help, NK_CARD_2, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(help, 1, 0);
    lv_obj_set_style_border_color(help, NK_WHITE, 0);
    lv_obj_set_style_border_opa(help, NK_OPA_LINE_2, 0);
    lv_obj_set_ext_click_area(help, 4);
    lv_obj_t *q = nk_ui_label(help, "?", NK_FB(18), NK_INK_2);
    lv_obj_center(q);
    lv_obj_add_event_cb(help, help_cb, LV_EVENT_CLICKED, NULL);
}

/* ---------------------------------------------------------------- 4 スコアのカード */

static lv_obj_t *score_card_base(lv_obj_t *body, const nk_demo_night *link) {
    lv_obj_t *card = nk_ui_card(body);
    /* いちばん大事な結果なので、枠を少し強くする */
    lv_obj_set_style_bg_color(card, lv_color_mix(NK_SERIES, NK_CARD, 24), 0);
    lv_obj_set_style_border_color(card, NK_SERIES, 0);
    lv_obj_set_style_border_opa(card, 110, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    if (link != NULL) {
        lv_obj_set_clickable(card, true);
        lv_obj_set_style_bg_color(card, lv_color_mix(NK_SERIES, NK_CARD, 40), LV_STATE_PRESSED);
        lv_obj_add_event_cb(card, day_view_cb, LV_EVENT_CLICKED, (void *)link);
    }
    return card;
}

static void score_card(lv_obj_t *body, const nk_demo_night *n, bool animate) {
    bool ref = n->quality == NK_Q_INSUFFICIENT;
    lv_obj_t *card = score_card_base(body, n);
    lv_obj_t *head = nk_ui_row(card, 8);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_ui_label(head, "睡眠スコア", NK_FB(13), NK_INK_2);
    if (ref) {
        nk_ui_tag_ref(head);
    } else {
        nk_tone tone =
            n->score >= 70 ? NK_TONE_GOOD : (n->score >= 55 ? NK_TONE_WARN : NK_TONE_SERIOUS);
        nk_ui_pill(head, nk_theme_score_label(n->score), tone, true);
    }

    lv_obj_t *row = nk_ui_row(card, 16);
    nk_ui_ring(row, 108, n->score, ref, animate);
    lv_obj_t *side = nk_ui_col(row, 4);
    lv_obj_set_width(side, 0);
    lv_obj_set_flex_grow(side, 1);
    nk_ui_label(side, "睡眠時間", NK_FR(12), NK_INK_3);
    char dur[64];
    char big[96];
    nk_fmt_duration(dur, sizeof(dur), n->sleep_min);
    (void)snprintf(big, sizeof(big), "%s%s", ref ? "約" : "", dur);
    nk_ui_label(side, big, NK_FB(22), ref ? NK_INK_2 : NK_INK);

    char a[8];
    char b[8];
    nk_fmt_clock(a, sizeof(a), n->onset);
    nk_fmt_clock(b, sizeof(b), n->wake);
    lv_obj_t *times = nk_ui_row(side, 6);
    nk_ui_icon(times, NK_ICON_MOON, 14, NK_INK_3);
    nk_ui_label(times, a, NK_FR(13), NK_INK_2);
    nk_ui_label(times, "→", NK_FR(13), NK_INK_3);
    nk_ui_icon(times, NK_ICON_SUN, 14, NK_INK_3);
    nk_ui_label(times, b, NK_FR(13), NK_INK_2);

    /* 前日比は、品質が OK の夜どうしだけで比べる。5 点未満は「ほぼ変わらず」 */
    char cmp[96];
    const nk_demo_night *prev = nk_demo_previous(n);
    while (prev != NULL && prev->quality != NK_Q_OK) {
        prev = nk_demo_previous(prev);
    }
    if (ref || prev == NULL) {
        (void)snprintf(cmp, sizeof(cmp), "前日比 —（比べられません）");
    } else {
        int diff = n->score - prev->score;
        (void)snprintf(cmp, sizeof(cmp), "前日比 %+d点%s", diff,
                       diff > -5 && diff < 5 ? "（ほぼ変わらず）" : "");
    }
    nk_ui_label(side, cmp, NK_FR(13), NK_INK_2);
}

/* 結果の代わりに、理由と次の行動を出すスコアのカード */
static void score_message(lv_obj_t *body, nk_icon icon, const char *title, const char *text,
                          const char *link, lv_event_cb_t cb, void *user_data) {
    lv_obj_t *card = score_card_base(body, NULL);
    nk_ui_label(card, "睡眠スコア", NK_FB(13), NK_INK_2);
    lv_obj_t *row = nk_ui_row(card, 12);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    nk_ui_icon_tile(row, icon, 44, NK_SERIES, 56, NK_SERIES_LT);
    lv_obj_t *col = nk_ui_col(row, 4);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    nk_ui_text(col, title, NK_FB(16), NK_INK);
    nk_ui_text(col, text, NK_FR(13), NK_INK_2);
    if (link != NULL) {
        nk_ui_link(col, link, cb, user_data);
    }
}

/* ---------------------------------------------------------------- 5 詳しく見る */

static void detail_button(lv_obj_t *body, const nk_demo_night *n) {
    lv_obj_t *btn = nk_ui_btn_ghost(body, "詳しく見る", detail_cb, (void *)n);
    lv_obj_set_height(btn, 50);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, NK_INK_2, 0);
    lv_obj_set_style_border_opa(btn, 150, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), NK_INK, 0);
    if (n == NULL) {
        lv_obj_set_disabled(btn, true);
    }
}

/* ---------------------------------------------------------------- 6 AI フィードバック */

static void feedback_card(lv_obj_t *body, const char *text, bool dim) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_clickable(card, true);
    lv_obj_set_style_bg_color(card, NK_CARD_2, LV_STATE_PRESSED);
    nk_ui_on_click_nav(card, NK_UI_AD01, NK_UI_ST_NONE, NK_UI_NAV_PUSH);
    lv_obj_t *head = nk_ui_row(card, 6);
    nk_ui_icon(head, NK_ICON_SPARK, 14, NK_BRAND);
    nk_ui_label(head, "AIフィードバック", NK_FB(12), NK_BRAND);
    nk_ui_spacer(head);
    nk_ui_icon(head, NK_ICON_RIGHT, 16, NK_INK_3);
    /* 昨夜の様子と、今夜へのひとこと。1〜2 文で、断定せず責めない（AI-5） */
    nk_ui_text(card, text, NK_FB(14), NK_INK);
    if (dim) {
        lv_obj_set_style_opa(card, LV_OPA_50, 0);
    }
}

/* ---------------------------------------------------------------- 7 今夜の目標と機器の接続状態 */

typedef enum link_state {
    LINK_CONNECTED,
    LINK_NONE,    /* 未接続 */
    LINK_OFFLINE, /* 接続が切れている */
} link_state;

static void tonight_card(lv_obj_t *body, link_state link) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(card, 12, 0);
    lv_obj_set_style_pad_column(card, 0, 0);

    lv_obj_t *goal = nk_ui_col(card, 2);
    lv_obj_set_width(goal, 0);
    lv_obj_set_flex_grow(goal, 1);
    lv_obj_set_flex_align(goal, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_clickable(goal, true);
    lv_obj_set_style_opa(goal, LV_OPA_70, LV_STATE_PRESSED);
    nk_ui_on_click_nav(goal, NK_UI_NT05, NK_UI_ST_NONE, NK_UI_NAV_PUSH);
    nk_ui_label(goal, "今夜の目標", NK_FR(12), NK_INK_3);
    char t[8];
    char text[64];
    nk_fmt_clock(t, sizeof(t), nk_demo_goal_bedtime());
    (void)snprintf(text, sizeof(text), "就寝 %s", t);
    nk_ui_label(goal, text, NK_FB(17), NK_INK);

    lv_obj_t *line = nk_ui_box(card);
    lv_obj_set_size(line, 1, 40);
    lv_obj_set_style_bg_color(line, NK_WHITE, 0);
    lv_obj_set_style_bg_opa(line, NK_OPA_LINE_2, 0);
    lv_obj_set_style_margin_hor(line, 12, 0);

    lv_obj_t *dev = nk_ui_col(card, 2);
    lv_obj_set_width(dev, 0);
    lv_obj_set_flex_grow(dev, 1);
    lv_obj_set_flex_align(dev, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_clickable(dev, true);
    lv_obj_set_style_opa(dev, LV_OPA_70, LV_STATE_PRESSED);
    nk_ui_label(dev, "機器の接続状態", NK_FR(12), NK_INK_3);
    lv_obj_t *status = nk_ui_row(dev, 6);
    lv_obj_set_width(status, LV_SIZE_CONTENT);
    lv_obj_t *dot = nk_ui_box(status);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    switch (link) {
    case LINK_NONE:
        lv_obj_set_style_bg_color(dot, NK_INK_3, 0);
        nk_ui_label(status, "未接続", NK_FB(17), NK_INK_2);
        nk_ui_label(dev, "つなぐ ›", NK_FB(12), NK_BRAND);
        nk_ui_on_click_nav(dev, NK_UI_BR02, NK_UI_ST_EMPTY_FIRST, NK_UI_NAV_PUSH);
        break;
    case LINK_OFFLINE:
        lv_obj_set_style_bg_color(dot, NK_WARN, 0);
        nk_ui_label(status, "接続が切れています", NK_FB(14), NK_WARN);
        nk_ui_label(dev, "つなぎ直す ›", NK_FB(12), NK_BRAND);
        nk_ui_on_click_nav(dev, NK_UI_BR02, NK_UI_ST_NONE, NK_UI_NAV_PUSH);
        break;
    case LINK_CONNECTED:
    default:
        lv_obj_set_style_bg_color(dot, NK_GOOD, 0);
        nk_ui_label(status, "接続中", NK_FB(17), NK_INK);
        nk_ui_on_click_nav(dev, NK_UI_BR01, NK_UI_ST_NONE, NK_UI_NAV_PUSH);
        break;
    }
}

/* ---------------------------------------------------------------- 8 カーテン操作（デモ） */

static struct {
    lv_obj_t *slot;
    lv_obj_t *open;
    lv_obj_t *close;
    int target;
} s_home;

static void curtain_result(nk_op op, const char *text, const char *sub, const char *link,
                           lv_event_cb_t cb) {
    lv_obj_clean(s_home.slot);
    nk_ui_op_result(s_home.slot, op, text, sub, link, cb, NULL);
}

static void curtain_busy(bool busy) {
    lv_obj_set_disabled(s_home.open, busy);
    lv_obj_set_disabled(s_home.close, busy);
}

static void moved_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    nk_demo_state_get()->curtain_pos = s_home.target;
    curtain_busy(false);
    /* 成功と出すのは、状態を確かめてから（UX-5 の 2 章） */
    char text[96];
    (void)snprintf(text, sizeof(text), "%s（%s 確認）",
                   s_home.target >= 100 ? "開けました" : "閉めました", NOW_TEXT);
    curtain_result(NK_OP_OK, text, NULL, NULL, NULL);
}

static void accepted_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    curtain_result(NK_OP_PENDING, "動いています", "開き具合を確かめています", NULL, NULL);
    (void)nk_ui_timer(moved_cb, 2400, NULL);
}

static void curtain_cb(lv_event_t *e) {
    s_home.target = (int)(intptr_t)lv_event_get_user_data(e);
    curtain_busy(true);
    curtain_result(NK_OP_PENDING, "カーテンに伝えています", NULL, NULL, NULL);
    (void)nk_ui_timer(accepted_cb, 900, NULL);
}

static void connect_cb(lv_event_t *e) {
    (void)e;
    nk_ui_push(NK_UI_BR02, NK_UI_ST_NONE);
}

static void recheck_cb(lv_event_t *e) {
    (void)e;
    nk_ui_replace(NK_UI_HM01, NK_UI_ST_NONE);
}

static lv_obj_t *big_outline(lv_obj_t *parent, const char *text, int target) {
    lv_obj_t *btn = nk_ui_btn_ghost(parent, text, curtain_cb, (void *)(intptr_t)target);
    lv_obj_set_height(btn, 56);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, NK_INK_2, 0);
    lv_obj_set_style_border_opa(btn, 150, 0);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_obj_set_style_text_color(label, NK_INK, 0);
    lv_obj_set_style_text_font(label, NK_FB(17), 0);
    return btn;
}

static void curtain_ops(lv_obj_t *body, nk_ui_state state, bool usable) {
    /* 親指の届く下側に、大きな 2 つのボタン（UX-3 改訂案の 8） */
    lv_obj_t *pair = nk_ui_btn_pair(body);
    s_home.open = big_outline(pair, "開ける", 100);
    s_home.close = big_outline(pair, "閉じる", 0);
    s_home.slot = nk_ui_col(body, 0);
    if (!usable) {
        curtain_busy(true);
    }
    switch (state) {
    case NK_UI_ST_CURTAIN_PENDING:
        curtain_busy(true);
        curtain_result(NK_OP_PENDING, "カーテンに伝えています", NULL, NULL, NULL);
        break;
    case NK_UI_ST_CURTAIN_FAILED:
        curtain_result(NK_OP_FAILED, "カーテンが動きませんでした",
                       "ハブがオフラインのようです。もう一度ためすか、接続を見てください",
                       "接続を見る", connect_cb);
        break;
    case NK_UI_ST_CURTAIN_UNCONFIRMED:
        curtain_result(NK_OP_UNCONFIRMED, "開ける命令は届きましたが、動いたか確認できませんでした",
                       NULL, "状態を確かめる", recheck_cb);
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------- 分析のやり直し（デモ） */

static void analyzed_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    nk_ui_replace(NK_UI_HM01, NK_UI_ST_NONE);
}

static void reanalyze_cb(lv_event_t *e) {
    (void)e;
    nk_ui_replace(NK_UI_HM01, NK_UI_ST_ANALYZING);
}

static void about_cb(lv_event_t *e) {
    (void)e;
    nk_ui_push(NK_UI_ST03, NK_UI_ST_NONE);
}

static void settings_cb(lv_event_t *e) {
    (void)e;
    nk_ui_go_tab(NK_UI_ST01);
}

static void dim_from(lv_obj_t *body, uint32_t from, uint32_t to) {
    for (uint32_t i = from; i < to; i++) {
        lv_obj_set_style_opa(lv_obj_get_child(body, (int32_t)i), LV_OPA_50, 0);
    }
}

/* ---------------------------------------------------------------- HM-01 */

#define FEEDBACK_OK "寝つきまで 28 分でした。今夜は 23:30 ごろに布団に入ってみませんか。"
#define FEEDBACK_PREV                                                                              \
    "寝る時刻をそろえると、朝が楽になりやすくなります。今夜は 23:30 ごろが目安です。"

void nk_scr_home(lv_obj_t *body, nk_ui_state state) {
    const nk_demo_night *latest = nk_demo_latest();
    char date[64];
    char date_line[96];
    nk_fmt_date(date, sizeof(date), latest->month, latest->day, latest->wday);
    (void)snprintf(date_line, sizeof(date_line), "%sの朝", date);
    header(body, date_line);
    bool animate = !nk_ui_reduce_motion();
    link_state link = state == NK_UI_ST_CURTAIN_OFFLINE ? LINK_OFFLINE : LINK_CONNECTED;

    switch (state) {
    case NK_UI_ST_EMPTY_FIRST:
        score_message(body, NK_ICON_MOON, "今夜から記録できます",
                      "寝る前に、下の「睡眠モード」で「おやすみ」を始めてください", "睡眠モードへ",
                      start_night_cb, NULL);
        detail_button(body, NULL);
        feedback_card(body, "置き方のコツ：スマホは枕元のマットレスの上に、充電しながら置きます。",
                      false);
        tonight_card(body, LINK_NONE);
        curtain_ops(body, state, false);
        return;

    case NK_UI_ST_EMPTY_DAY: {
        const nk_demo_night *prev = nk_demo_previous(latest);
        char d[64];
        char line[160];
        nk_fmt_date(d, sizeof(d), prev->month, prev->day, prev->wday);
        (void)snprintf(line, sizeof(line), "直近の記録は %sの朝（%d点）です", d, prev->score);
        score_message(body, NK_ICON_CHART, "昨夜の記録はありません", line, "直近の記録を見る",
                      detail_cb, (void *)prev);
        detail_button(body, prev);
        feedback_card(body, FEEDBACK_PREV, false);
        break;
    }

    case NK_UI_ST_SAVING:
    case NK_UI_ST_ANALYZING: {
        /* 前回の結果を薄く出し、帯で知らせる（UX-3 改訂案の「表示の決まり」） */
        bool saving = state == NK_UI_ST_SAVING;
        nk_ui_banner(body, NK_TONE_INFO, NK_ICON_NONE, true,
                     saving ? "記録を保存しています" : "昨夜の記録を分析しています",
                     "前回の結果を表示しています", NULL, NULL, NULL);
        const nk_demo_night *prev = nk_demo_previous(latest);
        uint32_t from = lv_obj_get_child_count(body);
        score_card(body, prev, false);
        detail_button(body, prev);
        dim_from(body, from, lv_obj_get_child_count(body));
        feedback_card(body, FEEDBACK_PREV, true);
        if (!nk_ui_instant() && !saving) {
            (void)nk_ui_timer(analyzed_cb, 2500, NULL);
        }
        break;
    }

    case NK_UI_ST_ANALYSIS_FAILED:
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_INFO, false, "分析を完了できませんでした",
                     "データは保存されています", "もう一度分析する", reanalyze_cb, NULL);
        score_message(body, NK_ICON_CHART, "今回は、まだスコアを出せていません",
                      "もう一度分析すると、出せることがあります。計測できた時間は 7時間32分です",
                      NULL, NULL, NULL);
        detail_button(body, latest);
        feedback_card(body, FEEDBACK_PREV, false);
        break;

    case NK_UI_ST_FAILED: {
        const nk_demo_night *failed = nk_demo_night_find(10, 24);
        char reason[256];
        (void)snprintf(reason, sizeof(reason),
                       "%s。今夜も計測すると、明日の朝にスコアが表示されます", failed->gap_reason);
        score_message(body, NK_ICON_INFO, "昨夜のデータを取得できませんでした", reason, NULL, NULL,
                      NULL);
        detail_button(body, NULL);
        feedback_card(body, FEEDBACK_PREV, false);
        break;
    }

    case NK_UI_ST_INSUFFICIENT: {
        const nk_demo_night *n = nk_demo_insufficient();
        char a[8];
        char b[8];
        char text[160];
        nk_fmt_clock(a, sizeof(a), n->gap_start);
        nk_fmt_clock(b, sizeof(b), n->gap_end);
        (void)snprintf(text, sizeof(text), "%s〜%s は計測できていません（%s）", a, b,
                       n->gap_reason);
        nk_ui_banner(body, NK_TONE_WARN, NK_ICON_INFO, false, text,
                     "計測できた時間が全体の 7 割に届かなかったため、スコアは参考値です",
                     "理由と対策を見る", about_cb, NULL);
        score_card(body, n, animate);
        detail_button(body, n);
        feedback_card(
            body, "記録が少ない夜でした。枕元のマットレスの上に、充電しながら置いてみませんか。",
            false);
        break;
    }

    case NK_UI_ST_PARTIAL_GAP:
    case NK_UI_ST_TRACKING_INTERRUPTED: {
        const nk_demo_night *n = nk_demo_partial_gap();
        if (state == NK_UI_ST_TRACKING_INTERRUPTED) {
            nk_ui_banner(body, NK_TONE_WARN, NK_ICON_INFO, false,
                         "3:10 に計測が止まっていました（アプリが停止しました）",
                         "電池の最適化の設定を見直すと、朝まで記録しやすくなります", "設定を見直す",
                         settings_cb, NULL);
        } else {
            nk_ui_banner(body, NK_TONE_INFO, NK_ICON_INFO, false, "3:10〜3:40 は計測できていません",
                         "ほかの時間で分析しました", "理由を見る", about_cb, NULL);
        }
        score_card(body, n, animate);
        detail_button(body, n);
        feedback_card(body, FEEDBACK_OK, false);
        break;
    }

    case NK_UI_ST_NONE:
    default:
        score_card(body, latest, animate);
        detail_button(body, latest);
        feedback_card(body, FEEDBACK_OK, false);
        break;
    }

    tonight_card(body, link);
    curtain_ops(body, state, link == LINK_CONNECTED);
}

/* ---------------------------------------------------------------- AD-01 */

typedef struct advice {
    const char *prio;
    nk_tone tone;
    nk_icon icon;
    const char *title;
    const char *text;
    const char *evidence[3];
    const char *steps[3];
    const char *apply;
} advice;

static const advice k_advice[] = {
    {"優先度 高",
     NK_TONE_SERIOUS,
     NK_ICON_CLOCK,
     "寝る時刻をそろえてみませんか",
     "この 1 週間、寝ついた時刻は 22:50〜1:15 と、2時間25分の幅がありました。"
     "毎晩の時刻が近いほど、体内時計が整いやすくなります。",
     {"中央値 23:45", "いちばん早い日 22:50", "いちばん遅い日 1:15"},
     {"23:45 ごろに布団に入る日を、まず週に 3 日つくる",
      "寝る時刻を先に決めて、お風呂や片づけはそこから逆算する",
      "おやすみの通知を 23:15 にセットする"},
     "おやすみの通知を 23:15 にセットしました"},
    {"優先度 中",
     NK_TONE_WARN,
     NK_ICON_MUSIC,
     "サウンドが合っているようです",
     "サウンドを使った夜は、使わなかった夜より寝つきが平均 9分 短い傾向がありました。",
     {"サウンドあり 17分（4 夜）", "なし 26分（3 夜）", NULL},
     {"今夜も「雨」を使う", NULL, NULL},
     NULL},
    {"優先度 中",
     NK_TONE_WARN,
     NK_ICON_CURTAIN,
     "アラームの少し前に、光を入れてみませんか",
     "スヌーズを 2 回以上使った朝が 3 回ありました。アラームの 10 分前にカーテンを開けると、"
     "光で目覚めやすくなることがあります。",
     {"スヌーズ 2 回以上の朝 3 回", NULL, NULL},
     {"アラームの 10 分前にカーテンを開ける", NULL, NULL},
     "カーテンを開ける時刻を、アラームの 10 分前にしました"},
    {"うまくいっていること",
     NK_TONE_INFO,
     NK_ICON_SPARK,
     "目標の睡眠時間に届いた夜が 2 夜ありました",
     "この調子で続けてみましょう。",
     {NULL, NULL, NULL},
     {NULL, NULL, NULL},
     NULL},
};

static void advice_toggle_cb(lv_event_t *e) {
    lv_obj_t *card = lv_event_get_user_data(e);
    lv_obj_t *detail = lv_obj_get_child(card, 1);
    if (lv_obj_is_hidden(detail)) {
        lv_obj_set_hidden(detail, false);
    } else {
        lv_obj_set_hidden(detail, true);
    }
}

static void advice_apply_cb(lv_event_t *e) {
    const char *message = lv_event_get_user_data(e);
    nk_ui_toast(message);
}

static void advice_item(lv_obj_t *body, const advice *a, bool open) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 0, 0);

    lv_obj_t *head = nk_ui_row(card, 12);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_clickable(head, true);
    lv_obj_add_event_cb(head, advice_toggle_cb, LV_EVENT_CLICKED, card);
    nk_ui_icon_tile(head, a->icon, 42, NK_SERIES, 56, NK_SERIES_LT);
    lv_obj_t *col = nk_ui_col(head, 4);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    nk_ui_pill(col, a->prio, a->tone, false);
    nk_ui_text(col, a->title, NK_FB(15), NK_INK);
    nk_ui_text(col, a->text, NK_FR(13), NK_INK_2);

    lv_obj_t *detail = nk_ui_col(card, 8);
    lv_obj_set_style_pad_top(detail, 12, 0);
    lv_obj_set_style_margin_top(detail, 12, 0);
    lv_obj_set_style_border_side(detail, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(detail, 1, 0);
    lv_obj_set_style_border_color(detail, NK_WHITE, 0);
    lv_obj_set_style_border_opa(detail, NK_OPA_LINE, 0);
    if (a->evidence[0] != NULL) {
        nk_ui_label(detail, "根拠", NK_FB(12), NK_INK_3);
        lv_obj_t *ev = nk_ui_row(detail, 8);
        lv_obj_set_flex_flow(ev, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_row(ev, 8, 0);
        for (int i = 0; i < 3 && a->evidence[i] != NULL; i++) {
            lv_obj_t *chip = nk_ui_box(ev);
            lv_obj_set_style_bg_color(chip, NK_CARD_2, 0);
            lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(chip, 10, 0);
            lv_obj_set_style_pad_hor(chip, 10, 0);
            lv_obj_set_style_pad_ver(chip, 6, 0);
            nk_ui_label(chip, a->evidence[i], NK_FR(12), NK_INK);
        }
    }
    if (a->steps[0] != NULL) {
        nk_ui_label(detail, "今夜からできること", NK_FB(12), NK_INK_3);
        for (int i = 0; i < 3 && a->steps[i] != NULL; i++) {
            nk_ui_notice_row(detail, NK_ICON_CHECK, NK_BRAND, a->steps[i]);
        }
    }
    if (a->apply != NULL) {
        lv_obj_t *pair = nk_ui_btn_pair(detail);
        lv_obj_t *close = nk_ui_btn_ghost(pair, "閉じる", advice_toggle_cb, card);
        lv_obj_set_flex_grow(close, 1);
        lv_obj_t *apply =
            nk_ui_btn_primary(pair, "反映する", NK_ICON_NONE, advice_apply_cb, (void *)a->apply);
        lv_obj_set_height(apply, 48);
        lv_obj_set_flex_grow(apply, 1);
    }
    if (!open || (a->evidence[0] == NULL && a->steps[0] == NULL)) {
        lv_obj_set_hidden(detail, true);
    }
}

void nk_scr_advice(lv_obj_t *body, nk_ui_state state) {
    nk_ui_subhead(body, "アドバイス");
    if (state == NK_UI_ST_NOT_ENOUGH_NIGHTS) {
        lv_obj_t *card = nk_ui_card(body);
        nk_ui_notice_row(card, NK_ICON_INFO, NK_SPIN, "傾向は 3 夜分の記録がそろうと表示されます");
        nk_ui_text(card,
                   "あと 2 夜でアドバイスが表示されます。今夜も「おやすみ」から計測してください",
                   NK_FR(13), NK_INK_2);
        nk_ui_btn_primary(body, "おやすみを始める", NK_ICON_MOON, start_night_cb, NULL);
        return;
    }
    nk_ui_text(body,
               "直近 7 "
               "日の記録から、試しやすいものを選びました。タップすると根拠と、今夜からできることを"
               "表示します。",
               NK_FR(13), NK_INK_2);
    for (size_t i = 0; i < sizeof(k_advice) / sizeof(k_advice[0]); i++) {
        advice_item(body, &k_advice[i], i == 0);
    }
    nk_ui_text(body, "アドバイスの内容を、自動で設定に反映することはありません。", NK_FR(12),
               NK_INK_3);
}
