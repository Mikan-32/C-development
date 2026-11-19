#include "data_session.h"
#include "nk_db.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

static const char *TEST_DB_FILE = "test_encrypted_verify.db";

void setUp(void) {
    nk_db_close();
    remove(TEST_DB_FILE);
}

void tearDown(void) {
    nk_db_close();
    remove(TEST_DB_FILE);
}

// 1. キーなし・空キーでは DB を開けないことを検証（Issue #28 完了条件）
static void test_nk_db_rejects_missing_or_empty_key(void) {
    TEST_ASSERT_NOT_EQUAL_INT(0, nk_db_open(":memory:", NULL, 0));
    TEST_ASSERT_FALSE(nk_db_is_open());

    TEST_ASSERT_NOT_EQUAL_INT(0, nk_db_open(":memory:", "", 0));
    TEST_ASSERT_FALSE(nk_db_is_open());
}

// 2. 誤ったキーでは既存DBを開けないことを検証
static void test_nk_db_rejects_wrong_key(void) {
    const char *correct_key = "correct_secret_key_999";
    const char *wrong_key = "wrong_password_000";

    // 正しいキーでDBを作成して閉じる
    TEST_ASSERT_EQUAL_INT(0, nk_db_open(TEST_DB_FILE, correct_key, (int)strlen(correct_key)));
    nk_db_close();

    // 誤ったキーで開こうとすると失敗するべき
    TEST_ASSERT_NOT_EQUAL_INT(0, nk_db_open(TEST_DB_FILE, wrong_key, (int)strlen(wrong_key)));
    TEST_ASSERT_FALSE(nk_db_is_open());

    // 正しいキーなら再び開けるべき
    TEST_ASSERT_EQUAL_INT(0, nk_db_open(TEST_DB_FILE, correct_key, (int)strlen(correct_key)));
    TEST_ASSERT_TRUE(nk_db_is_open());
}

// 3. 1夜の削除と全削除ができ、削除後に履歴から消えることを検証（Issue #28 完了条件）
static void test_nk_data_session_delete_single_and_all(void) {
    const char *key = "session_test_key";
    TEST_ASSERT_EQUAL_INT(0, nk_db_open(":memory:", key, (int)strlen(key)));

    int64_t id1 = 0, id2 = 0, id3 = 0;
    TEST_ASSERT_TRUE(nk_data_session_insert(1000, 2000, 85, &id1));
    TEST_ASSERT_TRUE(nk_data_session_insert(3000, 4000, 72, &id2));
    TEST_ASSERT_TRUE(nk_data_session_insert(5000, 6000, 91, &id3));

    TEST_ASSERT_EQUAL_INT(3, nk_data_session_count());

    // 1夜の削除（id2 のみ削除）
    TEST_ASSERT_TRUE(nk_data_session_delete_by_id(id2));
    TEST_ASSERT_EQUAL_INT(2, nk_data_session_count());
    TEST_ASSERT_FALSE(nk_data_session_exists(id2));
    TEST_ASSERT_TRUE(nk_data_session_exists(id1));
    TEST_ASSERT_TRUE(nk_data_session_exists(id3));

    // 全削除（残りの履歴がすべて消えること）
    TEST_ASSERT_TRUE(nk_data_session_delete_all());
    TEST_ASSERT_EQUAL_INT(0, nk_data_session_count());
    TEST_ASSERT_FALSE(nk_data_session_exists(id1));
    TEST_ASSERT_FALSE(nk_data_session_exists(id3));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nk_db_rejects_missing_or_empty_key);
    RUN_TEST(test_nk_db_rejects_wrong_key);
    RUN_TEST(test_nk_data_session_delete_single_and_all);
    return UNITY_END();
}
