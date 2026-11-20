#ifndef NK_ANDROID_NIGHT_H
#define NK_ANDROID_NIGHT_H

#include "data.h"

#include <SDL3/SDL.h>

/*
 * SDLのメインスレッドから直列に呼ぶ。
 * この段階では夜間サービスだけを操作する。
 */
void nk_android_night_ui_init(void);

nk_result nk_android_night_ui_event(nk_data *data, SDL_Renderer *renderer, const SDL_Event *event);

nk_result nk_android_night_ui_draw(SDL_Renderer *renderer);

nk_result nk_android_night_ui_shutdown(void);

#endif
