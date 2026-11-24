#include "data_consent.h"
#include "lvgl.h"
#include "nk_db.h"
#include "ui_agreement.h"
#include "ui_permissions.h"
#include "ui_privacy_settings.h"
#include "unity.h"

static lv_display_t *s_disp = NULL;
static bool s_next_called = false;

static void dummy_on_next(void) {
    s_next_called = true;
}

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, nk_db_open(":memory:", "ui-test-key", 11));
    lv_init();
    s_disp = lv_display_create(360, 640);
    s_next_called = false;
    nk_data_consent_reset();
}

void tearDown(void) {
    nk_ui_agreement_destroy();
    nk_ui_permissions_destroy();
    nk_ui_privacy_destroy();
    if (s_disp != NULL) {
        lv_display_delete(s_disp);
        s_disp = NULL;
    }
    nk_db_close();
}

static void test_nk_ui_agreement_flow(void) {
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

static void test_nk_ui_permissions_mic_denied_shows_warning(void) {
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

static void test_agreement_does_not_advance_when_persistence_fails(void) {
    nk_ui_agreement_create(dummy_on_next, false);
    nk_ui_agreement_toggle_checkbox();
    nk_db_close();
    nk_ui_agreement_click_next();
    TEST_ASSERT_FALSE(s_next_called);
    TEST_ASSERT_FALSE(nk_data_consent_is_agreed());
}

static void test_privacy_revoke_changes_the_acquisition_consent(void) {
    TEST_ASSERT_TRUE(nk_data_consent_set_agreed(true, NK_DATA_CONSENT_CURRENT_VERSION));
    nk_ui_privacy_create(dummy_on_next);
    nk_ui_privacy_click_revoke();
    int agreed = 1;
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_data_has_measurement_consent(nk_db_get_data(), &agreed));
    TEST_ASSERT_EQUAL_INT(0, agreed);
    TEST_ASSERT_TRUE(s_next_called);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nk_ui_agreement_flow);
    RUN_TEST(test_agreement_does_not_advance_when_persistence_fails);
    RUN_TEST(test_privacy_revoke_changes_the_acquisition_consent);
    RUN_TEST(test_nk_ui_permissions_mic_denied_shows_warning);
    return UNITY_END();
}
