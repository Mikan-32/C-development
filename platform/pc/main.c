#include "data_consent.h"
#include "lvgl.h"
#include "nk_db.h"
#include "ui_agreement.h"
#include "ui_font.h"
#include "ui_permissions.h"
#include "ui_privacy_settings.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string.h>

#define WINDOW_WIDTH  360
#define WINDOW_HEIGHT 640

static lv_display_t *s_disp = NULL;
static lv_indev_t *s_mouse_indev = NULL;
static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static bool s_running = true;

static void goto_permissions(void);
static void goto_privacy_settings(void);

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

static void goto_privacy_settings(void) {
    nk_ui_permissions_destroy();
    nk_ui_privacy_create(NULL);
}

static void goto_permissions(void) {
    nk_ui_agreement_destroy();
    nk_ui_permissions_create(goto_privacy_settings, request_sdl3_permissions);
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

    s_window = SDL_CreateWindow("nemunoki - Onboarding (UX-1)", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    s_renderer = SDL_CreateRenderer(s_window, NULL);
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  WINDOW_WIDTH, WINDOW_HEIGHT);

    uint32_t clear_pixels[WINDOW_WIDTH * WINDOW_HEIGHT];
    for (int i = 0; i < WINDOW_WIDTH * WINDOW_HEIGHT; i++) {
        clear_pixels[i] = 0xFFFFFFFF;
    }
    SDL_UpdateTexture(s_texture, NULL, clear_pixels, WINDOW_WIDTH * 4);

    lv_init();

    static uint32_t buf[WINDOW_WIDTH * 40];
    s_disp = lv_display_create(WINDOW_WIDTH, WINDOW_HEIGHT);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(s_disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);
    if (!nk_ui_font_init(s_disp, "assets/fonts/NotoSansJP-Regular.ttf", 16)) {
        SDL_Log("Warning: Failed to load NotoSansJP font. Falling back to default.");
    }

    s_mouse_indev = lv_indev_create();
    lv_indev_set_type(s_mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_mouse_indev, mouse_read_cb);

    // 初回同意 または 版改定による再同意が必要か判定
    if (nk_data_consent_needs_reagreement(NK_DATA_CONSENT_CURRENT_VERSION)) {
        bool is_reagree = nk_data_consent_is_agreed();
        nk_ui_agreement_create(goto_permissions, is_reagree);
    } else {
        // 既に最新版に同意済みなら ON-02 をスキップして ON-03 へ
        nk_ui_permissions_create(goto_privacy_settings, request_sdl3_permissions);
    }

    uint64_t last_tick = SDL_GetTicks();

    while (s_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                s_running = false;
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
    nk_ui_font_deinit();
    nk_db_close();

    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();

    return 0;
}
