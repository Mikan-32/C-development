#include "data.h"

#include <sqlcipher/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NK_DATA_ERROR_SIZE 256

struct nk_data {
    sqlite3 *database;
    char last_error[NK_DATA_ERROR_SIZE];
};

static void set_error(nk_data *data, const char *message) {
    if (data == NULL) {
        return;
    }

    if (message == NULL) {
        data->last_error[0] = '\0';
        return;
    }

    /*
     * エラーメッセージはバッファに収まる長さで保持する。
     * 長い場合の切り詰めを許容するため、文字数は使用しない。
     * snprintfにより末尾のNULL文字は保証される。
     */
    (void)snprintf(data->last_error, sizeof(data->last_error), "%s", message);
}

static nk_result execute_sql(nk_data *data, const char *sql) {
    char *error_message = NULL;
    int result;

    result = sqlite3_exec(data->database, sql, NULL, NULL, &error_message);

    if (result != SQLITE_OK) {
        if (error_message != NULL) {
            set_error(data, error_message);
            sqlite3_free(error_message);
        } else {
            set_error(data, sqlite3_errmsg(data->database));
        }

        return NK_ERROR_DATABASE;
    }

    set_error(data, NULL);
    return NK_OK;
}

static const char *event_type_name(nk_event_type type) {
    switch (type) {
    case NK_EVENT_TURN:
        return "TURN";

    case NK_EVENT_SNORE:
        return "SNORE";

    case NK_EVENT_NOISE:
        return "NOISE";

    case NK_EVENT_GAP_START:
        return "GAP_START";

    case NK_EVENT_GAP_END:
        return "GAP_END";

    case NK_EVENT_SOUND_START:
        return "SOUND_START";

    case NK_EVENT_SOUND_STOP:
        return "SOUND_STOP";

    case NK_EVENT_CURTAIN:
        return "CURTAIN";

    case NK_EVENT_ALARM:
        return "ALARM";

    case NK_EVENT_USER:
        return "USER";

    default:
        return NULL;
    }
}

static const char *session_end_reason_name(nk_session_end_reason reason) {
    switch (reason) {
    case NK_SESSION_END_USER_STOP:
        return "USER_STOP";

    case NK_SESSION_END_ALARM_STOP:
        return "ALARM_STOP";

    case NK_SESSION_END_TIMEOUT_14H:
        return "TIMEOUT_14H";

    case NK_SESSION_END_SERVICE_KILLED:
        return "SERVICE_KILLED";

    case NK_SESSION_END_BATTERY_LOW:
        return "BATTERY_LOW";

    case NK_SESSION_END_ERROR:
        return "ERROR";

    default:
        return NULL;
    }
}

static nk_result verify_key(nk_data *data) {
    sqlite3_stmt *statement = NULL;
    int result;

    result = sqlite3_prepare_v2(data->database, "SELECT count(*) FROM sqlite_master;", -1,
                                &statement, NULL);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_ROW) {
        set_error(data, sqlite3_errmsg(data->database));

        /*
         * 読み取り失敗をすでに記録しているため、
         * 後片付けの結果で元のエラーを上書きしない。
         * finalizeはエラーを返してもstatementを解放する。
         */
        (void)sqlite3_finalize(statement);

        return NK_ERROR_DATABASE;
    }

    /*
     * 読み取り成功時は、後片付けの失敗も確認する。
     */
    result = sqlite3_finalize(statement);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        return NK_ERROR_DATABASE;
    }

    set_error(data, NULL);

    return NK_OK;
}

static int bind_optional_int(sqlite3_stmt *statement, int index, nk_optional_int optional) {
    if (optional.has_value) {
        return sqlite3_bind_int(statement, index, optional.value);
    }

    return sqlite3_bind_null(statement, index);
}

static int bind_optional_double(sqlite3_stmt *statement, int index, nk_optional_double optional) {
    if (optional.has_value) {
        return sqlite3_bind_double(statement, index, optional.value);
    }

    return sqlite3_bind_null(statement, index);
}

nk_result nk_data_open(nk_data **out_data, const char *path, const void *key, size_t key_size) {
    nk_data *data;
    int result;

    if (out_data == NULL || path == NULL || key == NULL || key_size == 0) {
        return NK_ERROR_ARGUMENT;
    }

    *out_data = NULL;

    data = calloc(1, sizeof(*data));
    if (data == NULL) {
        return NK_ERROR_MEMORY;
    }

    result =
        sqlite3_open_v2(path, &data->database,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);

    if (result != SQLITE_OK) {
        /*
         * 接続開始に失敗した場合も、
         * SQLiteがDBハンドルを返すことがある。
         *
         * 接続はまだ外部へ公開しておらず、
         * SQL文も作成していないため、ここで解放する。
         * 呼び出し側には元の接続開始失敗を返す。
         */
        (void)nk_data_close(data);

        return NK_ERROR_DATABASE;
    }

    result = sqlite3_key(data->database, key, (int)key_size);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        /*
         * 初期化失敗の後片付け。
         * 接続は外部へ未公開で、SQL文も解放済み。
         * 終了処理の結果によらず、元の初期化失敗を返す。
         */
        (void)nk_data_close(data);

        return NK_ERROR_DATABASE;
    }

    if (verify_key(data) != NK_OK) {
        /*
         * 初期化失敗の後片付け。
         * 接続は外部へ未公開で、SQL文も解放済み。
         * 終了処理の結果によらず、元の初期化失敗を返す。
         */
        (void)nk_data_close(data);

        return NK_ERROR_DATABASE;
    }

    if (execute_sql(data, "PRAGMA foreign_keys = ON;"
                          "PRAGMA secure_delete = ON;"
                          "PRAGMA journal_mode = WAL;") != NK_OK) {
        /*
         * 初期化失敗の後片付け。
         * 接続は外部へ未公開で、SQL文も解放済み。
         * 終了処理の結果によらず、元の初期化失敗を返す。
         */
        (void)nk_data_close(data);

        return NK_ERROR_DATABASE;
    }

    *out_data = data;
    return NK_OK;
}

nk_result nk_data_create_schema(nk_data *data) {
    static const char *schema_sql = "BEGIN IMMEDIATE;"

                                    "CREATE TABLE IF NOT EXISTS consent ("
                                    "id INTEGER PRIMARY KEY,"
                                    "version TEXT NOT NULL,"
                                    "measurement INTEGER NOT NULL,"
                                    "microphone INTEGER NOT NULL,"
                                    "consented_ms INTEGER NOT NULL,"
                                    "revoked_ms INTEGER"
                                    ");"

                                    "CREATE TABLE IF NOT EXISTS session ("
                                    "id INTEGER PRIMARY KEY,"
                                    "start_ms INTEGER NOT NULL,"
                                    "end_ms INTEGER,"
                                    "tz_min INTEGER NOT NULL,"
                                    "night_date TEXT,"
                                    "end_reason TEXT,"
                                    "target_sleep_min INTEGER NOT NULL,"
                                    "alarm_ms INTEGER,"
                                    "mic_used INTEGER NOT NULL,"
                                    "light_used INTEGER NOT NULL,"
                                    "placement TEXT,"
                                    "battery_start INTEGER,"
                                    "battery_end INTEGER,"
                                    "app_ver TEXT NOT NULL,"
                                    "algo_ver TEXT NOT NULL,"
                                    "device_model TEXT"
                                    ");"

                                    "CREATE TABLE IF NOT EXISTS epoch ("
                                    "session_id INTEGER NOT NULL,"
                                    "idx INTEGER NOT NULL,"
                                    "acc_cov REAL NOT NULL,"
                                    "mic_cov REAL NOT NULL,"
                                    "move_n INTEGER,"
                                    "move_mg REAL,"
                                    "micro_mg REAL,"
                                    "posture INTEGER,"
                                    "snd_db_mean REAL,"
                                    "snd_db_p95 REAL,"
                                    "snore_s INTEGER,"
                                    "breath_bpm REAL,"
                                    "breath_q REAL,"
                                    "lux REAL,"
                                    "flags INTEGER NOT NULL,"
                                    "PRIMARY KEY (session_id, idx),"
                                    "FOREIGN KEY (session_id) REFERENCES session(id)"
                                    ");"

                                    "CREATE TABLE IF NOT EXISTS event ("
                                    "id INTEGER PRIMARY KEY,"
                                    "session_id INTEGER NOT NULL,"
                                    "t_ms INTEGER NOT NULL,"
                                    "dur_ms INTEGER NOT NULL,"
                                    "type TEXT NOT NULL,"
                                    "value REAL,"
                                    "reason TEXT,"
                                    "FOREIGN KEY (session_id) REFERENCES session(id)"
                                    ");"

                                    "CREATE INDEX IF NOT EXISTS event_session_time "
                                    "ON event(session_id, t_ms);"

                                    "PRAGMA user_version = 1;"
                                    "COMMIT;";

    nk_result result;

    if (data == NULL || data->database == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    result = execute_sql(data, schema_sql);

    if (result != NK_OK) {
        char original_error[NK_DATA_ERROR_SIZE];
        int rollback_result;

        /*
         * 同じサイズの配列へ、元のエラーをそのまま退避する。
         */
        memcpy(original_error, data->last_error, sizeof(original_error));

        /*
         * トランザクションが残っていない場合、
         * ROLLBACKは不要。元のエラーをそのまま返す。
         */
        if (sqlite3_get_autocommit(data->database) != 0) {
            return result;
        }

        rollback_result = sqlite3_exec(data->database, "ROLLBACK;", NULL, NULL, NULL);

        if (rollback_result != SQLITE_OK) {
            /*
             * ROLLBACKも失敗したことと、元のエラーを記録する。
             * 長いメッセージの切り詰めを許容するため、
             * snprintfの文字数は使用しない。
             */
            (void)snprintf(data->last_error, sizeof(data->last_error),
                           "Rollback failed (%s); original: %s", sqlite3_errstr(rollback_result),
                           original_error);
        } else {
            set_error(data, original_error);
        }
    }

    return result;
}

nk_result nk_data_consent_set(nk_data *data, const nk_consent_record *consent) {
    static const char *insert_sql = "INSERT INTO consent ("
                                    "version,"
                                    "measurement,"
                                    "microphone,"
                                    "consented_ms,"
                                    "revoked_ms"
                                    ") VALUES (?, ?, ?, ?, NULL);";

    sqlite3_stmt *statement = NULL;
    int result;

    if (data == NULL || data->database == NULL || consent == NULL || consent->version == NULL ||
        consent->consented_ms < 0) {
        return NK_ERROR_ARGUMENT;
    }

    if ((consent->measurement != 0 && consent->measurement != 1) ||
        (consent->microphone != 0 && consent->microphone != 1)) {
        set_error(data, "Consent values must be 0 or 1");
        return NK_ERROR_ARGUMENT;
    }

    result = sqlite3_prepare_v2(data->database, insert_sql, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_bind_text(statement, 1, consent->version, -1, SQLITE_TRANSIENT);

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 2, consent->measurement);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 3, consent->microphone);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 4, (sqlite3_int64)consent->consented_ms);
    }

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));

        /*
         * 値の設定に失敗した原因を優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);

        return NK_ERROR_DATABASE;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_DONE) {
        set_error(data, sqlite3_errmsg(data->database));

        /*
         * 保存に失敗した原因を優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);

        return NK_ERROR_DATABASE;
    }

    /*
     * 保存成功時は、後片付けの結果も確認する。
     */
    result = sqlite3_finalize(statement);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        return NK_ERROR_DATABASE;
    }

    set_error(data, NULL);

    return NK_OK;
}

nk_result nk_data_has_measurement_consent(nk_data *data, int *out_has_consent) {
    static const char *select_sql = "SELECT measurement "
                                    "FROM consent "
                                    "WHERE revoked_ms IS NULL "
                                    "ORDER BY consented_ms DESC, id DESC "
                                    "LIMIT 1;";

    sqlite3_stmt *statement = NULL;
    int has_consent = 0;
    int result;

    if (data == NULL || data->database == NULL || out_has_consent == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    /*
     * 途中で失敗した場合は「同意なし」のままにする。
     */
    *out_has_consent = 0;

    result = sqlite3_prepare_v2(data->database, select_sql, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_step(statement);

    if (result == SQLITE_ROW) {
        has_consent = sqlite3_column_int(statement, 0) != 0;
    } else if (result != SQLITE_DONE) {
        set_error(data, sqlite3_errmsg(data->database));

        /*
         * 読み取りに失敗した原因を優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);

        return NK_ERROR_DATABASE;
    }

    /*
     * SQLITE_DONEの場合は同意レコードがないため、
     * has_consentを0のままにする。
     *
     * 読み取り成功時は、後片付けの結果も確認する。
     */
    result = sqlite3_finalize(statement);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        return NK_ERROR_DATABASE;
    }

    /*
     * 後片付けまで成功してから結果を返す。
     */
    *out_has_consent = has_consent;

    set_error(data, NULL);

    return NK_OK;
}

nk_result nk_data_session_start(nk_data *data, const nk_session_record *session,
                                int64_t *out_session_id) {
    static const char *insert_sql = "INSERT INTO session ("
                                    "start_ms,"
                                    "tz_min,"
                                    "target_sleep_min,"
                                    "alarm_ms,"
                                    "mic_used,"
                                    "light_used,"
                                    "placement,"
                                    "battery_start,"
                                    "app_ver,"
                                    "algo_ver,"
                                    "device_model"
                                    ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *statement = NULL;
    int result;

    if (data == NULL || data->database == NULL || session == NULL || out_session_id == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (session->app_ver == NULL || session->algo_ver == NULL) {
        set_error(data, "app_ver and algo_ver must not be NULL");
        return NK_ERROR_ARGUMENT;
    }

    *out_session_id = 0;

    result = sqlite3_prepare_v2(data->database, insert_sql, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_bind_int64(statement, 1, (sqlite3_int64)session->start_ms);

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 2, session->tz_min);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 3, session->target_sleep_min);
    }

    if (result == SQLITE_OK) {
        if (session->has_alarm_ms) {
            result = sqlite3_bind_int64(statement, 4, (sqlite3_int64)session->alarm_ms);
        } else {
            result = sqlite3_bind_null(statement, 4);
        }
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 5, session->mic_used);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 6, session->light_used);
    }

    if (result == SQLITE_OK) {
        if (session->placement != NULL) {
            result = sqlite3_bind_text(statement, 7, session->placement, -1, SQLITE_TRANSIENT);
        } else {
            result = sqlite3_bind_null(statement, 7);
        }
    }

    if (result == SQLITE_OK) {
        if (session->has_battery_start) {
            result = sqlite3_bind_int(statement, 8, session->battery_start);
        } else {
            result = sqlite3_bind_null(statement, 8);
        }
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 9, session->app_ver, -1, SQLITE_TRANSIENT);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 10, session->algo_ver, -1, SQLITE_TRANSIENT);
    }

    if (result == SQLITE_OK) {
        if (session->device_model != NULL) {
            result = sqlite3_bind_text(statement, 11, session->device_model, -1, SQLITE_TRANSIENT);
        } else {
            result = sqlite3_bind_null(statement, 11);
        }
    }

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        set_error(data, sqlite3_errmsg(data->database));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_finalize(statement);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        return NK_ERROR_DATABASE;
    }

    /*
     * 後片付けまで成功してから作成したIDを返す。
     */
    *out_session_id = (int64_t)sqlite3_last_insert_rowid(data->database);

    set_error(data, NULL);

    return NK_OK;
}

nk_result nk_data_event_insert(nk_data *data, const nk_event_record *event) {
    static const char *insert_sql = "INSERT INTO event ("
                                    "session_id,"
                                    "t_ms,"
                                    "dur_ms,"
                                    "type,"
                                    "value,"
                                    "reason"
                                    ") VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *statement = NULL;
    const char *type_name;
    int result;

    if (data == NULL || data->database == NULL || event == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    type_name = event_type_name(event->type);
    if (type_name == NULL) {
        set_error(data, "Invalid event type");
        return NK_ERROR_ARGUMENT;
    }

    if (event->session_id <= 0 || event->t_ms < 0 || event->dur_ms < 0) {
        set_error(data, "Invalid event value");
        return NK_ERROR_ARGUMENT;
    }

    result = sqlite3_prepare_v2(data->database, insert_sql, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_bind_int64(statement, 1, (sqlite3_int64)event->session_id);

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 2, (sqlite3_int64)event->t_ms);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 3, (sqlite3_int64)event->dur_ms);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 4, type_name, -1, SQLITE_TRANSIENT);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 5, event->value);
    }

    if (result == SQLITE_OK) {
        if (event->reason != NULL) {
            result = sqlite3_bind_text(statement, 6, event->reason, -1, SQLITE_TRANSIENT);
        } else {
            result = sqlite3_bind_null(statement, 6);
        }
    }

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        set_error(data, sqlite3_errmsg(data->database));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_finalize(statement);
    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        return NK_ERROR_DATABASE;
    }
    set_error(data, NULL);

    return NK_OK;
}

const char *nk_data_last_error(const nk_data *data) {
    if (data == NULL) {
        return "nk_data is NULL";
    }

    return data->last_error;
}

nk_result nk_data_session_finish(nk_data *data, int64_t session_id, int64_t end_ms,
                                 const char *night_date, nk_session_end_reason end_reason,
                                 int has_battery_end, int battery_end) {
    static const char *update_sql = "UPDATE session SET "
                                    "end_ms = ?,"
                                    "night_date = ?,"
                                    "end_reason = ?,"
                                    "battery_end = ? "
                                    "WHERE id = ? AND end_ms IS NULL;";

    sqlite3_stmt *statement = NULL;
    const char *reason_name;
    int result;

    if (data == NULL || data->database == NULL || session_id <= 0 || end_ms < 0 ||
        night_date == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (strlen(night_date) != 10U) {
        set_error(data, "night_date must use YYYY-MM-DD format");
        return NK_ERROR_ARGUMENT;
    }

    if (has_battery_end && (battery_end < 0 || battery_end > 100)) {
        set_error(data, "battery_end must be between 0 and 100");
        return NK_ERROR_ARGUMENT;
    }

    reason_name = session_end_reason_name(end_reason);

    if (reason_name == NULL) {
        set_error(data, "Invalid session end reason");
        return NK_ERROR_ARGUMENT;
    }

    result = sqlite3_prepare_v2(data->database, update_sql, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_bind_int64(statement, 1, (sqlite3_int64)end_ms);

    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 2, night_date, -1, SQLITE_TRANSIENT);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_text(statement, 3, reason_name, -1, SQLITE_TRANSIENT);
    }

    if (result == SQLITE_OK) {
        if (has_battery_end) {
            result = sqlite3_bind_int(statement, 4, battery_end);
        } else {
            result = sqlite3_bind_null(statement, 4);
        }
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int64(statement, 5, (sqlite3_int64)session_id);
    }

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        set_error(data, sqlite3_errmsg(data->database));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    if (sqlite3_changes(data->database) != 1) {
        set_error(data, "Session was not found or was already finished");
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_finalize(statement);
    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        return NK_ERROR_DATABASE;
    }
    set_error(data, NULL);

    return NK_OK;
}

nk_result nk_data_epoch_insert(nk_data *data, const nk_epoch_record *epoch) {
    static const char *insert_sql = "INSERT INTO epoch ("
                                    "session_id,"
                                    "idx,"
                                    "acc_cov,"
                                    "mic_cov,"
                                    "move_n,"
                                    "move_mg,"
                                    "micro_mg,"
                                    "posture,"
                                    "snd_db_mean,"
                                    "snd_db_p95,"
                                    "snore_s,"
                                    "breath_bpm,"
                                    "breath_q,"
                                    "lux,"
                                    "flags"
                                    ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *statement = NULL;
    int result;

    if (data == NULL || data->database == NULL || epoch == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (epoch->session_id <= 0 || epoch->idx < 0 || epoch->acc_cov < 0.0 || epoch->acc_cov > 1.0 ||
        epoch->mic_cov < 0.0 || epoch->mic_cov > 1.0) {
        set_error(data, "Invalid epoch value");
        return NK_ERROR_ARGUMENT;
    }

    result = sqlite3_prepare_v2(data->database, insert_sql, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errmsg(data->database));
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_bind_int64(statement, 1, (sqlite3_int64)epoch->session_id);

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 2, epoch->idx);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_double(statement, 3, epoch->acc_cov);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_double(statement, 4, epoch->mic_cov);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_int(statement, 5, epoch->move_n);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 6, epoch->move_mg);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 7, epoch->micro_mg);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_int(statement, 8, epoch->posture);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 9, epoch->snd_db_mean);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 10, epoch->snd_db_p95);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_int(statement, 11, epoch->snore_s);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 12, epoch->breath_bpm);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 13, epoch->breath_q);
    }

    if (result == SQLITE_OK) {
        result = bind_optional_double(statement, 14, epoch->lux);
    }

    if (result == SQLITE_OK) {
        result = sqlite3_bind_int(statement, 15, (int)epoch->flags);
    }

    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        set_error(data, sqlite3_errmsg(data->database));
        /*
         * 元のエラーを優先するため、
         * 後片付けの結果では上書きしない。
         */
        (void)sqlite3_finalize(statement);
        return NK_ERROR_DATABASE;
    }

    result = sqlite3_finalize(statement);
    if (result != SQLITE_OK) {
        set_error(data, sqlite3_errstr(result));
        return NK_ERROR_DATABASE;
    }
    set_error(data, NULL);

    return NK_OK;
}

nk_result nk_data_close(nk_data *data) {
    int result;

    if (data == NULL) {
        return NK_OK;
    }

    if (data->database != NULL) {
        result = sqlite3_close(data->database);

        if (result != SQLITE_OK) {
            /*
             * SQL文が未解放などの理由で閉じられなかった場合、
             * DBと管理オブジェクトを残して呼び出し側へ返す。
             */
            set_error(data, sqlite3_errmsg(data->database));
            return NK_ERROR_DATABASE;
        }

        data->database = NULL;
    }

    /*
     * DB鍵そのものはこの構造体に保存していない。
     * 呼び出し側も鍵を使用後に消去すること。
     */
    free(data);

    return NK_OK;
}
