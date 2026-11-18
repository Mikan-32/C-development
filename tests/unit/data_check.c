#include "data.h"

#include <sqlcipher/sqlite3.h>
#include <stdint.h>
#include <stdio.h>

static void remove_test_files(void) {
    /* テスト用ファイルが存在しない場合もあるため、削除結果は無視する。 */
    (void)remove("nk_data_test.db");
    (void)remove("nk_data_test.db-wal");
    (void)remove("nk_data_test.db-shm");
}

static int close_data_checked(nk_data *data) {
    nk_result result = nk_data_close(data);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to close test DB: %s\n", nk_data_last_error(data));
        return 0;
    }

    return 1;
}

static int close_sqlite_checked(sqlite3 *database) {
    int result;

    if (database == NULL) {
        return 1;
    }

    result = sqlite3_close(database);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to close verification DB: %s\n", sqlite3_errmsg(database));
        return 0;
    }

    return 1;
}

static int expect_query_count(sqlite3 *database, const char *sql, int expected,
                              const char *description) {
    sqlite3_stmt *statement = NULL;
    int actual;
    int result;

    result = sqlite3_prepare_v2(database, sql, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare %s query: %s\n", description, sqlite3_errmsg(database));
        return 0;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_ROW) {
        fprintf(stderr, "Failed to execute %s query: %s\n", description, sqlite3_errmsg(database));

        result = sqlite3_finalize(statement);

        if (result != SQLITE_OK) {
            fprintf(stderr, "Failed to finalize verification query: %s\n", sqlite3_errstr(result));
        }

        return 0;
    }

    actual = sqlite3_column_int(statement, 0);

    result = sqlite3_finalize(statement);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to finalize verification query: %s\n", sqlite3_errstr(result));
        return 0;
    }

    if (actual != expected) {
        fprintf(stderr, "%s count was %d, expected %d\n", description, actual, expected);
        return 0;
    }

    printf("Verified %s: %d\n", description, actual);

    return 1;
}

static int open_verification_database(sqlite3 **out_database) {
    static const char *key_sql = "PRAGMA key = "
                                 "'0123456789abcdef0123456789abcdef';";

    sqlite3 *database = NULL;
    char *error_message = NULL;
    int result;

    *out_database = NULL;

    result = sqlite3_open("nk_data_test.db", &database);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to open verification database\n");

        /*
         * 元の失敗は報告済み。
         * 終了失敗も補助関数で報告し、戻り値は失敗のままにする。
         */
        (void)close_sqlite_checked(database);

        return 0;
    }

    result = sqlite3_exec(database, key_sql, NULL, NULL, &error_message);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to set verification key: %s\n",
                error_message != NULL ? error_message : sqlite3_errmsg(database));

        sqlite3_free(error_message);

        /*
         * 元の失敗は報告済み。
         * 終了失敗も補助関数で報告し、戻り値は失敗のままにする。
         */
        (void)close_sqlite_checked(database);

        return 0;
    }

    *out_database = database;

    return 1;
}

int main(void) {
    /*
     * テスト専用の鍵。
     * 実際のユーザーデータには使用しない。
     */
    static const unsigned char correct_key[] = "0123456789abcdef0123456789abcdef";

    static const unsigned char wrong_key[] = "abcdef0123456789abcdef0123456789";

    nk_data *data = NULL;
    sqlite3 *verification_database = NULL;
    nk_result result;

    nk_session_record session = {0};
    int64_t session_id = 0;

    nk_consent_record consent = {0};
    int has_measurement_consent = 0;

    nk_epoch_record epoch = {0};
    nk_epoch_record missing_epoch = {0};

    nk_event_record turn_event = {0};
    nk_event_record gap_start_event = {0};
    nk_event_record gap_end_event = {0};

    remove_test_files();

    /*
     * 新しい暗号化データベースを作成する。
     */
    result = nk_data_open(&data, "nk_data_test.db", correct_key, sizeof(correct_key) - 1);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to create encrypted database\n");
        remove_test_files();
        return 1;
    }

    /*
     * AI-1のテーブルを作成する。
     */
    result = nk_data_create_schema(data);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to create schema: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    /*
     * 同意情報がない場合は、計測同意なしになる。
     */
    result = nk_data_has_measurement_consent(data, &has_measurement_consent);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to check initial consent: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (has_measurement_consent != 0) {
        fprintf(stderr, "Measurement consent existed before it was saved\n");

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Verified initial measurement consent: none\n");

    /*
     * 計測とマイクの同意を保存する。
     */
    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 1;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(data, &consent);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to save consent: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    /*
     * 保存後は計測同意ありになる。
     */
    result = nk_data_has_measurement_consent(data, &has_measurement_consent);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to check saved consent: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (has_measurement_consent != 1) {
        fprintf(stderr, "Saved measurement consent was not found\n");

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Verified saved measurement consent: allowed\n");

    /*
     * 計測開始情報を用意する。
     */
    session.start_ms = INT64_C(1794326400000);
    session.tz_min = 540;
    session.target_sleep_min = 420;

    session.has_alarm_ms = 1;
    session.alarm_ms = INT64_C(1794355200000);

    session.mic_used = 1;
    session.light_used = 1;
    session.placement = "OK";

    session.has_battery_start = 1;
    session.battery_start = 95;

    session.app_ver = "0.1.0";
    session.algo_ver = "AI-1-test";
    session.device_model = "PC replay test";

    result = nk_data_session_start(data, &session, &session_id);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to save session: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (session_id <= 0) {
        fprintf(stderr, "Invalid session ID: %lld\n", (long long)session_id);

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Saved session ID: %lld\n", (long long)session_id);

    /*
     * 1件目のepoch。
     */
    epoch.session_id = session_id;
    epoch.idx = 0;
    epoch.acc_cov = 0.98;
    epoch.mic_cov = 0.95;

    epoch.move_n.has_value = 1;
    epoch.move_n.value = 3;

    epoch.move_mg.has_value = 1;
    epoch.move_mg.value = 12.5;

    epoch.micro_mg.has_value = 1;
    epoch.micro_mg.value = 0.8;

    epoch.posture.has_value = 1;
    epoch.posture.value = 0;

    epoch.snd_db_mean.has_value = 1;
    epoch.snd_db_mean.value = -42.0;

    epoch.snd_db_p95.has_value = 1;
    epoch.snd_db_p95.value = -30.0;

    epoch.snore_s.has_value = 1;
    epoch.snore_s.value = 4;

    epoch.breath_bpm.has_value = 1;
    epoch.breath_bpm.value = 15.5;

    epoch.breath_q.has_value = 1;
    epoch.breath_q.value = 0.82;

    epoch.lux.has_value = 1;
    epoch.lux.value = 1.5;

    epoch.flags = NK_EPF_CHARGING;

    result = nk_data_epoch_insert(data, &epoch);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to save epoch 0: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Saved epoch index: %d\n", epoch.idx);

    /*
     * 2件目のepoch。
     * has_valueが0の項目はNULLとして保存する。
     */
    missing_epoch.session_id = session_id;
    missing_epoch.idx = 1;
    missing_epoch.acc_cov = 0.90;
    missing_epoch.mic_cov = 0.0;

    missing_epoch.move_n.has_value = 1;
    missing_epoch.move_n.value = 1;

    missing_epoch.move_mg.has_value = 1;
    missing_epoch.move_mg.value = 6.2;

    missing_epoch.flags = NK_EPF_GAP_MIC;

    result = nk_data_epoch_insert(data, &missing_epoch);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to save epoch 1: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Saved epoch index: %d\n", missing_epoch.idx);

    /*
     * TURNイベント。
     */
    turn_event.session_id = session_id;
    turn_event.t_ms = session.start_ms + INT64_C(30000);
    turn_event.dur_ms = 0;
    turn_event.type = NK_EVENT_TURN;
    turn_event.value.has_value = 1;
    turn_event.value.value = 35.0;
    turn_event.reason = NULL;

    result = nk_data_event_insert(data, &turn_event);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to save TURN event: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Saved event: TURN\n");

    /*
     * GAP_STARTイベント。
     */
    gap_start_event.session_id = session_id;
    gap_start_event.t_ms = session.start_ms + INT64_C(60000);
    gap_start_event.dur_ms = 0;
    gap_start_event.type = NK_EVENT_GAP_START;
    gap_start_event.value.has_value = 0;
    gap_start_event.reason = "Q04";

    result = nk_data_event_insert(data, &gap_start_event);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to save GAP_START event: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Saved event: GAP_START reason=Q04\n");

    /*
     * GAP_ENDイベント。欠損時間は60秒。
     */
    gap_end_event.session_id = session_id;
    gap_end_event.t_ms = session.start_ms + INT64_C(120000);
    gap_end_event.dur_ms = INT64_C(60000);
    gap_end_event.type = NK_EVENT_GAP_END;
    gap_end_event.value.has_value = 0;
    gap_end_event.reason = "Q04";

    result = nk_data_event_insert(data, &gap_end_event);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to save GAP_END event: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Saved event: GAP_END reason=Q04\n");

    /*
     * 8時間後に計測を終了する。
     */
    result = nk_data_session_finish(data, session_id, session.start_ms + INT64_C(28800000),
                                    "2026-11-11", NK_SESSION_END_USER_STOP, 1, 72);

    if (result != NK_OK) {
        fprintf(stderr, "Failed to finish session: %s\n", nk_data_last_error(data));

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    printf("Finished session ID: %lld reason=USER_STOP\n", (long long)session_id);

    if (!close_data_checked(data)) {
        return 1;
    }

    data = NULL;

    /*
     * 間違った鍵では開けないことを確認する。
     */
    result = nk_data_open(&data, "nk_data_test.db", wrong_key, sizeof(wrong_key) - 1);

    if (result == NK_OK) {
        fprintf(stderr, "Database opened with the wrong key\n");

        if (!close_data_checked(data)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    data = NULL;

    /*
     * 正しい鍵なら再度開けることを確認する。
     */
    result = nk_data_open(&data, "nk_data_test.db", correct_key, sizeof(correct_key) - 1);

    if (result != NK_OK) {
        fprintf(stderr, "Database could not be reopened with the correct key\n");

        remove_test_files();
        return 1;
    }

    if (!close_data_checked(data)) {
        return 1;
    }

    data = NULL;

    /*
     * 別の接続から保存内容を読み返す。
     */
    if (!open_verification_database(&verification_database)) {
        remove_test_files();
        return 1;
    }

    if (!expect_query_count(verification_database,
                            "SELECT count(*) FROM consent "
                            "WHERE measurement = 1 "
                            "AND microphone = 1 "
                            "AND revoked_ms IS NULL;",
                            1, "active consent")) {
        if (!close_sqlite_checked(verification_database)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (!expect_query_count(verification_database, "SELECT count(*) FROM session;", 1,
                            "session rows")) {
        if (!close_sqlite_checked(verification_database)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (!expect_query_count(verification_database,
                            "SELECT count(*) FROM session "
                            "WHERE end_reason = 'USER_STOP' "
                            "AND night_date = '2026-11-11' "
                            "AND battery_end = 72 "
                            "AND end_ms IS NOT NULL;",
                            1, "finished session")) {
        if (!close_sqlite_checked(verification_database)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (!expect_query_count(verification_database, "SELECT count(*) FROM epoch;", 2,
                            "epoch rows")) {
        if (!close_sqlite_checked(verification_database)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (!expect_query_count(verification_database,
                            "SELECT count(*) FROM epoch "
                            "WHERE idx = 1 "
                            "AND snd_db_mean IS NULL "
                            "AND snd_db_p95 IS NULL "
                            "AND breath_bpm IS NULL "
                            "AND breath_q IS NULL "
                            "AND lux IS NULL "
                            "AND (flags & 2) != 0;",
                            1, "epoch NULL values")) {
        if (!close_sqlite_checked(verification_database)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (!expect_query_count(verification_database, "SELECT count(*) FROM event;", 3,
                            "event rows")) {
        if (!close_sqlite_checked(verification_database)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (!expect_query_count(verification_database,
                            "SELECT count(*) FROM event "
                            "WHERE type IN ('GAP_START', 'GAP_END') "
                            "AND reason = 'Q04';",
                            2, "Q04 gap events")) {
        if (!close_sqlite_checked(verification_database)) {
            return 1;
        }

        remove_test_files();
        return 1;
    }

    if (!close_sqlite_checked(verification_database)) {
        return 1;
    }

    /* 中身を確認するため、テスト成功時はDBを残す。 */
    // remove_test_files();

    printf("Encrypted database storage verification passed\n");

    return 0;
}
