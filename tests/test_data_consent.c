#include "data_consent.h"
#include "unity.h"

void setUp(void) {
    data_consent_reset();
}

void tearDown(void) {
    data_consent_reset();
}

// 初期状態は未同意（false）かつバージョン0であるべき
void test_data_consent_initial_state_is_false(void) {
    TEST_ASSERT_FALSE(data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(0, data_consent_get_agreed_version());
}

// 同意状態とバージョンを設定したら正しく取得できるべき
void test_data_consent_set_agreed_true(void) {
    bool result = data_consent_set_agreed(true, 1);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(1, data_consent_get_agreed_version());
}

// 同意を撤回したら false かつバージョン0になるべき
void test_data_consent_set_agreed_false(void) {
    data_consent_set_agreed(true, 1);
    TEST_ASSERT_TRUE(data_consent_is_agreed());

    bool result = data_consent_set_agreed(false, 0);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(0, data_consent_get_agreed_version());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_data_consent_initial_state_is_false);
    RUN_TEST(test_data_consent_set_agreed_true);
    RUN_TEST(test_data_consent_set_agreed_false);
    return UNITY_END();
}
