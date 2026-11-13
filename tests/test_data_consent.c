#include "data_consent.h"
#include "nk_db.h"
#include "unity.h"

#include <string.h>

void setUp(void) {
    const char *key = "test_key";
    nk_db_open(":memory:", key, (int)strlen(key));
    nk_data_consent_reset();
}

void tearDown(void) {
    nk_db_close();
}

void test_nk_data_consent_initial_requires_agreement(void) {
    TEST_ASSERT_FALSE(nk_data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(0, nk_data_consent_get_agreed_version());
    TEST_ASSERT_TRUE(nk_data_consent_needs_reagreement(1));
}

void test_nk_data_consent_persist_and_version_check(void) {
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, 1));
    TEST_ASSERT_TRUE(nk_data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(1, nk_data_consent_get_agreed_version());

    // メモリをリセットしてDBから再ロードしても維持されているか検証
    nk_data_consent_init();
    TEST_ASSERT_TRUE(nk_data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(1, nk_data_consent_get_agreed_version());

    // 同じ版(v1)なら再同意不要、新しい版(v2)になったら再同意が必要
    TEST_ASSERT_FALSE(nk_data_consent_needs_reagreement(1));
    TEST_ASSERT_TRUE(nk_data_consent_needs_reagreement(2));
}

void test_nk_data_consent_revoke_clears_state(void) {
    nk_data_consent_set_agreed(true, 1);
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(false, 0));
    TEST_ASSERT_FALSE(nk_data_consent_is_agreed());
    TEST_ASSERT_TRUE(nk_data_consent_needs_reagreement(1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nk_data_consent_initial_requires_agreement);
    RUN_TEST(test_nk_data_consent_persist_and_version_check);
    RUN_TEST(test_nk_data_consent_revoke_clears_state);
    return UNITY_END();
}
