#include "nk_db.h"
#include "unity.h"

#include <string.h>

void setUp(void) {
    nk_db_close();
}

void tearDown(void) {
    nk_db_close();
}

// 正常に暗号化DBが開いて閉じられるか
void test_nk_db_open_and_close(void) {
    const char *test_key = "test_secret_key_1234";
    int rc = nk_db_open(":memory:", test_key, (int)strlen(test_key));
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(nk_db_is_open());
    TEST_ASSERT_NOT_NULL(nk_db_get_handle());

    nk_db_close();
    TEST_ASSERT_FALSE(nk_db_is_open());
    TEST_ASSERT_NULL(nk_db_get_handle());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nk_db_open_and_close);
    return UNITY_END();
}
