/*
 * LVGL の設定（ねむのき）
 *
 * ここに書いていない項目は、LVGL の既定値を使う
 * （third_party/lvgl/include/lvgl/config/lv_conf_internal.h）。
 * 項目の意味は third_party/lvgl/lv_conf_template.h を参照。
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ---- 色とメモリ ---- */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_XRGB8888 /* SDL のテクスチャにそのまま渡す */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/* ---- 描画 ---- */
#define LV_USE_OS          LV_OS_NONE
#define LV_DEF_REFR_PERIOD 16 /* 約 60 fps */
#define LV_DPI_DEF         160
#define LV_USE_DRAW_SW     1
#define LV_DRAW_SW_COMPLEX 1 /* 角丸・円弧・アンチエイリアス */

/* ---- ログ（警告以上を printf で出す）と、内部の確認に失敗したとき ---- */
#define LV_USE_LOG                   1
#define LV_LOG_LEVEL                 LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF                1
#define LV_ASSERT_USE_CUSTOM_INCLUDE 1
#define LV_ASSERT_CUSTOM_INCLUDE     "nk_lv_assert.h"

/* ---- フォント ----
 * 日本語は Noto Sans JP などの TTF を Tiny TTF で読み込む（src/ui/nk_theme.c）。
 * Montserrat は、TTF が見つからないときと記号の予備に使う */
#define LV_FONT_MONTSERRAT_12       1
#define LV_FONT_MONTSERRAT_14       1
#define LV_FONT_MONTSERRAT_16       1
#define LV_FONT_MONTSERRAT_20       1
#define LV_FONT_MONTSERRAT_28       1
#define LV_FONT_MONTSERRAT_40       1
#define LV_FONT_DEFAULT             LV_FONT_DEFAULT_MONTSERRAT_14
#define LV_USE_TINY_TTF             1
#define LV_TINY_TTF_FILE_SUPPORT    0 /* フォントはアプリがメモリに読み込んで渡す */
#define LV_TINY_TTF_CACHE_GLYPH_CNT 256

/* ---- テーマ ---- */
#define LV_USE_THEME_DEFAULT             1
#define LV_THEME_DEFAULT_DARK            1
#define LV_THEME_DEFAULT_GROW            0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* ---- 使わないもの ---- */
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS    0
#define LV_USE_SYSMON     0

#endif /* LV_CONF_H */
