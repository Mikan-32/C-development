#include "session.h"
#include "unity.h"

#include <sqlcipher/sqlite3.h>
#include <stdint.h>
#include <stdio.h>

/*
 * テスト専用の鍵。
 * 実際のユーザーデータには使用しない。
 */
static const unsigned char test_key[] = "0123456789abcdef0123456789abcdef";

static nk_data *test_data = NULL;
static nk_session *test_session = NULL;

void setUp(void);
void tearDown(void);

/*
 * このテスト専用のDBを削除する。
 */
static void remove_unity_test_files(void) {
    /*
     * ファイルが存在しない場合もあるため、
     * remove()の戻り値はここでは無視する。
     */
    (void)remove("nk_session_unity_test.db");
    (void)remove("nk_session_unity_test.db-wal");
    (void)remove("nk_session_unity_test.db-shm");
}

/*
 * 各テストの実行前に新しいDBを用意する。
 */
void setUp(void) {
    nk_result result;

    test_data = NULL;
    test_session = NULL;

    remove_unity_test_files();

    result = nk_data_open(&test_data, "nk_session_unity_test.db", test_key, sizeof(test_key) - 1);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    result = nk_data_create_schema(test_data);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    result = nk_session_create(&test_session, test_data);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);
}

/*
 * 各テストの実行後にDBを閉じて削除する。
 */
void tearDown(void) {
    nk_result result;

    nk_session_destroy(test_session);
    test_session = NULL;

    result = nk_data_close(test_data);

    if (result != NK_OK) {
        /*
         * 閉じる処理に失敗した場合は、
         * DBを残してUnityテストを失敗にする。
         */
        TEST_FAIL_MESSAGE(nk_data_last_error(test_data));
        return;
    }

    test_data = NULL;

    remove_unity_test_files();
}

/*
 * 指定したsessionのGAPイベント数を読み返す。
 * 読み取りに失敗した場合は-1を返す。
 */
static int count_gap_events(int64_t session_id) {
    static const char *query = "SELECT count(*) FROM event "
                               "WHERE session_id = ? "
                               "AND type IN ('GAP_START', 'GAP_END');";

    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    int count = -1;
    int result;

    result = sqlite3_open_v2("nk_session_unity_test.db", &database, SQLITE_OPEN_READONLY, NULL);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    /*
     * テスト専用の鍵を設定する。
     */
    result = sqlite3_exec(database,
                          "PRAGMA key = "
                          "'0123456789abcdef0123456789abcdef';",
                          NULL, NULL, NULL);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_prepare_v2(database, query, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_bind_int64(statement, 1, (sqlite3_int64)session_id);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_ROW) {
        goto cleanup;
    }

    count = sqlite3_column_int(statement, 0);

cleanup:
    if (count < 0 && database != NULL) {
        fprintf(stderr, "Failed to count GAP events: %s\n", sqlite3_errmsg(database));
    }

    if (statement != NULL) {
        result = sqlite3_finalize(statement);
        if (result != SQLITE_OK) {
            count = -1;
        }
    }

    if (database != NULL) {
        result = sqlite3_close(database);
        if (result != SQLITE_OK) {
            count = -1;
        }
    }

    return count;
}

/*
 * Q04のGAP開始・終了が、期待した値で保存されているか確認する。
 *
 * 一致すれば1、不一致またはDBエラーなら0を返す。
 */
static int gap_pair_matches(int64_t session_id, int64_t start_ms, int64_t end_ms) {
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
    int matches = 0;
    int result;

    result = sqlite3_open_v2("nk_session_unity_test.db", &database, SQLITE_OPEN_READONLY, NULL);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_exec(database,
                          "PRAGMA key = "
                          "'0123456789abcdef0123456789abcdef';",
                          NULL, NULL, NULL);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_prepare_v2(database, query, -1, &statement, NULL);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_bind_int64(statement, 1, (sqlite3_int64)start_ms);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_bind_int64(statement, 2, (sqlite3_int64)end_ms);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_bind_int64(statement, 3, (sqlite3_int64)(end_ms - start_ms));

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_bind_int64(statement, 4, (sqlite3_int64)session_id);

    if (result != SQLITE_OK) {
        goto cleanup;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_ROW) {
        goto cleanup;
    }

    matches = sqlite3_column_int(statement, 0) == 2 && sqlite3_column_int(statement, 1) == 1 &&
              sqlite3_column_int(statement, 2) == 1;

cleanup:
    if (result != SQLITE_ROW && database != NULL) {
        fprintf(stderr, "Failed to verify GAP pair: %s\n", sqlite3_errmsg(database));
    }

    if (statement != NULL) {
        result = sqlite3_finalize(statement);
        if (result != SQLITE_OK) {
            matches = 0;
        }
    }

    if (database != NULL) {
        result = sqlite3_close(database);
        if (result != SQLITE_OK) {
            matches = 0;
        }
    }

    return matches;
}

/*
 * 同意情報がなければsession開始を拒否する。
 */
static void test_start_without_consent_is_rejected(void) {
    nk_session_record record = {0};
    nk_result result;

    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;

    record.mic_used = 0;
    record.light_used = 0;
    record.placement = "SKIP";

    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_CONSENT_REQUIRED, result);

    TEST_ASSERT_EQUAL_INT(0, nk_session_is_running(test_session));

    TEST_ASSERT_TRUE(nk_session_id(test_session) == 0);
}

/*
 * 同意情報があればsessionを開始できる。
 */
static void test_start_with_consent_succeeds(void) {
    nk_consent_record consent = {0};
    nk_session_record record = {0};
    nk_result result;

    /*
     * 計測への同意を保存する。
     * 今回はマイクを使わない。
     */
    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 0;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(test_data, &consent);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 計測開始情報を用意する。
     */
    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;

    record.mic_used = 0;
    record.light_used = 0;
    record.placement = "SKIP";

    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(1, nk_session_is_running(test_session));

    TEST_ASSERT_TRUE(nk_session_id(test_session) > 0);

    /*
     * 開始したsessionを終了する。
     */
    result = nk_session_finish(test_session, record.start_ms + INT64_C(60000), "2026-11-10",
                               NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(0, nk_session_is_running(test_session));

    TEST_ASSERT_TRUE(nk_session_id(test_session) == 0);
}

/*
 * 計測中の二重開始を拒否する。
 */
static void test_duplicate_start_is_rejected(void) {
    nk_consent_record consent = {0};
    nk_session_record record = {0};
    nk_result result;
    int64_t original_session_id;

    /*
     * 計測同意を保存する。
     */
    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 0;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(test_data, &consent);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 1回目の開始。
     */
    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;
    record.placement = "SKIP";
    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    original_session_id = nk_session_id(test_session);

    TEST_ASSERT_TRUE(original_session_id > 0);

    /*
     * 計測中に2回目の開始を要求する。
     */
    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_ALREADY_RUNNING, result);

    /*
     * 元のsessionが維持されていることを確認する。
     */
    TEST_ASSERT_EQUAL_INT(1, nk_session_is_running(test_session));

    TEST_ASSERT_TRUE(nk_session_id(test_session) == original_session_id);

    /*
     * 元のsessionを正常に終了できることも確認する。
     */
    result = nk_session_finish(test_session, record.start_ms + INT64_C(60000), "2026-11-10",
                               NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);
}

/*
 * 終了済みsessionの二重終了を拒否する。
 */
static void test_duplicate_finish_is_rejected(void) {
    nk_consent_record consent = {0};
    nk_session_record record = {0};
    nk_result result;

    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 0;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(test_data, &consent);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;
    record.placement = "SKIP";
    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 1回目の終了は成功する。
     */
    result = nk_session_finish(test_session, record.start_ms + INT64_C(60000), "2026-11-10",
                               NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 2回目の終了は拒否される。
     */
    result = nk_session_finish(test_session, record.start_ms + INT64_C(60000), "2026-11-10",
                               NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_NOT_RUNNING, result);

    TEST_ASSERT_EQUAL_INT(0, nk_session_is_running(test_session));

    TEST_ASSERT_TRUE(nk_session_id(test_session) == 0);
}

/*
 * 計測終了後はepoch・eventを保存できない。
 */
static void test_save_after_finish_is_rejected(void) {
    nk_consent_record consent = {0};
    nk_session_record record = {0};
    nk_epoch_record epoch = {0};
    nk_event_record event = {0};
    nk_result result;

    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 0;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(test_data, &consent);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;
    record.placement = "SKIP";
    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 計測を終了する。
     */
    result = nk_session_finish(test_session, record.start_ms + INT64_C(60000), "2026-11-10",
                               NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 有効なepochを用意しても、終了後は拒否される。
     */
    epoch.idx = 0;
    epoch.acc_cov = 0.98;
    epoch.mic_cov = 0.0;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_NOT_RUNNING, result);

    /*
     * 有効なeventを用意しても、終了後は拒否される。
     */
    event.t_ms = record.start_ms + INT64_C(30000);
    event.dur_ms = 0;
    event.type = NK_EVENT_TURN;
    event.value.has_value = 1;
    event.value.value = 35.0;

    result = nk_session_save_event(test_session, &event);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_NOT_RUNNING, result);

    TEST_ASSERT_EQUAL_INT(0, nk_session_is_running(test_session));

    TEST_ASSERT_TRUE(nk_session_id(test_session) == 0);
}

/*
 * 欠損が1エポックだけならGAPイベントを作らない。
 */
static void test_single_gap_epoch_creates_no_gap_events(void) {
    nk_consent_record consent = {0};
    nk_session_record record = {0};
    nk_epoch_record epoch = {0};
    nk_result result;
    int64_t session_id;

    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 0;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(test_data, &consent);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;
    record.placement = "SKIP";
    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    session_id = nk_session_id(test_session);

    TEST_ASSERT_TRUE(session_id > 0);

    /*
     * idx=0は加速度欠損。
     */
    epoch.idx = 0;
    epoch.acc_cov = 0.40;
    epoch.mic_cov = 0.0;
    epoch.flags = NK_EPF_GAP_ACC;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(0, count_gap_events(session_id));

    /*
     * idx=1で正常に戻る。
     */
    epoch.idx = 1;
    epoch.acc_cov = 0.98;
    epoch.flags = 0;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 正常復帰してもGAPイベントは0件のまま。
     */
    TEST_ASSERT_EQUAL_INT(0, count_gap_events(session_id));

    result = nk_session_finish(test_session, record.start_ms + INT64_C(60000), "2026-11-10",
                               NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    /*
     * 計測終了後もGAPイベントは0件。
     */
    TEST_ASSERT_EQUAL_INT(0, count_gap_events(session_id));
}

/*
 * 2エポック連続欠損でGAP_STARTを作成し、
 * 正常復帰でGAP_ENDを作成する。
 */
static void test_two_gap_epochs_close_on_recovery(void) {
    nk_consent_record consent = {0};
    nk_session_record record = {0};
    nk_epoch_record epoch = {0};
    nk_result result;
    int64_t session_id;

    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 0;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(test_data, &consent);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;
    record.placement = "SKIP";
    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    session_id = nk_session_id(test_session);

    TEST_ASSERT_TRUE(session_id > 0);

    /*
     * idx=0：最初の欠損。GAPイベントはまだ0件。
     */
    epoch.idx = 0;
    epoch.acc_cov = 0.40;
    epoch.mic_cov = 0.0;
    epoch.flags = NK_EPF_GAP_ACC;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(0, count_gap_events(session_id));

    /*
     * idx=1：連続2件目。GAP_STARTが1件作られる。
     */
    epoch.idx = 1;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(1, count_gap_events(session_id));

    /*
     * idx=2：開始60秒後に正常復帰。
     */
    epoch.idx = 2;
    epoch.acc_cov = 0.98;
    epoch.flags = 0;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(2, count_gap_events(session_id));

    /*
     * 開始時刻から60秒間の欠損として、
     * Q04付きの開始・終了イベントが保存されている。
     */
    TEST_ASSERT_TRUE(
        gap_pair_matches(session_id, record.start_ms, record.start_ms + INT64_C(60000)));

    /*
     * 正常復帰後に計測終了しても、
     * GAP_ENDが重複して作られない。
     */
    result = nk_session_finish(test_session, record.start_ms + INT64_C(90000), "2026-11-10",
                               NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_TRUE(
        gap_pair_matches(session_id, record.start_ms, record.start_ms + INT64_C(60000)));
}

/*
 * 欠損中に計測終了した場合、
 * 終了時刻でGAP_ENDを保存する。
 */
static void test_gap_closes_on_session_finish(void) {
    nk_consent_record consent = {0};
    nk_session_record record = {0};
    nk_epoch_record epoch = {0};
    nk_result result;
    int64_t session_id;
    int64_t end_ms;

    consent.version = "2026-11-01";
    consent.measurement = 1;
    consent.microphone = 0;
    consent.consented_ms = INT64_C(1794326300000);

    result = nk_data_consent_set(test_data, &consent);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    record.start_ms = INT64_C(1794326400000);
    record.tz_min = 540;
    record.target_sleep_min = 420;
    record.placement = "SKIP";
    record.app_ver = "0.1.0";
    record.algo_ver = "AI-1-test";
    record.device_model = "PC Unity test";

    result = nk_session_start(test_session, &record);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    session_id = nk_session_id(test_session);

    TEST_ASSERT_TRUE(session_id > 0);

    /*
     * idx=0：1件目の加速度欠損。
     */
    epoch.idx = 0;
    epoch.acc_cov = 0.40;
    epoch.mic_cov = 0.0;
    epoch.flags = NK_EPF_GAP_ACC;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(0, count_gap_events(session_id));

    /*
     * idx=1：2件目の欠損でGAP_STARTを作成する。
     */
    epoch.idx = 1;

    result = nk_session_save_epoch(test_session, &epoch);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(1, count_gap_events(session_id));

    /*
     * 正常epochへ戻らず、開始90秒後に計測終了。
     */
    end_ms = record.start_ms + INT64_C(90000);

    result = nk_session_finish(test_session, end_ms, "2026-11-10", NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(0, nk_session_is_running(test_session));

    TEST_ASSERT_TRUE(nk_session_id(test_session) == 0);

    /*
     * DBを読み返して確認する。
     *
     * GAP_START：計測開始時刻
     * GAP_END：計測終了時刻
     * dur_ms：90000
     * reason：Q04
     */
    TEST_ASSERT_TRUE(gap_pair_matches(session_id, record.start_ms, end_ms));

    /*
     * 二重終了を拒否し、
     * GAP_ENDも重複して作らない。
     */
    result = nk_session_finish(test_session, end_ms, "2026-11-10", NK_SESSION_END_USER_STOP, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_NOT_RUNNING, result);

    TEST_ASSERT_TRUE(gap_pair_matches(session_id, record.start_ms, end_ms));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_start_without_consent_is_rejected);
    RUN_TEST(test_start_with_consent_succeeds);
    RUN_TEST(test_duplicate_start_is_rejected);
    RUN_TEST(test_duplicate_finish_is_rejected);
    RUN_TEST(test_save_after_finish_is_rejected);
    RUN_TEST(test_single_gap_epoch_creates_no_gap_events);
    RUN_TEST(test_two_gap_epochs_close_on_recovery);
    RUN_TEST(test_gap_closes_on_session_finish);

    return UNITY_END();
}
