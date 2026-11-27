/*
 * 画面の単体テスト：すべての画面が開けて、描画でき、戻る先・記録操作・文字サイズが正しいこと
 * ウィンドウは使わず、メモリー上の表示先に描く。日本語フォントは読まない
 */
// clang-format off
#include <string.h>
#include "data_session.h"
#include "lvgl.h"
#include "nk_db.h"
#include "nk_theme.h"
#include "nk_ui_store.h"
#include "nk_widgets.h"
#include "ui.h"
#include "unity.h"
// clang-format on

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
    nk_modal_close();
    nk_theme_set_font_scale(NK_FONT_SCALE_100);
    nk_ui_store_init();
}

void tearDown(void) {
    nk_modal_close();
    nk_theme_set_font_scale(NK_FONT_SCALE_100);
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

static void test_date_navigation_blocks_future_and_edits_record(void) {
    /* 最新日（index 0）からは未来へ進めない */
    TEST_ASSERT_FALSE(nk_ui_store_can_next());
    TEST_ASSERT_FALSE(nk_ui_store_move_next());

    /* 過去日へは移動でき、再び最新日へ戻れる */
    TEST_ASSERT_TRUE(nk_ui_store_move_prev());
    TEST_ASSERT_EQUAL_INT(1, nk_ui_store_selected_index());
    TEST_ASSERT_TRUE(nk_ui_store_move_next());
    TEST_ASSERT_EQUAL_INT(0, nk_ui_store_selected_index());

    /* 目覚めの気分記録 */
    TEST_ASSERT_EQUAL_INT(0, nk_ui_store_selected()->mood);
    TEST_ASSERT_TRUE(nk_ui_store_set_mood(1));
    TEST_ASSERT_EQUAL_INT(1, nk_ui_store_selected()->mood);

    /* 1夜の記録削除 */
    int before = nk_ui_store_count();
    TEST_ASSERT_TRUE(nk_ui_store_delete_selected());
    TEST_ASSERT_EQUAL_INT(before - 1, nk_ui_store_count());
}

static void test_sub_modals_open_and_draw(void) {
    nk_open_help_st03();
    TEST_ASSERT_TRUE(nk_modal_is_open());
    lv_refr_now(s_display);
    TEST_ASSERT_TRUE(nk_ui_back());
    TEST_ASSERT_FALSE(nk_modal_is_open());

    nk_open_advice_list_ad01(nk_ui_store_latest());
    TEST_ASSERT_TRUE(nk_modal_is_open());
    lv_refr_now(s_display);
    nk_modal_close();

    nk_open_morning_summary_nt04();
    TEST_ASSERT_TRUE(nk_modal_is_open());
    lv_refr_now(s_display);
    nk_modal_close();

    nk_open_privacy_modal();
    TEST_ASSERT_TRUE(nk_modal_is_open());
    lv_refr_now(s_display);
    nk_modal_close();

    nk_open_license_modal();
    TEST_ASSERT_TRUE(nk_modal_is_open());
    lv_refr_now(s_display);
    nk_modal_close();
}

static void test_store_loads_latest_from_sqlite_and_deletes(void) {
    const char *key = "ui_db_test_key";
    TEST_ASSERT_EQUAL_INT(0, nk_db_open(":memory:", key, (int)strlen(key)));

    int64_t id1 = 0;
    int64_t id2 = 0;
    TEST_ASSERT_TRUE(nk_data_session_insert(1730240000, 1730261600, 74, &id1));
    TEST_ASSERT_TRUE(nk_data_session_insert(1730326400, 1730351600, 88, &id2));

    nk_ui_store_init();
    TEST_ASSERT_EQUAL_INT(2, nk_ui_store_count());
    TEST_ASSERT_EQUAL_INT64(id2, nk_ui_store_latest()->id);
    TEST_ASSERT_EQUAL_INT(88, nk_ui_store_latest()->score);

    TEST_ASSERT_TRUE(nk_ui_store_delete_selected());
    TEST_ASSERT_FALSE(nk_data_session_exists(id2));
    TEST_ASSERT_EQUAL_INT(1, nk_data_session_count());

    nk_db_close();
}

static void test_font_scale_settings_work(void) {
    TEST_ASSERT_EQUAL_INT(NK_FONT_SCALE_100, nk_theme_get_font_scale());
    TEST_ASSERT_EQUAL_INT32(15, nk_theme_scale_px(15));

    nk_theme_set_font_scale(NK_FONT_SCALE_120);
    TEST_ASSERT_EQUAL_INT(NK_FONT_SCALE_120, nk_theme_get_font_scale());
    TEST_ASSERT_EQUAL_INT32(18, nk_theme_scale_px(15));

    nk_theme_set_font_scale(NK_FONT_SCALE_140);
    TEST_ASSERT_EQUAL_INT(NK_FONT_SCALE_140, nk_theme_get_font_scale());
    TEST_ASSERT_EQUAL_INT32(21, nk_theme_scale_px(15));

    nk_theme_set_font_scale(NK_FONT_SCALE_100);
    nk_ui_open(NK_UI_SETTINGS);
    lv_refr_now(s_display);
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
    RUN_TEST(test_date_navigation_blocks_future_and_edits_record);
    RUN_TEST(test_sub_modals_open_and_draw);
    RUN_TEST(test_store_loads_latest_from_sqlite_and_deletes);
    RUN_TEST(test_font_scale_settings_work);
    int failures = UNITY_END();

    nk_ui_deinit();
    nk_theme_deinit();
    lv_deinit();
    return failures;
}
