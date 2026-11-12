/*
 * 睡眠記録（UX-4 改訂案）：LG-01 日の表示、LG-01W 週の表示、LG-02 日別の詳細（くわしく見る）
 *
 * 「日」で一日の結果を確かめ、「週」で傾向をつかみ、「くわしく見る」で理由まで確かめる。
 */
#include "ui_internal.h"

#include <stdint.h>
#include <stdio.h>

#define TODAY 31 /* デモの「今」：10 月 31 日の朝 */

static nk_ui_state state_for_day(int day) {
    const nk_demo_night *n = nk_demo_night_find(10, day);
    if (n == NULL) {
        return NK_UI_ST_EMPTY_DAY;
    }
    switch (n->quality) {
    case NK_Q_INSUFFICIENT:
        return NK_UI_ST_INSUFFICIENT;
    case NK_Q_FAILED:
        return NK_UI_ST_FAILED;
    case NK_Q_OK:
    case NK_Q_NONE:
    default:
        return NK_UI_ST_NONE;
    }
}

static void open_day(int day) {
    nk_demo_state_get()->history_day = day;
    nk_ui_replace(NK_UI_LG01, state_for_day(day));
}

static void open_detail_cb(lv_event_t *e) {
    const nk_demo_night *n = lv_event_get_user_data(e);
    nk_demo_state *st = nk_demo_state_get();
    for (size_t i = 0; i < nk_demo_night_count(); i++) {
        if (nk_demo_night_at(i) == n) {
            st->detail_index = (int)i;
        }
    }
    nk_ui_state state = NK_UI_ST_NONE;
    if (n->quality == NK_Q_INSUFFICIENT) {
        state = NK_UI_ST_INSUFFICIENT;
    } else if (n->quality == NK_Q_FAILED) {
        state = NK_UI_ST_FAILED;
    }
    nk_ui_push(NK_UI_LG02, state);
}

static void start_night_cb(lv_event_t *e) {
    (void)e;
    nk_ui_go_tab(NK_UI_NT01);
}

/* ---------------------------------------------------------------- 共通の部品 */

static void view_cb(lv_event_t *e) {
    bool week = (int)(intptr_t)lv_event_get_user_data(e) == 1;
    nk_ui_replace(week ? NK_UI_LG01W : NK_UI_LG01,
                  week ? NK_UI_ST_NONE : state_for_day(nk_demo_state_get()->history_day));
}

/* 1 日／週の切り替え（画面の幅いっぱい） */
static void view_switch(lv_obj_t *body, bool week) {
    static const char *const labels[] = {"日", "週"};
    lv_obj_t *seg = nk_ui_row(body, 4);
    lv_obj_set_style_pad_all(seg, 4, 0);
    lv_obj_set_style_radius(seg, 14, 0);
    lv_obj_set_style_bg_color(seg, NK_CARD, 0);
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(seg, 1, 0);
    lv_obj_set_style_border_color(seg, NK_WHITE, 0);
    lv_obj_set_style_border_opa(seg, NK_OPA_LINE, 0);
    lv_obj_set_style_margin_top(seg, 4, 0);
    for (int i = 0; i < 2; i++) {
        bool on = (i == 1) == week;
        lv_obj_t *b = lv_button_create(seg);
        lv_obj_remove_style_all(b);
        lv_obj_set_height(b, 38);
        lv_obj_set_width(b, 0);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_radius(b, 10, 0);
        lv_obj_set_style_bg_color(b, NK_SELECT, 0);
        lv_obj_set_style_bg_opa(b, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(b, on ? LV_OPA_COVER : 40, LV_STATE_PRESSED);
        lv_obj_t *l = nk_ui_label(b, labels[i], NK_FB(15), on ? NK_WHITE : NK_INK_2);
        lv_obj_center(l);
        if (!on) {
            lv_obj_add_event_cb(b, view_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
    }
}

/* 2 日付・期間（‹ 前　　次 ›）。未来には進めない */
static lv_obj_t *step_row(lv_obj_t *body, const char *text, lv_event_cb_t cb, bool can_prev,
                          bool can_next) {
    lv_obj_t *row = nk_ui_row(body, 8);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, NK_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 14, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, NK_WHITE, 0);
    lv_obj_set_style_border_opa(row, NK_OPA_LINE, 0);
    lv_obj_set_style_pad_hor(row, 6, 0);
    lv_obj_set_style_pad_ver(row, 4, 0);
    lv_obj_t *prev = nk_ui_icon_btn(row, NK_ICON_LEFT, 36, cb, (void *)(intptr_t)-1);
    lv_obj_set_style_border_width(prev, 0, 0);
    lv_obj_set_style_bg_opa(prev, LV_OPA_TRANSP, 0);
    nk_ui_label(row, text, NK_FB(16), NK_INK);
    lv_obj_t *next = nk_ui_icon_btn(row, NK_ICON_RIGHT, 36, cb, (void *)(intptr_t)1);
    lv_obj_set_style_border_width(next, 0, 0);
    lv_obj_set_style_bg_opa(next, LV_OPA_TRANSP, 0);
    if (!can_prev) {
        lv_obj_set_disabled(prev, true);
        lv_obj_set_style_opa(prev, LV_OPA_30, 0);
    }
    if (!can_next) {
        lv_obj_set_disabled(next, true);
        lv_obj_set_style_opa(next, LV_OPA_30, 0);
    }
    return row;
}

/* スコアなど大事な結果のカード（枠を少し強くする） */
static lv_obj_t *strong_card(lv_obj_t *body) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_bg_color(card, lv_color_mix(NK_SERIES, NK_CARD, 24), 0);
    lv_obj_set_style_border_color(card, NK_SERIES, 0);
    lv_obj_set_style_border_opa(card, 110, 0);
    return card;
}

static void outline_strong(lv_obj_t *btn) {
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, NK_INK_2, 0);
    lv_obj_set_style_border_opa(btn, 150, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), NK_INK, 0);
}

static void feedback_card(lv_obj_t *body, const char *title, const char *text) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_t *head = nk_ui_row(card, 6);
    nk_ui_icon(head, NK_ICON_SPARK, 14, NK_BRAND);
    nk_ui_label(head, title, NK_FB(12), NK_BRAND);
    nk_ui_text(card, text, NK_FB(14), NK_INK);
}

static void legend_item(lv_obj_t *row, lv_color_t color, bool dashed, const char *text) {
    lv_obj_t *item = nk_ui_box(row);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(item, 4, 0);
    lv_obj_t *sw = nk_ui_box(item);
    lv_obj_set_size(sw, 11, 11);
    lv_obj_set_style_radius(sw, 2, 0);
    if (dashed) {
        lv_obj_set_style_border_width(sw, 1, 0);
        lv_obj_set_style_border_color(sw, NK_INK_3, 0);
    } else {
        lv_obj_set_style_bg_color(sw, color, 0);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    }
    nk_ui_label(item, text, NK_FR(11), NK_INK_3);
}

static const char *interpretation(const nk_demo_night *n) {
    if (n->quality == NK_Q_INSUFFICIENT) {
        return "参考値：記録が少ない夜のため、目安です";
    }
    if (n->score >= 85) {
        return "とても良い：ぐっすり眠れたようです";
    }
    if (n->score >= 70) {
        return "良い：寝つきも目覚めも、落ち着いていました";
    }
    if (n->score >= 55) {
        return "まずまず：少し整えどころがありそうです";
    }
    return "回復を優先：今夜は早めに休んでみませんか";
}

/* ---------------------------------------------------------------- LG-01 日の表示 */

static void day_step_cb(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    int day = nk_demo_state_get()->history_day + delta;
    if (day >= 1 && day <= TODAY) {
        open_day(day);
    }
}

static void day_score_card(lv_obj_t *body, const nk_demo_night *n) {
    bool ref = n->quality == NK_Q_INSUFFICIENT;
    lv_obj_t *card = strong_card(body);
    lv_obj_set_style_pad_row(card, 2, 0);
    lv_obj_t *head = nk_ui_row(card, 8);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_ui_label(head, "睡眠スコア", NK_FB(13), NK_INK_2);
    if (ref) {
        nk_ui_tag_ref(head);
    }
    lv_obj_t *num = nk_ui_row(card, 2);
    lv_obj_set_flex_align(num, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    char score[16];
    (void)snprintf(score, sizeof(score), "%d", n->score);
    lv_color_t color = ref ? NK_INK_2 : nk_theme_score_color(n->score, false);
    nk_ui_label(num, score, NK_FB(44), color);
    lv_obj_t *unit = nk_ui_label(num, "点", NK_FB(18), color);
    lv_obj_set_style_pad_bottom(unit, 8, 0);
    nk_ui_text(card, interpretation(n), NK_FR(13), NK_INK_2);
}

static void times_row(lv_obj_t *body, const nk_demo_night *n) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_ver(card, 12, 0);
    char a[8];
    char b[8];
    char c[64];
    nk_fmt_clock(a, sizeof(a), n->onset);
    nk_fmt_clock(b, sizeof(b), n->wake);
    nk_fmt_duration(c, sizeof(c), n->sleep_min);
    const char *keys[] = {"就寝", "起床", "睡眠時間"};
    const char *values[] = {a, b, c};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *col = nk_ui_col(card, 2);
        lv_obj_set_width(col, 0);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        nk_ui_label(col, keys[i], NK_FR(12), NK_INK_3);
        nk_ui_label(col, values[i], NK_FB(17), NK_INK);
    }
}

static void stage_overview(lv_obj_t *body, const nk_demo_night *n) {
    bool two_tone = n->quality == NK_Q_INSUFFICIENT;
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, "睡眠段階の概要");
    nk_ui_tag_est(lv_obj_get_child(head, 0));
    nk_ui_hint(head, "1時間ごとに区切る");
    nk_ui_stage_band(card, n, two_tone);
    lv_obj_t *legend = nk_ui_row(card, 12);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(legend, 4, 0);
    if (two_tone) {
        legend_item(legend, NK_COLOR_SLEEP, false, "眠り");
    } else {
        legend_item(legend, NK_COLOR_LIGHT, false, "浅い睡眠");
        legend_item(legend, NK_COLOR_DEEP, false, "深い睡眠");
    }
    legend_item(legend, NK_COLOR_WAKE, false, "覚醒");
    legend_item(legend, NK_INK_3, true, "取得なし");
    nk_ui_text(card,
               two_tone ? "記録が少ない夜は、浅い睡眠と深い睡眠を分けずに表示します"
                        : "取得なしの時間は、点線の枠で示します",
               NK_FR(11), NK_INK_3);
}

static const char *day_feedback(const nk_demo_night *n, char *buf, size_t size) {
    if (n->quality == NK_Q_INSUFFICIENT) {
        return "記録が少ない夜でした。枕元のマットレスの上に、充電しながら置いてみませんか。";
    }
    if (n->day == TODAY) {
        return "寝つきまで 28 分でした。今夜は 23:30 ごろに布団に入ってみませんか。";
    }
    char w[64];
    nk_fmt_duration(w, sizeof(w), n->waso_min);
    (void)snprintf(buf, size, "寝つきまで %d 分、夜中に目覚めた時間は合計 %s でした。", n->latency,
                   w);
    return buf;
}

void nk_scr_history_day(lv_obj_t *body, nk_ui_state state) {
    nk_demo_state *st = nk_demo_state_get();
    /* 状態を指定して開いたときは、その状態の日を出す */
    if (state == NK_UI_ST_EMPTY_DAY && nk_demo_night_find(10, st->history_day) != NULL) {
        st->history_day = 21;
    } else if (state == NK_UI_ST_INSUFFICIENT) {
        st->history_day = 27;
    } else if (state == NK_UI_ST_FAILED) {
        st->history_day = 24;
    }
    int day = st->history_day;
    view_switch(body, false);

    char date[64];
    nk_fmt_date(date, sizeof(date), 10, day, nk_demo_wday(10, day));
    step_row(body, date, day_step_cb, day > 1, day < TODAY);

    if (state == NK_UI_ST_EMPTY_FIRST) {
        nk_ui_empty(body, "まだ記録がありません", "最初の記録は、今夜おやすみを始めると作られます",
                    "睡眠モードへ", start_night_cb, NULL);
        return;
    }
    const nk_demo_night *n = nk_demo_night_find(10, day);
    if (n == NULL) {
        /* 記録がない日は「くわしく見る」を出さない（UX-4 改訂案） */
        lv_obj_t *card = nk_ui_card(body);
        lv_obj_set_style_pad_ver(card, 22, 0);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        nk_ui_icon(card, NK_ICON_MOON, 40, lv_color_hex(0x5C66A8));
        nk_ui_label(card, "この日の記録はありません", NK_FB(15), NK_INK);
        nk_ui_label(card, "計測しなかった夜は、ここが空きます", NK_FR(13), NK_INK_2);
        return;
    }
    if (n->quality == NK_Q_FAILED) {
        lv_obj_t *card = strong_card(body);
        nk_ui_label(card, "睡眠スコア", NK_FB(13), NK_INK_2);
        nk_ui_text(card, "この夜のデータを取得できませんでした", NK_FB(16), NK_INK);
        nk_ui_text(card, n->gap_reason, NK_FR(13), NK_INK_2);
        nk_ui_text(card, "今夜も計測すると、次の朝にスコアが表示されます", NK_FR(13), NK_INK_2);
        lv_obj_t *more =
            nk_ui_btn_ghost(body, "くわしく見る（取れた範囲）", open_detail_cb, (void *)n);
        outline_strong(more);
        return;
    }

    day_score_card(body, n);
    times_row(body, n);
    stage_overview(body, n);
    char buf[192];
    feedback_card(body, "その日のひとこと", day_feedback(n, buf, sizeof(buf)));
    lv_obj_t *more = nk_ui_btn_ghost(body, "くわしく見る", open_detail_cb, (void *)n);
    lv_obj_set_height(more, 50);
    outline_strong(more);
}

/* ---------------------------------------------------------------- LG-01W 週の表示 */

static void week_step_cb(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    int monday = nk_demo_state_get()->history_week + delta * 7;
    if (monday >= 5 && monday <= 26) {
        nk_demo_state_get()->history_week = monday;
        nk_ui_replace(NK_UI_LG01W, NK_UI_ST_NONE);
    }
}

static void week_chart_cb(lv_event_t *e) {
    int day = nk_ui_week_chart_day(lv_event_get_current_target_obj(e));
    if (day >= 1 && day <= TODAY) {
        open_day(day);
    }
}

static void week_chart_card(lv_obj_t *body, const char *title, const char *hint, bool scores,
                            int monday) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, title);
    nk_ui_hint(head, hint);
    lv_obj_t *chart = scores ? nk_ui_week_scores(card, monday) : nk_ui_week_bars(card, monday);
    lv_obj_add_event_cb(chart, week_chart_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void nk_scr_history_week(lv_obj_t *body, nk_ui_state state) {
    nk_demo_state *st = nk_demo_state_get();
    if (state == NK_UI_ST_NOT_ENOUGH_NIGHTS) {
        st->history_week = 12;
    }
    int monday = st->history_week;
    view_switch(body, true);

    char period[64];
    int sunday = monday + 6;
    if (sunday > 31) {
        (void)snprintf(period, sizeof(period), "10/%d〜11/%d", monday, sunday - 31);
    } else {
        (void)snprintf(period, sizeof(period), "10/%d〜10/%d", monday, sunday);
    }
    step_row(body, period, week_step_cb, monday > 5, monday < 26);

    if (state == NK_UI_ST_EMPTY_FIRST) {
        nk_ui_empty(body, "まだ記録がありません", "最初の記録は、今夜おやすみを始めると作られます",
                    "睡眠モードへ", start_night_cb, NULL);
        return;
    }

    /* 3 継続記録 */
    lv_obj_t *streak = nk_ui_card(body);
    lv_obj_set_flex_flow(streak, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(streak, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(streak, 12, 0);
    nk_ui_icon_tile(streak, NK_ICON_CHECK, 30, NK_GOOD, 41, NK_GOOD);
    nk_ui_label(streak, "継続記録", NK_FR(14), NK_INK_2);
    nk_ui_spacer(streak);
    char days[32];
    (void)snprintf(days, sizeof(days), "%d日連続", nk_demo_streak());
    nk_ui_label(streak, days, NK_FB(18), NK_INK);

    /* 4・5 推移のグラフ */
    week_chart_card(body, "睡眠時間の推移", "棒を押すと日の表示へ", false, monday);
    week_chart_card(body, "睡眠スコアの推移", "点を押すと日の表示へ", true, monday);

    /* 6 週の平均（OK の夜だけ。3 夜未満なら出さない） */
    int ok = 0;
    int sum_sleep = 0;
    int sum_score = 0;
    for (int i = 0; i < 7; i++) {
        const nk_demo_night *n = monday + i <= 31 ? nk_demo_night_find(10, monday + i) : NULL;
        if (n != NULL && n->quality == NK_Q_OK) {
            ok++;
            sum_sleep += n->sleep_min;
            sum_score += n->score;
        }
    }
    lv_obj_t *avg = nk_ui_card(body);
    lv_obj_set_style_pad_row(avg, 4, 0);
    nk_ui_label(avg, "週の平均", NK_FB(15), NK_INK);
    if (ok >= 3) {
        char sleep[64];
        char line[128];
        char note[64];
        nk_fmt_duration(sleep, sizeof(sleep), (sum_sleep + ok / 2) / ok);
        (void)snprintf(line, sizeof(line), "睡眠時間 %s　スコア %d点", sleep,
                       (sum_score + ok / 2) / ok);
        (void)snprintf(note, sizeof(note), "分析できた %d 夜の平均", ok);
        nk_ui_label(avg, line, NK_FB(16), NK_INK);
        nk_ui_label(avg, note, NK_FR(12), NK_INK_3);
    } else {
        char line[96];
        (void)snprintf(line, sizeof(line), "あと %d 夜で平均が表示されます", 3 - ok);
        nk_ui_notice_row(avg, NK_ICON_INFO, NK_SPIN, line);
        nk_ui_label(avg, "平均は、分析できた夜が 3 夜そろうと出します", NK_FR(12), NK_INK_3);
    }

    /* 7 週のひとこと（文言は仮） */
    feedback_card(
        body, "週のひとこと",
        ok >= 3
            ? "寝ついた時刻に 2 時間ほどの幅がありました。そろえると、朝が楽になりやすくなります。"
            : "記録がそろってくると、ここに週の傾向が出ます。");
}

/* ---------------------------------------------------------------- LG-02 日別の詳細 */

static const nk_demo_night *current_night(nk_ui_state state) {
    const nk_demo_night *n = nk_demo_night_at((size_t)nk_demo_state_get()->detail_index);
    if (state == NK_UI_ST_PARTIAL_GAP) {
        return nk_demo_partial_gap();
    }
    if (state == NK_UI_ST_INSUFFICIENT && (n == NULL || n->quality != NK_Q_INSUFFICIENT)) {
        return nk_demo_insufficient();
    }
    if (state == NK_UI_ST_FAILED && (n == NULL || n->quality != NK_Q_FAILED)) {
        return nk_demo_night_find(10, 24);
    }
    return n != NULL ? n : nk_demo_latest();
}

static void detail_step_cb(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    nk_demo_state *st = nk_demo_state_get();
    int next = st->detail_index + delta;
    if (next >= 0 && next < (int)nk_demo_night_count()) {
        st->detail_index = next;
        const nk_demo_night *n = nk_demo_night_at((size_t)next);
        nk_ui_state state = NK_UI_ST_NONE;
        if (n->quality == NK_Q_INSUFFICIENT) {
            state = NK_UI_ST_INSUFFICIENT;
        } else if (n->quality == NK_Q_FAILED) {
            state = NK_UI_ST_FAILED;
        }
        nk_ui_replace(NK_UI_LG02, state);
    }
}

static void night_title(char *buf, size_t size, const nk_demo_night *n) {
    (void)snprintf(buf, size, "%d月%d日（%s）の夜 → %d日（%s）の朝", n->month, n->day - 1,
                   nk_demo_wday_name(n->wday + 6), n->day, nk_demo_wday_name(n->wday));
}

typedef struct score_part {
    const char *name;
    int points;
    int max;
} score_part;

/* 項目ごとの点数（AI-4 の 1 章）。合計がスコアと合うように、規則性で調整する（デモ） */
static void score_parts(const nk_demo_night *n, score_part parts[5]) {
    static const char *const names[] = {"睡眠時間", "寝つき", "連続性", "効率", "規則性"};
    static const int maxes[] = {30, 15, 25, 15, 15};
    int p[5];
    int tst = n->sleep_min;
    p[0] = tst >= 420 ? 30 : (tst <= 240 ? 0 : (30 * (tst - 240) + 90) / 180);
    p[1] = n->latency <= 15 ? 15 : (n->latency <= 30 ? 12 : (n->latency <= 45 ? 8 : 5));
    p[2] =
        n->waso_min <= 20 ? 25 : (n->waso_min >= 120 ? 0 : (25 * (120 - n->waso_min) + 50) / 100);
    p[3] = n->efficiency >= 85 ? 15 : (n->efficiency <= 60 ? 0 : 15 * (n->efficiency - 60) / 25);
    int rest = n->score - (p[0] + p[1] + p[2] + p[3]);
    p[4] = rest < 0 ? 0 : (rest > 15 ? 15 : rest);
    if (rest < 0) {
        p[2] = p[2] + rest < 0 ? 0 : p[2] + rest;
    }
    for (int i = 0; i < 5; i++) {
        parts[i].name = names[i];
        parts[i].points = p[i];
        parts[i].max = maxes[i];
    }
}

static void detail_score_card(lv_obj_t *body, const nk_demo_night *n, bool reference) {
    lv_obj_t *card = strong_card(body);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_t *head = nk_ui_row(card, 8);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_ui_label(head, "睡眠スコア", NK_FB(13), NK_INK_2);
    if (reference) {
        nk_ui_tag_ref(head);
    } else {
        nk_tone tone =
            n->score >= 70 ? NK_TONE_GOOD : (n->score >= 55 ? NK_TONE_WARN : NK_TONE_SERIOUS);
        nk_ui_pill(head, nk_theme_score_label(n->score), tone, true);
    }

    lv_obj_t *row = nk_ui_row(card, 14);
    nk_ui_ring(row, 88, n->score, reference, false);
    lv_obj_t *side = nk_ui_col(row, 4);
    lv_obj_set_width(side, 0);
    lv_obj_set_flex_grow(side, 1);
    char dur[64];
    char line[128];
    nk_fmt_duration(dur, sizeof(dur), n->sleep_min);
    (void)snprintf(line, sizeof(line), "睡眠時間 %s%s", reference ? "約" : "", dur);
    nk_ui_label(side, line, NK_FB(15), NK_INK);
    const nk_demo_night *prev = nk_demo_previous(n);
    while (prev != NULL && prev->quality != NK_Q_OK) {
        prev = nk_demo_previous(prev);
    }
    if (reference || prev == NULL) {
        (void)snprintf(line, sizeof(line), "前日比 —（比べられません）");
    } else {
        int diff = n->score - prev->score;
        (void)snprintf(line, sizeof(line), "前日比 %+d点%s", diff,
                       diff > -5 && diff < 5 ? "（ほぼ変わらず）" : "");
    }
    nk_ui_label(side, line, NK_FR(13), NK_INK_2);

    /* 内訳（5 項目）。最も達成率が低い項目を 1 つだけ強調する（AI-4） */
    score_part parts[5];
    score_parts(n, parts);
    int low = 0;
    for (int i = 1; i < 5; i++) {
        if (parts[i].points * parts[low].max < parts[low].points * parts[i].max) {
            low = i;
        }
    }
    lv_obj_t *t = nk_ui_label(card, "内訳（項目ごとの点数）", NK_FB(13), NK_INK);
    lv_obj_set_style_margin_top(t, 4, 0);
    for (int i = 0; i < 5; i++) {
        lv_obj_t *r = nk_ui_row(card, 10);
        lv_obj_t *k = nk_ui_label(r, parts[i].name, NK_FR(13), NK_INK_2);
        lv_obj_set_width(k, 64);
        lv_obj_t *bar = lv_bar_create(r);
        lv_obj_set_height(bar, 8);
        lv_obj_set_width(bar, 0);
        lv_obj_set_flex_grow(bar, 1);
        lv_bar_set_range(bar, 0, parts[i].max);
        lv_bar_set_value(bar, parts[i].points, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, NK_BG, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_color_t color = reference ? NK_INK_3 : (i == low ? NK_SERIOUS : NK_SERIES);
        lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
        char v[16];
        (void)snprintf(v, sizeof(v), "%d/%d", parts[i].points, parts[i].max);
        lv_obj_t *val = nk_ui_label(r, v, NK_FB(13), NK_INK);
        lv_obj_set_width(val, 48);
        lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
    }
    char hint[192];
    if (reference) {
        (void)snprintf(hint, sizeof(hint), "参考値のため、項目ごとの点数は目安です");
        nk_ui_text(card, hint, NK_FR(12), NK_INK_3);
    } else {
        (void)snprintf(hint, sizeof(hint), "最も低い「%s」：ここを整えると良さそう",
                       parts[low].name);
        nk_ui_text(card, hint, NK_FB(12), NK_SERIOUS);
    }
}

static void about_cb(lv_event_t *e) {
    (void)e;
    nk_ui_push(NK_UI_ST03, NK_UI_ST_NONE);
}

static void stage_card(lv_obj_t *body, const nk_demo_night *n) {
    static const char *const names[] = {"寝返り",   "いびき",   "大きな音",
                                        "サウンド", "カーテン", "アラーム"};
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, "睡眠段階の推移");
    nk_ui_tag_est(lv_obj_get_child(head, 0));
    nk_ui_link(head, "推定できる範囲のみ ?", about_cb, NULL);
    nk_ui_stage_steps(card, n);
    lv_obj_t *legend = nk_ui_row(card, 10);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(legend, 4, 0);
    for (int i = 0; i < 6; i++) {
        lv_obj_t *item = nk_ui_box(legend);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(item, 4, 0);
        lv_obj_t *dot = nk_ui_box(item);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, nk_ui_mark_color(i), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        nk_ui_label(item, names[i], NK_FR(11), NK_INK_3);
    }
    nk_ui_text(card, "帯の上の印は、出来事の時刻です。横軸は時刻（1時間ごと）。取得なしは点線",
               NK_FR(11), NK_INK_3);
}

static void metric(lv_obj_t *grid, const char *key, const char *value, bool estimated) {
    lv_obj_t *cell = nk_ui_col(grid, 2);
    lv_obj_set_width(cell, (NK_CARD_INNER_W - 12) / 2);
    lv_obj_t *k = nk_ui_row(cell, 4);
    nk_ui_label(k, key, NK_FR(12), NK_INK_3);
    if (estimated) {
        nk_ui_tag_est(k);
    }
    nk_ui_text(cell, value, NK_FB(15), NK_INK);
}

static void metrics_card(lv_obj_t *body, const nk_demo_night *n, bool reference) {
    lv_obj_t *card = nk_ui_card(body);
    nk_ui_card_head(card, "指標の表");
    lv_obj_t *grid = nk_ui_row(card, 12);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid, 10, 0);
    char a[64];
    char b[96];
    nk_fmt_duration(a, sizeof(a), n->in_bed_min);
    metric(grid, "寝床にいた時間", a, false);
    nk_fmt_duration(a, sizeof(a), n->sleep_min);
    (void)snprintf(b, sizeof(b), "%s%s", reference ? "約" : "", a);
    metric(grid, "睡眠時間", b, false);
    nk_fmt_duration(a, sizeof(a), n->latency);
    metric(grid, "寝つき", a, false);
    nk_fmt_duration(a, sizeof(a), n->waso_min);
    (void)snprintf(b, sizeof(b), "%d回・合計%s", n->waso_count, a);
    metric(grid, "中途覚醒", b, false);
    if (n->efficiency >= 0) {
        (void)snprintf(b, sizeof(b), "%d%%", n->efficiency);
        metric(grid, "効率", b, false);
    } else {
        metric(grid, "効率", "記録なし（データ不足）", false);
    }
    if (n->deep_min >= 0) {
        nk_fmt_duration(a, sizeof(a), n->deep_min);
        metric(grid, "深い眠り", a, true);
        nk_fmt_duration(a, sizeof(a), n->sleep_min - n->deep_min);
        metric(grid, "浅い眠り", a, true);
    } else {
        metric(grid, "深い眠り", "分けずに表示（不足）", true);
        metric(grid, "浅い眠り", "分けずに表示（不足）", true);
    }
    (void)snprintf(b, sizeof(b), "%d回", n->turns);
    metric(grid, "寝返り", b, false);
    if (n->snore_min >= 0) {
        nk_fmt_duration(a, sizeof(a), n->snore_min);
        metric(grid, "いびき", a, true);
        (void)snprintf(b, sizeof(b), "平均 %d回/分", n->breath_rate);
        metric(grid, "呼吸数", b, true);
    } else {
        metric(grid, "いびき", "記録なし（止まっていた時間）", true);
        metric(grid, "呼吸数", "記録なし（止まっていた時間）", true);
    }
    nk_ui_text(card, "推定の項目には「推定」の札。取れない項目は理由を示します", NK_FR(11),
               NK_INK_3);
}

static void acquisition_card(lv_obj_t *body, const nk_demo_night *n) {
    lv_obj_t *card = nk_ui_card(body);
    nk_ui_card_head(card, "取得状況");
    char a[64];
    char b[64];
    char text[192];
    nk_fmt_duration(a, sizeof(a), n->measured_min);
    nk_fmt_duration(b, sizeof(b), n->in_bed_min);
    lv_obj_t *row = nk_ui_row(card, 8);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_ui_label(row, "計測できた時間", NK_FR(13), NK_INK_2);
    (void)snprintf(text, sizeof(text), "%s／%s", a, b);
    nk_ui_label(row, text, NK_FB(13), NK_INK);
    nk_ui_quality_bar(card, n);
    if (n->gap_start >= 0) {
        char s[8];
        char e[8];
        nk_fmt_clock(s, sizeof(s), n->gap_start);
        nk_fmt_clock(e, sizeof(e), n->gap_end);
        (void)snprintf(text, sizeof(text), "取得できなかった区間 %s〜%s（%s）", s, e,
                       n->gap_reason);
        nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN, text);
    } else if (n->quality == NK_Q_FAILED) {
        nk_ui_notice_row(card, NK_ICON_INFO, NK_SERIOUS, n->gap_reason);
    } else {
        nk_ui_notice_row(card, NK_ICON_CHECK, NK_GOOD, "取得できなかった区間：なし");
    }
    nk_ui_notice_row(card, NK_ICON_PHONE, NK_INK_3,
                     "置き方の確認：OK ／ 使ったセンサー：スマホ（加速度・マイク）");
}

static void advice_card(lv_obj_t *body, bool reference) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 4, 0);
    nk_ui_card_head(card, "その日のアドバイス");
    nk_ui_label(card, "根拠", NK_FB(12), NK_INK_3);
    nk_ui_text(card,
               reference
                   ? "計測できた時間が全体の 7 割に届かなかったため、記録が少ない夜でした"
                   : "寝ついた時刻が、いつもより遅かったため、規則性の点数が下がる傾向があります",
               NK_FR(13), NK_INK);
    lv_obj_t *l = nk_ui_label(card, "提案（睡眠習慣）", NK_FB(12), NK_INK_3);
    lv_obj_set_style_margin_top(l, 6, 0);
    nk_ui_text(card,
               reference ? "枕元のマットレスの上に、充電しながら置いてみませんか"
                         : "今夜は 23:30 ごろに布団に入ってみませんか",
               NK_FR(13), NK_INK);
    l = nk_ui_label(card, "提案（カーテン設定）", NK_FB(12), NK_INK_3);
    lv_obj_set_style_margin_top(l, 6, 0);
    nk_ui_text(card, "カーテンを開ける時刻を、アラームの 10 分前に変えてみませんか", NK_FR(13),
               NK_INK);
}

static void delete_confirm_cb(lv_event_t *e) {
    (void)e;
    nk_ui_sheet_close();
    nk_ui_toast("この夜の記録を削除しました（デモ）");
}

static void delete_cb(lv_event_t *e) {
    (void)e;
    lv_obj_t *sheet =
        nk_ui_sheet_open("この夜の記録を削除しますか",
                         "スコアと睡眠段階、細かな計測値を削除します。元に戻せません。");
    lv_obj_t *pair = nk_ui_btn_pair(sheet);
    lv_obj_t *cancel = nk_ui_btn_ghost(pair, "やめる", nk_ui_sheet_close_cb, NULL);
    lv_obj_set_flex_grow(cancel, 1);
    lv_obj_t *ok = nk_ui_btn_serious(pair, "削除する", delete_confirm_cb, NULL);
    lv_obj_set_flex_grow(ok, 1);
}

static void mood_cb(lv_event_t *e) {
    (void)e;
    lv_obj_t *sheet = nk_ui_sheet_open("今朝の目覚めは？", "任意です。スコアには入れません。");
    lv_obj_t *row = nk_ui_row(sheet, 8);
    static const char *const moods[] = {"すっきり", "ふつう", "ねむい"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *chip = nk_ui_chip(row, moods[i], false, nk_ui_sheet_close_cb, NULL);
        lv_obj_set_flex_grow(chip, 1);
        lv_obj_set_height(chip, 48);
    }
}

void nk_scr_day(lv_obj_t *body, nk_ui_state state) {
    const nk_demo_night *n = current_night(state);
    nk_ui_subhead(body, "日別の詳細");

    /* 1 見出しの日付と、前の日・次の日 */
    char title[160];
    night_title(title, sizeof(title), n);
    lv_obj_t *nav = nk_ui_row(body, 4);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_top(nav, -6, 0);
    int idx = nk_demo_state_get()->detail_index;
    lv_obj_t *prev = nk_ui_icon_btn(nav, NK_ICON_LEFT, 32, detail_step_cb, (void *)(intptr_t)-1);
    lv_obj_t *label = nk_ui_label(nav, title, NK_FB(14), NK_INK);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *next = nk_ui_icon_btn(nav, NK_ICON_RIGHT, 32, detail_step_cb, (void *)(intptr_t)1);
    if (idx <= 0) {
        lv_obj_set_disabled(prev, true);
        lv_obj_set_style_opa(prev, LV_OPA_30, 0);
    }
    if (idx >= (int)nk_demo_night_count() - 1) {
        lv_obj_set_disabled(next, true);
        lv_obj_set_style_opa(next, LV_OPA_30, 0);
    }

    /* 2 分析の確かさ（先に「どこまで信頼できるか」を示す） */
    lv_obj_t *pills = nk_ui_row(body, 6);
    if (state == NK_UI_ST_FAILED || n->quality == NK_Q_FAILED) {
        nk_ui_pill(pills, "分析の確かさ：取得失敗", NK_TONE_SERIOUS, true);
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_INFO, false, "この夜は分析できませんでした",
                     n->gap_reason, NULL, NULL, NULL);
        acquisition_card(body, n);
        nk_ui_btn_ghost(body, "この夜の記録を削除（確認つき）", delete_cb, NULL);
        return;
    }
    if (state == NK_UI_ST_ANALYSIS_FAILED) {
        nk_ui_pill(pills, "分析の確かさ：分析できず", NK_TONE_SERIOUS, true);
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_INFO, false, "分析を完了できませんでした",
                     "データは保存されています", "もう一度分析する", NULL, NULL);
        acquisition_card(body, n);
        return;
    }
    bool reference = n->quality == NK_Q_INSUFFICIENT;
    if (reference) {
        nk_ui_pill(pills, "分析の確かさ：不足", NK_TONE_WARN, true);
    } else if (n->gap_start >= 0) {
        nk_ui_pill(pills, "分析の確かさ：分析可能（一部欠損）", NK_TONE_GOOD, true);
    } else {
        nk_ui_pill(pills, "分析の確かさ：分析可能", NK_TONE_GOOD, true);
    }

    detail_score_card(body, n, reference);
    stage_card(body, n);
    metrics_card(body, n, reference);
    acquisition_card(body, n);
    advice_card(body, reference);

    lv_obj_t *mood = nk_ui_btn_ghost(body, "目覚めの気分を記録", mood_cb, NULL);
    outline_strong(mood);
    nk_ui_btn_ghost(body, "この夜の記録を削除（確認つき）", delete_cb, NULL);
}
