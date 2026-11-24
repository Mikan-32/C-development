#include "session.h"

#include <sqlcipher/sqlite3.h>
#include <stdint.h>
#include <stdio.h>

static void remove_test_files(void) {
    (void)remove("nk_session_test.db");
    (void)remove("nk_session_test.db-wal");
    (void)remove("nk_session_test.db-shm");
}

static int verify_gap_closed_at_finish(int64_t session_id, int64_t start_ms, int64_t end_ms) {
    static const char *query = "SELECT "
                               "count(*),"
                               "sum(CASE WHEN type = 'GAP_START' "
                               "AND reason = 'Q04' AND t_ms = ? "
                               "AND dur_ms = 0 THEN 1 ELSE 0 END),"
                               "sum(CASE WHEN type = 'GAP_END' "
                               "AND reason = 'Q04' AND t_ms = ? "
                               "AND dur_ms = ? THEN 1 ELSE 0 END) "
                               "FROM event "
                               "WHERE session_id = ? "
                               "AND type IN ('GAP_START', 'GAP_END');";

    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    int passed = 0;
    int result;

    result = sqlite3_open_v2("nk_session_test.db", &database, SQLITE_OPEN_READONLY, NULL);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to open verification DB\n");
        goto cleanup;
    }

    /*
     * テスト専用の鍵で復号する。
     */
    result = sqlite3_exec(database,
                          "PRAGMA key = "
                          "'0123456789abcdef0123456789abcdef';",
                          NULL, NULL, NULL);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to set verification key\n");
        goto cleanup;
    }

    result = sqlite3_prepare_v2(database, query, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare GAP query: %s\n", sqlite3_errmsg(database));
        goto cleanup;
    }

    if (sqlite3_bind_int64(statement, 1, (sqlite3_int64)start_ms) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 2, (sqlite3_int64)end_ms) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)(end_ms - start_ms)) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 4, (sqlite3_int64)session_id) != SQLITE_OK) {
        fprintf(stderr, "Failed to bind GAP query values\n");
        goto cleanup;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_ROW) {
        fprintf(stderr, "Failed to read GAP events: %s\n", sqlite3_errmsg(database));
        goto cleanup;
    }

    /*
     * GAPイベントは合計2件。
     * 開始・終了の時刻、時間、理由がそれぞれ一致する。
     */
    if (sqlite3_column_int(statement, 0) != 2 || sqlite3_column_int(statement, 1) != 1 ||
        sqlite3_column_int(statement, 2) != 1) {
        fprintf(stderr, "GAP events did not match expected values\n");
        goto cleanup;
    }

    printf("Verified GAP_END saved at session finish\n");
    printf("Verified finished GAP duration: %lld ms\n", (long long)(end_ms - start_ms));

    passed = 1;

cleanup:
    result = sqlite3_finalize(statement);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to finalize GAP query: %s\n", sqlite3_errstr(result));
        passed = 0;
    }

    if (database != NULL) {
        result = sqlite3_close(database);

        if (result != SQLITE_OK) {
            fprintf(stderr, "Failed to close GAP verification DB: %s\n", sqlite3_errmsg(database));
            passed = 0;
        }
    }

    return passed;
}

int main(void) {
    static const unsigned char key[] = "0123456789abcdef0123456789abcdef";

    nk_data *data = NULL;
    nk_session *session = NULL;

    nk_result data_result;
    nk_result session_result;

    nk_consent_record consent = {0};
    nk_session_record record = {0};

    nk_epoch_record epoch = {0};
    nk_event_record event = {0};

    nk_epoch_record first_gap_epoch = {0};
    nk_epoch_record second_gap_epoch = {0};
    nk_epoch_record recovered_epoch = {0};

    int64_t session_id;
    int exit_code = 1;

    remove_test_files();

    /*
     * 暗号化DBを作成する。
     */
    data_result = nk_data_open(&data, "nk_session_test.db", key, sizeof(key) - 1);

    if (data_result != NK_OK) {
        fprintf(stderr, "Failed to open session test DB\n");
        goto cleanup;
    }

    data_result = nk_data_create_schema(data);
    if (data_result != NK_OK) {
        fprintf(stderr, "Failed to create schema: %s\n", nk_data_last_error(data));
        goto cleanup;
    }

    /*
     * session管理オブジェクトを作成する。
     */
    session_result = nk_session_create(&session, data);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to create session manager\n");
        goto cleanup;
    }

    /*
     * テスト用の計測開始情報。
     */
    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;

    record.has_alarm_ms = 1;
    record.alarm_ms = INT64_C(1794355200000);

    record.mic_used = 1;
    record.light_used = 1;
    record.placement = "OK";

    record.has_battery_start = 1;
    record.battery_start = 95;

    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC session test";

    /*
     * 同意を保存する前は計測を開始できないことを確認する。
     */
    session_result = nk_session_start(session, &record);

    if (session_result != NK_ERROR_CONSENT_REQUIRED) {
        fprintf(stderr, "Session started without measurement consent\n");
        goto cleanup;
    }

    if (nk_session_is_running(session) || nk_session_id(session) != 0) {
        fprintf(stderr, "Session became active without consent\n");
        goto cleanup;
    }

    printf("Verified start rejected without consent\n");

    /*
     * 計測同意を保存する。
     */
    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 1;
    consent.consented_ms = record.start_ms - INT64_C(100000);

    data_result = nk_data_consent_set(data, &consent);

    if (data_result != NK_OK) {
        fprintf(stderr, "Failed to save consent: %s\n", nk_data_last_error(data));
        goto cleanup;
    }

    /*
     * 同意保存後は計測を開始できることを確認する。
     */
    session_result = nk_session_start(session, &record);

    if (session_result != NK_OK) {
        fprintf(stderr, "Session did not start after consent\n");
        goto cleanup;
    }

    session_id = nk_session_id(session);

    if (!nk_session_is_running(session) || session_id <= 0) {
        fprintf(stderr, "Session did not enter running state\n");
        goto cleanup;
    }

    printf("Verified session started with consent: %lld\n", (long long)session_id);

    /*
     * 計測中にもう一度開始できないことを確認する。
     */
    session_result = nk_session_start(session, &record);

    if (session_result != NK_ERROR_ALREADY_RUNNING) {
        fprintf(stderr, "Running session was started twice\n");
        goto cleanup;
    }

    printf("Verified duplicate start rejected\n");

    /*
     * session経由でepochを保存する。
     *
     * session_idには意図的に0を指定する。
     * nk_session_save_epoch()が現在のsession IDへ
     * 自動的に置き換えることを確認する。
     */
    epoch.session_id = 0;
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

    session_result = nk_session_save_epoch(session, &epoch);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to save epoch through session\n");
        goto cleanup;
    }

    printf("Verified epoch saved through session\n");

    /*
     * session経由でTURNイベントを保存する。
     *
     * event.session_idも意図的に0を指定する。
     */
    event.session_id = 0;
    event.t_ms = record.start_ms + INT64_C(30000);
    event.dur_ms = 0;
    event.type = NK_EVENT_TURN;

    event.value.has_value = 1;
    event.value.value = 35.0;

    event.reason = NULL;

    session_result = nk_session_save_event(session, &event);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to save event through session\n");
        goto cleanup;
    }

    printf("Verified event saved through session\n");

    /*
     * 1件目の欠損epoch。
     *
     * 1エポックだけではGAPイベントを作らない。
     */
    first_gap_epoch.session_id = 0;
    first_gap_epoch.idx = 1;

    first_gap_epoch.acc_cov = 0.50;
    first_gap_epoch.mic_cov = 0.95;

    first_gap_epoch.flags = NK_EPF_GAP_ACC;

    session_result = nk_session_save_epoch(session, &first_gap_epoch);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to save first gap epoch\n");
        goto cleanup;
    }

    printf("Saved first gap epoch: no event yet\n");

    /*
     * 2件目の連続した欠損epoch。
     *
     * 2エポック連続したため、
     * GAP_STARTが自動保存される。
     */
    second_gap_epoch.session_id = 0;
    second_gap_epoch.idx = 2;

    second_gap_epoch.acc_cov = 0.40;
    second_gap_epoch.mic_cov = 0.95;

    second_gap_epoch.flags = NK_EPF_GAP_ACC;

    session_result = nk_session_save_epoch(session, &second_gap_epoch);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to save second gap epoch\n");
        goto cleanup;
    }

    printf("Saved second gap epoch: GAP_START created\n");

    /*
     * 正常なepochへ復帰する。
     *
     * 復帰時点でGAP_ENDが自動保存される。
     */
    recovered_epoch.session_id = 0;
    recovered_epoch.idx = 3;

    recovered_epoch.acc_cov = 0.98;
    recovered_epoch.mic_cov = 0.95;

    recovered_epoch.move_n.has_value = 1;
    recovered_epoch.move_n.value = 1;

    recovered_epoch.move_mg.has_value = 1;
    recovered_epoch.move_mg.value = 4.5;

    recovered_epoch.flags = 0;

    session_result = nk_session_save_epoch(session, &recovered_epoch);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to save recovered epoch\n");
        goto cleanup;
    }

    printf("Saved recovered epoch: GAP_END created\n");

    /*
     * 計測を終了する。
     */
    session_result = nk_session_finish(session, record.start_ms + INT64_C(28800000), "2026-11-11",
                                       NK_SESSION_END_USER_STOP, 1, 72);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to finish session\n");
        goto cleanup;
    }

    if (nk_session_is_running(session) || nk_session_id(session) != 0) {
        fprintf(stderr, "Session remained active after finish\n");
        goto cleanup;
    }

    printf("Verified session finished\n");

    /*
     * 計測終了後はepochを保存できないことを確認する。
     */
    epoch.idx = 1;

    session_result = nk_session_save_epoch(session, &epoch);

    if (session_result != NK_ERROR_NOT_RUNNING) {
        fprintf(stderr, "Epoch was saved after session finished\n");
        goto cleanup;
    }

    printf("Verified epoch rejected after finish\n");

    /*
     * 計測終了後はeventも保存できないことを確認する。
     */
    session_result = nk_session_save_event(session, &event);

    if (session_result != NK_ERROR_NOT_RUNNING) {
        fprintf(stderr, "Event was saved after session finished\n");
        goto cleanup;
    }

    printf("Verified event rejected after finish\n");

    /*
     * 終了後にもう一度終了できないことを確認する。
     */
    session_result = nk_session_finish(session, record.start_ms + INT64_C(28800000), "2026-11-11",
                                       NK_SESSION_END_USER_STOP, 1, 72);

    if (session_result != NK_ERROR_NOT_RUNNING) {
        fprintf(stderr, "Inactive session was finished twice\n");
        goto cleanup;
    }

    printf("Verified duplicate finish rejected\n");

    /*
     * 2回目の計測を開始する。
     */
    session_result = nk_session_start(session, &record);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to start second session\n");
        goto cleanup;
    }

    session_id = nk_session_id(session);

    /*
     * idx=0から加速度欠損。
     */
    first_gap_epoch = (nk_epoch_record){0};

    first_gap_epoch.idx = 0;
    first_gap_epoch.acc_cov = 0.30;
    first_gap_epoch.mic_cov = 0.95;
    first_gap_epoch.flags = NK_EPF_GAP_ACC;

    session_result = nk_session_save_epoch(session, &first_gap_epoch);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to save gap epoch 0\n");
        goto cleanup;
    }

    /*
     * idx=1も欠損なのでGAP_STARTが作成される。
     */
    second_gap_epoch = first_gap_epoch;
    second_gap_epoch.idx = 1;

    session_result = nk_session_save_epoch(session, &second_gap_epoch);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to save gap epoch 1\n");
        goto cleanup;
    }

    /*
     * 正常epochへ戻らず、開始90秒後に終了する。
     */
    session_result = nk_session_finish(session, record.start_ms + INT64_C(90000), "2026-11-11",
                                       NK_SESSION_END_USER_STOP, 1, 94);

    if (session_result != NK_OK) {
        fprintf(stderr, "Failed to finish during gap\n");
        goto cleanup;
    }

    /*
     * 別のDB接続から実際の保存結果を確認する。
     */
    if (!verify_gap_closed_at_finish(session_id, record.start_ms,
                                     record.start_ms + INT64_C(90000))) {
        goto cleanup;
    }

    exit_code = 0;

cleanup:
    nk_session_destroy(session);

    data_result = nk_data_close(data);

    if (data_result != NK_OK) {
        fprintf(stderr, "Failed to close session test DB: %s\n", nk_data_last_error(data));

        /*
         * 閉じられなかったDBは削除せず、
         * テスト失敗として終了する。
         */
        return 1;
    }

    data = NULL;
    remove_test_files();

    if (exit_code == 0) {
        printf("Session consent test passed\n");
    }

    return exit_code;
}
