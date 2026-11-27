#include "nk_ui_store.h"

#include "data_session.h"
#include "nk_db.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static nk_sleep_record s_records[NK_UI_MAX_RECORDS];
static int s_record_count = 0;
static int s_selected_idx = 0;
static int s_week_offset = 0;
static char s_banner_text[96];
static bool s_banner_warn = false;

static void copy_str(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    lv_snprintf(dst, dst_size, "%s", src != NULL ? src : "");
}

static void format_jp_date_from_epoch(int64_t ts, char *out_short, size_t short_sz,
                                      char *out_morning, size_t morning_sz, char *out_span,
                                      size_t span_sz) {
    static const char *const k_wday[] = {"日", "月", "火", "水", "木", "金", "土"};
    int64_t sec = (ts > 100000000000LL) ? (ts / 1000LL) : ts;
    time_t t = (time_t)sec;
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
#if defined(_WIN32)
    (void)localtime_s(&tm_val, &t);
#else
    const struct tm *ptm = localtime(&t);
    if (ptm != NULL) {
        tm_val = *ptm;
    }
#endif
    int m = tm_val.tm_mon + 1;
    int d = tm_val.tm_mday;
    const char *w = (tm_val.tm_wday >= 0 && tm_val.tm_wday < 7) ? k_wday[tm_val.tm_wday] : "土";
    lv_snprintf(out_short, short_sz, "%d月%d日（%s）", m, d, w);
    lv_snprintf(out_morning, morning_sz, "%d月%d日（%s）の朝", m, d, w);
    lv_snprintf(out_span, span_sz, "%d月%d日（%s）の朝 の記録", m, d, w);
}

static void init_sample_records(void) {
    memset(s_records, 0, sizeof(s_records));

    /* [0] 最新：10月31日（土）の朝（OK・82点） */
    nk_sleep_record *r0 = &s_records[0];
    r0->id = 106;
    copy_str(r0->date_short, sizeof(r0->date_short), "10月31日（土）");
    copy_str(r0->morning_title, sizeof(r0->morning_title), "10月31日（土）の朝");
    copy_str(r0->night_span, sizeof(r0->night_span), "10月30日（金）の夜 → 31日（土）の朝");
    copy_str(r0->weekday, sizeof(r0->weekday), "土");
    r0->quality = NK_QUALITY_OK;
    r0->score = 82;
    r0->score_diff = 3;
    copy_str(r0->score_label, sizeof(r0->score_label), "良い");
    copy_str(r0->score_comment, sizeof(r0->score_comment), "よく眠れました");
    r0->sleep_min = 398;
    r0->tib_min = 452;
    r0->measured_min = 452;
    r0->sol_min = 28;
    r0->waso_count = 2;
    r0->waso_min = 22;
    r0->efficiency_pct = 88;
    r0->deep_min = 72;
    r0->light_min = 326;
    r0->turns = 14;
    r0->breath_bpm = 14;
    copy_str(r0->snore_text, sizeof(r0->snore_text), "記録なし（マイクオフ）");
    copy_str(r0->bedtime, sizeof(r0->bedtime), "23:48");
    copy_str(r0->wake_time, sizeof(r0->wake_time), "6:48");
    r0->bed_min_from_21 = 168;  /* 23:48 */
    r0->wake_min_from_21 = 588; /* 6:48 */
    r0->sub_scores[0] = 26;
    r0->sub_scores[1] = 12;
    r0->sub_scores[2] = 25;
    r0->sub_scores[3] = 15;
    r0->sub_scores[4] = 4;
    r0->in_bed_from = NK_DEMO_IN_BED_FROM;
    r0->segment_count = nk_demo_segment_count < 16 ? nk_demo_segment_count : 16;
    for (int i = 0; i < r0->segment_count; i++) {
        r0->segments[i] = nk_demo_segments[i];
    }
    r0->gap_from_min = -1;
    r0->gap_to_min = -1;
    copy_str(r0->gap_reason, sizeof(r0->gap_reason), "なし");
    r0->marker_count = 4;
    r0->markers[0].minute_from_23 = 75;
    r0->markers[0].type = NK_MARKER_TURN;
    r0->markers[1].minute_from_23 = 190;
    r0->markers[1].type = NK_MARKER_SNORE;
    r0->markers[2].minute_from_23 = 310;
    r0->markers[2].type = NK_MARKER_TURN;
    r0->markers[3].minute_from_23 = 440;
    r0->markers[3].type = NK_MARKER_CURTAIN;
    copy_str(r0->short_advice, sizeof(r0->short_advice),
             "寝つきまで 28 分でした。今夜は 23:30 ごろに布団に入ってみませんか。");
    copy_str(r0->advice_basis, sizeof(r0->advice_basis),
             "就寝がいつもより 40 分遅く、規則性の点が下がっています");
    copy_str(r0->advice_habit, sizeof(r0->advice_habit),
             "今夜は 23:30 ごろに布団に入ってみませんか");
    copy_str(r0->advice_curtain, sizeof(r0->advice_curtain),
             "閉める時刻の予約（22:30）を、オンにしてみませんか");
    r0->mood = 0;

    /* [1] 10月30日（金） */
    s_records[1] = *r0;
    s_records[1].id = 105;
    copy_str(s_records[1].date_short, sizeof(s_records[1].date_short), "10月30日（金）");
    copy_str(s_records[1].morning_title, sizeof(s_records[1].morning_title), "10月30日（金）の朝");
    copy_str(s_records[1].night_span, sizeof(s_records[1].night_span),
             "10月29日（木）の夜 → 30日（金）の朝");
    copy_str(s_records[1].weekday, sizeof(s_records[1].weekday), "金");
    s_records[1].score = 79;
    s_records[1].score_diff = 2;
    s_records[1].sleep_min = 429;
    s_records[1].tib_min = 470;
    s_records[1].measured_min = 470;
    copy_str(s_records[1].bedtime, sizeof(s_records[1].bedtime), "23:10");
    copy_str(s_records[1].wake_time, sizeof(s_records[1].wake_time), "7:00");
    s_records[1].bed_min_from_21 = 130;
    s_records[1].wake_min_from_21 = 600;
    copy_str(s_records[1].short_advice, sizeof(s_records[1].short_advice),
             "7時間以上しっかり眠れています。このリズムを保てると良さそうです。");
    s_records[1].mood = 1;

    /* [2] 10月29日（木） */
    s_records[2] = *r0;
    s_records[2].id = 104;
    copy_str(s_records[2].date_short, sizeof(s_records[2].date_short), "10月29日（木）");
    copy_str(s_records[2].morning_title, sizeof(s_records[2].morning_title), "10月29日（木）の朝");
    copy_str(s_records[2].night_span, sizeof(s_records[2].night_span),
             "10月28日（水）の夜 → 29日（木）の朝");
    copy_str(s_records[2].weekday, sizeof(s_records[2].weekday), "木");
    s_records[2].score = 77;
    s_records[2].score_diff = -3;
    s_records[2].sleep_min = 386;
    copy_str(s_records[2].bedtime, sizeof(s_records[2].bedtime), "23:35");
    copy_str(s_records[2].wake_time, sizeof(s_records[2].wake_time), "6:30");
    s_records[2].bed_min_from_21 = 155;
    s_records[2].wake_min_from_21 = 570;
    copy_str(s_records[2].short_advice, sizeof(s_records[2].short_advice),
             "深い眠りが安定していました。今夜も寝る前のスマホを控えめにしてみませんか。");
    s_records[2].mood = 2;

    /* [3] 10月28日（水） */
    s_records[3] = *r0;
    s_records[3].id = 103;
    copy_str(s_records[3].date_short, sizeof(s_records[3].date_short), "10月28日（水）");
    copy_str(s_records[3].morning_title, sizeof(s_records[3].morning_title), "10月28日（水）の朝");
    copy_str(s_records[3].night_span, sizeof(s_records[3].night_span),
             "10月27日（火）の夜 → 28日（水）の朝");
    copy_str(s_records[3].weekday, sizeof(s_records[3].weekday), "水");
    s_records[3].score = 80;
    s_records[3].score_diff = 999;
    s_records[3].sleep_min = 405;
    copy_str(s_records[3].bedtime, sizeof(s_records[3].bedtime), "23:15");
    copy_str(s_records[3].wake_time, sizeof(s_records[3].wake_time), "6:35");
    s_records[3].bed_min_from_21 = 135;
    s_records[3].wake_min_from_21 = 575;
    s_records[3].mood = 1;

    /* [4] 10月27日（火）：INSUFFICIENT（参考値・欠損区間あり） */
    s_records[4] = *r0;
    s_records[4].id = 102;
    copy_str(s_records[4].date_short, sizeof(s_records[4].date_short), "10月27日（火）");
    copy_str(s_records[4].morning_title, sizeof(s_records[4].morning_title), "10月27日（火）の朝");
    copy_str(s_records[4].night_span, sizeof(s_records[4].night_span),
             "10月26日（月）の夜 → 27日（火）の朝");
    copy_str(s_records[4].weekday, sizeof(s_records[4].weekday), "火");
    s_records[4].quality = NK_QUALITY_INSUFFICIENT;
    s_records[4].score = 61;
    s_records[4].score_diff = 999;
    copy_str(s_records[4].score_label, sizeof(s_records[4].score_label), "参考値");
    copy_str(s_records[4].score_comment, sizeof(s_records[4].score_comment),
             "計測できた時間が短めです");
    s_records[4].sleep_min = 260;
    s_records[4].tib_min = 380;
    s_records[4].measured_min = 260;
    s_records[4].gap_from_min = 250; /* 3:10 */
    s_records[4].gap_to_min = 340;   /* 4:40 */
    copy_str(s_records[4].gap_reason, sizeof(s_records[4].gap_reason),
             "3:10〜4:40（アプリが停止しました）");
    copy_str(s_records[4].bedtime, sizeof(s_records[4].bedtime), "0:15");
    copy_str(s_records[4].wake_time, sizeof(s_records[4].wake_time), "6:35");
    s_records[4].bed_min_from_21 = 195;
    s_records[4].wake_min_from_21 = 575;
    copy_str(s_records[4].short_advice, sizeof(s_records[4].short_advice),
             "途中で計測が止まったため参考値です。充電ケーブルをつないで計測すると、朝まで記録でき"
             "ます。");
    s_records[4].mood = 3;

    /* [5] 10月26日（月） */
    s_records[5] = *r0;
    s_records[5].id = 101;
    copy_str(s_records[5].date_short, sizeof(s_records[5].date_short), "10月26日（月）");
    copy_str(s_records[5].morning_title, sizeof(s_records[5].morning_title), "10月26日（月）の朝");
    copy_str(s_records[5].night_span, sizeof(s_records[5].night_span),
             "10月25日（日）の夜 → 26日（月）の朝");
    copy_str(s_records[5].weekday, sizeof(s_records[5].weekday), "月");
    s_records[5].score = 66;
    s_records[5].score_diff = 999;
    s_records[5].sleep_min = 371;
    copy_str(s_records[5].bedtime, sizeof(s_records[5].bedtime), "23:55");
    copy_str(s_records[5].wake_time, sizeof(s_records[5].wake_time), "6:40");
    s_records[5].bed_min_from_21 = 175;
    s_records[5].wake_min_from_21 = 580;
    s_records[5].mood = 2;

    s_record_count = 6;
}

static void load_records_from_db(void) {
    if (!nk_db_is_open()) {
        return;
    }
    nk_sleep_session_t rows[NK_UI_MAX_RECORDS];
    int n = nk_data_session_list(rows, NK_UI_MAX_RECORDS);
    if (n <= 0) {
        return;
    }

    nk_sleep_record base = s_records[0];
    memset(s_records, 0, sizeof(s_records));

    for (int i = 0; i < n; i++) {
        s_records[i] = base;
        s_records[i].id = rows[i].id;
        s_records[i].score = rows[i].sleep_score;

        int64_t diff_raw = rows[i].ended_at - rows[i].started_at;
        int64_t dur_sec = (diff_raw > 100000000LL) ? (diff_raw / 1000LL) : diff_raw;
        int32_t mins = (dur_sec > 0) ? (int32_t)(dur_sec / 60LL) : 360;
        s_records[i].sleep_min = mins;
        s_records[i].tib_min = mins + 20;
        s_records[i].measured_min = mins + 20;

        if (rows[i].sleep_score < 0) {
            s_records[i].quality = NK_QUALITY_FAILED;
            copy_str(s_records[i].score_label, sizeof(s_records[i].score_label), "取得失敗");
            copy_str(s_records[i].score_comment, sizeof(s_records[i].score_comment),
                     "計測データを分析できませんでした");
        } else if (mins < 180) {
            s_records[i].quality = NK_QUALITY_INSUFFICIENT;
            copy_str(s_records[i].score_label, sizeof(s_records[i].score_label), "参考値");
            copy_str(s_records[i].score_comment, sizeof(s_records[i].score_comment),
                     "計測時間が3時間未満のため参考値です");
        } else {
            s_records[i].quality = NK_QUALITY_OK;
            copy_str(s_records[i].score_label, sizeof(s_records[i].score_label),
                     rows[i].sleep_score >= 70 ? "良い" : "回復を優先");
        }

        format_jp_date_from_epoch(rows[i].ended_at, s_records[i].date_short,
                                  sizeof(s_records[i].date_short), s_records[i].morning_title,
                                  sizeof(s_records[i].morning_title), s_records[i].night_span,
                                  sizeof(s_records[i].night_span));
    }

    for (int i = 0; i < n; i++) {
        if (i + 1 < n && s_records[i].quality == NK_QUALITY_OK &&
            s_records[i + 1].quality == NK_QUALITY_OK) {
            s_records[i].score_diff = s_records[i].score - s_records[i + 1].score;
        } else {
            s_records[i].score_diff = 999;
        }
    }

    s_record_count = n;
}

void nk_ui_store_init(void) {
    init_sample_records();
    load_records_from_db();
    s_selected_idx = 0;
    s_week_offset = 0;
    s_banner_text[0] = '\0';
    s_banner_warn = false;
}

int nk_ui_store_count(void) {
    return s_record_count;
}

int nk_ui_store_selected_index(void) {
    return s_selected_idx;
}

bool nk_ui_store_select_index(int index) {
    if (index < 0 || index >= s_record_count) {
        return false;
    }
    s_selected_idx = index;
    return true;
}

const nk_sleep_record *nk_ui_store_latest(void) {
    return s_record_count > 0 ? &s_records[0] : NULL;
}

const nk_sleep_record *nk_ui_store_selected(void) {
    if (s_selected_idx < 0 || s_selected_idx >= s_record_count) {
        return NULL;
    }
    return &s_records[s_selected_idx];
}

const nk_sleep_record *nk_ui_store_get(int index) {
    if (index < 0 || index >= s_record_count) {
        return NULL;
    }
    return &s_records[index];
}

bool nk_ui_store_can_prev(void) {
    return s_selected_idx + 1 < s_record_count;
}

bool nk_ui_store_can_next(void) {
    return s_selected_idx > 0;
}

bool nk_ui_store_move_prev(void) {
    if (!nk_ui_store_can_prev()) {
        return false;
    }
    s_selected_idx++;
    return true;
}

bool nk_ui_store_move_next(void) {
    if (!nk_ui_store_can_next()) {
        return false;
    }
    s_selected_idx--;
    return true;
}

int nk_ui_store_week_offset(void) {
    return s_week_offset;
}

bool nk_ui_store_can_next_week(void) {
    return s_week_offset > 0;
}

void nk_ui_store_move_prev_week(void) {
    if (s_week_offset < 4) {
        s_week_offset++;
    }
}

void nk_ui_store_move_next_week(void) {
    if (s_week_offset > 0) {
        s_week_offset--;
    }
}

int nk_ui_store_streak_days(void) {
    return s_record_count > 0 ? s_record_count + 1 : 0;
}

int32_t nk_ui_store_goal_sleep_min(void) {
    return 420; /* 既定 7時間 */
}

const char *nk_ui_store_goal_bedtime(void) {
    return NK_DEMO_GOAL;
}

bool nk_ui_store_set_mood(int mood) {
    if (s_selected_idx < 0 || s_selected_idx >= s_record_count || mood < 1 || mood > 3) {
        return false;
    }
    s_records[s_selected_idx].mood = mood;
    return true;
}

bool nk_ui_store_delete_selected(void) {
    if (s_selected_idx < 0 || s_selected_idx >= s_record_count) {
        return false;
    }
    int64_t target_id = s_records[s_selected_idx].id;
    if (nk_db_is_open()) {
        (void)nk_data_session_delete_by_id(target_id);
    }
    for (int i = s_selected_idx; i + 1 < s_record_count; i++) {
        s_records[i] = s_records[i + 1];
    }
    s_record_count--;
    if (s_selected_idx >= s_record_count && s_selected_idx > 0) {
        s_selected_idx--;
    }
    return true;
}

void nk_ui_store_operate_curtain(bool open) {
    s_banner_warn = false;
    copy_str(s_banner_text, sizeof(s_banner_text),
             open ? "カーテンを開けました（目標 100%）" : "カーテンを閉じました（目標 0%）");
}

const char *nk_ui_store_banner_text(void) {
    return s_banner_text;
}

bool nk_ui_store_banner_is_warn(void) {
    return s_banner_warn;
}

void nk_ui_store_clear_banner(void) {
    s_banner_text[0] = '\0';
    s_banner_warn = false;
}

void nk_ui_format_duration(int32_t minutes, char *buf, size_t buf_size) {
    if (buf == NULL || buf_size == 0) {
        return;
    }
    if (minutes < 0) {
        lv_snprintf(buf, buf_size, "記録なし");
        return;
    }
    int32_t h = minutes / 60;
    int32_t m = minutes % 60;
    if (h > 0) {
        lv_snprintf(buf, buf_size, "%d時間%d分", (int)h, (int)m);
    } else {
        lv_snprintf(buf, buf_size, "%d分", (int)m);
    }
}
