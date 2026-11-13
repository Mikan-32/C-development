#include "data_consent.h"
#include "lvgl.h"
#include "ui_agreement.h"
#include "ui_permissions.h"
#include "unity.h"

static lv_display_t *s_disp = NULL;
static bool s_next_called = false;

static void dummy_on_next(void) {
    s_next_called = true;
}

void setUp(void) {
    lv_init();
    s_disp = lv_display_create(360, 640);
    s_next_called = false;
    nk_data_consent_reset();
}

void tearDown(void) {
    nk_ui_agreement_destroy();
    nk_ui_permissions_destroy();
    if (s_disp != NULL) {
        lv_display_delete(s_disp);
        s_disp = NULL;
    }
}

void test_nk_ui_agreement_flow(void) {
    nk_ui_agreement_create(dummy_on_next, false);

    TEST_ASSERT_FALSE(nk_ui_agreement_is_checked());
    TEST_ASSERT_FALSE(nk_ui_agreement_is_next_enabled());

    nk_ui_agreement_toggle_checkbox();
    TEST_ASSERT_TRUE(nk_ui_agreement_is_checked());
    TEST_ASSERT_TRUE(nk_ui_agreement_is_next_enabled());

    nk_ui_agreement_click_next();
    TEST_ASSERT_TRUE(s_next_called);
    TEST_ASSERT_TRUE(nk_data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(NK_DATA_CONSENT_CURRENT_VERSION, nk_data_consent_get_agreed_version());
}

void test_nk_ui_permissions_mic_denied_shows_warning(void) {
    nk_ui_permissions_create(NULL, NULL);

    // 初期状態：エラー表示は隠れており、完了ボタンは無効
    TEST_ASSERT_FALSE(nk_ui_permissions_is_mic_denied_shown());
    TEST_ASSERT_FALSE(nk_ui_permissions_is_done_enabled());

    // マイク権限が拒否された場合：ST-PERM-MIC 警告が表示され、完了ボタンは無効のまま
    nk_ui_permissions_set_mic_status(NK_PERM_STATUS_DENIED);
    TEST_ASSERT_TRUE(nk_ui_permissions_is_mic_denied_shown());
    TEST_ASSERT_FALSE(nk_ui_permissions_is_done_enabled());

    // 再試行でマイク権限が許可された場合：警告が消え、完了ボタンが有効になる
    nk_ui_permissions_set_mic_status(NK_PERM_STATUS_GRANTED);
    TEST_ASSERT_FALSE(nk_ui_permissions_is_mic_denied_shown());
    TEST_ASSERT_TRUE(nk_ui_permissions_is_done_enabled());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nk_ui_agreement_flow);
    RUN_TEST(test_nk_ui_permissions_mic_denied_shows_warning);
    return UNITY_END();
}
