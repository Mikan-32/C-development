#include "data_consent.h"
#include "lvgl.h"
#include "nk_db.h"
#include "nk_theme.h"
#include "ui.h"
#include "ui_agreement.h"
#include "ui_font.h"
#include "ui_permissions.h"
#include "ui_privacy_settings.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string.h>

#define WINDOW_WIDTH  390
#define WINDOW_HEIGHT 844

static lv_display_t *s_disp = NULL;
static lv_indev_t *s_mouse_indev = NULL;
static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static void *s_font_data = NULL;
static size_t s_font_size = 0;
static bool s_running = true;

static void goto_permissions(void);
static void goto_main_ui(void);

// SDL3 経由でマイクアクセスを試行し、結果を ON-03 画面へ反映する
static void request_sdl3_permissions(void) {
    SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, 16000};
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec);

    if (dev != 0) {
        SDL_CloseAudioDevice(dev);
        nk_ui_permissions_set_mic_status(NK_PERM_STATUS_GRANTED);
    } else {
        SDL_Log("Microphone permission/device denied: %s", SDL_GetError());
        nk_ui_permissions_set_mic_status(NK_PERM_STATUS_DENIED);
    }

    // PC版では通知権限は許可扱いとする
    nk_ui_permissions_set_notification_status(NK_PERM_STATUS_GRANTED);
}

// 次のイベントループ周回で安全に権限画面を破棄してホーム画面（HM-01）を開く
static void switch_to_main_ui_async(void *unused) {
    (void)unused;
    lv_obj_t *main_scr = lv_obj_create(NULL);
    lv_screen_load(main_scr);
    nk_ui_permissions_destroy();
    nk_ui_init();
}

static void goto_main_ui(void) {
    (void)lv_async_call(switch_to_main_ui_async, NULL);
}

static void switch_to_permissions_async(void *unused) {
    (void)unused;
    nk_ui_agreement_destroy();
    nk_ui_permissions_create(goto_main_ui, request_sdl3_permissions);
}

static void goto_permissions(void) {
    (void)lv_async_call(switch_to_permissions_async, NULL);
}

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px_map) {
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    SDL_Rect rect = {area->x1, area->y1, w, h};
    SDL_UpdateTexture(s_texture, &rect, px_map, w * 4);
    lv_display_flush_ready(d);
}

static void mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    data->point.x = (int32_t)mouse_x;
    data->point.y = (int32_t)mouse_y;
    data->state = (buttons & SDL_BUTTON_LMASK) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // DB初期化と保存済み同意状態の読み込み
    const char *dev_key = "nemunoki_dev_key";
    nk_db_open("nemunoki_dev.db", dev_key, (int)strlen(dev_key));
    nk_data_consent_init();

    s_window = SDL_CreateWindow("nemunoki", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    s_renderer = SDL_CreateRenderer(s_window, NULL);
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  WINDOW_WIDTH, WINDOW_HEIGHT);

    lv_init();

    static uint32_t buf[WINDOW_WIDTH * 40];
    s_disp = lv_display_create(WINDOW_WIDTH, WINDOW_HEIGHT);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(s_disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);

    // オンボーディング用フォント（ui_font）とメイン画面用テーマフォント（nk_theme）の両方を初期化
    if (!nk_ui_font_init(s_disp, "assets/fonts/NotoSansJP-Regular.ttf", 16)) {
        SDL_Log("Warning: Failed to load ui_font NotoSansJP.");
    }
    s_font_data = SDL_LoadFile("assets/fonts/NotoSansJP-Regular.ttf", &s_font_size);
    if (s_font_data == NULL) {
        SDL_Log("Warning: Failed to load NotoSansJP font for theme.");
    }
    nk_theme_init(s_font_data, s_font_size, NULL, 0);

    s_mouse_indev = lv_indev_create();
    lv_indev_set_type(s_mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_mouse_indev, mouse_read_cb);

    // 初回同意・再同意が必要ならオンボーディング、同意済みなら直接ホーム画面（HM-01）を開く
    if (nk_data_consent_needs_reagreement(NK_DATA_CONSENT_CURRENT_VERSION)) {
        bool is_reagree = nk_data_consent_is_agreed();
        nk_ui_agreement_create(goto_permissions, is_reagree);
    } else {
        nk_ui_init();
    }

    uint64_t last_tick = SDL_GetTicks();

    while (s_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                s_running = false;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                nk_ui_scroll_by((int32_t)(event.wheel.y * 40.0f));
            }
        }

        uint64_t current_tick = SDL_GetTicks();
        lv_tick_inc((uint32_t)(current_tick - last_tick));
        last_tick = current_tick;

        lv_timer_handler();

        SDL_RenderClear(s_renderer);
        SDL_RenderTexture(s_renderer, s_texture, NULL, NULL);
        SDL_RenderPresent(s_renderer);

        SDL_Delay(5);
    }

    nk_ui_agreement_destroy();
    nk_ui_permissions_destroy();
    nk_ui_privacy_destroy();
    nk_ui_deinit();
    nk_theme_deinit();
    nk_ui_font_deinit();
    if (s_font_data != NULL) {
        SDL_free(s_font_data);
    }
    nk_db_close();

    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();

    return 0;
}
