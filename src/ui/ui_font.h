#pragma once

#include "lvgl.h"

#include <stdbool.h>

// TTFフォントを読み込んでLVGLのデフォルトテーマに適用する
bool nk_ui_font_init(lv_display_t *disp, const char *ttf_path, int32_t font_size);

// 確保したフォントとメモリを解放する
void nk_ui_font_deinit(void);
