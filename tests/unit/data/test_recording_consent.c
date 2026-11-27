#include "android_bridge.h"
#include "android_recording.h"
#include "data_consent.h"
#include "nk_db.h"
#include "unity.h"

#include <SDL3/SDL.h>
#include <android/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>

static unsigned char key[NK_ANDROID_DB_KEY_SIZE];
static nk_android_recording *recording;
static int saw_summary;
#define NK_TEST_START_MS INT64_C(1794326400000)

void setUp(void);
void tearDown(void);

/* Only platform/SDL boundaries are replaced. All persistence and sensing are real. */
char *SDL_GetPrefPath(const char *organization, const char *application) {
    (void)organization;
    (void)application;
    char *path = malloc(1);
    if (path != NULL) {
        path[0] = '\0';
    }
    return path;
}

int SDL_asprintf(char **text, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0) {
        return length;
    }
    *text = malloc((size_t)length + 1);
    if (*text == NULL) {
        return -1;
    }
    va_start(args, format);
    (void)vsnprintf(*text, (size_t)length + 1, format, args);
    va_end(args);
    return length;
}

void SDL_free(void *pointer) {
    free(pointer);
}

SDL_PowerState SDL_GetPowerInfo(int *seconds, int *percent) {
    if (seconds != NULL) {
        *seconds = -1;
    }
    if (percent != NULL) {
        *percent = 90;
    }
    return SDL_POWERSTATE_ON_BATTERY;
}

int __system_property_get(const char *name, char *value) {
    (void)name;
    strcpy(value, "SyntheticDevice");
    return (int)strlen(value);
}

int __android_log_print(int priority, const char *tag, const char *format, ...) {
    (void)priority;
    (void)tag;
    if (strstr(format, "TM02 summary:") != NULL) {
        ++saw_summary;
    }
    return 0;
}

nk_result nk_android_bridge_db_key(unsigned char *output, size_t size) {
    if (size != sizeof(key)) {
        return NK_ERROR_ARGUMENT;
    }
    memcpy(output, key, sizeof(key));
    return NK_OK;
}

void nk_android_bridge_clear_key(void *buffer, size_t size) {
    memset(buffer, 0, size);
}

static void remove_files(void) {
    (void)remove("nemunoki.db");
    (void)remove("nemunoki.db-wal");
    (void)remove("nemunoki.db-shm");
}

void setUp(void) {
    remove_files();
    memset(key, 0x5a, sizeof(key));
    recording = NULL;
    saw_summary = 0;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          nk_db_open("nemunoki.db", (const char *)key, (int)sizeof(key)));
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, NK_DATA_CONSENT_CURRENT_VERSION));
}

void tearDown(void) {
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_recording_close(&recording));
    nk_db_close();
    remove_files();
}

static void start_and_collect(void) {
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_recording_open(&recording));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_recording_start(recording, NK_TEST_START_MS));
    for (int i = 0; i < 45; ++i) {
        nk_sensing_second second = {0};
        second.idx = i;
        second.acc_sample_count = 50;
        second.acc_hp = (nk_optional_double){1, 1.0};
        TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_recording_push_second(recording, &second));
    }
}

static void test_revocation_allows_flush_and_records_its_reason(void) {
    start_and_collect();
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(false, 0));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_CONSENT_REQUIRED, nk_android_recording_check_consent(recording));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_recording_finish(recording, NK_TEST_START_MS + 45000,
                                                             NK_SESSION_END_USER_STOP));
    TEST_ASSERT_EQUAL_INT(1, saw_summary);
    sqlite3_stmt *statement = NULL;
    TEST_ASSERT_EQUAL_INT(
        SQLITE_OK, sqlite3_prepare_v2(
                       nk_db_get_handle(),
                       "SELECT end_reason,(SELECT COUNT(*) FROM epoch),(SELECT reason FROM event "
                       "WHERE type='USER' ORDER BY id DESC LIMIT 1) FROM session;",
                       -1, &statement, NULL));
    TEST_ASSERT_EQUAL_INT(SQLITE_ROW, sqlite3_step(statement));
    TEST_ASSERT_EQUAL_STRING("USER_STOP", sqlite3_column_text(statement, 0));
    TEST_ASSERT_EQUAL_INT(2, sqlite3_column_int(statement, 1));
    TEST_ASSERT_EQUAL_STRING("CONSENT_REVOKED", sqlite3_column_text(statement, 2));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_finalize(statement));
}

static void test_version_change_blocks_worker_before_start(void) {
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, NK_DATA_CONSENT_CURRENT_VERSION + 1));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_CONSENT_REQUIRED, nk_android_recording_open(&recording));
    TEST_ASSERT_EQUAL_INT(0, saw_summary);
}

static void test_normal_stop_keeps_the_user_stop_reason(void) {
    start_and_collect();
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_recording_check_consent(recording));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_android_recording_finish(recording, NK_TEST_START_MS + 45000,
                                                             NK_SESSION_END_USER_STOP));
    TEST_ASSERT_EQUAL_INT(1, saw_summary);
}

static void test_consent_read_failure_is_reported_after_saving_error_end(void) {
    start_and_collect();
    TEST_ASSERT_EQUAL_INT(
        SQLITE_OK, sqlite3_exec(nk_db_get_handle(), "DROP TABLE consent;", NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE, nk_android_recording_check_consent(recording));
    TEST_ASSERT_EQUAL_INT(
        NK_ERROR_DATABASE,
        nk_android_recording_finish(recording, NK_TEST_START_MS + 45000, NK_SESSION_END_USER_STOP));
    sqlite3_stmt *statement = NULL;
    TEST_ASSERT_EQUAL_INT(SQLITE_OK,
                          sqlite3_prepare_v2(nk_db_get_handle(), "SELECT end_reason FROM session;",
                                             -1, &statement, NULL));
    TEST_ASSERT_EQUAL_INT(SQLITE_ROW, sqlite3_step(statement));
    TEST_ASSERT_EQUAL_STRING("ERROR", sqlite3_column_text(statement, 0));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_finalize(statement));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_revocation_allows_flush_and_records_its_reason);
    RUN_TEST(test_version_change_blocks_worker_before_start);
    RUN_TEST(test_normal_stop_keeps_the_user_stop_reason);
    RUN_TEST(test_consent_read_failure_is_reported_after_saving_error_end);
    return UNITY_END();
}
