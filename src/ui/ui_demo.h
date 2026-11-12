/*
 * 画面の試作で表示するデモ用のデータ（画面モック v2 と同じ値）
 *
 * 記録は「朝の日付」で持つ（10 月 31 日の記録 = 30 日の夜〜31 日の朝）。
 * 分析（src/analysis/）とデータ（src/data/）ができたら、この中身を置き換える。
 */
#ifndef NK_UI_DEMO_H
#define NK_UI_DEMO_H

#include <stdbool.h>
#include <stddef.h>

/* 記録の品質（AI-2） */
typedef enum nk_quality {
    NK_Q_NONE, /* 記録なし */
    NK_Q_OK,
    NK_Q_INSUFFICIENT,
    NK_Q_FAILED,
} nk_quality;

/* 眠りの段階（眠りの帯の 1 区間） */
typedef enum nk_stage {
    NK_STAGE_WAKE,
    NK_STAGE_LIGHT,
    NK_STAGE_DEEP,
    NK_STAGE_SLEEP, /* 浅い・深いを分けない */
    NK_STAGE_GAP,   /* 計測できなかった */
} nk_stage;

typedef struct nk_demo_segment {
    int minutes;
    nk_stage stage;
} nk_demo_segment;

typedef struct nk_demo_night {
    int month;
    int day;
    int wday; /* 0 = 日曜 */
    nk_quality quality;
    int score;
    int sleep_min;    /* 睡眠時間 */
    int in_bed_min;   /* 寝床にいた時間（計測の全体） */
    int measured_min; /* 計測できた時間 */
    int start;        /* 計測を始めた時刻（0 時からの分） */
    int onset;        /* 寝ついた時刻 */
    int wake;         /* 起きた時刻 */
    int latency;      /* 寝つき（分） */
    int waso_count;   /* 中途覚醒の回数 */
    int waso_min;     /* 中途覚醒の合計 */
    int turns;        /* 寝返り */
    int deep_min;     /* 深い眠り（推定） */
    int efficiency;   /* 睡眠効率（%） */
    int snore_min;    /* いびき（推定）。-1 は記録なし */
    int breath_rate;  /* 呼吸数（回/分）。-1 は記録なし */
    int gap_start;    /* 計測できなかった時間（-1 はなし） */
    int gap_end;
    const char *gap_reason;
    const nk_demo_segment *segments;
    size_t segment_count;
} nk_demo_night;

/* 記録（朝の日付の古い順） */
size_t nk_demo_night_count(void);
const nk_demo_night *nk_demo_night_at(size_t index);
/* 月・日から探す。なければ NULL */
const nk_demo_night *nk_demo_night_find(int month, int day);
/* 最新の記録（ホーム）、データ不足の例、一部が取れていない例 */
const nk_demo_night *nk_demo_latest(void);
const nk_demo_night *nk_demo_insufficient(void);
const nk_demo_night *nk_demo_partial_gap(void);
/* 直前の記録（前日比） */
const nk_demo_night *nk_demo_previous(const nk_demo_night *night);
const nk_demo_night *nk_demo_next(const nk_demo_night *night);

/* 曜日の 1 文字（「土」） */
const char *nk_demo_wday_name(int wday);
/* 2026 年 month 月 day 日の曜日 */
int nk_demo_wday(int month, int day);

/* 操作で変わる状態（アプリを閉じるまで保つ） */
typedef struct nk_demo_state {
    bool automode;
    bool automode_skip;
    int lead_min;    /* カーテンを開けるのはアラームの何分前か */
    int curtain_pos; /* 開き具合 0〜100 */
    bool curtain_connected;
    int curtain_choice; /* 使うカーテン */
    bool alarm_on;
    int alarm_min;         /* 平日の起きる時刻（0 時からの分） */
    int alarm_holiday_min; /* 休日の起きる時刻 */
    int snooze_len;        /* スヌーズの長さ（分） */
    int snooze_max;        /* スヌーズの回数 */
    bool vibration;
    int sound; /* 選んだサウンド（-1 なし） */
    bool playing;
    int volume;
    int timer_choice;      /* 0:15分 1:30分 2:60分 3:朝まで */
    bool keep_snore;       /* いびきの録音を残す */
    bool morning_question; /* 朝の気分を聞く */
    int goal_min;          /* 目標の睡眠時間 */
    int history_day;       /* 睡眠記録（日）で表示する日（10 月の日。11 月 1 日は 32） */
    int history_week;      /* 睡眠記録（週）の月曜日（10 月の日） */
    int detail_index;      /* LG-02 で表示する記録 */
    bool consented;        /* 計測に同意したか（同意しないで続けた場合は false） */
} nk_demo_state;

nk_demo_state *nk_demo_state_get(void);
void nk_demo_state_reset(void);

/* デモの「今」は 10 月 31 日（土）の朝。次のアラーム（あすは日曜なので休日の時刻） */
int nk_demo_next_alarm(void);
/* 目標の就寝時刻（平日の起きる時刻 − 目標の睡眠時間） */
int nk_demo_goal_bedtime(void);
/* 連続して記録できている日数（最新の朝からさかのぼる） */
int nk_demo_streak(void);

#endif /* NK_UI_DEMO_H */
