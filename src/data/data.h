#ifndef NK_DATA_H
#define NK_DATA_H

#include "result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct nk_data nk_data;

/*
 * 欠損可能な整数。
 * has_valueが0ならDBへNULLとして保存する。
 */
typedef struct nk_optional_int {
    int has_value;
    int value;
} nk_optional_int;

/*
 * 欠損可能な実数。
 * has_valueが0ならDBへNULLとして保存する。
 */
typedef struct nk_optional_double {
    int has_value;
    double value;
} nk_optional_double;

/*
 * 同意情報。
 */
typedef struct nk_consent_record {
    const char *version;
    int measurement;
    int microphone;
    int64_t consented_ms;
} nk_consent_record;

/*
 * 計測開始時に保存する情報。
 */
typedef struct nk_session_record {
    int64_t start_ms;
    int tz_min;
    int target_sleep_min;

    int has_alarm_ms;
    int64_t alarm_ms;

    int mic_used;
    int light_used;

    const char *placement;

    int has_battery_start;
    int battery_start;

    const char *app_ver;
    const char *algo_ver;
    const char *device_model;
} nk_session_record;

/*
 * epochのフラグ。
 */
typedef enum nk_epoch_flag {
    NK_EPF_GAP_ACC = 1U << 0,
    NK_EPF_GAP_MIC = 1U << 1,
    NK_EPF_OFFBED = 1U << 2,
    NK_EPF_MIC_BUSY = 1U << 3,
    NK_EPF_CHARGING = 1U << 4,
    NK_EPF_SOUND_ON = 1U << 5
} nk_epoch_flag;

/*
 * イベントの種類。
 */
typedef enum nk_event_type {
    NK_EVENT_TURN = 0,
    NK_EVENT_SNORE,
    NK_EVENT_NOISE,
    NK_EVENT_GAP_START,
    NK_EVENT_GAP_END,
    NK_EVENT_SOUND_START,
    NK_EVENT_SOUND_STOP,
    NK_EVENT_CURTAIN,
    NK_EVENT_ALARM,
    NK_EVENT_USER
} nk_event_type;

/*
 * 計測の終了理由。
 */
typedef enum nk_session_end_reason {
    NK_SESSION_END_USER_STOP = 0,
    NK_SESSION_END_ALARM_STOP,
    NK_SESSION_END_TIMEOUT_14H,
    NK_SESSION_END_SERVICE_KILLED,
    NK_SESSION_END_BATTERY_LOW,
    NK_SESSION_END_ERROR
} nk_session_end_reason;

/*
 * 30秒分のepoch。
 */
typedef struct nk_epoch_record {
    int64_t session_id;
    int idx;

    double acc_cov;
    double mic_cov;

    nk_optional_int move_n;
    nk_optional_double move_mg;
    nk_optional_double micro_mg;
    nk_optional_int posture;

    nk_optional_double snd_db_mean;
    nk_optional_double snd_db_p95;
    nk_optional_int snore_s;

    nk_optional_double breath_bpm;
    nk_optional_double breath_q;
    nk_optional_double lux;

    unsigned int flags;
} nk_epoch_record;

/*
 * イベント。
 */
typedef struct nk_event_record {
    int64_t session_id;
    int64_t t_ms;
    int64_t dur_ms;

    nk_event_type type;
    nk_optional_double value;

    const char *reason;
} nk_event_record;

/*
 * 暗号化DBを開く。
 *
 * 成功時に返される*out_dataは、
 * 呼び出し側がnk_data_close()で解放する。
 */
nk_result nk_data_open(nk_data **out_data, const char *path, const void *key, size_t key_size);

/*
 * 必要なテーブルを作成する。
 */
nk_result nk_data_create_schema(nk_data *data);

/*
 * 同意情報を保存する。
 */
nk_result nk_data_consent_set(nk_data *data, const nk_consent_record *consent);

/*
 * 計測同意があれば*out_has_consentへ1、
 * なければ0を格納する。
 */
nk_result nk_data_has_measurement_consent(nk_data *data, int *out_has_consent);

/* Application policy: only the latest active consent for this exact version counts. */
nk_result nk_data_has_versioned_consent(nk_data *data, const char *version, int *out_agreed);
nk_result nk_data_revoke_consent(nk_data *data, int64_t revoked_ms);

/* Delete session/epoch/event atomically; unfinished recordings are rejected. */
nk_result nk_data_delete_session(nk_data *data, int64_t session_id);
nk_result nk_data_delete_all_sessions(nk_data *data);

/*
 * sessionの開始情報を保存する。
 * 成功時に作成されたIDを*out_session_idへ格納する。
 */
nk_result nk_data_session_start(nk_data *data, const nk_session_record *session,
                                int64_t *out_session_id);

/*
 * sessionの終了情報を保存する。
 *
 * night_dateはYYYY-MM-DD形式。
 * has_battery_endが0ならbattery_endはNULLとして保存する。
 */
nk_result nk_data_session_finish(nk_data *data, int64_t session_id, int64_t end_ms,
                                 const char *night_date, nk_session_end_reason end_reason,
                                 int has_battery_end, int battery_end);

/*
 * epochを保存する。
 */
nk_result nk_data_epoch_insert(nk_data *data, const nk_epoch_record *epoch);

/*
 * イベントを保存する。
 */
nk_result nk_data_event_insert(nk_data *data, const nk_event_record *event);

/*
 * エラーメッセージを返す。
 * 戻り値は呼び出し側で解放しない。
 */
const char *nk_data_last_error(const nk_data *data);

/*
 * DBを閉じてnk_dataを解放する。
 *
 * 成功するとdataは無効になるため、以降は使用しない。
 * 失敗した場合は解放せず、再試行できるように残す。
 *
 * dataがNULLの場合は何もせずNK_OKを返す。
 */
nk_result nk_data_close(nk_data *data);

#endif
