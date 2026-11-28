#include "curtain.h"
#include "curtain_api.h"
#include "nk_db.h"
#include "unity.h"

#include <string.h>

void setUp(void) {
    const char *key = "curtain_unit_test_key";
    TEST_ASSERT_EQUAL_INT(0, nk_db_open(":memory:", key, (int)strlen(key)));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_curtain_init());
}

void tearDown(void) {
    nk_curtain_deinit();
    nk_db_close();
}

static void test_switchbot_v11_hmac_signature(void) {
    char sign_a[128];
    char sign_b[128];

    TEST_ASSERT_EQUAL_INT(NK_OK, nk_curtain_api_sign("test_token_1234", "test_secret_5678",
                                                     1730326400000LL, "nonce_abc", sign_a,
                                                     sizeof(sign_a)));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_curtain_api_sign("test_token_1234", "test_secret_5678",
                                                     1730326400000LL, "nonce_abc", sign_b,
                                                     sizeof(sign_b)));

    /* 同一入力では同一の Base64 署名（44文字）が生成される */
    TEST_ASSERT_EQUAL_STRING(sign_a, sign_b);
    TEST_ASSERT_EQUAL_UINT32(44, (uint32_t)strlen(sign_a));
}

static void test_credentials_masked_to_last_4_chars(void) {
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    TEST_ASSERT_EQUAL_INT(NK_CURTAIN_CONN_DISCONNECTED, snap.conn_state);
    TEST_ASSERT_EQUAL_STRING("未設定", snap.masked_token);

    /* 存在しないローカルポートへ向けて接続試行しても、認証情報は末尾4文字のみ保持される */
    TEST_ASSERT_EQUAL_INT(
        NK_OK, nk_curtain_set_base_url("[http://127.0.0.1:1/v1.1](http://127.0.0.1:1/v1.1)"));
    nk_curtain_api_device devs[4];
    size_t count = 0;
    TEST_ASSERT_NOT_EQUAL(
        NK_OK, nk_curtain_connect("secret_token_a1b2", "very_secret_9z8y", devs, 4, &count));

    nk_curtain_get_snapshot(&snap);
    TEST_ASSERT_EQUAL_STRING("…a1b2", snap.masked_token);
    TEST_ASSERT_EQUAL_STRING("…9z8y", snap.masked_secret);
    TEST_ASSERT_NULL(strstr(snap.status_message, "secret_token"));
    TEST_ASSERT_NULL(strstr(snap.status_message, "very_secret"));
}

static void test_schedule_persistence_and_next_trigger(void) {
    nk_curtain_schedule sched = {
        .open_enabled = true,
        .open_hour = 6,
        .open_min = 20,
        .close_enabled = true,
        .close_hour = 22,
        .close_min = 30,
        .repeat = NK_CURTAIN_REPEAT_WEEKDAYS,
    };
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_curtain_set_schedule(&sched));

    /* 再初期化しても DB から予約設定が復元される */
    nk_curtain_deinit();
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_curtain_init());

    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    TEST_ASSERT_TRUE(snap.schedule.open_enabled);
    TEST_ASSERT_EQUAL_INT(6, snap.schedule.open_hour);
    TEST_ASSERT_EQUAL_INT(20, snap.schedule.open_min);
    TEST_ASSERT_EQUAL_INT(NK_CURTAIN_REPEAT_WEEKDAYS, snap.schedule.repeat);

    int64_t now_ms = 1730326400000LL;
    int64_t next_ms = nk_curtain_next_trigger_ms(6, 20, NK_CURTAIN_REPEAT_WEEKDAYS, now_ms);
    TEST_ASSERT_TRUE(next_ms > now_ms);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_switchbot_v11_hmac_signature);
    RUN_TEST(test_credentials_masked_to_last_4_chars);
    RUN_TEST(test_schedule_persistence_and_next_trigger);
    return UNITY_END();
}
