/*
 * LG-01 睡眠記録（日・週）と LG-02 日別の詳細（UX-4 改訂案の図）
 */
#include "nk_widgets.h"
#include "ui_internal.h"

#define DAYS 7

static lv_point_precise_t s_goal_line[2];
static lv_point_precise_t s_score_line[DAYS];
static bool s_record_store_ready = false;

static void ensure_record_store(void) {
    if (!s_record_store_ready) {
        nk_ui_store_init();
        s_record_store_ready = true;
    }
}

static void axis_label(lv_obj_t *parent, const char *text, int32_t x, int32_t y, int edge) {
    lv_obj_t *label = nk_text(parent, text, NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_translate_x(label, LV_PCT(edge < 0 ? 0 : edge > 0 ? -100 : -50), 0);
}

static lv_obj_t *plot_area(lv_obj_t *parent, int32_t h) {
    lv_obj_t *area = nk_rect(parent, NK_CARD_INNER_W, h, NK_CARD, 0);
    lv_obj_set_style_bg_opa(area, LV_OPA_TRANSP, 0);
    return area;
}

static void legend_item(lv_obj_t *parent, lv_color_t color, const char *text, bool outline) {
    lv_obj_t *item = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(item, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(item, 4, 0);
    lv_obj_t *swatch = nk_rect(item, 12, 12, color, 2);
    if (outline) {
        lv_obj_set_style_bg_opa(swatch, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(swatch, 1, 0);
        lv_obj_set_style_border_color(swatch, NK_INK_3, 0);
    }
    nk_text(item, text, NK_FS_CAPTION, false, NK_INK_2);
}

/* 日付ナビゲーター（< で前日、> で翌日。未来には進めない） */
static void on_prev_day_clicked(lv_event_t *e) {
    (void)e;
    if (nk_ui_store_move_prev()) {
        nk_ui_open(nk_ui_current());
    }
}

static void on_next_day_clicked(lv_event_t *e) {
    (void)e;
    if (nk_ui_store_move_next()) {
        nk_ui_open(nk_ui_current());
    }
}

static void interactive_day_pager(lv_obj_t *parent, const char *text) {
    lv_obj_t *bar = nk_card(parent);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(bar, 10, 0);

    bool can_prev = nk_ui_store_can_prev();
    bool can_next = nk_ui_store_can_next();

    lv_obj_t *prev = nk_text(bar, "<", NK_FS_TITLE, true, can_prev ? NK_INK : NK_CARD_2);
    if (can_prev) {
        lv_obj_set_clickable(prev, true);
        lv_obj_set_ext_click_area(prev, 14);
        lv_obj_add_event_cb(prev, on_prev_day_clicked, LV_EVENT_CLICKED, NULL);
    }

    nk_text(bar, text, NK_FS_BODY, true, NK_INK);

    lv_obj_t *next = nk_text(bar, ">", NK_FS_TITLE, true, can_next ? NK_INK : NK_CARD_2);
    if (can_next) {
        lv_obj_set_clickable(next, true);
        lv_obj_set_ext_click_area(next, 14);
        lv_obj_add_event_cb(next, on_next_day_clicked, LV_EVENT_CLICKED, NULL);
    }
}

/* ---------------------------------------------------------------- 日 */

void nk_build_record_day(lv_obj_t *page) {
    ensure_record_store();
    const nk_sleep_record *rec = nk_ui_store_selected();

    /* 1・2 日／週の切り替えと日付 */
    nk_record_toggle(page, false);

    if (rec == NULL) {
        lv_obj_t *empty = nk_card(page);
        nk_text(empty, "記録がまだありません", NK_FS_TITLE, true, NK_INK);
        nk_wrap_text(empty, "最初の記録は、今夜おやすみを始めると作られます。", NK_FS_BODY, false,
                     NK_INK_2);
        return;
    }

    interactive_day_pager(page, rec->date_short);

    /* 3 睡眠スコア */
    lv_obj_t *score = nk_card(page);
    lv_obj_set_style_border_width(score, 2, 0);
    lv_obj_set_style_border_color(score, NK_SERIES, 0);
    nk_text(score, "睡眠スコア", NK_FS_BODY, false, NK_INK_2);

    lv_obj_t *row = nk_box(score, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    char score_buf[24];
    lv_snprintf(score_buf, sizeof(score_buf), "%d点", (int)rec->score);
    lv_color_t score_col = (rec->quality == NK_QUALITY_INSUFFICIENT) ? NK_INK_2 : NK_INK;
    nk_text(row, score_buf, NK_FS_BIG, true, score_col);
    nk_text(row, rec->quality == NK_QUALITY_INSUFFICIENT ? "参考値" : rec->score_comment,
            NK_FS_BODY, false, rec->quality == NK_QUALITY_INSUFFICIENT ? NK_WARN : NK_GOOD);

    /* 4 就寝・起床・睡眠時間 */
    char sleep_buf[32];
    nk_ui_format_duration(rec->sleep_min, sleep_buf, sizeof(sleep_buf));
    lv_obj_t *times = nk_card(page);
    lv_obj_set_flex_flow(times, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(times, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_stat(times, "就寝", rec->bedtime, true);
    nk_stat(times, "起床", rec->wake_time, true);
    nk_stat(times, "睡眠時間", sleep_buf, true);

    /* 5 睡眠段階の概要 */
    lv_obj_t *stages = nk_card(page);
    nk_card_title(stages, "睡眠段階の概要", "1時間ごとに区切る");
    nk_hypnogram_band(stages, rec);
    lv_obj_t *legend = nk_box(stages, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(legend, 12, 0);
    legend_item(legend, NK_LIGHT, "浅い睡眠", false);
    legend_item(legend, NK_DEEP, "深い睡眠", false);
    legend_item(legend, NK_WAKE, "覚醒", false);
    legend_item(legend, NK_INK_3, "取得なし", true);
    nk_text(stages, "取得なしの時間は、点線の枠で示します", NK_FS_CAPTION, false, NK_INK_3);

    /* 6 AI フィードバック */
    nk_advice_card(page, "AIフィードバック", rec->short_advice, false);

    /* 7 くわしく見る */
    nk_on_click_open(nk_button(page, "くわしく見る", false), NK_UI_RECORD_DETAIL);
}

/* ---------------------------------------------------------------- 週 */

static int32_t day_center(int day) {
    return (2 * day + 1) * NK_CARD_INNER_W / (2 * DAYS);
}

static void day_labels(lv_obj_t *area, int32_t y) {
    static const char *const k_days[DAYS] = {"月", "火", "水", "木", "金", "土", "日"};
    for (int i = 0; i < DAYS; i++) {
        axis_label(area, k_days[i], day_center(i), y, 0);
    }
}

static void on_prev_week_clicked(lv_event_t *e) {
    (void)e;
    nk_ui_store_move_prev_week();
    nk_ui_open(NK_UI_RECORD_WEEK);
}

static void on_next_week_clicked(lv_event_t *e) {
    (void)e;
    nk_ui_store_move_next_week();
    nk_ui_open(NK_UI_RECORD_WEEK);
}

static void interactive_week_pager(lv_obj_t *parent, const char *text) {
    lv_obj_t *bar = nk_card(parent);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(bar, 10, 0);

    bool can_next = nk_ui_store_can_next_week();
    lv_obj_t *prev = nk_text(bar, "<", NK_FS_TITLE, true, NK_INK);
    lv_obj_set_clickable(prev, true);
    lv_obj_set_ext_click_area(prev, 14);
    lv_obj_add_event_cb(prev, on_prev_week_clicked, LV_EVENT_CLICKED, NULL);

    nk_text(bar, text, NK_FS_BODY, true, NK_INK);

    lv_obj_t *next = nk_text(bar, ">", NK_FS_TITLE, true, can_next ? NK_INK : NK_CARD_2);
    if (can_next) {
        lv_obj_set_clickable(next, true);
        lv_obj_set_ext_click_area(next, 14);
        lv_obj_add_event_cb(next, on_next_week_clicked, LV_EVENT_CLICKED, NULL);
    }
}

/* 棒グラフの棒・折れ線の点を押すと、その日の「日」表示へ遷移 */
static void on_week_day_tapped(lv_event_t *e) {
    int store_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (nk_ui_store_select_index(store_idx)) {
        nk_ui_open(NK_UI_RECORD_DAY);
    }
}

/* 月(0)〜日(6) の各曜日に紐づくレコード配列を構築 */
static void map_week_records(const nk_sleep_record *out_week[DAYS], int out_indices[DAYS]) {
    for (int i = 0; i < DAYS; i++) {
        out_week[i] = NULL;
        out_indices[i] = -1;
    }
    if (nk_ui_store_week_offset() > 0) {
        return; /* 過去週はデータなし（記録なし表示の確認用） */
    }
    /* ストアは [0]=土, [1]=金, [2]=木, [3]=水, [4]=火, [5]=月 の順で保持 */
    int cnt = nk_ui_store_count();
    for (int idx = 0; idx < cnt && idx < 6; idx++) {
        int wd = 5 - idx; /* 0:月 .. 5:土 */
        out_week[wd] = nk_ui_store_get(idx);
        out_indices[wd] = idx;
    }
}

static void sleep_bars(lv_obj_t *card, const nk_sleep_record *const week[DAYS],
                       const int indices[DAYS]) {
    const int32_t h = 110; /* 縦軸は 0〜10 時間（600分）で固定 */
    lv_obj_t *area = plot_area(card, h + 22);

    for (int i = 0; i < DAYS; i++) {
        const nk_sleep_record *rec = week[i];
        if (rec == NULL || rec->quality == NK_QUALITY_FAILED) {
            /* 記録なし日の点線枠 */
            lv_obj_t *empty = nk_rect(area, 26, 68, NK_CARD, 6);
            lv_obj_set_pos(empty, day_center(i) - 13, h - 68);
            lv_obj_set_style_border_width(empty, 1, 0);
            lv_obj_set_style_border_color(empty, NK_INK_3, 0);
            lv_obj_center(nk_text(empty, "記録\nなし", NK_FS_CAPTION, false, NK_INK_3));
            continue;
        }
        int32_t bar_h = rec->sleep_min * h / 600;
        if (bar_h > h) {
            bar_h = h;
        }
        lv_obj_t *bar = nk_rect(area, 26, bar_h, NK_SERIES, 4);
        lv_obj_set_pos(bar, day_center(i) - 13, h - bar_h);
        if (rec->quality == NK_QUALITY_INSUFFICIENT) {
            lv_obj_set_style_bg_opa(bar, LV_OPA_40, 0);
        }
        lv_obj_set_clickable(bar, true);
        lv_obj_add_event_cb(bar, on_week_day_tapped, LV_EVENT_CLICKED,
                            (void *)(intptr_t)indices[i]);
    }

    int32_t goal_y = h - nk_ui_store_goal_sleep_min() * h / 600;
    s_goal_line[0].x = 0;
    s_goal_line[0].y = goal_y;
    s_goal_line[1].x = NK_CARD_INNER_W;
    s_goal_line[1].y = goal_y;
    lv_obj_t *goal = lv_line_create(area);
    lv_line_set_points(goal, s_goal_line, 2);
    lv_obj_set_style_line_width(goal, 2, 0);
    lv_obj_set_style_line_color(goal, NK_INK_2, 0);
    lv_obj_set_style_line_dash_width(goal, 5, 0);
    lv_obj_set_style_line_dash_gap(goal, 4, 0);
    axis_label(area, "目標 7時間", NK_CARD_INNER_W, goal_y - 18, 1);
    day_labels(area, h + 6);
}

static void score_line(lv_obj_t *card, const nk_sleep_record *const week[DAYS],
                       const int indices[DAYS]) {
    const int32_t h = 90; /* 縦軸は 50〜100 点 */
    lv_obj_t *area = plot_area(card, h + 22);
    int count = 0;
    int dot_indices[DAYS];
    bool dot_ref[DAYS];

    for (int i = 0; i < DAYS; i++) {
        const nk_sleep_record *rec = week[i];
        if (rec == NULL || rec->quality == NK_QUALITY_FAILED || rec->score < 0) {
            continue;
        }
        int32_t clamped = rec->score < 50 ? 50 : (rec->score > 100 ? 100 : rec->score);
        s_score_line[count].x = day_center(i);
        s_score_line[count].y = h - (clamped - 50) * h / 50;
        dot_indices[count] = indices[i];
        dot_ref[count] = (rec->quality == NK_QUALITY_INSUFFICIENT);
        count++;
    }

    if (count >= 2) {
        lv_obj_t *line = lv_line_create(area);
        lv_line_set_points(line, s_score_line, (uint32_t)count);
        lv_obj_set_style_line_width(line, 3, 0);
        lv_obj_set_style_line_color(line, NK_SERIES, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
    }

    for (int i = 0; i < count; i++) {
        lv_obj_t *dot = nk_rect(area, 14, 14, NK_SERIES, LV_RADIUS_CIRCLE);
        lv_obj_set_pos(dot, (int32_t)s_score_line[i].x - 7, (int32_t)s_score_line[i].y - 7);
        if (dot_ref[i]) {
            lv_obj_set_style_bg_color(dot, NK_CARD, 0);
            lv_obj_set_style_border_width(dot, 2, 0);
            lv_obj_set_style_border_color(dot, NK_SERIES, 0);
        }
        lv_obj_set_clickable(dot, true);
        lv_obj_add_event_cb(dot, on_week_day_tapped, LV_EVENT_CLICKED,
                            (void *)(intptr_t)dot_indices[i]);
    }
    day_labels(area, h + 6);
}

void nk_build_record_week(lv_obj_t *page) {
    ensure_record_store();
    const nk_sleep_record *week[DAYS];
    int indices[DAYS];
    map_week_records(week, indices);

    /* 1・2 日／週の切り替えと期間 */
    nk_record_toggle(page, true);
    interactive_week_pager(page, nk_ui_store_week_offset() == 0 ? "10/26〜11/1" : "10/19〜10/25");

    /* 3 継続記録 */
    lv_obj_t *streak = nk_card(page);
    lv_obj_set_flex_flow(streak, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(streak, 12, 0);
    nk_text(streak, "継続記録", NK_FS_BODY, false, NK_INK_2);
    char streak_buf[24];
    lv_snprintf(streak_buf, sizeof(streak_buf), "%d日連続", nk_ui_store_streak_days());
    nk_text(streak, streak_buf, NK_FS_TITLE, true, NK_INK);

    /* 4 睡眠時間の推移（棒を押すとその日の表示へ） */
    lv_obj_t *hours = nk_card(page);
    nk_card_title(hours, "睡眠時間の推移", "棒を押すと日の表示へ");
    sleep_bars(hours, week, indices);
    nk_text(hours, "薄い棒は参考値（分析が不確かな夜）", NK_FS_CAPTION, false, NK_INK_3);

    /* 5 睡眠スコアの推移 */
    lv_obj_t *score = nk_card(page);
    nk_card_title(score, "睡眠スコアの推移", "点を押すと日の表示へ");
    score_line(score, week, indices);

    /* 6 睡眠リズム（UX-4 1章 #5） */
    lv_obj_t *rhythm = nk_card(page);
    nk_card_title(rhythm, "寝た時刻・起きた時刻のリズム", "21:00〜翌9:00");
    nk_rhythm(rhythm, week, DAYS);

    /* 7 週の平均（OK の夜だけで計算。3夜未満は出さない） */
    int ok_nights = 0;
    int32_t sum_sleep = 0;
    int32_t sum_score = 0;
    for (int i = 0; i < DAYS; i++) {
        if (week[i] != NULL && week[i]->quality == NK_QUALITY_OK) {
            ok_nights++;
            sum_sleep += week[i]->sleep_min;
            sum_score += week[i]->score;
        }
    }

    lv_obj_t *avg = nk_card(page);
    if (ok_nights < 3) {
        nk_text(avg, "週の平均", NK_FS_BODY, false, NK_INK_2);
        char need_buf[64];
        lv_snprintf(need_buf, sizeof(need_buf), "あと %d 夜で平均が表示されます", 3 - ok_nights);
        nk_text(avg, need_buf, NK_FS_BODY, true, NK_INK);
    } else {
        char avg_sleep_buf[32];
        nk_ui_format_duration(sum_sleep / ok_nights, avg_sleep_buf, sizeof(avg_sleep_buf));
        lv_obj_t *row = nk_box(avg, LV_FLEX_FLOW_ROW);
        nk_text(row, "週の平均", NK_FS_BODY, false, NK_INK_2);
        char s1[48];
        lv_snprintf(s1, sizeof(s1), "睡眠時間 %s", avg_sleep_buf);
        nk_text(row, s1, NK_FS_BODY, true, NK_INK);
        char s2[32];
        lv_snprintf(s2, sizeof(s2), "スコア %d点", (int)(sum_score / ok_nights));
        nk_text(row, s2, NK_FS_BODY, true, NK_INK);

        char note_buf[48];
        lv_snprintf(note_buf, sizeof(note_buf), "分析できた%d夜の平均", ok_nights);
        nk_text(avg, note_buf, NK_FS_CAPTION, false, NK_INK_3);
    }

    /* 8 週のひとこと */
    nk_advice_card(page, "週のひとこと",
                   ok_nights >= 3 ? "火曜は短めでしたが、ほかの夜は 6 時間以上眠れています。"
                                  : "3 夜分の記録がそろうと、週の傾向が表示されます。",
                   false);

    /* 9 日ごとの一覧（グラフ外でも数値を読めるようにする：UX-4 3章 / UX-8 7章） */
    static const char *const k_wd_names[DAYS] = {"月", "火", "水", "木", "金", "土", "日"};
    lv_obj_t *list_card = nk_card(page);
    nk_card_title(list_card, "日ごとの一覧", "押すと日の表示へ");
    for (int i = 0; i < DAYS; i++) {
        const nk_sleep_record *drec = week[i];
        lv_obj_t *lrow = nk_box(list_card, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_ver(lrow, 8, 0);
        if (i > 0) {
            lv_obj_set_style_border_side(lrow, LV_BORDER_SIDE_TOP, 0);
            lv_obj_set_style_border_width(lrow, 1, 0);
            lv_obj_set_style_border_color(lrow, NK_CARD_2, 0);
        }
        if (drec == NULL) {
            char empty_lbl[24];
            lv_snprintf(empty_lbl, sizeof(empty_lbl), "（%s）", k_wd_names[i]);
            nk_text(lrow, empty_lbl, NK_FS_SMALL, false, NK_INK_3);
            nk_text(lrow, "この日の記録はありません", NK_FS_SMALL, false, NK_INK_3);
        } else {
            char dur_s[24];
            char sc_s[24];
            nk_ui_format_duration(drec->sleep_min, dur_s, sizeof(dur_s));
            lv_snprintf(sc_s, sizeof(sc_s), "%d点", (int)drec->score);
            nk_text(lrow, drec->date_short, NK_FS_SMALL, false, NK_INK);
            nk_text(lrow, dur_s, NK_FS_SMALL, false, NK_INK_2);
            nk_text(lrow, sc_s, NK_FS_SMALL, true,
                    drec->quality == NK_QUALITY_INSUFFICIENT ? NK_WARN : NK_INK);
            lv_obj_set_clickable(lrow, true);
            lv_obj_add_event_cb(lrow, on_week_day_tapped, LV_EVENT_CLICKED,
                                (void *)(intptr_t)indices[i]);
        }
    }
}

/* ---------------------------------------------------------------- 日別の詳細 */

static void breakdown(lv_obj_t *card, const nk_sleep_record *rec) {
    static const char *const k_names[] = {"睡眠時間", "寝つき", "連続性", "効率", "規則性"};
    static const int32_t k_max[] = {30, 15, 25, 15, 15};

    /* 満点に対する割合が最も低い項目を 1 つ特定 */
    int lowest = 0;
    float lowest_ratio = 2.0f;
    for (int i = 0; i < 5; i++) {
        float ratio = (float)rec->sub_scores[i] / (float)k_max[i];
        if (ratio < lowest_ratio) {
            lowest_ratio = ratio;
            lowest = i;
        }
    }

    nk_text(card, "内訳（項目ごとの点数）", NK_FS_BODY, true, NK_INK);
    for (int i = 0; i < 5; i++) {
        lv_color_t color = (i == lowest) ? NK_SERIOUS : NK_SERIES;
        lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(nk_text(row, k_names[i], NK_FS_SMALL, false, NK_INK_2), 64);

        lv_obj_t *bar = lv_bar_create(row);
        lv_obj_set_height(bar, 10);
        lv_obj_set_flex_grow(bar, 1);
        lv_bar_set_range(bar, 0, k_max[i]);
        lv_bar_set_value(bar, rec->sub_scores[i], LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, NK_CARD_2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);

        char text[16];
        lv_snprintf(text, sizeof(text), "%d／%d", (int)rec->sub_scores[i], (int)k_max[i]);
        lv_obj_t *value = nk_text(row, text, NK_FS_SMALL, false, NK_INK_2);
        lv_obj_set_width(value, 48);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    }

    char tip_buf[64];
    lv_snprintf(tip_buf, sizeof(tip_buf), "最も低い「%s」：ここを整えると良さそう",
                k_names[lowest]);
    nk_text(card, tip_buf, NK_FS_SMALL, true, NK_SERIOUS);
}

static void on_record_mood_clicked(lv_event_t *e) {
    (void)e;
    nk_open_mood_picker();
}

static void on_delete_night_clicked(lv_event_t *e) {
    (void)e;
    nk_open_delete_confirm();
}

void nk_build_record_detail(lv_obj_t *page) {
    ensure_record_store();
    const nk_sleep_record *rec = nk_ui_store_selected();

    /* 1 見出しと日付 */
    nk_header(page, "睡眠記録", "日別の詳細");
    if (rec == NULL) {
        lv_obj_t *empty = nk_card(page);
        nk_text(empty, "表示できる記録がありません", NK_FS_BODY, true, NK_INK);
        return;
    }
    interactive_day_pager(page, rec->night_span);

    /* 2 分析の確かさ（先に示す） */
    lv_obj_t *sure = nk_box(page, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sure, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (rec->quality == NK_QUALITY_OK) {
        nk_pill(sure, "分析の確かさ：分析可能", NK_GOOD, true);
    } else if (rec->quality == NK_QUALITY_INSUFFICIENT) {
        nk_pill(sure, "分析の確かさ：不足（参考値）", NK_WARN, true);
    } else {
        nk_pill(sure, "分析の確かさ：取得失敗", NK_SERIOUS, true);
    }

    /* 3 スコアと内訳 */
    lv_obj_t *score = nk_card(page);
    lv_obj_set_style_border_width(score, 2, 0);
    lv_obj_set_style_border_color(score, NK_SERIES, 0);
    lv_obj_t *top = nk_box(score, LV_FLEX_FLOW_ROW);
    nk_text(top, "睡眠スコア", NK_FS_BODY, false, NK_INK_2);
    nk_pill(top, rec->score_label, rec->quality == NK_QUALITY_OK ? NK_GOOD : NK_WARN, false);

    lv_obj_t *body = nk_box(score, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(body, 20, 0);
    nk_ring(body, 84, rec->score >= 0 ? rec->score : 0);

    lv_obj_t *facts = nk_box(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(facts, 1);
    char sleep_str[32];
    nk_ui_format_duration(rec->sleep_min, sleep_str, sizeof(sleep_str));
    char line1[48];
    lv_snprintf(line1, sizeof(line1), "睡眠時間  %s", sleep_str);
    nk_text(facts, line1, NK_FS_BODY, true, NK_INK);

    char line2[48];
    if (rec->score_diff == 999) {
        lv_snprintf(line2, sizeof(line2), "前日比  比較なし");
    } else {
        lv_snprintf(line2, sizeof(line2), "前日比  %+d点", (int)rec->score_diff);
    }
    nk_text(facts, line2, NK_FS_BODY, true, NK_INK);
    breakdown(score, rec);

    /* 4 睡眠段階の推移（イベント印＋色凡例＋欠損点線枠つき） */
    lv_obj_t *stages = nk_card(page);
    nk_card_title(stages, "睡眠段階の推移", "推定できる範囲のみ");
    nk_hypnogram_detail(stages, rec);
    lv_obj_t *ev_legend = nk_box(stages, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(ev_legend, 10, 0);
    lv_obj_set_style_pad_row(ev_legend, 4, 0);
    legend_item(ev_legend, NK_SERIES, "寝返り", false);
    legend_item(ev_legend, NK_SERIOUS, "いびき", false);
    legend_item(ev_legend, NK_WARN, "大きな音", false);
    legend_item(ev_legend, NK_LIGHT, "サウンド", false);
    legend_item(ev_legend, NK_GOOD, "カーテン", false);
    legend_item(ev_legend, NK_BRAND, "アラーム", false);
    nk_text(stages, "横軸は時刻（1時間ごと）。取得なしは点線", NK_FS_CAPTION, false, NK_INK_3);

    /* 5 指標の表 */
    char tib_str[32], sol_str[24], waso_str[32], eff_str[16], deep_str[32], light_str[32],
        turn_str[24], breath_str[32];
    nk_ui_format_duration(rec->tib_min, tib_str, sizeof(tib_str));
    lv_snprintf(sol_str, sizeof(sol_str), "%d分", (int)rec->sol_min);
    lv_snprintf(waso_str, sizeof(waso_str), "%d回・合計%d分", (int)rec->waso_count,
                (int)rec->waso_min);
    lv_snprintf(eff_str, sizeof(eff_str), "%d%%", (int)rec->efficiency_pct);
    nk_ui_format_duration(rec->deep_min, deep_str, sizeof(deep_str));
    nk_ui_format_duration(rec->light_min, light_str, sizeof(light_str));
    lv_snprintf(turn_str, sizeof(turn_str), "%d回", (int)rec->turns);
    if (rec->breath_bpm > 0) {
        lv_snprintf(breath_str, sizeof(breath_str), "平均 %d回/分", (int)rec->breath_bpm);
    } else {
        lv_snprintf(breath_str, sizeof(breath_str), "記録なし（確からしさ不足）");
    }

    const char *metrics[10][2] = {
        {"寝床にいた時間", tib_str},     {"睡眠時間", sleep_str}, {"寝つき", sol_str},
        {"中途覚醒", waso_str},          {"効率", eff_str},       {"深い眠り（推定）", deep_str},
        {"浅い眠り（推定）", light_str}, {"寝返り", turn_str},    {"いびき", rec->snore_text},
        {"呼吸数", breath_str},
    };
    lv_obj_t *table = nk_card(page);
    nk_card_title(table, "指標の表", NULL);
    lv_obj_t *grid = nk_box(table, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);
    for (int i = 0; i < 10; i++) {
        lv_obj_set_width(nk_stat(grid, metrics[i][0], metrics[i][1], false), LV_PCT(50));
    }
    nk_text(table, "推定の項目には「推定」の札。取れない項目は理由つき", NK_FS_CAPTION, false,
            NK_INK_3);

    /* 6 取得状況 */
    lv_obj_t *capture = nk_card(page);
    nk_card_title(capture, "取得状況", NULL);
    char meas_buf[64], meas_a[32], meas_b[32];
    nk_ui_format_duration(rec->measured_min, meas_a, sizeof(meas_a));
    nk_ui_format_duration(rec->tib_min, meas_b, sizeof(meas_b));
    lv_snprintf(meas_buf, sizeof(meas_buf), "%s／%s", meas_a, meas_b);

    lv_obj_t *measured = nk_box(capture, LV_FLEX_FLOW_ROW);
    nk_text(measured, "計測できた時間", NK_FS_SMALL, false, NK_INK_2);
    nk_text(measured, meas_buf, NK_FS_SMALL, true, NK_INK);
    nk_rect(capture, LV_PCT(100), 8, NK_SERIES, 4);

    lv_obj_t *missing = nk_box(capture, LV_FLEX_FLOW_ROW);
    nk_text(missing, "取得できなかった区間", NK_FS_SMALL, false, NK_INK_2);
    nk_text(missing, rec->gap_reason, NK_FS_SMALL, true, NK_INK);

    lv_obj_t *sensor = nk_box(capture, LV_FLEX_FLOW_ROW);
    nk_text(sensor, "置き方の確認  済み", NK_FS_SMALL, false, NK_INK_2);
    nk_text(sensor, "使ったセンサー  スマホ", NK_FS_SMALL, false, NK_INK_2);

    /* 7 その日のアドバイス */
    lv_obj_t *tips = nk_card(page);
    nk_card_title(tips, "その日のアドバイス", NULL);
    nk_text(tips, "根拠", NK_FS_CAPTION, false, NK_INK_3);
    nk_wrap_text(tips, rec->advice_basis, NK_FS_BODY, false, NK_INK);
    nk_text(tips, "提案（睡眠習慣）", NK_FS_CAPTION, false, NK_INK_3);
    nk_wrap_text(tips, rec->advice_habit, NK_FS_BODY, false, NK_INK);
    nk_text(tips, "提案（カーテン設定）", NK_FS_CAPTION, false, NK_INK_3);
    nk_wrap_text(tips, rec->advice_curtain, NK_FS_BODY, false, NK_INK);

    /* 8 操作（目覚めの気分は未入力時のみ表示、削除は確認つき） */
    if (rec->mood == 0) {
        lv_obj_t *mood_btn = nk_button(page, "目覚めの気分を記録", false);
        lv_obj_add_event_cb(mood_btn, on_record_mood_clicked, LV_EVENT_CLICKED, NULL);
    } else {
        static const char *const k_mood_names[] = {"", "すっきり", "ふつう", "ねむい"};
        char mood_done[48];
        lv_snprintf(mood_done, sizeof(mood_done), "今朝の目覚め：%s（記録済み）",
                    k_mood_names[rec->mood]);
        nk_text(page, mood_done, NK_FS_SMALL, false, NK_INK_2);
    }

    lv_obj_t *erase = nk_button(page, "この夜の記録を削除", false);
    lv_obj_set_style_border_color(erase, NK_CARD_2, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(erase, 0), NK_INK_3, 0);
    lv_obj_add_event_cb(erase, on_delete_night_clicked, LV_EVENT_CLICKED, NULL);
}
