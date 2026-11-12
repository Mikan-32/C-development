/*
 * 画面の試作で表示するデモ用のデータ
 */
#include "ui_demo.h"

#include "ui_internal.h"

#include <stdio.h>

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

/* 分析できた夜の眠りの帯（画面モック v2 の HM-01 と同じ。合計 452 分） */
static const nk_demo_segment k_seg_ok[] = {
    {28, NK_STAGE_WAKE},  {20, NK_STAGE_LIGHT}, {40, NK_STAGE_DEEP},  {40, NK_STAGE_LIGHT},
    {6, NK_STAGE_WAKE},   {30, NK_STAGE_LIGHT}, {22, NK_STAGE_DEEP},  {60, NK_STAGE_LIGHT},
    {40, NK_STAGE_LIGHT}, {16, NK_STAGE_WAKE},  {70, NK_STAGE_LIGHT}, {10, NK_STAGE_DEEP},
    {66, NK_STAGE_LIGHT}, {4, NK_STAGE_WAKE},
};

/* データ不足の夜（1:40〜4:10 が計測できていない。合計 410 分） */
static const nk_demo_segment k_seg_gap[] = {
    {22, NK_STAGE_WAKE},   {88, NK_STAGE_SLEEP}, {150, NK_STAGE_GAP},
    {140, NK_STAGE_SLEEP}, {10, NK_STAGE_WAKE},
};

/* 一部の時間（3:10〜3:40）が取れていない夜（UX-7 の ST-PARTIAL-GAP） */
static const nk_demo_segment k_seg_partial[] = {
    {28, NK_STAGE_WAKE}, {20, NK_STAGE_LIGHT}, {40, NK_STAGE_DEEP}, {40, NK_STAGE_LIGHT},
    {6, NK_STAGE_WAKE},  {30, NK_STAGE_LIGHT}, {22, NK_STAGE_DEEP}, {44, NK_STAGE_LIGHT},
    {30, NK_STAGE_GAP},  {26, NK_STAGE_LIGHT}, {16, NK_STAGE_WAKE}, {70, NK_STAGE_LIGHT},
    {10, NK_STAGE_DEEP}, {66, NK_STAGE_LIGHT}, {4, NK_STAGE_WAKE},
};

#define OK_NIGHT(m, d, w, sc, sl, ib, st, lat, wn, wm, tu, dp, ef, sn, br)                         \
    {(m),                                                                                          \
     (d),                                                                                          \
     (w),                                                                                          \
     NK_Q_OK,                                                                                      \
     (sc),                                                                                         \
     (sl),                                                                                         \
     (ib),                                                                                         \
     (ib),                                                                                         \
     (st),                                                                                         \
     ((st) + (lat)) % 1440,                                                                        \
     ((st) + (lat) + (sl) + (wm)) % 1440,                                                          \
     (lat),                                                                                        \
     (wn),                                                                                         \
     (wm),                                                                                         \
     (tu),                                                                                         \
     (dp),                                                                                         \
     (ef),                                                                                         \
     (sn),                                                                                         \
     (br),                                                                                         \
     -1,                                                                                           \
     -1,                                                                                           \
     NULL,                                                                                         \
     k_seg_ok,                                                                                     \
     LEN(k_seg_ok)}

static const nk_demo_night k_nights[] = {
    OK_NIGHT(10, 18, 0, 76, 405, 444, 1395, 20, 2, 15, 16, 78, 91, 8, 14),
    OK_NIGHT(10, 19, 1, 71, 395, 444, 1413, 25, 3, 20, 19, 70, 89, 15, 15),
    OK_NIGHT(10, 20, 2, 79, 418, 447, 1385, 15, 1, 10, 14, 85, 94, 5, 14),
    OK_NIGHT(10, 22, 4, 74, 401, 445, 1403, 22, 2, 18, 17, 74, 90, 10, 14),
    OK_NIGHT(10, 23, 5, 68, 365, 424, 0, 30, 3, 25, 21, 60, 86, 18, 15),
    /* 24 日：計測が 30 分より短かった（AI-2 の Q03） */
    {10,   24,
     6,    NK_Q_FAILED,
     -1,   0,
     22,   22,
     70,   -1,
     -1,   0,
     0,    0,
     0,    0,
     0,    -1,
     -1,   -1,
     -1,   "計測が 30 分より短かったため、分析していません",
     NULL, 0},
    OK_NIGHT(10, 25, 0, 85, 452, 476, 1358, 12, 1, 8, 15, 92, 95, 6, 13),
    OK_NIGHT(10, 26, 1, 66, 371, 434, 40, 35, 3, 24, 22, 64, 85, 20, 15),
    /* 27 日：途中で計測が止まった（Q04）ため、計測できた時間が 7 割に届かない（Q05） */
    {10,        27,
     2,         NK_Q_INSUFFICIENT,
     61,        260,
     410,       260,
     1430,      12,
     390,       22,
     1,         10,
     12,        -1,
     -1,        -1,
     -1,        100,
     250,       "アプリが停止しました",
     k_seg_gap, LEN(k_seg_gap)},
    OK_NIGHT(10, 28, 3, 80, 405, 441, 1392, 18, 2, 14, 16, 80, 92, 9, 14),
    OK_NIGHT(10, 29, 4, 77, 386, 429, 1415, 20, 2, 19, 18, 72, 90, 11, 14),
    OK_NIGHT(10, 30, 5, 79, 429, 460, 1375, 15, 1, 12, 15, 88, 93, 7, 13),
    OK_NIGHT(10, 31, 6, 82, 398, 452, 1400, 28, 2, 22, 18, 72, 88, 12, 14),
};

size_t nk_demo_night_count(void) {
    return LEN(k_nights);
}

const nk_demo_night *nk_demo_night_at(size_t index) {
    return index < LEN(k_nights) ? &k_nights[index] : NULL;
}

const nk_demo_night *nk_demo_night_find(int month, int day) {
    for (size_t i = 0; i < LEN(k_nights); i++) {
        if (k_nights[i].month == month && k_nights[i].day == day) {
            return &k_nights[i];
        }
    }
    return NULL;
}

const nk_demo_night *nk_demo_latest(void) {
    return &k_nights[LEN(k_nights) - 1];
}

const nk_demo_night *nk_demo_insufficient(void) {
    return nk_demo_night_find(10, 27);
}

const nk_demo_night *nk_demo_partial_gap(void) {
    static nk_demo_night night;
    night = k_nights[LEN(k_nights) - 1];
    night.measured_min = night.in_bed_min - 30;
    night.gap_start = 3 * 60 + 10;
    night.gap_end = 3 * 60 + 40;
    night.gap_reason = "アプリが停止しました";
    night.segments = k_seg_partial;
    night.segment_count = LEN(k_seg_partial);
    return &night;
}

const nk_demo_night *nk_demo_previous(const nk_demo_night *night) {
    for (size_t i = 1; i < LEN(k_nights); i++) {
        if (&k_nights[i] == night) {
            return &k_nights[i - 1];
        }
    }
    return NULL;
}

const nk_demo_night *nk_demo_next(const nk_demo_night *night) {
    for (size_t i = 0; i + 1 < LEN(k_nights); i++) {
        if (&k_nights[i] == night) {
            return &k_nights[i + 1];
        }
    }
    return NULL;
}

const char *nk_demo_wday_name(int wday) {
    static const char *const names[] = {"日", "月", "火", "水", "木", "金", "土"};
    return names[((wday % 7) + 7) % 7];
}

int nk_demo_wday(int month, int day) {
    /* 2026 年の各月 1 日の曜日（0 = 日曜） */
    static const int first[] = {4, 0, 0, 3, 5, 1, 3, 6, 2, 4, 0, 2};
    if (month < 1 || month > 12) {
        return 0;
    }
    return (first[month - 1] + day - 1) % 7;
}

static nk_demo_state s_state;
static bool s_state_ready;

void nk_demo_state_reset(void) {
    s_state.automode = false; /* 初期値はオフ（PRD 4.5） */
    s_state.automode_skip = false;
    s_state.lead_min = 10;
    s_state.curtain_pos = 60;
    s_state.curtain_connected = true;
    s_state.curtain_choice = 0;
    s_state.alarm_on = true;
    s_state.alarm_min = 6 * 60 + 30;
    s_state.alarm_holiday_min = 8 * 60;
    s_state.snooze_len = 5;
    s_state.snooze_max = 3;
    s_state.vibration = true;
    s_state.sound = 1;
    s_state.playing = true;
    s_state.volume = 42;
    s_state.timer_choice = 1;
    s_state.keep_snore = false;
    s_state.morning_question = true;
    s_state.goal_min = 7 * 60;
    s_state.history_day = 31;
    s_state.history_week = 26;
    s_state.detail_index = (int)LEN(k_nights) - 1;
    s_state.consented = true;
    s_state_ready = true;
}

int nk_demo_next_alarm(void) {
    return nk_demo_state_get()->alarm_holiday_min;
}

int nk_demo_goal_bedtime(void) {
    int m = nk_demo_state_get()->alarm_min - nk_demo_state_get()->goal_min;
    return ((m % 1440) + 1440) % 1440;
}

int nk_demo_streak(void) {
    int streak = 0;
    for (int day = 31; day >= 1; day--) {
        const nk_demo_night *n = nk_demo_night_find(10, day);
        if (n == NULL || n->quality == NK_Q_FAILED) {
            break;
        }
        streak++;
    }
    return streak;
}

nk_demo_state *nk_demo_state_get(void) {
    if (!s_state_ready) {
        nk_demo_state_reset();
    }
    return &s_state;
}

/* ---- 書式（UX-8 の 6 章：時刻は「6:30」、長さは「7時間10分」、日付は「10月31日（土）」） ---- */

void nk_fmt_duration(char *buf, size_t size, int minutes) {
    if (minutes < 0) {
        minutes = 0;
    }
    if (minutes >= 60 && minutes % 60 == 0) {
        (void)snprintf(buf, size, "%d時間", minutes / 60);
    } else if (minutes >= 60) {
        (void)snprintf(buf, size, "%d時間%d分", minutes / 60, minutes % 60);
    } else {
        (void)snprintf(buf, size, "%d分", minutes);
    }
}

void nk_fmt_clock(char *buf, size_t size, int minutes_of_day) {
    int m = ((minutes_of_day % 1440) + 1440) % 1440;
    (void)snprintf(buf, size, "%d:%02d", m / 60, m % 60);
}

void nk_fmt_date(char *buf, size_t size, int month, int day, int wday) {
    (void)snprintf(buf, size, "%d月%d日（%s）", month, day, nk_demo_wday_name(wday));
}
