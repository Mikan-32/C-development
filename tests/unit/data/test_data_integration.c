#include "data_consent.h"
#include "data_session.h"
#include "nk_db.h"
#include "session.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

#define NK_TEST_FILE     "nk_shared_data_test.db"
#define NK_TEST_KEY      "integration-test-key"
#define NK_TEST_START_MS INT64_C(1794326400000)

void setUp(void);
void tearDown(void);

static void remove_files(void) {
    (void)remove(NK_TEST_FILE);
    (void)remove(NK_TEST_FILE "-wal");
    (void)remove(NK_TEST_FILE "-shm");
}

void setUp(void) {
    nk_db_close();
    remove_files();
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          nk_db_open(NK_TEST_FILE, NK_TEST_KEY, (int)strlen(NK_TEST_KEY)));
}

void tearDown(void) {
    nk_db_close();
    remove_files();
}

static int scalar(const char *sql) {
    sqlite3_stmt *statement = NULL;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_prepare_v2(nk_db_get_handle(), sql, -1, &statement, NULL));
    TEST_ASSERT_EQUAL_INT(SQLITE_ROW, sqlite3_step(statement));
    int value = sqlite3_column_int(statement, 0);
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_finalize(statement));
    return value;
}

static int64_t seed_recording(int finish) {
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, NK_DATA_CONSENT_CURRENT_VERSION));
    nk_session *session = NULL;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_session_create(&session, nk_db_get_data()));
    nk_session_record record = {0};
    record.start_ms = NK_TEST_START_MS;
    record.target_sleep_min = 420;
    record.app_ver = "integration-test";
    record.algo_ver = "unscored";
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_session_start(session, &record));
    int64_t id = nk_session_id(session);
    nk_epoch_record epoch = {0};
    epoch.acc_cov = 1.0;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_session_save_epoch(session, &epoch));
    nk_event_record event = {0};
    event.type = NK_EVENT_USER;
    event.t_ms = NK_TEST_START_MS;
    event.reason = "TEST";
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_session_save_event(session, &event));
    if (finish) {
        TEST_ASSERT_EQUAL_INT(NK_OK,
                              nk_session_finish(session, NK_TEST_START_MS + 30000, "2026-11-11",
                                                NK_SESSION_END_USER_STOP, 0, 0));
    }
    nk_session_destroy(session);
    return id;
}

static void test_history_reads_real_acquisition_rows(void) {
    int64_t id = seed_recording(1);
    TEST_ASSERT_EQUAL_INT(1, nk_data_session_count());
    TEST_ASSERT_TRUE(nk_data_session_exists(id));
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM sqlite_master WHERE name IN "
                                    "('user_consent','sleep_sessions','_nk_key_check');"));
}

static void test_consent_version_is_shared_and_requires_exact_version(void) {
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, 1));
    int agreed = 0;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_has_versioned_consent(nk_db_get_data(), "1", &agreed));
    TEST_ASSERT_EQUAL_INT(1, agreed);
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, 2));
    TEST_ASSERT_TRUE(nk_data_consent_needs_reagreement(1));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_has_versioned_consent(nk_db_get_data(), "1", &agreed));
    TEST_ASSERT_EQUAL_INT(0, agreed);
}

static void test_development_consent_requires_explicit_reagreement(void) {
    nk_consent_record consent = {"dev-accel-1", 1, 0, NK_TEST_START_MS};
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_consent_set(nk_db_get_data(), &consent));
    TEST_ASSERT_TRUE(nk_data_consent_needs_reagreement(1));
    int agreed = 1;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_has_versioned_consent(nk_db_get_data(), "1", &agreed));
    TEST_ASSERT_EQUAL_INT(0, agreed);
}

static void test_revocation_is_visible_to_another_connection(void) {
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, 1));
    nk_data *worker = NULL;
    TEST_ASSERT_EQUAL_INT(NK_OK,
                          nk_data_open(&worker, NK_TEST_FILE, NK_TEST_KEY, strlen(NK_TEST_KEY)));
    int agreed = 0;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_has_versioned_consent(worker, "1", &agreed));
    TEST_ASSERT_EQUAL_INT(1, agreed);
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(false, 0));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_has_measurement_consent(worker, &agreed));
    TEST_ASSERT_EQUAL_INT(0, agreed);
    nk_session *session = NULL;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_session_create(&session, worker));
    nk_session_record record = {0};
    record.start_ms = NK_TEST_START_MS;
    TEST_ASSERT_EQUAL_INT(NK_ERROR_CONSENT_REQUIRED, nk_session_start(session, &record));
    nk_session_destroy(session);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_close(worker));
}

static void test_single_delete_removes_children_and_preserves_other_night(void) {
    int64_t first = seed_recording(1);
    int64_t second = seed_recording(1);
    TEST_ASSERT_TRUE(nk_data_session_delete_by_id(first));
    TEST_ASSERT_EQUAL_INT(1, nk_data_session_count());
    TEST_ASSERT_FALSE(nk_data_session_exists(first));
    TEST_ASSERT_TRUE(nk_data_session_exists(second));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM epoch;"));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM event;"));
    TEST_ASSERT_FALSE(nk_data_session_delete_by_id(first));
}

static void test_all_delete_keeps_consent_but_removes_session_epoch_event(void) {
    (void)seed_recording(1);
    (void)seed_recording(1);
    TEST_ASSERT_TRUE(nk_data_session_delete_all());
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM session;"));
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM epoch;"));
    TEST_ASSERT_EQUAL_INT(0, scalar("SELECT COUNT(*) FROM event;"));
    TEST_ASSERT_TRUE(nk_data_consent_is_agreed());
}

static void test_running_session_is_not_deleted(void) {
    int64_t id = seed_recording(0);
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ALREADY_RUNNING, nk_data_delete_session(nk_db_get_data(), id));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ALREADY_RUNNING, nk_data_delete_all_sessions(nk_db_get_data()));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM session;"));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM epoch;"));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM event;"));
    TEST_ASSERT_EQUAL_INT(1, sqlite3_get_autocommit(nk_db_get_handle()));
}

static void test_delete_failure_rolls_back_children(void) {
    int64_t id = seed_recording(1);
    TEST_ASSERT_EQUAL_INT(
        SQLITE_OK, sqlite3_exec(nk_db_get_handle(),
                                "CREATE TRIGGER reject_delete BEFORE DELETE ON session BEGIN "
                                "SELECT RAISE(ABORT,'injected failure'); END;",
                                NULL, NULL, NULL));
    TEST_ASSERT_FALSE(nk_data_session_delete_by_id(id));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM session;"));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM epoch;"));
    TEST_ASSERT_EQUAL_INT(1, scalar("SELECT COUNT(*) FROM event;"));
    TEST_ASSERT_EQUAL_INT(1, sqlite3_get_autocommit(nk_db_get_handle()));
}

static void test_borrowed_ui_connection_is_not_closed_by_adapter(void) {
    nk_db_close();
    nk_data *owner = NULL;
    TEST_ASSERT_EQUAL_INT(NK_OK,
                          nk_data_open(&owner, NK_TEST_FILE, NK_TEST_KEY, strlen(NK_TEST_KEY)));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_db_bind(owner));
    nk_db_close();
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_create_schema(owner));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_close(owner));
}

static void test_no_database_cannot_create_phantom_consent(void) {
    nk_db_close();
    TEST_ASSERT_FALSE(nk_data_consent_set_agreed(true, 1));
    TEST_ASSERT_FALSE(nk_data_consent_is_agreed());
}

static void test_failed_consent_write_does_not_grant_consent(void) {
    TEST_ASSERT_EQUAL_INT(
        SQLITE_OK, sqlite3_exec(nk_db_get_handle(),
                                "CREATE TRIGGER reject_consent BEFORE INSERT ON consent BEGIN "
                                "SELECT RAISE(ABORT,'injected failure'); END;",
                                NULL, NULL, NULL));
    TEST_ASSERT_FALSE(nk_data_consent_set_agreed(true, 1));
    TEST_ASSERT_FALSE(nk_data_consent_is_agreed());
}

static void test_encrypted_database_rejects_a_keyless_reader(void) {
    (void)seed_recording(1);
    nk_db_close();
    FILE *file = fopen(NK_TEST_FILE, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char header[16];
    TEST_ASSERT_EQUAL_UINT(16, fread(header, 1, sizeof(header), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    TEST_ASSERT_TRUE(memcmp(header, "SQLite format 3", 16) != 0);
    sqlite3 *plain = NULL;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_open_v2(NK_TEST_FILE, &plain, SQLITE_OPEN_READONLY, NULL));
    TEST_ASSERT_NOT_EQUAL_INT(SQLITE_OK,
                              sqlite3_exec(plain, "SELECT * FROM session;", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_close(plain));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_history_reads_real_acquisition_rows);
    RUN_TEST(test_consent_version_is_shared_and_requires_exact_version);
    RUN_TEST(test_development_consent_requires_explicit_reagreement);
    RUN_TEST(test_revocation_is_visible_to_another_connection);
    RUN_TEST(test_single_delete_removes_children_and_preserves_other_night);
    RUN_TEST(test_all_delete_keeps_consent_but_removes_session_epoch_event);
    RUN_TEST(test_running_session_is_not_deleted);
    RUN_TEST(test_delete_failure_rolls_back_children);
    RUN_TEST(test_borrowed_ui_connection_is_not_closed_by_adapter);
    RUN_TEST(test_no_database_cannot_create_phantom_consent);
    RUN_TEST(test_failed_consent_write_does_not_grant_consent);
    RUN_TEST(test_encrypted_database_rejects_a_keyless_reader);
    return UNITY_END();
}
