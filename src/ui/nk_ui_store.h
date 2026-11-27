#ifndef NK_UI_STORE_H
#define NK_UI_STORE_H

#include "ui_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define NK_UI_MAX_RECORDS 14
#define NK_UI_MAX_EVENTS  12

/* 分析品質（AI-2 準拠） */
typedef enum nk_quality {
    NK_QUALITY_OK = 0,
    NK_QUALITY_INSUFFICIENT,
    NK_QUALITY_FAILED
} nk_quality;

/* 帯の上に重ねるイベント種別（UX-4 2章 #3） */
typedef enum nk_marker_type {
    NK_MARKER_TURN = 0,
    NK_MARKER_SNORE,
    NK_MARKER_NOISE,
    NK_MARKER_SOUND,
    NK_MARKER_CURTAIN,
    NK_MARKER_ALARM
} nk_marker_type;

typedef struct nk_stage_marker {
    int32_t minute_from_23; /* 23:00 からの経過分 (0〜480) */
    nk_marker_type type;
} nk_stage_marker;

/* 1晩分の睡眠記録モデル */
typedef struct nk_sleep_record {
    int64_t id;
    char date_short[24];    /* 例: "10月31日（土）" */
    char morning_title[32]; /* 例: "10月31日（土）の朝" */
    char night_span[48];    /* 例: "10月30日（金）の夜 → 31日（土）の朝" */
    char weekday[8];        /* 例: "土" */
    nk_quality quality;
    int32_t score;            /* 0〜100（FAILED 時は -1） */
    int32_t score_diff;       /* 前日比（比較不可時は 999） */
    char score_label[24];     /* 例: "良い" / "参考値" */
    char score_comment[48];   /* 例: "よく眠れました" */
    int32_t sleep_min;        /* 睡眠時間（分） */
    int32_t tib_min;          /* 寝床にいた時間（分） */
    int32_t measured_min;     /* 計測できた時間（分） */
    int32_t sol_min;          /* 寝つき（分） */
    int32_t waso_count;       /* 中途覚醒回数 */
    int32_t waso_min;         /* 中途覚醒合計（分） */
    int32_t efficiency_pct;   /* 睡眠効率（%） */
    int32_t deep_min;         /* 深い眠り（分） */
    int32_t light_min;        /* 浅い眠り（分） */
    int32_t turns;            /* 寝返り回数 */
    int32_t breath_bpm;       /* 呼吸数（不明時は -1） */
    char snore_text[32];      /* いびき表示（理由つき可） */
    char bedtime[8];          /* 就寝 "23:48" */
    char wake_time[8];        /* 起床 "6:48" */
    int32_t bed_min_from_21;  /* 21:00 からの就寝分（リズムグラフ用） */
    int32_t wake_min_from_21; /* 21:00 からの起床分（リズムグラフ用） */
    int32_t sub_scores[5];    /* 睡眠時間/30, 寝つき/15, 連続性/25, 効率/15, 規則性/15 */
    int32_t in_bed_from;      /* 23:00 からの開始分 */
    nk_segment segments[16];
    int segment_count;
    int32_t gap_from_min; /* 欠損区間開始（なしなら -1） */
    int32_t gap_to_min;   /* 欠損区間終了 */
    char gap_reason[48];  /* 例: "なし" / "3:10〜4:40（アプリ停止）" */
    nk_stage_marker markers[NK_UI_MAX_EVENTS];
    int marker_count;
    char short_advice[128];   /* ホーム・日表示の1〜2文 */
    char advice_basis[128];   /* 詳細：根拠 */
    char advice_habit[128];   /* 詳細：睡眠習慣の提案 */
    char advice_curtain[128]; /* 詳細：カーテン設定の提案 */
    int mood;                 /* 0: 未入力, 1: すっきり, 2: ふつう, 3: ねむい */
} nk_sleep_record;

/* ストア初期化・リセット（DBに保存済みの記録があれば反映） */
void nk_ui_store_init(void);

/* 記録件数・現在の選択インデックス */
int nk_ui_store_count(void);
int nk_ui_store_selected_index(void);
bool nk_ui_store_select_index(int index);

/* 最新記録（index 0）および現在選択中の記録を取得（0件時は NULL） */
const nk_sleep_record *nk_ui_store_latest(void);
const nk_sleep_record *nk_ui_store_selected(void);
const nk_sleep_record *nk_ui_store_get(int index);

/* 前日・翌日移動（index が大きいほど過去、0 が最新＝未来には進めない） */
bool nk_ui_store_can_prev(void);
bool nk_ui_store_can_next(void);
bool nk_ui_store_move_prev(void);
bool nk_ui_store_move_next(void);

/* 週表示の期間ナビ（0 が今週＝未来の週には進めない） */
int nk_ui_store_week_offset(void);
bool nk_ui_store_can_next_week(void);
void nk_ui_store_move_prev_week(void);
void nk_ui_store_move_next_week(void);

/* 継続記録日数・目標睡眠時間（分）・今夜の目標時刻 */
int nk_ui_store_streak_days(void);
int32_t nk_ui_store_goal_sleep_min(void);
const char *nk_ui_store_goal_bedtime(void);

/* 目覚めの気分を記録（1: すっきり, 2: ふつう, 3: ねむい） */
bool nk_ui_store_set_mood(int mood);

/* 現在選択中の夜の記録を削除する */
bool nk_ui_store_delete_selected(void);

/* カーテン操作と通知メッセージ（HM-01 用） */
void nk_ui_store_operate_curtain(bool open);
const char *nk_ui_store_banner_text(void);
bool nk_ui_store_banner_is_warn(void);
void nk_ui_store_clear_banner(void);

/* 分を "X時間Y分" に整形するユーティリティ */
void nk_ui_format_duration(int32_t minutes, char *buf, size_t buf_size);

#endif /* NK_UI_STORE_H */
