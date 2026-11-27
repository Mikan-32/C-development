#ifndef NK_ANDROID_CONSENT_H
#define NK_ANDROID_CONSENT_H

#include "data.h"

#include <SDL3/SDL.h>

/*
 * 開発テスト用の加速度計測同意。
 * マイク同意は0で保存する。
 */
nk_result nk_android_consent_request(SDL_Window *window, nk_data *data, int *out_has_consent);

#endif
