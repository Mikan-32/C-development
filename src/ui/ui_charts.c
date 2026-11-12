/*
 * グラフと図：スコアのリング、計測できた時間の帯、睡眠段階の概要と推移、週の棒グラフと折れ線、
 * カーテンの図、再生中の印、長押しのボタン
 */
#include "ui_internal.h"

#include <stdint.h>
#include <stdio.h>

/* ---------------------------------------------------------------- 描画の小道具 */

static void fill_rect(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                      lv_color_t color, lv_opa_t opa, int32_t radius) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = opa;
    d.radius = radius;
    lv_area_t a;
    a.x1 = x1;
    a.y1 = y1;
    a.x2 = x2;
    a.y2 = y2;
    lv_draw_rect(layer, &d, &a);
}

static void stroke_line(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                        lv_color_t color, lv_opa_t opa, int32_t width, int32_t dash) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.opa = opa;
    d.width = width;
    d.dash_width = dash;
    d.dash_gap = dash > 0 ? dash : 0;
    d.p1.x = (lv_value_precise_t)x1;
    d.p1.y = (lv_value_precise_t)y1;
    d.p2.x = (lv_value_precise_t)x2;
    d.p2.y = (lv_value_precise_t)y2;
    lv_draw_line(layer, &d);
}

static void draw_text(lv_layer_t *layer, const char *text, const lv_font_t *font, lv_color_t color,
                      int32_t x1, int32_t y1, int32_t x2, lv_text_align_t align) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = text;
    d.text_local = 1; /* 文字はこの関数の外で消えるので、LVGL に写させる */
    d.font = font;
    d.color = color;
    d.align = align;
    lv_area_t a;
    a.x1 = x1;
    a.y1 = y1;
    a.x2 = x2;
    a.y2 = y1 + lv_font_get_line_height(font);
    lv_draw_label(layer, &d, &a);
}

/* 斜線（計測できなかった時間）。四角 (x1, y1)-(x2, y2) の中に 45 度の線を引く */
static void draw_hatch(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       int32_t pitch, int32_t width) {
    int32_t h = y2 - y1;
    if (h <= 0 || x2 <= x1) {
        return;
    }
    for (int32_t xa = x1 - h; xa < x2; xa += pitch) {
        /* 線 (xa, y2) → (xa + h, y1) を、x1〜x2 の範囲に切る */
        float t0 = (float)(x1 - xa) / (float)h;
        float t1 = (float)(x2 - xa) / (float)h;
        if (t0 < 0.0f) {
            t0 = 0.0f;
        }
        if (t1 > 1.0f) {
            t1 = 1.0f;
        }
        if (t0 >= t1) {
            continue;
        }
        int32_t sx = xa + (int32_t)(t0 * (float)h);
        int32_t sy = y2 - (int32_t)(t0 * (float)h);
        int32_t ex = xa + (int32_t)(t1 * (float)h);
        int32_t ey = y2 - (int32_t)(t1 * (float)h);
        stroke_line(layer, sx, sy, ex, ey, NK_SERIOUS, 191, width, 0);
    }
}

static lv_obj_t *drawing(lv_obj_t *parent, int32_t width, int32_t height, lv_event_cb_t cb,
                         void *user_data) {
    lv_obj_t *obj = nk_ui_box(parent);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_user_data(obj, user_data);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

/* ---------------------------------------------------------------- スコアのリング */

typedef struct ring_data {
    int score;
    int shown;
    bool reference;
} ring_data;

static void ring_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    const ring_data *rd = lv_obj_get_user_data(obj);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int32_t size = lv_area_get_width(&a);
    int32_t w = size * 9 / 128;
    lv_color_t color = nk_theme_score_color(rd->score, rd->reference);

    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center.x = a.x1 + size / 2;
    d.center.y = a.y1 + size / 2;
    d.radius = (uint16_t)(size / 2 - 2);
    d.width = w;
    d.color = color;

    if (rd->reference) {
        /* 参考値：点線の輪（UX-3 の 2 章） */
        d.opa = 115;
        for (int ang = 0; ang < 360; ang += 11) {
            d.start_angle = (lv_value_precise_t)ang;
            d.end_angle = (lv_value_precise_t)(ang + 4);
            lv_draw_arc(layer, &d);
        }
        d.opa = LV_OPA_COVER;
        int end = 270 + rd->shown * 360 / 100;
        for (int ang = 270; ang + 4 <= end; ang += 13) {
            d.start_angle = (lv_value_precise_t)(ang % 360);
            d.end_angle = (lv_value_precise_t)((ang + 7) % 360);
            lv_draw_arc(layer, &d);
        }
        return;
    }

    d.opa = 41;
    d.start_angle = 0;
    d.end_angle = 360;
    lv_draw_arc(layer, &d);
    if (rd->shown > 0) {
        d.opa = LV_OPA_COVER;
        d.rounded = 1;
        d.start_angle = 270;
        d.end_angle = (lv_value_precise_t)((270 + rd->shown * 360 / 100) % 360);
        if (rd->shown >= 100) {
            d.end_angle = 269;
        }
        lv_draw_arc(layer, &d);
    }
}

static void ring_delete_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    lv_free(lv_obj_get_user_data(obj));
    lv_obj_set_user_data(obj, NULL);
}

static void ring_anim_cb(void *var, int32_t value) {
    lv_obj_t *obj = var;
    ring_data *rd = lv_obj_get_user_data(obj);
    if (rd != NULL) {
        rd->shown = (int)value;
        lv_obj_invalidate(obj);
    }
}

lv_obj_t *nk_ui_ring(lv_obj_t *parent, int32_t size, int score, bool reference, bool animate) {
    ring_data *rd = lv_malloc(sizeof(ring_data));
    LV_ASSERT_MALLOC(rd);
    rd->score = score;
    rd->reference = reference;
    rd->shown = animate ? 0 : score;
    lv_obj_t *obj = drawing(parent, size, size, ring_draw_cb, rd);
    lv_obj_add_event_cb(obj, ring_delete_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *col = nk_ui_box(obj);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(col);
    char num[8];
    (void)snprintf(num, sizeof(num), "%d", score);
    nk_ui_label(col, num, NK_FB(size >= 110 ? 40 : 28), reference ? NK_INK_2 : NK_INK);
    nk_ui_label(col, reference ? "参考値" : "睡眠スコア", NK_FB(11), NK_INK_3);

    if (animate) {
        /* スコアのリングだけ 0.6 秒で伸ばす（UX-3 の 3 章） */
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, obj);
        lv_anim_set_values(&anim, 0, score);
        lv_anim_set_duration(&anim, 600);
        lv_anim_set_delay(&anim, 150);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, ring_anim_cb);
        lv_anim_start(&anim);
    }
    return obj;
}

/* ---------------------------------------------------------------- 夜の時刻の軸 */

static int segments_total(const nk_demo_night *night) {
    int total = 0;
    for (size_t i = 0; i < night->segment_count; i++) {
        total += night->segments[i].minutes;
    }
    return total > 0 ? total : 1;
}

static void free_user_data_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    lv_free(lv_obj_get_user_data(obj));
    lv_obj_set_user_data(obj, NULL);
}

/* 計測の始まりと終わりを、ちょうどの時刻（1 時間ごとの目盛り）まで広げた横軸 */
typedef struct night_axis {
    int start;  /* 計測を始めた時刻（0 時からの分。日をまたぐ前提で、朝の時刻は 1440 を足す） */
    int total;  /* 計測の長さ（分） */
    int from;   /* 軸の左端（ちょうどの時刻） */
    int to;     /* 軸の右端 */
    int32_t x1; /* 描く範囲 */
    int32_t x2;
} night_axis;

static night_axis make_axis(const nk_demo_night *n, int32_t x1, int32_t x2) {
    night_axis ax;
    ax.start = n->start < 12 * 60 ? n->start + 1440 : n->start;
    ax.total = n->in_bed_min > 0 ? n->in_bed_min : 1;
    ax.from = (ax.start / 60) * 60;
    ax.to = ((ax.start + ax.total + 59) / 60) * 60;
    if (ax.to <= ax.from) {
        ax.to = ax.from + 60;
    }
    ax.x1 = x1;
    ax.x2 = x2;
    return ax;
}

/* 計測の始まりから offset 分の位置（区間の分は、計測の長さに比で合わせる） */
static int32_t axis_x(const night_axis *ax, int offset_min, int segments_sum) {
    int64_t m = (int64_t)offset_min * ax->total / (segments_sum > 0 ? segments_sum : 1);
    int64_t span = ax->to - ax->from;
    return ax->x1 + (int32_t)(((int64_t)ax->start - ax->from + m) * (ax->x2 - ax->x1) / span);
}

static int32_t axis_hour_x(const night_axis *ax, int minutes) {
    int64_t span = ax->to - ax->from;
    return ax->x1 + (int32_t)((int64_t)(minutes - ax->from) * (ax->x2 - ax->x1) / span);
}

/* 1 時間ごとの目盛りの文字（「23時」）。幅が足りないときは 1 つおき */
static void draw_hour_labels(lv_layer_t *layer, const night_axis *ax, int32_t y) {
    const lv_font_t *font = NK_FR(11);
    int hours = (ax->to - ax->from) / 60;
    int step = (ax->x2 - ax->x1) / (hours > 0 ? hours : 1) < 34 ? 2 : 1;
    int i = 0;
    for (int m = ax->from; m <= ax->to; m += 60, i++) {
        if (i % step != 0) {
            continue;
        }
        char buf[16];
        (void)snprintf(buf, sizeof(buf), "%d時", (m / 60) % 24);
        int32_t x = axis_hour_x(ax, m);
        lv_text_align_t align = LV_TEXT_ALIGN_CENTER;
        int32_t l = x - 22;
        int32_t r = x + 22;
        if (m == ax->from) {
            align = LV_TEXT_ALIGN_LEFT;
            l = x;
            r = x + 44;
        } else if (m >= ax->to) {
            align = LV_TEXT_ALIGN_RIGHT;
            l = x - 44;
            r = x;
        }
        draw_text(layer, buf, font, NK_INK_3, l, y, r, align);
    }
}

/* 点線の四角（取得なし・記録なし） */
static void dashed_box(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       lv_color_t color) {
    stroke_line(layer, x1, y1, x2, y1, color, LV_OPA_COVER, 1, 3);
    stroke_line(layer, x1, y2, x2, y2, color, LV_OPA_COVER, 1, 3);
    stroke_line(layer, x1, y1, x1, y2, color, LV_OPA_COVER, 1, 3);
    stroke_line(layer, x2, y1, x2, y2, color, LV_OPA_COVER, 1, 3);
}

static lv_color_t stage_color(nk_stage st) {
    switch (st) {
    case NK_STAGE_WAKE:
        return NK_COLOR_WAKE;
    case NK_STAGE_DEEP:
        return NK_COLOR_DEEP;
    case NK_STAGE_SLEEP:
        return NK_COLOR_SLEEP;
    case NK_STAGE_LIGHT:
    case NK_STAGE_GAP:
    default:
        return NK_COLOR_LIGHT;
    }
}

/* ---------------------------------------------------------------- 計測できた時間の帯 */

static void quality_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    const nk_demo_night *n = lv_obj_get_user_data(obj);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int32_t w = lv_area_get_width(&a);
    fill_rect(layer, a.x1, a.y1, a.x2, a.y2, NK_CARD_2, LV_OPA_COVER, 3);
    if (n->segment_count == 0) {
        int32_t x2 = a.x1 + (int32_t)((int64_t)n->measured_min * w /
                                      (n->in_bed_min > 0 ? n->in_bed_min : 1));
        fill_rect(layer, a.x1, a.y1, x2, a.y2, NK_SERIES, LV_OPA_COVER, 3);
        return;
    }
    int total = segments_total(n);
    int t = 0;
    int run_start = 0;
    bool in_gap = n->segments[0].stage == NK_STAGE_GAP;
    for (size_t i = 0; i <= n->segment_count; i++) {
        bool end = i == n->segment_count;
        bool gap = !end && n->segments[i].stage == NK_STAGE_GAP;
        if (end || gap != in_gap) {
            int32_t x1 = a.x1 + (int32_t)((int64_t)run_start * w / total);
            int32_t x2 = a.x1 + (int32_t)((int64_t)t * w / total) - 1;
            if (in_gap) {
                draw_hatch(layer, x1, a.y1, x2, a.y2, 6, 3);
            } else {
                fill_rect(layer, x1, a.y1, x2, a.y2, NK_SERIES, LV_OPA_COVER, 3);
            }
            run_start = t;
            in_gap = gap;
        }
        if (!end) {
            t += n->segments[i].minutes;
        }
    }
}

lv_obj_t *nk_ui_quality_bar(lv_obj_t *parent, const nk_demo_night *night) {
    return drawing(parent, LV_PCT(100), 6, quality_draw_cb, (void *)night);
}

/* ---------------------------------------------------------------- 睡眠段階の概要（日の表示） */

typedef struct stage_data {
    const nk_demo_night *night;
    bool two_tone;
} stage_data;

#define BAND_H 30

static void band_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    const stage_data *sd = lv_obj_get_user_data(obj);
    const nk_demo_night *n = sd->night;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    night_axis ax = make_axis(n, a.x1, a.x2);
    int32_t y1 = a.y1;
    int32_t y2 = a.y1 + BAND_H - 1;

    fill_rect(layer, a.x1, y1, a.x2, y2, NK_CARD_2, LV_OPA_COVER, 4);
    int sum = segments_total(n);
    int t = 0;
    for (size_t i = 0; i < n->segment_count; i++) {
        const nk_demo_segment *seg = &n->segments[i];
        int32_t x1 = axis_x(&ax, t, sum);
        int32_t x2 = axis_x(&ax, t + seg->minutes, sum) - 1;
        t += seg->minutes;
        if (seg->stage == NK_STAGE_GAP) {
            /* 取得できなかった時間は、点線の枠（UX-4 改訂案） */
            fill_rect(layer, x1, y1, x2, y2, NK_CARD, LV_OPA_COVER, 0);
            dashed_box(layer, x1, y1, x2, y2, NK_INK_3);
            continue;
        }
        nk_stage st = seg->stage;
        if (sd->two_tone && (st == NK_STAGE_LIGHT || st == NK_STAGE_DEEP)) {
            st = NK_STAGE_SLEEP;
        }
        fill_rect(layer, x1, y1, x2 > x1 ? x2 - 1 : x2, y2, stage_color(st), LV_OPA_COVER, 0);
    }

    /* 1 時間ごとの目盛り */
    for (int m = ax.from; m <= ax.to; m += 60) {
        int32_t x = axis_hour_x(&ax, m);
        stroke_line(layer, x, y2 + 2, x, y2 + 6, NK_INK_3, LV_OPA_COVER, 1, 0);
    }
    draw_hour_labels(layer, &ax, y2 + 8);
}

lv_obj_t *nk_ui_stage_band(lv_obj_t *parent, const nk_demo_night *night, bool two_tone) {
    stage_data *sd = lv_malloc(sizeof(stage_data));
    LV_ASSERT_MALLOC(sd);
    sd->night = night;
    sd->two_tone = two_tone;
    int32_t height = BAND_H + 10 + lv_font_get_line_height(NK_FR(11));
    lv_obj_t *obj = drawing(parent, LV_PCT(100), height, band_draw_cb, sd);
    lv_obj_add_event_cb(obj, free_user_data_cb, LV_EVENT_DELETE, NULL);
    return obj;
}

/* ---------------------------------------------------------------- 睡眠段階の推移（LG-02） */

/* 出来事の印（分。合計 452 分の夜に合わせた位置。ほかの夜は比で置く） */
typedef struct mark {
    int minute;
    int kind; /* 0 寝返り 1 いびき 2 大きな音 3 サウンド 4 カーテン 5 アラーム */
} mark;

static const mark k_marks[] = {
    {0, 3},   {52, 0},  {95, 0},  {130, 0}, {140, 1}, {150, 1}, {171, 0},
    {200, 0}, {236, 2}, {236, 0}, {262, 0}, {300, 1}, {312, 1}, {331, 0},
    {360, 0}, {389, 0}, {410, 0}, {420, 4}, {448, 5},
};
#define MARK_BASE 452

lv_color_t nk_ui_mark_color(int kind) {
    switch (kind) {
    case 1:
        return NK_WARN;
    case 2:
        return NK_SERIOUS;
    case 3:
        return NK_BRAND;
    case 4:
        return NK_SERIES;
    case 5:
        return lv_color_hex(0xB48CF0);
    case 0:
    default:
        return NK_INK_3;
    }
}

#define STEPS_LEFT  34
#define STEPS_MARKS 16
#define STEPS_ROW   30

static void steps_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    const nk_demo_night *n = lv_obj_get_user_data(obj);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    night_axis ax = make_axis(n, a.x1 + STEPS_LEFT, a.x2 - 4);
    int32_t top = a.y1 + STEPS_MARKS + 4;
    int32_t level_y[3];
    for (int i = 0; i < 3; i++) {
        level_y[i] = top + STEPS_ROW / 2 + i * STEPS_ROW;
    }
    int32_t bottom = top + 3 * STEPS_ROW;
    const lv_font_t *font = NK_FR(11);
    int32_t fh = lv_font_get_line_height(font);
    static const char *const names[] = {"覚醒", "浅い", "深い"};
    for (int i = 0; i < 3; i++) {
        draw_text(layer, names[i], font, NK_INK_3, a.x1, level_y[i] - fh / 2, a.x1 + STEPS_LEFT - 6,
                  LV_TEXT_ALIGN_LEFT);
    }
    for (int m = ax.from; m <= ax.to; m += 60) {
        int32_t x = axis_hour_x(&ax, m);
        stroke_line(layer, x, top, x, bottom, NK_WHITE, NK_OPA_LINE, 1, 0);
    }
    stroke_line(layer, ax.x1, bottom, ax.x2, bottom, NK_WHITE, NK_OPA_LINE_2, 1, 0);

    /* 段階の上下を線で結ぶ。取得なしの区間は線を切って点線の枠 */
    int sum = segments_total(n);
    int t = 0;
    int32_t prev_x = 0;
    int32_t prev_y = 0;
    bool has_prev = false;
    for (size_t i = 0; i < n->segment_count; i++) {
        const nk_demo_segment *seg = &n->segments[i];
        int32_t x1 = axis_x(&ax, t, sum);
        int32_t x2 = axis_x(&ax, t + seg->minutes, sum);
        t += seg->minutes;
        if (seg->stage == NK_STAGE_GAP) {
            dashed_box(layer, x1, top + 2, x2, bottom - 2, NK_INK_3);
            draw_text(layer, "取得なし", font, NK_INK_3, x1, (top + bottom) / 2 - fh / 2, x2,
                      LV_TEXT_ALIGN_CENTER);
            has_prev = false;
            continue;
        }
        int lane = seg->stage == NK_STAGE_WAKE ? 0 : (seg->stage == NK_STAGE_DEEP ? 2 : 1);
        int32_t y = level_y[lane];
        if (has_prev && prev_y != y) {
            stroke_line(layer, prev_x, prev_y, prev_x, y, NK_SERIES, LV_OPA_COVER, 2, 0);
        }
        stroke_line(layer, x1, y, x2, y, NK_SERIES, LV_OPA_COVER, 2, 0);
        prev_x = x2;
        prev_y = y;
        has_prev = true;
    }

    /* 帯の上の印 */
    for (size_t i = 0; i < sizeof(k_marks) / sizeof(k_marks[0]); i++) {
        int32_t x = axis_x(&ax, k_marks[i].minute * sum / MARK_BASE, sum);
        int32_t r = k_marks[i].kind == 0 ? 2 : 4;
        int32_t cy = a.y1 + STEPS_MARKS / 2;
        fill_rect(layer, x - r, cy - r, x + r, cy + r, nk_ui_mark_color(k_marks[i].kind),
                  LV_OPA_COVER, LV_RADIUS_CIRCLE);
    }
    draw_hour_labels(layer, &ax, bottom + 4);
}

lv_obj_t *nk_ui_stage_steps(lv_obj_t *parent, const nk_demo_night *night) {
    int32_t height = STEPS_MARKS + 4 + 3 * STEPS_ROW + 6 + lv_font_get_line_height(NK_FR(11));
    return drawing(parent, LV_PCT(100), height, steps_draw_cb, (void *)night);
}

/* ---------------------------------------------------------------- 週のグラフ */

#define WEEK_MAX_MIN 600 /* 縦軸は 0〜10 時間で固定（UX-4 の 3 章） */
#define WEEK_LEFT    28
#define WEEK_RIGHT   8
#define WEEK_TOP     18
#define SCORE_MIN    40

typedef struct week_data {
    int monday;  /* 10 月の日 */
    int clicked; /* 押した日（10 月の日） */
    bool scores;
} week_data;

/* 週の i 日目（0 = 月曜）の記録。10 月の日で、31 を超えたら 11 月（まだ来ていない日） */
static const nk_demo_night *week_night(int monday, int i, int *day_out) {
    int day = monday + i;
    *day_out = day;
    return day >= 1 && day <= 31 ? nk_demo_night_find(10, day) : NULL;
}

static void week_day_labels(lv_layer_t *layer, const week_data *wd, int32_t left, int32_t band,
                            int32_t y) {
    static const char *const wnames[] = {"月", "火", "水", "木", "金", "土", "日"};
    int sel = nk_demo_state_get()->history_day;
    int32_t row = lv_font_get_line_height(NK_FR(12));
    for (int i = 0; i < 7; i++) {
        int day = wd->monday + i;
        int32_t cx = left + band * i + band / 2;
        bool on = day == sel;
        draw_text(layer, wnames[i], on ? NK_FB(12) : NK_FR(12), on ? NK_INK : NK_INK_3, cx - 20, y,
                  cx + 20, LV_TEXT_ALIGN_CENTER);
        char d[16];
        if (day <= 31) {
            (void)snprintf(d, sizeof(d), "10/%d", day);
        } else {
            (void)snprintf(d, sizeof(d), "11/%d", day - 31);
        }
        draw_text(layer, d, NK_FR(10), NK_INK_3, cx - 22, y + row, cx + 22, LV_TEXT_ALIGN_CENTER);
    }
}

static void no_record_box(lv_layer_t *layer, int32_t cx, int32_t y1, int32_t y2, int32_t w) {
    dashed_box(layer, cx - w / 2, y1, cx + w / 2, y2, NK_INK_3);
    const lv_font_t *font = NK_FR(10);
    int32_t fh = lv_font_get_line_height(font);
    int32_t mid = (y1 + y2) / 2;
    draw_text(layer, "記録", font, NK_INK_3, cx - 20, mid - fh, cx + 20, LV_TEXT_ALIGN_CENTER);
    draw_text(layer, "なし", font, NK_INK_3, cx - 20, mid, cx + 20, LV_TEXT_ALIGN_CENTER);
}

static int32_t week_labels_height(void) {
    return lv_font_get_line_height(NK_FR(12)) + lv_font_get_line_height(NK_FR(10));
}

static void week_bars_draw(lv_layer_t *layer, const week_data *wd, const lv_area_t *a) {
    int32_t left = a->x1 + WEEK_LEFT;
    int32_t right = a->x2 - WEEK_RIGHT;
    int32_t base = a->y2 - week_labels_height() - 6;
    int32_t plot_h = base - (a->y1 + WEEK_TOP);
    const lv_font_t *small = NK_FR(11);
    int32_t sh = lv_font_get_line_height(small);
    for (int m = 0; m <= 540; m += 180) {
        int32_t y = base - m * plot_h / WEEK_MAX_MIN;
        stroke_line(layer, left, y, right, y, NK_WHITE, NK_OPA_LINE, 1, 0);
        char buf[8];
        (void)snprintf(buf, sizeof(buf), "%dh", m / 60);
        draw_text(layer, buf, small, NK_INK_3, a->x1, y - sh / 2, left - 6, LV_TEXT_ALIGN_RIGHT);
    }
    int goal = nk_demo_state_get()->goal_min;
    int32_t gy = base - goal * plot_h / WEEK_MAX_MIN;
    stroke_line(layer, left, gy, right, gy, NK_INK_2, LV_OPA_COVER, 1, 4);
    draw_text(layer, "目標", NK_FB(11), NK_INK_2, right - 40, gy - sh - 1, right,
              LV_TEXT_ALIGN_RIGHT);

    int32_t band = (right - left) / 7;
    const int32_t bw = 22;
    int sel = nk_demo_state_get()->history_day;
    for (int i = 0; i < 7; i++) {
        int day;
        const nk_demo_night *n = week_night(wd->monday, i, &day);
        int32_t cx = left + band * i + band / 2;
        if (day > 31) {
            continue; /* まだ来ていない日 */
        }
        if (n == NULL) {
            no_record_box(layer, cx, base - plot_h / 2, base, bw + 6);
            continue;
        }
        if (n->quality == NK_Q_FAILED) {
            /* 取得できなかった夜は、棒を出さずに × */
            draw_text(layer, "×", NK_FB(14), NK_INK_3, cx - 20, base - sh - 6, cx + 20,
                      LV_TEXT_ALIGN_CENTER);
            continue;
        }
        int32_t top = base - n->sleep_min * plot_h / WEEK_MAX_MIN;
        bool ref = n->quality == NK_Q_INSUFFICIENT;
        lv_opa_t opa = ref ? 70 : (day == sel ? LV_OPA_COVER : 140);
        fill_rect(layer, cx - bw / 2, top, cx + bw / 2, base, NK_SERIES, opa, 4);
        if (ref) {
            draw_text(layer, "参考値", NK_FR(10), NK_INK_3, cx - 24, top - sh, cx + 24,
                      LV_TEXT_ALIGN_CENTER);
        } else if (day == sel) {
            char v[16];
            (void)snprintf(v, sizeof(v), "%dh%02dm", n->sleep_min / 60, n->sleep_min % 60);
            draw_text(layer, v, NK_FB(11), NK_INK, cx - 30, top - sh - 2, cx + 30,
                      LV_TEXT_ALIGN_CENTER);
        }
    }
    week_day_labels(layer, wd, left, band, base + 4);
}

static void week_scores_draw(lv_layer_t *layer, const week_data *wd, const lv_area_t *a) {
    int32_t left = a->x1 + WEEK_LEFT;
    int32_t right = a->x2 - WEEK_RIGHT;
    int32_t base = a->y2 - week_labels_height() - 6;
    int32_t top = a->y1 + WEEK_TOP;
    int32_t plot_h = base - top;
    const lv_font_t *small = NK_FR(11);
    int32_t sh = lv_font_get_line_height(small);
    for (int v = 50; v <= 100; v += 25) {
        int32_t y = base - (v - SCORE_MIN) * plot_h / (100 - SCORE_MIN);
        stroke_line(layer, left, y, right, y, NK_WHITE, NK_OPA_LINE, 1, 0);
        char buf[8];
        (void)snprintf(buf, sizeof(buf), "%d", v);
        draw_text(layer, buf, small, NK_INK_3, a->x1, y - sh / 2, left - 6, LV_TEXT_ALIGN_RIGHT);
    }
    int32_t band = (right - left) / 7;
    int sel = nk_demo_state_get()->history_day;
    int32_t px = 0;
    int32_t py = 0;
    bool has_prev = false;
    for (int i = 0; i < 7; i++) {
        int day;
        const nk_demo_night *n = week_night(wd->monday, i, &day);
        int32_t cx = left + band * i + band / 2;
        if (day > 31) {
            has_prev = false;
            continue;
        }
        if (n == NULL || n->quality == NK_Q_FAILED) {
            /* 記録がない日は空ける（UX-4 改訂案） */
            no_record_box(layer, cx, top + plot_h / 4, base - plot_h / 6, 30);
            has_prev = false;
            continue;
        }
        int32_t y = base - (n->score - SCORE_MIN) * plot_h / (100 - SCORE_MIN);
        bool ref = n->quality == NK_Q_INSUFFICIENT;
        if (has_prev) {
            stroke_line(layer, px, py, cx, y, NK_SERIES, ref ? 120 : LV_OPA_COVER, 2, ref ? 4 : 0);
        }
        int32_t r = day == sel ? 6 : 5;
        if (ref) {
            /* 参考値は中の抜けた点 */
            fill_rect(layer, cx - r, y - r, cx + r, y + r, NK_INK_3, LV_OPA_COVER,
                      LV_RADIUS_CIRCLE);
            fill_rect(layer, cx - r + 2, y - r + 2, cx + r - 2, y + r - 2, NK_CARD, LV_OPA_COVER,
                      LV_RADIUS_CIRCLE);
        } else {
            fill_rect(layer, cx - r, y - r, cx + r, y + r, NK_SERIES, LV_OPA_COVER,
                      LV_RADIUS_CIRCLE);
        }
        char v[8];
        (void)snprintf(v, sizeof(v), "%d", n->score);
        draw_text(layer, v, day == sel ? NK_FB(11) : small, ref ? NK_INK_3 : NK_INK_2, cx - 20,
                  y - r - sh - 1, cx + 20, LV_TEXT_ALIGN_CENTER);
        px = cx;
        py = y;
        has_prev = true;
    }
    week_day_labels(layer, wd, left, band, base + 4);
}

static void week_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    const week_data *wd = lv_obj_get_user_data(obj);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    if (wd->scores) {
        week_scores_draw(lv_event_get_layer(e), wd, &a);
    } else {
        week_bars_draw(lv_event_get_layer(e), wd, &a);
    }
}

static void week_click_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    week_data *wd = lv_obj_get_user_data(obj);
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int32_t left = a.x1 + WEEK_LEFT;
    int32_t right = a.x2 - WEEK_RIGHT;
    if (p.x < left || p.x >= right) {
        return;
    }
    int i = (int)((p.x - left) * 7 / (right - left));
    int day;
    (void)week_night(wd->monday, i, &day);
    if (day >= 1 && day <= 31) {
        wd->clicked = day;
        lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

static lv_obj_t *week_chart(lv_obj_t *parent, int monday, bool scores) {
    week_data *wd = lv_malloc(sizeof(week_data));
    LV_ASSERT_MALLOC(wd);
    wd->monday = monday;
    wd->clicked = 0;
    wd->scores = scores;
    lv_obj_t *obj = drawing(parent, LV_PCT(100), scores ? 150 : 176, week_draw_cb, wd);
    lv_obj_set_clickable(obj, true);
    lv_obj_add_event_cb(obj, week_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(obj, free_user_data_cb, LV_EVENT_DELETE, NULL);
    return obj;
}

lv_obj_t *nk_ui_week_bars(lv_obj_t *parent, int monday) {
    return week_chart(parent, monday, false);
}

lv_obj_t *nk_ui_week_scores(lv_obj_t *parent, int monday) {
    return week_chart(parent, monday, true);
}

int nk_ui_week_chart_day(lv_obj_t *chart) {
    const week_data *wd = lv_obj_get_user_data(chart);
    return wd != NULL ? wd->clicked : 0;
}

/* ---------------------------------------------------------------- カーテンの図 */

static void curtain_panel_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    for (int32_t x = a.x1; x <= a.x2; x += 10) {
        bool odd = ((x - a.x1) / 10) % 2 == 1;
        int32_t x2 = x + 9 < a.x2 ? x + 9 : a.x2;
        fill_rect(layer, x, a.y1, x2, a.y2, odd ? lv_color_hex(0xB86A81) : lv_color_hex(0xC7798F),
                  LV_OPA_COVER, 0);
    }
}

static int32_t curtain_panel_width(lv_obj_t *view, int position) {
    lv_obj_update_layout(view);
    int32_t w = lv_obj_get_width(view);
    if (w <= 0) {
        w = NK_CONTENT_W - 2 * NK_PAD;
    }
    /* 全開（100%）で両側 9%、全閉（0%）で両側 50% */
    int32_t pct10 = 500 - (500 - 90) * position / 100;
    return w * pct10 / 1000;
}

lv_obj_t *nk_ui_curtain_view(lv_obj_t *parent, int32_t height, int position) {
    lv_obj_t *view = nk_ui_box(parent);
    lv_obj_set_size(view, LV_PCT(100), height);
    lv_obj_set_style_radius(view, NK_RADIUS_S, 0);
    lv_obj_set_style_clip_corner(view, true, 0);
    lv_obj_set_style_bg_color(view, lv_color_hex(0x2A3772), 0);
    lv_obj_set_style_bg_opa(view, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view, 1, 0);
    lv_obj_set_style_border_color(view, NK_WHITE, 0);
    lv_obj_set_style_border_opa(view, NK_OPA_LINE_2, 0);
    lv_obj_set_style_border_post(view, true, 0);

    lv_obj_t *mid = nk_ui_box(view);
    lv_obj_set_size(mid, 2, LV_PCT(100));
    lv_obj_set_style_bg_color(mid, NK_BG, 0);
    lv_obj_set_style_bg_opa(mid, LV_OPA_50, 0);
    lv_obj_align(mid, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *rail = nk_ui_box(view);
    lv_obj_set_size(rail, LV_PCT(100), 8);
    lv_obj_set_style_bg_color(rail, lv_color_hex(0x4B568F), 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
    lv_obj_align(rail, LV_ALIGN_TOP_MID, 0, 0);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *panel = nk_ui_box(view);
        lv_obj_set_height(panel, height - 8);
        lv_obj_align(panel, i == 0 ? LV_ALIGN_BOTTOM_LEFT : LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_add_event_cb(panel, curtain_panel_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    }
    nk_ui_curtain_view_set(view, position);
    return view;
}

void nk_ui_curtain_view_set(lv_obj_t *view, int position) {
    if (position < 0) {
        position = 0;
    }
    if (position > 100) {
        position = 100;
    }
    int32_t pw = curtain_panel_width(view, position);
    /* 子の並び：0 中央の線、1 レール、2 左、3 右 */
    lv_obj_set_width(lv_obj_get_child(view, 2), pw);
    lv_obj_set_width(lv_obj_get_child(view, 3), pw);
}

/* ---------------------------------------------------------------- 再生中の印 */

static void eq_anim_cb(void *var, int32_t value) {
    lv_obj_set_height(var, value);
}

lv_obj_t *nk_ui_eq(lv_obj_t *parent, lv_color_t color, bool animate) {
    static const int32_t heights[] = {6, 12, 8};
    lv_obj_t *eq = nk_ui_box(parent);
    lv_obj_set_height(eq, 12);
    lv_obj_set_flex_flow(eq, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(eq, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(eq, 2, 0);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *bar = nk_ui_box(eq);
        lv_obj_set_size(bar, 3, heights[i]);
        lv_obj_set_style_radius(bar, 2, 0);
        lv_obj_set_style_bg_color(bar, color, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        if (animate) {
            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, bar);
            lv_anim_set_values(&anim, 3, 12);
            lv_anim_set_duration(&anim, (uint32_t)(380 + i * 110));
            lv_anim_set_reverse_duration(&anim, (uint32_t)(380 + i * 110));
            lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_delay(&anim, (uint32_t)(i * 120));
            lv_anim_set_exec_cb(&anim, eq_anim_cb);
            lv_anim_start(&anim);
        }
    }
    return eq;
}

/* ---------------------------------------------------------------- 長押しのボタン */

static void hold_ring_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    int progress = (int)(intptr_t)lv_obj_get_user_data(obj);
    lv_color_t color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center.x = a.x1 + lv_area_get_width(&a) / 2;
    d.center.y = a.y1 + lv_area_get_height(&a) / 2;
    d.radius = (uint16_t)(lv_area_get_width(&a) / 2);
    d.width = 3;
    d.color = color;
    d.opa = 90;
    d.start_angle = 0;
    d.end_angle = 360;
    lv_draw_arc(layer, &d);
    /* 押していないときは上の 1/4 だけ（モックの輪）、押している間は進み具合 */
    int sweep = progress > 0 ? progress * 360 / 100 : 90;
    d.opa = LV_OPA_COVER;
    d.rounded = 1;
    d.start_angle = 270;
    d.end_angle = (lv_value_precise_t)((270 + sweep) % 360);
    if (sweep >= 360) {
        d.end_angle = 269;
    }
    lv_draw_arc(layer, &d);
}

static void hold_anim_cb(void *var, int32_t value) {
    lv_obj_set_user_data(var, (void *)(intptr_t)value);
    lv_obj_invalidate(var);
}

static void hold_anim_done_cb(lv_anim_t *anim) {
    lv_obj_t *ring = lv_anim_get_user_data(anim);
    lv_obj_t *btn = lv_obj_get_parent(ring);
    if ((int)(intptr_t)lv_obj_get_user_data(ring) >= 100) {
        lv_obj_send_event(btn, LV_EVENT_READY, NULL);
    }
}

static void hold_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_current_target_obj(e);
    lv_obj_t *ring = lv_obj_get_child(btn, 0);
    if (code == LV_EVENT_PRESSED) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, ring);
        lv_anim_set_values(&anim, 1, 100);
        lv_anim_set_duration(&anim, 1000);
        lv_anim_set_exec_cb(&anim, hold_anim_cb);
        lv_anim_set_user_data(&anim, ring);
        lv_anim_set_completed_cb(&anim, hold_anim_done_cb);
        lv_anim_start(&anim);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if ((int)(intptr_t)lv_obj_get_user_data(ring) < 100) {
            (void)lv_anim_delete(ring, hold_anim_cb);
            hold_anim_cb(ring, 0);
        }
    }
}

lv_obj_t *nk_ui_hold_button(lv_obj_t *parent, const char *text, lv_color_t color,
                            lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, LV_PCT(100), 58);
    lv_obj_set_style_radius(btn, 29, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, color, 0);
    lv_obj_set_style_border_opa(btn, 115, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, 20, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn, 10, 0);
    lv_obj_t *ring = nk_ui_box(btn);
    lv_obj_set_size(ring, 26, 26);
    lv_obj_set_style_text_color(ring, color, 0);
    lv_obj_set_user_data(ring, (void *)(intptr_t)0);
    lv_obj_add_event_cb(ring, hold_ring_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    nk_ui_label(btn, text, NK_FB(15), color);
    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_PRESS_LOST, NULL);
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_READY, NULL);
    }
    return btn;
}
