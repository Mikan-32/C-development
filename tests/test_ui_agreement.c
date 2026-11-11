#include "lvgl.h"
#include "ui_agreement.h"
#include "unity.h"

static lv_display_t *disp = NULL;

void setUp(void) {
    lv_init();
    disp = lv_display_create(320, 480);
}

void tearDown(void) {
    ui_agreement_destroy();
    if (disp != NULL) {
        lv_display_delete(disp);
        disp = NULL;
    }
}

// 1. 画面がクラッシュせずに生成・破棄できるかのテスト
void test_ui_agreement_create_and_destroy(void) {
    ui_agreement_create();
    lv_obj_t *act_scr = lv_screen_active();
    TEST_ASSERT_NOT_NULL(act_scr);
}

// 2. チェックボックスのオン／オフで「次へ」ボタンの有効状態が正しく切り替わるかのテスト
void test_ui_agreement_checkbox_toggle_enables_next_button(void) {
    ui_agreement_create();

    // 初期状態：チェックは外れていて、ボタンは無効（押せない）であるべき
    TEST_ASSERT_FALSE(ui_agreement_is_checked());
    TEST_ASSERT_FALSE(ui_agreement_is_next_enabled());

    // 1回トグル：チェックが入り、ボタンが有効化されるべき
    ui_agreement_toggle_checkbox();
    TEST_ASSERT_TRUE(ui_agreement_is_checked());
    TEST_ASSERT_TRUE(ui_agreement_is_next_enabled());

    // もう1回トグル：チェックが外れ、ボタンが再び無効化されるべき
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
