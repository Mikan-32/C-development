/*
 * ui：画面の一覧と状態の一覧を、ウィンドウなし（メモリ上の画面）で 1 つずつ作って描く。
 * 見た目は確かめない。見た目は PC 版の --shots で撮ったスクリーンショットで確かめる
 *（ADR-0011 の段 4）
 */
#include "lvgl.h"
#include "ui.h"
#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 390
#define SCREEN_H 844

static lv_display_t *s_display;
static uint8_t *s_buffer;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

/* 画面の切り替えは lv_async_call で次の周回に行うため、タイマーを回す */
static void run_pending(void) {
    for (int i = 0; i < 3; i++) {
        (void)lv_timer_handler();
    }
}

void setUp(void) {
}

void tearDown(void) {
}

static void test_names_round_trip(void) {
    for (int i = 0; i < NK_UI_SCREEN_COUNT; i++) {
        nk_ui_screen_id id = (nk_ui_screen_id)i;
        TEST_ASSERT_EQUAL_INT(id, nk_ui_screen_from_name(nk_ui_screen_name(id)));
    }
    for (int i = 0; i < NK_UI_STATE_COUNT; i++) {
        nk_ui_state st = (nk_ui_state)i;
        TEST_ASSERT_EQUAL_INT(st, nk_ui_state_from_name(nk_ui_state_name(st)));
    }
    TEST_ASSERT_EQUAL_INT(NK_UI_HM01, nk_ui_screen_from_name("hm-01"));
    TEST_ASSERT_EQUAL_INT(NK_UI_ST_ANALYZING, nk_ui_state_from_name("ANALYZING"));
    TEST_ASSERT_EQUAL_INT(NK_UI_SCREEN_COUNT, nk_ui_screen_from_name("XX-99"));
    TEST_ASSERT_EQUAL_INT(NK_UI_STATE_COUNT, nk_ui_state_from_name(NULL));
}

static void test_every_screen_and_state_can_be_drawn(void) {
    size_t count = nk_ui_shot_count();
    TEST_ASSERT_TRUE(count >= (size_t)NK_UI_SCREEN_COUNT);
    for (size_t i = 0; i < count; i++) {
        nk_ui_screen_id screen;
        nk_ui_state state;
        TEST_ASSERT_TRUE(nk_ui_shot_get(i, &screen, &state));
        TEST_ASSERT_TRUE_MESSAGE(nk_ui_open(screen, state), nk_ui_screen_name(screen));
        lv_refr_now(s_display);
        TEST_ASSERT_EQUAL_INT(screen, nk_ui_current_screen());
        TEST_ASSERT_EQUAL_INT(state, nk_ui_current_state());
        TEST_ASSERT_NOT_NULL(lv_screen_active());
    }
    nk_ui_screen_id screen;
    nk_ui_state state;
    TEST_ASSERT_FALSE(nk_ui_shot_get(count, &screen, &state));
}

static void test_back_from_tab_goes_home(void) {
    TEST_ASSERT_TRUE(nk_ui_open(NK_UI_LG01, NK_UI_ST_NONE));
    TEST_ASSERT_TRUE(nk_ui_back());
    run_pending();
    TEST_ASSERT_EQUAL_INT(NK_UI_HM01, nk_ui_current_screen());
    /* ホームで戻ると、アプリを閉じる（呼び出し側に任せる） */
    TEST_ASSERT_FALSE(nk_ui_back());
}

static void test_back_is_ignored_while_tracking(void) {
    TEST_ASSERT_TRUE(nk_ui_open(NK_UI_NT02, NK_UI_ST_TRACKING));
    TEST_ASSERT_TRUE(nk_ui_back());
    run_pending();
    TEST_ASSERT_EQUAL_INT(NK_UI_NT02, nk_ui_current_screen());
}

static void test_invalid_ids_are_rejected(void) {
    TEST_ASSERT_FALSE(nk_ui_open(NK_UI_SCREEN_COUNT, NK_UI_ST_NONE));
    TEST_ASSERT_FALSE(nk_ui_open(NK_UI_HM01, NK_UI_STATE_COUNT));
}

int main(void) {
    lv_init();
    s_display = lv_display_create(SCREEN_W, SCREEN_H);
    uint32_t stride = lv_draw_buf_width_to_stride(SCREEN_W, LV_COLOR_FORMAT_XRGB8888);
    s_buffer = malloc((size_t)stride * SCREEN_H);
    if (s_buffer == NULL) {
        return 1;
    }
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(s_display, s_buffer, NULL, stride * SCREEN_H,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_display, flush_cb);

    nk_ui_config config;
    memset(&config, 0, sizeof(config));
    config.instant = true;
    config.show_status_bar = true;
    nk_ui_init(&config);

    UNITY_BEGIN();
    RUN_TEST(test_names_round_trip);
    RUN_TEST(test_every_screen_and_state_can_be_drawn);
    RUN_TEST(test_back_from_tab_goes_home);
    RUN_TEST(test_back_is_ignored_while_tracking);
    RUN_TEST(test_invalid_ids_are_rejected);
    int failures = UNITY_END();

    nk_ui_deinit();
    lv_deinit();
    free(s_buffer);
    return failures;
}
