#include "data/data_consent.h"
#include "lvgl.h"
#include "ui_agreement.h"
#include "unity.h"

static lv_display_t *disp = NULL;
static bool s_next_called = false;

static void dummy_on_next(void) {
    s_next_called = true;
}

void setUp(void) {
    lv_init();
    disp = lv_display_create(320, 480);
    s_next_called = false;
    data_consent_reset();
}

void tearDown(void) {
    ui_agreement_destroy();
    if (disp != NULL) {
        lv_display_delete(disp);
        disp = NULL;
    }
}

// 画面生成・破棄テスト
void test_ui_agreement_create_and_destroy(void) {
    ui_agreement_create(dummy_on_next);
    lv_obj_t *act_scr = lv_screen_active();
    TEST_ASSERT_NOT_NULL(act_scr);
}

// チェックボックストグルで次へボタンが有効化されるテスト
void test_ui_agreement_checkbox_toggle_enables_next_button(void) {
    ui_agreement_create(dummy_on_next);

    TEST_ASSERT_FALSE(ui_agreement_is_checked());
    TEST_ASSERT_FALSE(ui_agreement_is_next_enabled());

    // チェックを入れる
    ui_agreement_toggle_checkbox();
    TEST_ASSERT_TRUE(ui_agreement_is_checked());
    TEST_ASSERT_TRUE(ui_agreement_is_next_enabled());

    // 次へをクリックしたらコールバックが呼ばれ、同意データが保存されること
    ui_agreement_click_next();
    TEST_ASSERT_TRUE(s_next_called);
    TEST_ASSERT_TRUE(data_consent_is_agreed());
    TEST_ASSERT_EQUAL_UINT32(NK_CURRENT_AGREEMENT_VERSION, data_consent_get_agreed_version());

    // チェックを外す
    ui_agreement_toggle_checkbox();
    TEST_ASSERT_FALSE(ui_agreement_is_checked());
    TEST_ASSERT_FALSE(ui_agreement_is_next_enabled());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ui_agreement_create_and_destroy);
    RUN_TEST(test_ui_agreement_checkbox_toggle_enables_next_button);
    return UNITY_END();
}
