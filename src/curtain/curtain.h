#ifndef NK_CURTAIN_H
#define NK_CURTAIN_H

#include "curtain_api.h"
#include "result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NK_CURTAIN_MAX_LOGS 5

/* 接続の札（UX-5 1章 #1 / UX-7） */
typedef enum nk_curtain_conn_state {
    NK_CURTAIN_CONN_DISCONNECTED = 0, /* 未接続 */
    NK_CURTAIN_CONN_CHECKING,         /* 確認中 */
    NK_CURTAIN_CONN_CONNECTED,        /* 接続済み */
    NK_CURTAIN_CONN_OFFLINE           /* つながりません */
} nk_curtain_conn_state;

/* 操作結果の 4 状態（UX-5 2章 / ADR-0006） */
typedef enum nk_curtain_op_state {
    NK_CURTAIN_OP_IDLE = 0,
    NK_CURTAIN_OP_PENDING,    /* 依頼中（伝えています / 動いています） */
    NK_CURTAIN_OP_CONFIRMED,  /* 成功（確認済み） */
    NK_CURTAIN_OP_FAILED,     /* 失敗（理由と再試行案内） */
    NK_CURTAIN_OP_UNCONFIRMED /* 未確認（45秒以内に動いたか確認できず） */
} nk_curtain_op_state;

typedef enum nk_curtain_repeat_rule {
    NK_CURTAIN_REPEAT_EVERYDAY = 0,
    NK_CURTAIN_REPEAT_WEEKDAYS = 1
} nk_curtain_repeat_rule;

typedef struct nk_curtain_schedule {
    bool open_enabled;
    int open_hour;
    int open_min;
    bool close_enabled;
    int close_hour;
    int close_min;
    nk_curtain_repeat_rule repeat;
} nk_curtain_schedule;

typedef struct nk_curtain_log_entry {
    char when_text[24];
    char message[96];
    bool is_error;
} nk_curtain_log_entry;

typedef struct nk_curtain_snapshot {
    nk_curtain_conn_state conn_state;
    char device_id[NK_CURTAIN_API_ID_MAX];
    char device_name[NK_CURTAIN_API_NAME_MAX];
    char masked_token[16];
    char masked_secret[16];
    char base_url[NK_CURTAIN_API_URL_MAX];

    int open_pct;        /* 現在の開き具合（0=全閉, 100=全開） */
    int target_open_pct; /* 目標の開き具合 */
    char last_checked_time[16];

    nk_curtain_op_state op_state;
    bool command_accepted; /* true なら「動いています」、false なら「カーテンに伝えています」 */
    char status_message[128];

    nk_curtain_schedule schedule;
    nk_curtain_log_entry logs[NK_CURTAIN_MAX_LOGS];
    int log_count;
} nk_curtain_snapshot;

/* OS 側の予約アラーム（Android AlarmManager 等）へ同期するためのコールバック */
typedef void (*nk_curtain_platform_schedule_fn)(const nk_curtain_schedule *sched);

/* 初期化・終了（DB が開いていれば保存済みの認証情報・端末・予約を復元する） */
nk_result nk_curtain_init(void);
void nk_curtain_deinit(void);

/* プラットフォーム予約同期コールバックの登録 */
void nk_curtain_set_platform_scheduler(nk_curtain_platform_schedule_fn fn);

/* タイミング設定（既定: ポーリング間隔 3000ms、確認上限 45000ms、自動再試行 15000ms / 60000ms） */
void nk_curtain_set_timing_ms(int poll_interval_ms, int confirm_timeout_ms, int retry1_delay_ms,
                              int retry2_delay_ms);

/* 接続設定（BR-02）・接続解除 */
nk_result nk_curtain_set_base_url(const char *base_url);
nk_result nk_curtain_connect(const char *token, const char *secret,
                             nk_curtain_api_device *out_devices, size_t max_devices,
                             size_t *out_count);
nk_result nk_curtain_select_device(const char *device_id, const char *device_name);
nk_result nk_curtain_disconnect(void);

/* 現在の状態スナップショットをスレッドセーフに取得する */
void nk_curtain_get_snapshot(nk_curtain_snapshot *out_snapshot);

/* 状態即時確認（手動更新 / 未確認からの再確認） */
nk_result nk_curtain_refresh_status(void);

/* 手動操作（同期版：テスト・ヘッドレス用／非同期版：UI スレッド用） */
nk_result nk_curtain_open_sync(void);
nk_result nk_curtain_close_sync(void);
nk_result nk_curtain_set_position_sync(int open_pct);
nk_result nk_curtain_test_nudge_sync(void);

nk_result nk_curtain_open_async(void);
nk_result nk_curtain_close_async(void);
nk_result nk_curtain_set_position_async(int open_pct);
nk_result nk_curtain_test_nudge_async(void);

/* 予約設定の保存とヘッドレス実行（15秒後・60秒後の再試行つき：TM-04 No.6） */
nk_result nk_curtain_set_schedule(const nk_curtain_schedule *sched);
nk_result nk_curtain_execute_scheduled(bool open_action, bool is_weekday);

/* Android Scheduler.kt のスケジュール種別定数 */
#define NK_CURTAIN_SCHED_ID_OPEN  101
#define NK_CURTAIN_SCHED_ID_CLOSE 102

/* 次の予約発火時刻（UNIX エポックミリ秒）を計算する */
int64_t nk_curtain_next_trigger_ms(int hour, int minute, nk_curtain_repeat_rule repeat,
                                   int64_t now_ms);

#endif /* NK_CURTAIN_H */
