/*
 * LG-01 睡眠記録（日・週）と LG-02 日別の詳細（UX-4 改訂案の図）
 */
#include "ui_internal.h"

#define DAYS 7

/* 線の点は、線が消えるまで残しておく必要がある。画面を開くたびに書き直す */
static lv_point_precise_t s_goal_line[2];
static lv_point_precise_t s_score_line[DAYS];
static lv_point_precise_t s_stage_line[2 * 16];

/* 軸の目盛り。edge が -1 なら左端、1 なら右端、0 なら x を中心にそろえる */
static void axis_label(lv_obj_t *parent, const char *text, int32_t x, int32_t y, int edge) {
    lv_obj_t *label = nk_text(parent, text, NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_translate_x(label, LV_PCT(edge < 0 ? 0 : edge > 0 ? -100 : -50), 0);
}

/* 23時〜7時の目盛り（1 時間ごと）。左に余白がなければ、端の目盛りは内側に寄せる */
static void hour_labels(lv_obj_t *parent, int32_t x, int32_t y, int32_t w) {
    static const char *const k_hours[] = {"23時", "0時", "1時", "2時", "3時",
                                          "4時",  "5時", "6時", "7時"};
    for (int i = 0; i <= 8; i++) {
        int edge = (i == 0 && x == 0) ? -1 : i == 8 ? 1 : 0;
        axis_label(parent, k_hours[i], x + i * w / 8, y, edge);
    }
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

static lv_obj_t *ai_card(lv_obj_t *page, const char *caption, const char *text) {
    lv_obj_t *card = nk_card(page);
    nk_text(card, caption, NK_FS_SMALL, false, NK_INK_3);
    nk_wrap_text(card, text, NK_FS_BODY + 1, true, NK_INK);
    return card;
}

/* ---------------------------------------------------------------- 日 */

static void stage_band(lv_obj_t *card) {
    const int32_t w = NK_CARD_INNER_W;
    lv_obj_t *area = plot_area(card, 48);
    lv_obj_t *band = nk_rect(area, w, 28, NK_CARD_2, 4);
    int32_t start = NK_DEMO_IN_BED_FROM;
    for (int i = 0; i < nk_demo_segment_count; i++) {
        int32_t from = start * w / NK_AXIS_MINUTES;
        int32_t to = (start + nk_demo_segments[i].minutes) * w / NK_AXIS_MINUTES;
        lv_obj_t *seg = nk_rect(band, to - from, 28, nk_stage_color(nk_demo_segments[i].stage), 0);
        lv_obj_set_pos(seg, from, 0);
        start += nk_demo_segments[i].minutes;
    }
    hour_labels(area, 0, 32, w);
}

void nk_build_record_day(lv_obj_t *page) {
    /* 1・2 日／週の切り替えと日付 */
    nk_record_toggle(page, false);
    nk_pager(page, "10月31日（土）");

    /* 3 睡眠スコア */
    lv_obj_t *score = nk_card(page);
    lv_obj_set_style_border_width(score, 2, 0);
    lv_obj_set_style_border_color(score, NK_SERIES, 0);
    nk_text(score, "睡眠スコア", NK_FS_BODY, false, NK_INK_2);
    lv_obj_t *row = nk_box(score, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    nk_text(row, "82点", NK_FS_BIG, true, NK_INK);
    nk_text(row, "よく眠れました", NK_FS_BODY, false, NK_GOOD);

    /* 4 就寝・起床・睡眠時間 */
    lv_obj_t *times = nk_card(page);
    lv_obj_set_flex_flow(times, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(times, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_stat(times, "就寝", NK_DEMO_BEDTIME, true);
    nk_stat(times, "起床", NK_DEMO_WAKE, true);
    nk_stat(times, "睡眠時間", NK_DEMO_SLEEP, true);

    /* 5 睡眠段階の概要 */
    lv_obj_t *stages = nk_card(page);
    nk_card_title(stages, "睡眠段階の概要", "1時間ごとに区切る");
    stage_band(stages);
    lv_obj_t *legend = nk_box(stages, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(legend, 12, 0);
    legend_item(legend, NK_LIGHT, "浅い睡眠", false);
    legend_item(legend, NK_DEEP, "深い睡眠", false);
    legend_item(legend, NK_WAKE, "覚醒", false);
    legend_item(legend, NK_INK_3, "取得なし", true);
    nk_text(stages, "取得なしの時間は、点線の枠で示します", NK_FS_CAPTION, false, NK_INK_3);

    /* 6 AI フィードバック */
    ai_card(page, "AIフィードバック", "寝つきに時間がかかりましたが、そのあとはよく眠れています。");

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

/* 10/26（月）〜11/1（日）。火曜は参考値、日曜はまだ来ていない（-1） */
static const int32_t k_week_sleep[DAYS] = {371, 260, 405, 386, 429, 398, -1};
static const int32_t k_week_score[DAYS] = {66, 61, 80, 77, 79, 82, -1};
#define REFERENCE_DAY 1

static void sleep_bars(lv_obj_t *card) {
    const int32_t h = 110; /* 縦軸は 0〜10 時間で固定 */
    lv_obj_t *area = plot_area(card, h + 22);
    for (int i = 0; i < DAYS; i++) {
        if (k_week_sleep[i] < 0) {
            continue;
        }
        int32_t bar_h = k_week_sleep[i] * h / 600;
        lv_obj_t *bar = nk_rect(area, 26, bar_h, NK_SERIES, 4);
        lv_obj_set_pos(bar, day_center(i) - 13, h - bar_h);
        if (i == REFERENCE_DAY) {
            lv_obj_set_style_bg_opa(bar, LV_OPA_40, 0);
        }
    }
    /* 目標（7 時間）の点線 */
    int32_t goal_y = h - 420 * h / 600;
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

static void score_line(lv_obj_t *card) {
    const int32_t h = 90; /* 縦軸は 50〜100 点 */
    lv_obj_t *area = plot_area(card, h + 22);
    int count = 0;
    for (int i = 0; i < DAYS; i++) {
        if (k_week_score[i] < 0) {
            continue;
        }
        s_score_line[count].x = day_center(i);
        s_score_line[count].y = h - (k_week_score[i] - 50) * h / 50;
        count++;
    }
    lv_obj_t *line = lv_line_create(area);
    lv_line_set_points(line, s_score_line, (uint32_t)count);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_color(line, NK_SERIES, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    for (int i = 0; i < count; i++) {
        lv_obj_t *dot = nk_rect(area, 12, 12, NK_SERIES, LV_RADIUS_CIRCLE);
        lv_obj_set_pos(dot, (int32_t)s_score_line[i].x - 6, (int32_t)s_score_line[i].y - 6);
        if (i == REFERENCE_DAY) {
            lv_obj_set_style_bg_color(dot, NK_CARD, 0);
            lv_obj_set_style_border_width(dot, 2, 0);
            lv_obj_set_style_border_color(dot, NK_SERIES, 0);
        }
    }
    day_labels(area, h + 6);
}

void nk_build_record_week(lv_obj_t *page) {
    /* 1・2 日／週の切り替えと期間 */
    nk_record_toggle(page, true);
    nk_pager(page, "10/26〜11/1");

    /* 3 継続記録 */
    lv_obj_t *streak = nk_card(page);
    lv_obj_set_flex_flow(streak, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(streak, 12, 0);
    nk_text(streak, "継続記録", NK_FS_BODY, false, NK_INK_2);
    nk_text(streak, "7日連続", NK_FS_TITLE, true, NK_INK);

    /* 4 睡眠時間の推移（押すと日の表示へ） */
    lv_obj_t *hours = nk_card(page);
    nk_card_title(hours, "睡眠時間の推移", "棒を押すと日の表示へ");
    sleep_bars(hours);
    nk_text(hours, "薄い棒は参考値（分析が不確かな夜）", NK_FS_CAPTION, false, NK_INK_3);
    nk_on_click_open(hours, NK_UI_RECORD_DAY);

    /* 5 睡眠スコアの推移 */
    lv_obj_t *score = nk_card(page);
    nk_card_title(score, "睡眠スコアの推移", "点を押すと日の表示へ");
    score_line(score);
    nk_on_click_open(score, NK_UI_RECORD_DAY);

    /* 6 週の平均（分析できた夜だけ） */
    lv_obj_t *avg = nk_card(page);
    lv_obj_t *row = nk_box(avg, LV_FLEX_FLOW_ROW);
    nk_text(row, "週の平均", NK_FS_BODY, false, NK_INK_2);
    nk_text(row, "睡眠時間 6時間38分", NK_FS_BODY, true, NK_INK);
    nk_text(row, "スコア 77点", NK_FS_BODY, true, NK_INK);
    nk_text(avg, "分析できた5夜の平均", NK_FS_CAPTION, false, NK_INK_3);

    /* 7 週のひとこと */
    ai_card(page, "週のひとこと", "火曜は短めでしたが、ほかの夜は 6 時間以上眠れています。");
}

/* ---------------------------------------------------------------- 日別の詳細 */

static void breakdown(lv_obj_t *card) {
    static const char *const k_names[] = {"睡眠時間", "寝つき", "連続性", "効率", "規則性"};
    static const int32_t k_points[] = {26, 12, 25, 15, 4};
    static const int32_t k_max[] = {30, 15, 25, 15, 15};
    const int lowest = 4; /* 満点に対して最も低い項目を 1 つだけ強調する */
    nk_text(card, "内訳（項目ごとの点数）", NK_FS_BODY, true, NK_INK);
    for (int i = 0; i < 5; i++) {
        lv_color_t color = i == lowest ? NK_SERIOUS : NK_SERIES;
        lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(nk_text(row, k_names[i], NK_FS_SMALL, false, NK_INK_2), 64);
        lv_obj_t *bar = lv_bar_create(row);
        lv_obj_set_height(bar, 10);
        lv_obj_set_flex_grow(bar, 1);
        lv_bar_set_range(bar, 0, k_max[i]);
        lv_bar_set_value(bar, k_points[i], LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, NK_CARD_2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
        char text[16];
        lv_snprintf(text, sizeof(text), "%d／%d", (int)k_points[i], (int)k_max[i]);
        lv_obj_t *value = nk_text(row, text, NK_FS_SMALL, false, NK_INK_2);
        lv_obj_set_width(value, 48);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    }
    nk_text(card, "最も低い「規則性」：ここを整えると良さそう", NK_FS_SMALL, true, NK_SERIOUS);
}

static void stage_steps(lv_obj_t *card) {
    static const char *const k_levels[] = {"覚醒", "浅い", "深い"};
    const int32_t left = 36; /* 段階の名前の幅 */
    const int32_t w = NK_CARD_INNER_W - left;
    const int32_t level_y[] = {10, 45, 80};
    lv_obj_t *area = plot_area(card, 112);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *label = nk_text(area, k_levels[i], NK_FS_CAPTION, false, NK_INK_3);
        lv_obj_set_pos(label, 0, level_y[i] - 8);
        lv_obj_set_pos(nk_rect(area, w, 1, NK_CARD_2, 0), left, level_y[i]);
    }
    int32_t start = NK_DEMO_IN_BED_FROM;
    int count = 0;
    for (int i = 0; i < nk_demo_segment_count && count + 2 <= 2 * 16; i++) {
        int32_t y = level_y[nk_demo_segments[i].stage];
        s_stage_line[count].x = left + start * w / NK_AXIS_MINUTES;
        s_stage_line[count].y = y;
        start += nk_demo_segments[i].minutes;
        s_stage_line[count + 1].x = left + start * w / NK_AXIS_MINUTES;
        s_stage_line[count + 1].y = y;
        count += 2;
    }
    lv_obj_t *line = lv_line_create(area);
    lv_line_set_points(line, s_stage_line, (uint32_t)count);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_color(line, NK_SERIES, 0);
    hour_labels(area, left, 94, w);
}

static void advice(lv_obj_t *card, const char *caption, const char *text) {
    nk_text(card, caption, NK_FS_CAPTION, false, NK_INK_3);
    nk_wrap_text(card, text, NK_FS_BODY, false, NK_INK);
}

void nk_build_record_detail(lv_obj_t *page) {
    /* 1 見出しと日付 */
    nk_header(page, "睡眠記録", "日別の詳細");
    nk_pager(page, "10月30日（金）の夜 → 31日（土）の朝");

    /* 2 分析の確かさ（先に示す） */
    lv_obj_t *sure = nk_box(page, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sure, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_pill(sure, "分析の確かさ：分析可能", NK_GOOD, true);

    /* 3 スコアと内訳 */
    lv_obj_t *score = nk_card(page);
    lv_obj_set_style_border_width(score, 2, 0);
    lv_obj_set_style_border_color(score, NK_SERIES, 0);
    lv_obj_t *top = nk_box(score, LV_FLEX_FLOW_ROW);
    nk_text(top, "睡眠スコア", NK_FS_BODY, false, NK_INK_2);
    nk_pill(top, NK_DEMO_LABEL, NK_GOOD, false);
    lv_obj_t *body = nk_box(score, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(body, 20, 0);
    nk_ring(body, 84, NK_DEMO_SCORE);
    lv_obj_t *facts = nk_box(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(facts, 1);
    nk_text(facts, "睡眠時間  " NK_DEMO_SLEEP, NK_FS_BODY, true, NK_INK);
    nk_text(facts, "前日比  " NK_DEMO_DIFF, NK_FS_BODY, true, NK_INK);
    breakdown(score);

    /* 4 睡眠段階の推移 */
    lv_obj_t *stages = nk_card(page);
    nk_card_title(stages, "睡眠段階の推移", "推定できる範囲のみ");
    stage_steps(stages);
    nk_text(stages, "横軸は時刻（1時間ごと）。取得なしは点線", NK_FS_CAPTION, false, NK_INK_3);

    /* 5 指標の表 */
    static const char *const k_metrics[][2] = {
        {"寝床にいた時間", "7時間32分"},
        {"睡眠時間", NK_DEMO_SLEEP},
        {"寝つき", "28分"},
        {"中途覚醒", "2回・合計22分"},
        {"効率", "88%"},
        {"深い眠り（推定）", "1時間12分"},
        {"浅い眠り（推定）", "5時間26分"},
        {"寝返り", "14回"},
        {"いびき", "なし（マイクオフ）"},
        {"呼吸数", "平均 14回/分"},
    };
    lv_obj_t *table = nk_card(page);
    nk_card_title(table, "指標の表", NULL);
    lv_obj_t *grid = nk_box(table, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);
    for (int i = 0; i < 10; i++) {
        lv_obj_set_width(nk_stat(grid, k_metrics[i][0], k_metrics[i][1], false), LV_PCT(50));
    }
    nk_text(table, "推定の項目には「推定」の札。取れない項目は理由つき", NK_FS_CAPTION, false,
            NK_INK_3);

    /* 6 取得状況 */
    lv_obj_t *capture = nk_card(page);
    nk_card_title(capture, "取得状況", NULL);
    lv_obj_t *measured = nk_box(capture, LV_FLEX_FLOW_ROW);
    nk_text(measured, "計測できた時間", NK_FS_SMALL, false, NK_INK_2);
    nk_text(measured, "7時間32分／7時間32分", NK_FS_SMALL, true, NK_INK);
    nk_rect(capture, LV_PCT(100), 8, NK_SERIES, 4);
    lv_obj_t *missing = nk_box(capture, LV_FLEX_FLOW_ROW);
    nk_text(missing, "取得できなかった区間", NK_FS_SMALL, false, NK_INK_2);
    nk_text(missing, "なし", NK_FS_SMALL, true, NK_INK);
    lv_obj_t *sensor = nk_box(capture, LV_FLEX_FLOW_ROW);
    nk_text(sensor, "置き方の確認  済み", NK_FS_SMALL, false, NK_INK_2);
    nk_text(sensor, "使ったセンサー  スマホ", NK_FS_SMALL, false, NK_INK_2);

    /* 7 その日のアドバイス */
    lv_obj_t *tips = nk_card(page);
    nk_card_title(tips, "その日のアドバイス", NULL);
    advice(tips, "根拠", "就寝がいつもより 40 分遅く、規則性の点が下がっています");
    advice(tips, "提案（睡眠習慣）", "今夜は 23:30 ごろに布団に入ってみませんか");
    advice(tips, "提案（カーテン設定）", "閉める時刻の予約（22:30）を、オンにしてみませんか");

    /* 8 操作 */
    nk_button(page, "目覚めの気分を記録", false);
    lv_obj_t *erase = nk_button(page, "この夜の記録を削除", false);
    lv_obj_set_style_border_color(erase, NK_CARD_2, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(erase, 0), NK_INK_3, 0);
}
