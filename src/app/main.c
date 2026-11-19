/*
 * ねむのきの入口（PC 版）：SDL3 のウィンドウに、LVGL で作った画面を出す
 *
 * 操作：クリックで押す、ホイールで上下にスクロール、Esc・Backspace で戻る
 */
#include "lvgl.h"
#include "nk_theme.h"
#include "ui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include <stdint.h>

#define LOGICAL_W 390
#define LOGICAL_H 844

static struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    lv_display_t *display;
    lv_indev_t *pointer;
    uint8_t *framebuffer;
    uint32_t stride;
    float mouse_x;
    float mouse_y;
    bool mouse_down;
    bool dirty;
    void *font_regular;
    size_t font_regular_size;
    void *font_bold;
    size_t font_bold_size;
} s_app;

/* ---------------------------------------------------------------- フォント */

/* 候補を順に試して、最初に読めたファイルの中身を返す（呼び出し側が SDL_free する） */
static void *load_font(const char *const *candidates, size_t count, size_t *size) {
    for (size_t i = 0; i < count; i++) {
        void *data = SDL_LoadFile(candidates[i], size);
        if (data != NULL) {
            SDL_Log("フォント: %s", candidates[i]);
            return data;
        }
    }
    return NULL;
}

/* OS の日本語フォントを使う（Windows、Linux、macOS の順に探す） */
static void load_fonts(void) {
    const char *windir = SDL_getenv("WINDIR");
    char win_r[2][260];
    char win_b[2][260];
    static const char *const names_r[2] = {"YuGothM.ttc", "meiryo.ttc"};
    static const char *const names_b[2] = {"YuGothB.ttc", "meiryob.ttc"};
    for (int i = 0; i < 2; i++) {
        (void)SDL_snprintf(win_r[i], sizeof(win_r[i]), "%s/Fonts/%s",
                           windir != NULL ? windir : "C:/Windows", names_r[i]);
        (void)SDL_snprintf(win_b[i], sizeof(win_b[i]), "%s/Fonts/%s",
                           windir != NULL ? windir : "C:/Windows", names_b[i]);
    }
    const char *const regular[] = {
        win_r[0],
        win_r[1],
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/System/Library/Fonts/ヒラギノ角ゴシック W4.ttc",
    };
    const char *const bold[] = {
        win_b[0],
        win_b[1],
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
        "/System/Library/Fonts/ヒラギノ角ゴシック W7.ttc",
    };
    s_app.font_regular = load_font(regular, SDL_arraysize(regular), &s_app.font_regular_size);
    s_app.font_bold = load_font(bold, SDL_arraysize(bold), &s_app.font_bold_size);
    if (s_app.font_regular == NULL && s_app.font_bold == NULL) {
        SDL_Log("日本語フォントが見つかりません（日本語が表示されません）");
    }
}

/* ---------------------------------------------------------------- LVGL の表示先 */

static uint32_t tick_cb(void) {
    return (uint32_t)SDL_GetTicks();
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    (void)area;
    (void)px_map;
    /* DIRECT モードなので、描画はもう framebuffer に入っている。最後の領域で画面に出す */
    if (lv_display_flush_is_last(disp)) {
        s_app.dirty = true;
    }
    lv_display_flush_ready(disp);
}

static void pointer_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    data->point.x = (int32_t)s_app.mouse_x;
    data->point.y = (int32_t)s_app.mouse_y;
    data->state = s_app.mouse_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static bool create_display(void) {
    s_app.stride = lv_draw_buf_width_to_stride(LOGICAL_W, LV_COLOR_FORMAT_XRGB8888);
    size_t size = (size_t)s_app.stride * LOGICAL_H;
    s_app.framebuffer = SDL_calloc(1, size);
    if (s_app.framebuffer == NULL) {
        return false;
    }
    s_app.display = lv_display_create(LOGICAL_W, LOGICAL_H);
    lv_display_set_color_format(s_app.display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(s_app.display, s_app.framebuffer, NULL, (uint32_t)size,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_app.display, flush_cb);

    s_app.pointer = lv_indev_create();
    lv_indev_set_type(s_app.pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_app.pointer, pointer_read_cb);
    lv_indev_set_display(s_app.pointer, s_app.display);
    return true;
}

/* ---------------------------------------------------------------- ウィンドウ */

/* 画面の拡大率に合わせつつ、縦が画面に収まる大きさにする */
static float choose_zoom(void) {
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    float zoom = SDL_GetDisplayContentScale(display);
    if (zoom <= 0.0f) {
        zoom = 1.0f;
    }
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(display, &usable)) {
        float fit = (float)(usable.h - 60) / (float)LOGICAL_H;
        if (fit < zoom) {
            zoom = fit;
        }
    }
    return zoom < 0.5f ? 0.5f : zoom;
}

static bool create_window(void) {
    float zoom = choose_zoom();
    int w = (int)((float)LOGICAL_W * zoom + 0.5f);
    int h = (int)((float)LOGICAL_H * zoom + 0.5f);
    if (!SDL_CreateWindowAndRenderer("ねむのき（PC 版）", w, h, SDL_WINDOW_RESIZABLE, &s_app.window,
                                     &s_app.renderer)) {
        SDL_Log("ウィンドウを作れませんでした: %s", SDL_GetError());
        return false;
    }
    (void)SDL_SetRenderVSync(s_app.renderer, 1);
    (void)SDL_SetRenderLogicalPresentation(s_app.renderer, LOGICAL_W, LOGICAL_H,
                                           SDL_LOGICAL_PRESENTATION_LETTERBOX);
    s_app.texture = SDL_CreateTexture(s_app.renderer, SDL_PIXELFORMAT_XRGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, LOGICAL_W, LOGICAL_H);
    if (s_app.texture == NULL) {
        SDL_Log("テクスチャを作れませんでした: %s", SDL_GetError());
        return false;
    }
    (void)SDL_SetTextureScaleMode(s_app.texture, SDL_SCALEMODE_LINEAR);
    return true;
}

static void present(void) {
    (void)SDL_UpdateTexture(s_app.texture, NULL, s_app.framebuffer, (int)s_app.stride);
    (void)SDL_SetRenderDrawColor(s_app.renderer, 7, 10, 28, 255);
    (void)SDL_RenderClear(s_app.renderer);
    (void)SDL_RenderTexture(s_app.renderer, s_app.texture, NULL, NULL);
    (void)SDL_RenderPresent(s_app.renderer);
}

static void handle_event(SDL_Event *e, bool *running) {
    switch (e->type) {
    case SDL_EVENT_QUIT:
        *running = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        (void)SDL_ConvertEventToRenderCoordinates(s_app.renderer, e);
        s_app.mouse_x = e->motion.x;
        s_app.mouse_y = e->motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e->button.button == SDL_BUTTON_LEFT) {
            (void)SDL_ConvertEventToRenderCoordinates(s_app.renderer, e);
            s_app.mouse_x = e->button.x;
            s_app.mouse_y = e->button.y;
            s_app.mouse_down = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        nk_ui_scroll_by((int32_t)(e->wheel.y * 80.0f));
        break;
    case SDL_EVENT_KEY_DOWN:
        if (e->key.key == SDLK_ESCAPE || e->key.key == SDLK_BACKSPACE) {
            (void)nk_ui_back();
        }
        break;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        s_app.dirty = true;
        break;
    default:
        break;
    }
}

static void run(void) {
    bool running = true;
    while (running) {
        uint32_t wait = lv_timer_handler();
        if (s_app.dirty) {
            s_app.dirty = false;
            present();
        }
        if (wait > 16) {
            wait = 16;
        }
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, (Sint32)wait)) {
            do {
                handle_event(&e, &running);
            } while (SDL_PollEvent(&e));
        }
    }
}

/* ---------------------------------------------------------------- 入口 */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    (void)SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_INFO, "");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL を初期化できませんでした: %s", SDL_GetError());
        return 1;
    }
    load_fonts();
    lv_init();
    lv_tick_set_cb(tick_cb);

    int result = 1;
    if (create_display() && create_window()) {
        nk_theme_init(s_app.font_regular, s_app.font_regular_size, s_app.font_bold,
                      s_app.font_bold_size);
        nk_ui_init();
        s_app.dirty = true;
        run();
        nk_ui_deinit();
        nk_theme_deinit();
        result = 0;
    }

    lv_deinit();
    SDL_free(s_app.framebuffer);
    SDL_free(s_app.font_regular);
    SDL_free(s_app.font_bold);
    if (s_app.texture != NULL) {
        SDL_DestroyTexture(s_app.texture);
    }
    if (s_app.renderer != NULL) {
        SDL_DestroyRenderer(s_app.renderer);
    }
    if (s_app.window != NULL) {
        SDL_DestroyWindow(s_app.window);
    }
    SDL_Quit();
    return result;
}
