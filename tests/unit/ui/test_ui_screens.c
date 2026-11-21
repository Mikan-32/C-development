/*
 * 画面の単体テスト：すべての画面が開けて、描画でき、戻る先が正しいこと
 * ウィンドウは使わず、メモリー上の表示先に描く。日本語フォントは読まない
 */
#include "lvgl.h"
#include "nk_theme.h"
#include "ui.h"
#include "unity.h"

#define W 390
#define H 844

static uint32_t s_framebuffer[W * H]; /* XRGB8888（4 バイトそろえ） */
static lv_display_t *s_display;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

void setUp(void) {
}

void tearDown(void) {
}

static void test_every_screen_opens_and_draws(void) {
    for (int i = 0; i < NK_UI_SCREEN_COUNT; i++) {
        nk_ui_open((nk_ui_screen)i);
        TEST_ASSERT_EQUAL_INT(i, nk_ui_current());
        lv_refr_now(s_display);
    }
}

static void test_back_goes_to_the_parent_screen(void) {
    nk_ui_open(NK_UI_RECORD_DETAIL);
    TEST_ASSERT_TRUE(nk_ui_back());
    TEST_ASSERT_EQUAL_INT(NK_UI_RECORD_DAY, nk_ui_current());

    nk_ui_open(NK_UI_BEDROOM);
    TEST_ASSERT_TRUE(nk_ui_back());
    TEST_ASSERT_EQUAL_INT(NK_UI_SETTINGS, nk_ui_current());

    /* タブの画面には戻る先がない */
    TEST_ASSERT_FALSE(nk_ui_back());
    TEST_ASSERT_EQUAL_INT(NK_UI_SETTINGS, nk_ui_current());
}

int main(void) {
    lv_init();
    s_display = lv_display_create(W, H);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(s_display, s_framebuffer, NULL, sizeof(s_framebuffer),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_display, flush_cb);
    nk_theme_init(NULL, 0, NULL, 0);
    nk_ui_init();

    UNITY_BEGIN();
    RUN_TEST(test_every_screen_opens_and_draws);
    RUN_TEST(test_back_goes_to_the_parent_screen);
    int failures = UNITY_END();

    nk_ui_deinit();
    nk_theme_deinit();
    lv_deinit();
    return failures;
}
