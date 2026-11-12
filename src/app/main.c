/*
 * ねむのきの入口（PC 版）：SDL3 のウィンドウに、LVGL で作った画面を出す
 *
 * 画面の試作を確かめるための CLI を持つ（ADR-0011 の段 4「画面の確認」）。
 *   nemunoki                         ホームから起動
 *   nemunoki --screen BR-01          画面を指定して起動（--list で一覧）
 *   nemunoki --screen HM-01 --state ST-INSUFFICIENT
 *   nemunoki --shots build/shots     すべての画面と状態を PNG で保存して終了（ウィンドウなし）
 * 起動中のキー：Esc・Backspace 戻る、F2 スクリーンショット、F12 画面と状態の一覧
 */
#include "lvgl.h"
#include "ui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define LOGICAL_W 390
#define LOGICAL_H 844

typedef struct options {
    const char *screen;
    const char *state;
    const char *shot;
    const char *shots_dir;
    const char *font_regular;
    const char *font_bold;
    float zoom;
    float font_scale;
    int scroll;
    bool reduce_motion;
    bool list;
    bool help;
} options;

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

/* ---------------------------------------------------------------- 引数 */

static void print_help(void) {
    SDL_Log("ねむのき（PC 版・画面の試作）\n"
            "\n"
            "使い方: nemunoki [オプション]\n"
            "  --screen <ID>      開く画面（例: HM-01、LG-02、NT-01、BR-01、ST-02、DEV）\n"
            "  --state <ID>       画面に出す状態（例: ST-ANALYZING、ST-CURTAIN-FAILED）\n"
            "  --list             画面と状態の一覧を表示して終了\n"
            "  --shot <file.png>  --screen の画面を 1 枚保存して終了（ウィンドウなし）\n"
            "  --shots <dir>      すべての画面と状態を PNG で保存して終了（ウィンドウなし）\n"
            "  --scroll <px>      開いた画面を下へスクロールしておく（長い画面の撮影用）\n"
            "  --zoom <倍率>       ウィンドウの拡大率（既定：画面に収まる大きさ）\n"
            "  --font-scale <n>   文字の大きさ 1.0（標準）・1.2（大）・1.4（特大）\n"
            "  --reduce-motion    動きを減らす\n"
            "  --font <file>      本文のフォント（TTF・TTC）\n"
            "  --font-bold <file> 太字のフォント\n"
            "\n"
            "起動中のキー: Esc・Backspace 戻る / F2 スクリーンショット / F12 画面と状態の一覧");
}

static bool parse_args(int argc, char *argv[], options *o) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *next = i + 1 < argc ? argv[i + 1] : NULL;
        bool takes_value = true;
        if (SDL_strcmp(a, "--screen") == 0 && next != NULL) {
            o->screen = next;
        } else if (SDL_strcmp(a, "--state") == 0 && next != NULL) {
            o->state = next;
        } else if (SDL_strcmp(a, "--shot") == 0 && next != NULL) {
            o->shot = next;
        } else if (SDL_strcmp(a, "--shots") == 0 && next != NULL) {
            o->shots_dir = next;
        } else if (SDL_strcmp(a, "--zoom") == 0 && next != NULL) {
            o->zoom = (float)SDL_atof(next);
        } else if (SDL_strcmp(a, "--scroll") == 0 && next != NULL) {
            o->scroll = SDL_atoi(next);
        } else if (SDL_strcmp(a, "--font-scale") == 0 && next != NULL) {
            o->font_scale = (float)SDL_atof(next);
        } else if (SDL_strcmp(a, "--font") == 0 && next != NULL) {
            o->font_regular = next;
        } else if (SDL_strcmp(a, "--font-bold") == 0 && next != NULL) {
            o->font_bold = next;
        } else {
            takes_value = false;
            if (SDL_strcmp(a, "--reduce-motion") == 0) {
                o->reduce_motion = true;
            } else if (SDL_strcmp(a, "--list") == 0) {
                o->list = true;
            } else if (SDL_strcmp(a, "--help") == 0 || SDL_strcmp(a, "-h") == 0) {
                o->help = true;
            } else {
                SDL_Log("知らないオプションです: %s（--help で使い方）", a);
                return false;
            }
        }
        if (takes_value) {
            i++;
        }
    }
    return true;
}

static void print_list(void) {
    SDL_Log("画面と、その画面で確認できる状態:");
    nk_ui_screen_id last = NK_UI_SCREEN_COUNT;
    char line[512];
    line[0] = '\0';
    size_t count = nk_ui_shot_count();
    for (size_t i = 0; i <= count; i++) {
        nk_ui_screen_id screen = NK_UI_SCREEN_COUNT;
        nk_ui_state state = NK_UI_ST_NONE;
        bool ok = i < count && nk_ui_shot_get(i, &screen, &state);
        if (!ok || screen != last) {
            if (last != NK_UI_SCREEN_COUNT) {
                SDL_Log("%s", line);
            }
            if (!ok) {
                break;
            }
            last = screen;
            (void)SDL_snprintf(line, sizeof(line), "  %-6s %s:", nk_ui_screen_name(screen),
                               nk_ui_screen_title(screen));
        }
        (void)SDL_strlcat(line, " ", sizeof(line));
        (void)SDL_strlcat(line, nk_ui_state_name(state), sizeof(line));
    }
}

/* ---------------------------------------------------------------- フォント */

/* 候補を順に試して、最初に読めたファイルの中身を返す（呼び出し側が SDL_free する） */
static void *load_font(const char *const *candidates, size_t count, size_t *size) {
    for (size_t i = 0; i < count; i++) {
        if (candidates[i] == NULL) {
            continue;
        }
        void *data = SDL_LoadFile(candidates[i], size);
        if (data != NULL) {
            SDL_Log("フォント: %s", candidates[i]);
            return data;
        }
    }
    return NULL;
}

static void load_fonts(const options *o) {
    /* 1. 指定されたファイル 2. アプリに同梱したフォント（今後） 3. OS の日本語フォント */
    const char *base = SDL_GetBasePath();
    const char *windir = SDL_getenv("WINDIR");
    char bundled_r[512];
    char bundled_b[512];
    char win_r[3][260];
    char win_b[3][260];
    (void)SDL_snprintf(bundled_r, sizeof(bundled_r), "%sassets/fonts/NotoSansJP-Regular.ttf",
                       base != NULL ? base : "");
    (void)SDL_snprintf(bundled_b, sizeof(bundled_b), "%sassets/fonts/NotoSansJP-Bold.ttf",
                       base != NULL ? base : "");
    static const char *const win_names_r[] = {"YuGothM.ttc", "meiryo.ttc", "BIZ-UDGothicR.ttc"};
    static const char *const win_names_b[] = {"YuGothB.ttc", "meiryob.ttc", "BIZ-UDGothicB.ttc"};
    for (int i = 0; i < 3; i++) {
        (void)SDL_snprintf(win_r[i], sizeof(win_r[i]), "%s/Fonts/%s",
                           windir != NULL ? windir : "C:/Windows", win_names_r[i]);
        (void)SDL_snprintf(win_b[i], sizeof(win_b[i]), "%s/Fonts/%s",
                           windir != NULL ? windir : "C:/Windows", win_names_b[i]);
    }
    const char *regular[] = {
        o->font_regular,
        bundled_r,
        win_r[0],
        win_r[1],
        win_r[2],
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/System/Library/Fonts/ヒラギノ角ゴシック W4.ttc",
    };
    const char *bold[] = {
        o->font_bold,
        bundled_b,
        win_b[0],
        win_b[1],
        win_b[2],
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc",
        "/System/Library/Fonts/ヒラギノ角ゴシック W7.ttc",
    };
    s_app.font_regular = load_font(regular, SDL_arraysize(regular), &s_app.font_regular_size);
    s_app.font_bold = load_font(bold, SDL_arraysize(bold), &s_app.font_bold_size);
    if (s_app.font_regular == NULL && s_app.font_bold == NULL) {
        SDL_Log("日本語フォントが見つかりません。--font で TTF を指定してください"
                "（このままでは日本語が表示されません）");
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

static bool save_png(const char *path) {
    SDL_Surface *surface = SDL_CreateSurfaceFrom(LOGICAL_W, LOGICAL_H, SDL_PIXELFORMAT_XRGB8888,
                                                 s_app.framebuffer, (int)s_app.stride);
    if (surface == NULL) {
        return false;
    }
    bool ok = SDL_SavePNG(surface, path);
    SDL_DestroySurface(surface);
    if (!ok) {
        SDL_Log("保存できませんでした: %s（%s）", path, SDL_GetError());
    }
    return ok;
}

/* いまの画面を描いて保存する */
static bool render_and_save(const char *path, int scroll) {
    if (scroll > 0) {
        nk_ui_scroll_to(scroll);
    }
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(s_app.display);
    return save_png(path);
}

/* ---------------------------------------------------------------- スクリーンショットだけ撮る */

static int run_shots(const options *o) {
    if (o->shot != NULL) {
        if (!render_and_save(o->shot, o->scroll)) {
            return 1;
        }
        SDL_Log("保存しました: %s", o->shot);
        return 0;
    }
    if (!SDL_CreateDirectory(o->shots_dir)) {
        SDL_Log("フォルダを作れませんでした: %s", o->shots_dir);
        return 1;
    }
    size_t count = nk_ui_shot_count();
    int saved = 0;
    for (size_t i = 0; i < count; i++) {
        nk_ui_screen_id screen;
        nk_ui_state state;
        if (!nk_ui_shot_get(i, &screen, &state) || !nk_ui_open(screen, state)) {
            continue;
        }
        char path[512];
        if (state == NK_UI_ST_NONE) {
            (void)SDL_snprintf(path, sizeof(path), "%s/%s.png", o->shots_dir,
                               nk_ui_screen_name(screen));
        } else {
            (void)SDL_snprintf(path, sizeof(path), "%s/%s_%s.png", o->shots_dir,
                               nk_ui_screen_name(screen), nk_ui_state_name(state));
        }
        if (render_and_save(path, o->scroll)) {
            saved++;
        }
    }
    SDL_Log("%d 枚を保存しました: %s", saved, o->shots_dir);
    return saved == (int)count ? 0 : 1;
}

/* ---------------------------------------------------------------- ウィンドウ */

static float choose_zoom(float requested) {
    if (requested > 0.1f) {
        return requested;
    }
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

static bool create_window(float zoom) {
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

static void screenshot_now(void) {
    char path[256];
    nk_ui_screen_id screen = nk_ui_current_screen();
    nk_ui_state state = nk_ui_current_state();
    (void)SDL_snprintf(path, sizeof(path), "nemunoki_%s_%s_%llu.png", nk_ui_screen_name(screen),
                       nk_ui_state_name(state), (unsigned long long)SDL_GetTicks());
    if (save_png(path)) {
        SDL_Log("スクリーンショットを保存しました: %s", path);
    }
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
        nk_ui_scroll_by((int)(e->wheel.y * 80.0f));
        break;
    case SDL_EVENT_KEY_DOWN:
        if (e->key.key == SDLK_ESCAPE || e->key.key == SDLK_BACKSPACE ||
            e->key.key == SDLK_AC_BACK) {
            /* ホームで戻ると、Android ではアプリを閉じる（UX-2 の 4 章）。PC 版は閉じない */
            (void)nk_ui_back();
        } else if (e->key.key == SDLK_F2) {
            screenshot_now();
        } else if (e->key.key == SDLK_F12) {
            (void)nk_ui_open(NK_UI_DEV, NK_UI_ST_NONE);
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

static void run_window(void) {
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
    (void)SDL_SetLogPriorityPrefix(SDL_LOG_PRIORITY_INFO, "");
    options o;
    SDL_zero(o);
    o.font_scale = 1.0f;
    if (!parse_args(argc, argv, &o)) {
        return 2;
    }
    if (o.help) {
        print_help();
        return 0;
    }
    if (o.list) {
        print_list();
        return 0;
    }

    nk_ui_screen_id screen = NK_UI_HM01;
    nk_ui_state state = NK_UI_ST_NONE;
    if (o.screen != NULL) {
        screen = nk_ui_screen_from_name(o.screen);
        if (screen == NK_UI_SCREEN_COUNT) {
            SDL_Log("画面 %s はありません（--list で一覧）", o.screen);
            return 2;
        }
    }
    if (o.state != NULL) {
        state = nk_ui_state_from_name(o.state);
        if (state == NK_UI_STATE_COUNT) {
            SDL_Log("状態 %s はありません（--list で一覧）", o.state);
            return 2;
        }
    }

    bool headless = o.shot != NULL || o.shots_dir != NULL;
    (void)SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
    (void)SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_Init(headless ? 0 : SDL_INIT_VIDEO)) {
        SDL_Log("SDL を初期化できませんでした: %s", SDL_GetError());
        return 1;
    }
    load_fonts(&o);

    lv_init();
    lv_tick_set_cb(tick_cb);
    int result = 0;
    if (!create_display()) {
        result = 1;
    } else {
        nk_ui_config cfg;
        SDL_zero(cfg);
        cfg.font_regular = s_app.font_regular;
        cfg.font_regular_size = s_app.font_regular_size;
        cfg.font_bold = s_app.font_bold;
        cfg.font_bold_size = s_app.font_bold_size;
        cfg.font_scale = o.font_scale;
        cfg.reduce_motion = o.reduce_motion;
        cfg.instant = headless;
        cfg.show_status_bar = true;
        nk_ui_init(&cfg);
        (void)nk_ui_open(screen, state);
        if (o.scroll > 0) {
            nk_ui_scroll_to(o.scroll);
        }

        if (headless) {
            result = run_shots(&o);
        } else if (create_window(choose_zoom(o.zoom))) {
            s_app.dirty = true;
            run_window();
        } else {
            result = 1;
        }
        nk_ui_deinit();
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
