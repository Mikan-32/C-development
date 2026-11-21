#include "data.h"
#include "data_recovery.h"
#include "unity.h"

#include <sqlcipher/sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_DB "nk_data_recovery_test.db"
#define BASE_MS INT64_C(1794326400000)

static const unsigned char test_key[] = "0123456789abcdef0123456789abcdef";
static nk_data *data;
static sqlite3 *probe;

void setUp(void);
void tearDown(void);

static void remove_files(void) {
    (void)remove(TEST_DB);
    (void)remove(TEST_DB "-wal");
    (void)remove(TEST_DB "-shm");
}

static void open_handles(void) {
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_open(&data, TEST_DB, test_key, sizeof(test_key) - 1));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_create_schema(data));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open_v2(TEST_DB, &probe, SQLITE_OPEN_READWRITE, NULL));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_key(probe, test_key, (int)sizeof(test_key) - 1));
}

void setUp(void) {
    data = NULL;
    probe = NULL;
    remove_files();
    open_handles();
}

void tearDown(void) {
    if (probe != NULL) {
        TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_close(probe));
        probe = NULL;
    }
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_close(data));
    data = NULL;
    remove_files();
}

static int64_t scalar(const char *sql, int64_t id) {
    sqlite3_stmt *statement = NULL;
    int prepared = sqlite3_prepare_v2(probe, sql, -1, &statement, NULL);
    int bound;
    int step;
    int finalized;
    int64_t value;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, prepared);
    bound = sqlite3_bind_parameter_count(statement) > 0 ? sqlite3_bind_int64(statement, 1, id)
                                                        : SQLITE_OK;
    step = bound == SQLITE_OK ? sqlite3_step(statement) : bound;
    value = step == SQLITE_ROW ? sqlite3_column_int64(statement, 0) : -1;
    finalized = sqlite3_finalize(statement);
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, bound);
    TEST_ASSERT_EQUAL_INT(SQLITE_ROW, step);
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, finalized);
    return value;
}

static void execute_probe(const char *sql) {
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(probe, sql, NULL, NULL, NULL));
}

static int64_t seed_session(int64_t start_ms, int tz_min, int mic_used) {
    nk_session_record record = {0};
    int64_t id = 0;
    record.start_ms = start_ms;
    record.tz_min = tz_min;
    record.target_sleep_min = 420;
    record.mic_used = mic_used;
    record.placement = "SKIP";
    record.app_ver = "recovery-test";
    record.algo_ver = "AI-1-recovery";
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_session_start(data, &record, &id));
    TEST_ASSERT_TRUE(id > 0);
    return id;
}

static void seed_epoch(int64_t id, int idx, double coverage) {
    nk_epoch_record epoch = {0};
    epoch.session_id = id;
    epoch.idx = idx;
    epoch.acc_cov = coverage;
    if (coverage > 0.0) {
        epoch.move_mg.has_value = 1;
        epoch.move_mg.value = 1.25;
    }
    epoch.flags = coverage < 0.8 ? NK_EPF_GAP_ACC : 0U;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_epoch_insert(data, &epoch));
}

static void seed_event(int64_t id, nk_event_type type, int64_t time_ms, int64_t duration_ms,
                       const char *reason) {
    nk_event_record event = {0};
    event.session_id = id;
    event.type = type;
    event.t_ms = time_ms;
    event.dur_ms = duration_ms;
    event.reason = reason;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_event_insert(data, &event));
}

static nk_data_recovery_report recover(int64_t at_ms) {
    nk_data_recovery_report report = {0};
    nk_result result = nk_data_recover_unfinished(data, at_ms, &report);
    TEST_ASSERT_EQUAL_INT_MESSAGE(NK_OK, result, nk_data_last_error(data));
    return report;
}

static void test_interruption_preserves_data_and_adds_null_epochs(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.98);
    seed_epoch(id, 1, 0.99);
    nk_data_recovery_report report = recover(BASE_MS + 150000);
    TEST_ASSERT_EQUAL_INT(1, report.recovered_sessions);
    TEST_ASSERT_EQUAL_INT(1, report.interrupted_sessions);
    TEST_ASSERT_EQUAL_INT(1, report.q04_sessions);
    TEST_ASSERT_EQUAL_INT(3, report.added_epochs);
    TEST_ASSERT_TRUE(report.gap_start_ms == BASE_MS + 60000);
    TEST_ASSERT_TRUE(report.gap_end_ms == BASE_MS + 150000);
    TEST_ASSERT_EQUAL_INT(5, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1", id));
    TEST_ASSERT_EQUAL_INT(2, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1 AND idx<2 "
                                    "AND move_mg=1.25 AND acc_cov>=0.98",
                                    id));
    TEST_ASSERT_EQUAL_INT(
        3, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1 AND idx>=2 "
                  "AND acc_cov=0 AND mic_cov=0 AND flags=1 "
                  "AND move_n IS NULL AND move_mg IS NULL AND micro_mg IS NULL "
                  "AND posture IS NULL AND snd_db_mean IS NULL AND snd_db_p95 IS NULL "
                  "AND snore_s IS NULL AND breath_bpm IS NULL AND breath_q IS NULL "
                  "AND lux IS NULL",
                  id));
    TEST_ASSERT_EQUAL_INT(
        2, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1 AND reason='Q04'", id));
    TEST_ASSERT_EQUAL_INT(
        90000, scalar("SELECT dur_ms FROM event WHERE session_id=?1 AND type='GAP_END'", id));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_reason='SERVICE_KILLED' AND battery_end IS NULL "
                                    "AND night_date=date(end_ms/1000+tz_min*60,'unixepoch') "
                                    "FROM session WHERE id=?1",
                                    id));
    int consent = 99;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_has_measurement_consent(data, &consent));
    TEST_ASSERT_EQUAL_INT(0, consent);
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM session", 0));
}

static void test_reopen_is_idempotent_and_notice_persists(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    (void)recover(BASE_MS + 120000);
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_close(probe));
    probe = NULL;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_close(data));
    data = NULL;
    open_handles();
    nk_data_recovery_report report = recover(BASE_MS + 300000);
    TEST_ASSERT_EQUAL_INT(0, report.recovered_sessions);
    TEST_ASSERT_EQUAL_INT(0, report.added_epochs);
    TEST_ASSERT_EQUAL_INT(1, report.has_interruption);
    TEST_ASSERT_TRUE(report.interrupted_session_id == id);
    TEST_ASSERT_TRUE(report.gap_end_ms == BASE_MS + 120000);
    TEST_ASSERT_EQUAL_INT(4, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1", id));
    TEST_ASSERT_EQUAL_INT(2, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
    TEST_ASSERT_TRUE(scalar("SELECT end_ms FROM session WHERE id=?1", id) == BASE_MS + 120000);
}

static void test_finished_sessions_are_unchanged(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_session_finish(data, id, BASE_MS + 30000, "2026-11-11",
                                                        NK_SESSION_END_USER_STOP, 1, 84));
    nk_data_recovery_report report = recover(BASE_MS + 300000);
    TEST_ASSERT_EQUAL_INT(0, report.recovered_sessions);
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1", id));
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_reason='USER_STOP' AND battery_end=84 "
                                    "AND night_date='2026-11-11' FROM session WHERE id=?1",
                                    id));
}

static void test_gap_shorter_than_one_minute_does_not_create_q04(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    nk_data_recovery_report report = recover(BASE_MS + 89999);
    TEST_ASSERT_EQUAL_INT(1, report.interrupted_sessions);
    TEST_ASSERT_EQUAL_INT(0, report.q04_sessions);
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
}

static void test_exactly_one_minute_creates_q04(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    nk_data_recovery_report report = recover(BASE_MS + 90000);
    TEST_ASSERT_EQUAL_INT(1, report.q04_sessions);
    TEST_ASSERT_EQUAL_INT(
        60000, scalar("SELECT dur_ms FROM event WHERE session_id=?1 AND type='GAP_END'", id));
}

static void test_open_q04_is_closed_without_a_duplicate_start(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    seed_epoch(id, 1, 0.0);
    seed_epoch(id, 2, 0.0);
    seed_event(id, NK_EVENT_GAP_START, BASE_MS + 30000, 0, "Q04");
    nk_data_recovery_report report = recover(BASE_MS + 150000);
    TEST_ASSERT_EQUAL_INT(2, report.added_epochs);
    TEST_ASSERT_EQUAL_INT(
        1, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1 AND type='GAP_START'", id));
    TEST_ASSERT_EQUAL_INT(
        120000, scalar("SELECT dur_ms FROM event WHERE session_id=?1 AND type='GAP_END'", id));
}

static void test_trailing_missing_epochs_without_a_marker_are_included(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    seed_epoch(id, 1, 0.0);
    seed_epoch(id, 2, 0.0);
    nk_data_recovery_report report = recover(BASE_MS + 150000);
    TEST_ASSERT_TRUE(report.gap_start_ms == BASE_MS + 30000);
    TEST_ASSERT_EQUAL_INT(2, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
}

static void test_open_microphone_gap_is_closed_before_process_gap(void) {
    int64_t id = seed_session(BASE_MS, 540, 1);
    seed_epoch(id, 0, 0.99);
    seed_epoch(id, 1, 0.99);
    seed_event(id, NK_EVENT_GAP_START, BASE_MS, 0, "Q08");
    (void)recover(BASE_MS + 150000);
    TEST_ASSERT_EQUAL_INT(60000, scalar("SELECT dur_ms FROM event WHERE session_id=?1 "
                                        "AND type='GAP_END' AND reason='Q08'",
                                        id));
    TEST_ASSERT_EQUAL_INT(
        3, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1 AND idx>=2 AND flags=3", id));
    TEST_ASSERT_EQUAL_INT(4, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
}

static void test_persisted_user_stop_does_not_become_a_crash_gap(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    seed_epoch(id, 1, 0.5);
    seed_event(id, NK_EVENT_USER, BASE_MS + 45000, 0, "USER_STOP");
    nk_data_recovery_report report = recover(BASE_MS + 300000);
    TEST_ASSERT_EQUAL_INT(1, report.completed_user_stops);
    TEST_ASSERT_EQUAL_INT(0, report.interrupted_sessions);
    TEST_ASSERT_EQUAL_INT(0, report.added_epochs);
    TEST_ASSERT_TRUE(scalar("SELECT end_ms FROM session WHERE id=?1", id) == BASE_MS + 45000);
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_reason='USER_STOP' FROM session WHERE id=?1", id));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
}

static void test_persisted_consent_revoke_does_not_become_a_crash_gap(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    seed_epoch(id, 1, 0.5);
    seed_event(id, NK_EVENT_USER, BASE_MS + 45000, 0, "CONSENT_REVOKED");
    nk_data_recovery_report report = recover(BASE_MS + 300000);
    TEST_ASSERT_EQUAL_INT(1, report.completed_user_stops);
    TEST_ASSERT_EQUAL_INT(0, report.interrupted_sessions);
    TEST_ASSERT_EQUAL_INT(0, report.added_epochs);
    TEST_ASSERT_TRUE(scalar("SELECT end_ms FROM session WHERE id=?1", id) == BASE_MS + 45000);
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_reason='USER_STOP' FROM session WHERE id=?1", id));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
}

static void test_recovery_is_capped_at_fourteen_hours(void) {
    int64_t id = seed_session(BASE_MS, -480, 0);
    (void)recover(BASE_MS + INT64_C(86400000));
    TEST_ASSERT_EQUAL_INT(1680, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1", id));
    TEST_ASSERT_TRUE(scalar("SELECT end_ms FROM session WHERE id=?1", id) ==
                     BASE_MS + INT64_C(50400000));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT night_date=date(end_ms/1000+tz_min*60,'unixepoch') "
                                    "FROM session WHERE id=?1",
                                    id));
}

static void test_multiple_sessions_are_recovered_in_one_transaction(void) {
    int64_t first = seed_session(BASE_MS - 120000, 540, 0);
    int64_t second = seed_session(BASE_MS, 540, 0);
    seed_epoch(first, 0, 0.99);
    seed_epoch(second, 0, 0.99);
    nk_data_recovery_report report = recover(BASE_MS + 150000);
    TEST_ASSERT_EQUAL_INT(2, report.recovered_sessions);
    TEST_ASSERT_EQUAL_INT(2, report.q04_sessions);
    TEST_ASSERT_EQUAL_INT(12, report.added_epochs);
    TEST_ASSERT_TRUE(report.interrupted_session_id == second);
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM session WHERE end_ms IS NULL", 0));
}

static void test_failure_rolls_back_other_recovered_sessions_too(void) {
    int64_t first = seed_session(BASE_MS - 120000, 540, 0);
    int64_t second = seed_session(BASE_MS, 540, 0);
    seed_epoch(first, 0, 0.99);
    seed_epoch(second, 0, 0.99);
    execute_probe(
        "CREATE TRIGGER block_recovery BEFORE INSERT ON epoch "
        "WHEN NEW.session_id=2 AND NEW.idx=2 BEGIN SELECT RAISE(ABORT,'recovery blocked'); END;");
    nk_data_recovery_report report = {0};
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE,
                          nk_data_recover_unfinished(data, BASE_MS + 150000, &report));
    TEST_ASSERT_NOT_NULL(strstr(nk_data_last_error(data), "recovery blocked"));
    TEST_ASSERT_EQUAL_INT(0, report.recovered_sessions);
    TEST_ASSERT_EQUAL_INT(2, scalar("SELECT COUNT(*) FROM session WHERE end_ms IS NULL", 0));
    TEST_ASSERT_EQUAL_INT(2, scalar("SELECT COUNT(*) FROM epoch", 0));
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM event", 0));
    execute_probe("DROP TRIGGER block_recovery;");
    report = recover(BASE_MS + 150000);
    TEST_ASSERT_EQUAL_INT(2, report.recovered_sessions);
}

static void test_commit_failure_rolls_back_and_allows_retry(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    execute_probe("CREATE TABLE recovery_commit_guard(sid INTEGER REFERENCES session(id) "
                  "DEFERRABLE INITIALLY DEFERRED);"
                  "CREATE TRIGGER block_commit AFTER UPDATE OF end_ms ON session "
                  "BEGIN INSERT INTO recovery_commit_guard VALUES(-999); END;");
    nk_data_recovery_report report = {0};
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE,
                          nk_data_recover_unfinished(data, BASE_MS + 120000, &report));
    TEST_ASSERT_EQUAL_INT(0, report.recovered_sessions);
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_ms IS NULL FROM session WHERE id=?1", id));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1", id));
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
    execute_probe("DROP TRIGGER block_commit; DROP TABLE recovery_commit_guard;");
    report = recover(BASE_MS + 120000);
    TEST_ASSERT_EQUAL_INT(1, report.recovered_sessions);
}

static void test_clock_before_session_start_preserves_records(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    nk_data_recovery_report report = {0};
    TEST_ASSERT_EQUAL_INT(NK_ERROR_FORMAT, nk_data_recover_unfinished(data, BASE_MS - 1, &report));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_ms IS NULL FROM session WHERE id=?1", id));
    TEST_ASSERT_EQUAL_INT(0, report.recovered_sessions);
}

static void test_unbalanced_markers_are_preserved_for_diagnosis(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    seed_event(id, NK_EVENT_GAP_START, BASE_MS, 0, "Q04");
    seed_event(id, NK_EVENT_GAP_START, BASE_MS + 1000, 0, "Q04");
    nk_data_recovery_report report = {0};
    TEST_ASSERT_EQUAL_INT(NK_ERROR_FORMAT,
                          nk_data_recover_unfinished(data, BASE_MS + 120000, &report));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1", id));
    TEST_ASSERT_EQUAL_INT(2, scalar("SELECT COUNT(*) FROM event WHERE session_id=?1", id));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_ms IS NULL FROM session WHERE id=?1", id));
}

static void test_epoch_holes_are_not_silently_overwritten(void) {
    int64_t id = seed_session(BASE_MS, 540, 0);
    seed_epoch(id, 0, 0.99);
    seed_epoch(id, 2, 0.99);
    nk_data_recovery_report report = {0};
    TEST_ASSERT_EQUAL_INT(NK_ERROR_FORMAT,
                          nk_data_recover_unfinished(data, BASE_MS + 120000, &report));
    TEST_ASSERT_EQUAL_INT(2, scalar("SELECT COUNT(*) FROM epoch WHERE session_id=?1", id));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_interruption_preserves_data_and_adds_null_epochs);
    RUN_TEST(test_reopen_is_idempotent_and_notice_persists);
    RUN_TEST(test_finished_sessions_are_unchanged);
    RUN_TEST(test_gap_shorter_than_one_minute_does_not_create_q04);
    RUN_TEST(test_exactly_one_minute_creates_q04);
    RUN_TEST(test_open_q04_is_closed_without_a_duplicate_start);
    RUN_TEST(test_trailing_missing_epochs_without_a_marker_are_included);
    RUN_TEST(test_open_microphone_gap_is_closed_before_process_gap);
    RUN_TEST(test_persisted_user_stop_does_not_become_a_crash_gap);
    RUN_TEST(test_persisted_consent_revoke_does_not_become_a_crash_gap);
    RUN_TEST(test_recovery_is_capped_at_fourteen_hours);
    RUN_TEST(test_multiple_sessions_are_recovered_in_one_transaction);
    RUN_TEST(test_failure_rolls_back_other_recovered_sessions_too);
    RUN_TEST(test_commit_failure_rolls_back_and_allows_retry);
    RUN_TEST(test_clock_before_session_start_preserves_records);
    RUN_TEST(test_unbalanced_markers_are_preserved_for_diagnosis);
    RUN_TEST(test_epoch_holes_are_not_silently_overwritten);
    return UNITY_END();
}
