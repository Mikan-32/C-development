#include "android_saved_report.h"
#include "nk_db.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

#define TEST_DB  "nk_saved_report_test.db"
#define TEST_KEY "saved-report-test-key"
#define START_MS INT64_C(1795190400000)

void setUp(void);
void tearDown(void);

static void execute(const char *sql) {
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(nk_db_get_handle(), sql, NULL, NULL, NULL));
}

static int scalar(const char *sql) {
    sqlite3_stmt *statement = NULL;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_prepare_v2(nk_db_get_handle(), sql, -1, &statement, NULL));
    TEST_ASSERT_EQUAL_INT(SQLITE_ROW, sqlite3_step(statement));
    int result = sqlite3_column_int(statement, 0);
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_finalize(statement));
    return result;
}

static void remove_files(void) {
    (void)remove(TEST_DB);
    (void)remove(TEST_DB "-wal");
    (void)remove(TEST_DB "-shm");
}

void setUp(void) {
    remove_files();
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, nk_db_open(TEST_DB, TEST_KEY, sizeof(TEST_KEY) - 1));
}

void tearDown(void) {
    nk_db_close();
    remove_files();
}

static nk_result read_report(nk_android_saved_report *report) {
    return nk_android_saved_report_read(TEST_DB, TEST_KEY, sizeof(TEST_KEY) - 1, report);
}

static void seed_session(void) {
    execute("INSERT INTO "
            "session(id,start_ms,tz_min,target_sleep_min,mic_used,light_used,app_ver,algo_ver) "
            "VALUES(1,1795190400000,540,420,0,0,'dev-issue27','sensing-accel-v1');");
}

static void test_no_session_is_reported_as_empty(void) {
    nk_android_saved_report report;
    char text[1024];
    TEST_ASSERT_EQUAL_INT(NK_OK, read_report(&report));
    TEST_ASSERT_EQUAL_INT(0, report.found);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_saved_report_format(&report, text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "no sessions found"));
}

static void test_overnight_metrics_and_stored_battery_are_reconstructed(void) {
    seed_session();
    execute("UPDATE session SET "
            "end_ms=start_ms+31960423,end_reason='USER_STOP',battery_start=100,battery_end=87;"
            "WITH RECURSIVE n(idx) AS (VALUES(0) UNION ALL SELECT idx+1 FROM n WHERE idx<1065) "
            "INSERT INTO epoch(session_id,idx,acc_cov,mic_cov,flags) "
            "SELECT 1,idx,CASE WHEN idx<1065 THEN 1.0 ELSE 0.5 END,0.0,0 FROM n;");
    int version = scalar("PRAGMA data_version;");
    nk_android_saved_report report;
    char text[1024];
    TEST_ASSERT_EQUAL_INT(NK_OK, read_report(&report));
    TEST_ASSERT_EQUAL_INT(version, scalar("PRAGMA data_version;"));
    TEST_ASSERT_TRUE(report.finished);
    TEST_ASSERT_EQUAL_INT64(START_MS, report.start_ms);
    TEST_ASSERT_EQUAL_INT64(1066, report.expected_epochs);
    TEST_ASSERT_EQUAL_INT64(1066, report.epochs);
    TEST_ASSERT_EQUAL_INT64(1065, report.good_acc_epochs);
    TEST_ASSERT_EQUAL_INT64(0, report.charging_epochs);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_saved_report_format(&report, text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "duration_ms=31960423"));
    TEST_ASSERT_NOT_NULL(strstr(text, "good_acc_pct=99.906 missing_epochs=0 charging_epochs=0"));
    TEST_ASSERT_NOT_NULL(strstr(text, "start_pct=100 end_pct=87 drop_pp=13"));
}

static void test_missing_rows_are_included_in_quality_denominator(void) {
    seed_session();
    execute("UPDATE session SET end_ms=start_ms+120000,end_reason='USER_STOP';"
            "INSERT INTO epoch(session_id,idx,acc_cov,mic_cov,flags) "
            "VALUES(1,0,1.0,0.0,16),(1,2,1.0,0.0,0);");
    nk_android_saved_report report;
    char text[1024];
    TEST_ASSERT_EQUAL_INT(NK_OK, read_report(&report));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_saved_report_format(&report, text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "good_acc_pct=50.000 missing_epochs=2 charging_epochs=1"));
    TEST_ASSERT_NOT_NULL(strstr(text, "start_pct=unknown end_pct=unknown drop_pp=unknown"));
}

static void test_latest_unfinished_session_is_not_recovered_or_hidden(void) {
    seed_session();
    execute("UPDATE session SET end_ms=start_ms+30000,end_reason='USER_STOP';"
            "INSERT INTO "
            "session(id,start_ms,tz_min,target_sleep_min,mic_used,light_used,app_ver,algo_ver) "
            "VALUES(2,1795190500000,540,420,0,0,'dev-issue27','sensing-accel-v1');"
            "INSERT INTO epoch(session_id,idx,acc_cov,mic_cov,flags) VALUES(2,0,1.0,0.0,0);");
    nk_android_saved_report report;
    char text[1024];
    int version = scalar("PRAGMA data_version;");
    TEST_ASSERT_EQUAL_INT(NK_OK, read_report(&report));
    TEST_ASSERT_EQUAL_INT64(2, report.session_id);
    TEST_ASSERT_FALSE(report.finished);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_saved_report_format(&report, text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "finished=0"));
    TEST_ASSERT_NULL(strstr(text, "good_acc_pct="));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT end_ms IS NULL FROM session WHERE id=2;"));
    TEST_ASSERT_EQUAL_INT(version, scalar("PRAGMA data_version;"));
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM event;"));
}

static void test_wrong_key_is_rejected_without_returning_partial_data(void) {
    seed_session();
    nk_android_saved_report report;
    memset(&report, 0x5a, sizeof(report));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE,
                          nk_android_saved_report_read(TEST_DB, "wrong-key", 9, &report));
    TEST_ASSERT_EQUAL_INT(0, report.found);
    TEST_ASSERT_EQUAL_INT64(0, report.session_id);
}

static void test_missing_database_is_not_created(void) {
    const char *missing = "nk_saved_report_absent.db";
    (void)remove(missing);
    nk_android_saved_report report;
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE, nk_android_saved_report_read(
                                                 missing, TEST_KEY, sizeof(TEST_KEY) - 1, &report));
    FILE *file = fopen(missing, "rb");
    TEST_ASSERT_NULL(file);
}

static void test_out_of_range_epoch_is_not_reported_as_healthy(void) {
    seed_session();
    execute("UPDATE session SET end_ms=start_ms+30000,end_reason='USER_STOP';"
            "INSERT INTO epoch(session_id,idx,acc_cov,mic_cov,flags) VALUES(1,4,1.0,0.0,0);");
    nk_android_saved_report report;
    TEST_ASSERT_EQUAL_INT(NK_ERROR_FORMAT, read_report(&report));
    TEST_ASSERT_EQUAL_INT(0, report.found);
}

static void test_zero_duration_and_small_output_buffer(void) {
    seed_session();
    execute("UPDATE session SET end_ms=start_ms,end_reason='ERROR';");
    nk_android_saved_report report;
    char text[1024];
    TEST_ASSERT_EQUAL_INT(NK_OK, read_report(&report));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_saved_report_format(&report, text, sizeof(text)));
    TEST_ASSERT_NOT_NULL(strstr(text, "end_reason=ERROR"));
    TEST_ASSERT_NOT_NULL(strstr(text, "good_acc_pct=unknown"));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_android_saved_report_format(&report, text, 4));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_android_saved_report_read(
                                                 NULL, TEST_KEY, sizeof(TEST_KEY) - 1, &report));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT,
                          nk_android_saved_report_format(NULL, text, sizeof(text)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_session_is_reported_as_empty);
    RUN_TEST(test_overnight_metrics_and_stored_battery_are_reconstructed);
    RUN_TEST(test_missing_rows_are_included_in_quality_denominator);
    RUN_TEST(test_latest_unfinished_session_is_not_recovered_or_hidden);
    RUN_TEST(test_wrong_key_is_rejected_without_returning_partial_data);
    RUN_TEST(test_missing_database_is_not_created);
    RUN_TEST(test_out_of_range_epoch_is_not_reported_as_healthy);
    RUN_TEST(test_zero_duration_and_small_output_buffer);
    return UNITY_END();
}
