#include "lvgl.h"
#include "ui_agreement.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WINDOW_WIDTH  360
#define WINDOW_HEIGHT 640

static lv_display_t *disp = NULL;
static lv_indev_t *mouse_indev = NULL;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static bool running = true;

// LVGLの描画完了時にSDLのテクスチャへピクセルを転送するコールバック
static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px_map) {
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    SDL_Rect rect = {area->x1, area->y1, w, h};
    // 32-bit (ARGB8888) なので 1ピクセルあたり 4バイト
    SDL_UpdateTexture(texture, &rect, px_map, w * 4);
    lv_display_flush_ready(d);
}

// マウス位置とクリック状態をLVGLに伝えるコールバック
static void mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    float mouse_x, mouse_y;
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mouse_x, &mouse_y);

    data->point.x = (int32_t)mouse_x;
    data->point.y = (int32_t)mouse_y;
    data->state = (buttons & SDL_BUTTON_LMASK) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("ねむのき - UI Preview (ON-02)", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, NULL);
    // SDL 側を ARGB8888 (32-bit) に設定
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                WINDOW_WIDTH, WINDOW_HEIGHT);

    // テクスチャの初期ノイズ（砂嵐）を防ぐため、白で初期クリア
    uint32_t clear_pixels[WINDOW_WIDTH * WINDOW_HEIGHT];
    for (int i = 0; i < WINDOW_WIDTH * WINDOW_HEIGHT; i++) {
        clear_pixels[i] = 0xFFFFFFFF; // 不透明な白色
    }
    SDL_UpdateTexture(texture, NULL, clear_pixels, WINDOW_WIDTH * 4);

    lv_init();

    // 描画バッファ（40行分 × 4バイト）
    static uint32_t buf[WINDOW_WIDTH * 40];
    disp = lv_display_create(WINDOW_WIDTH, WINDOW_HEIGHT);
    // LVGL 側も ARGB8888 に明示的に固定
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    // マウス入力デバイスの登録
    mouse_indev = lv_indev_create();
    lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse_indev, mouse_read_cb);

    // 同意画面のUIを作成して表示
    ui_agreement_create();

    uint64_t last_tick = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        uint64_t current_tick = SDL_GetTicks();
        lv_tick_inc((uint32_t)(current_tick - last_tick));
        last_tick = current_tick;

        lv_timer_handler();

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Delay(5);
    }

    ui_agreement_destroy();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
