/*
 * 色と文字（UX-8 のデザイントークン）
 */
#ifndef NK_THEME_H
#define NK_THEME_H

#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>

#define NK_BG        lv_color_hex(0x0E1330) /* 画面の背景 */
#define NK_CARD      lv_color_hex(0x1A2147) /* カード */
#define NK_CARD_2    lv_color_hex(0x232B5A) /* カードの中の区画 */
#define NK_INK       lv_color_hex(0xF2F3FA) /* 主な文字 */
#define NK_INK_2     lv_color_hex(0xB8BCD8) /* 補助の文字 */
#define NK_INK_3     lv_color_hex(0x9298C2) /* 注記・目盛り */
#define NK_BRAND     lv_color_hex(0xF08CA8) /* 主ボタン、選択中のタブ */
#define NK_BRAND_INK lv_color_hex(0x2A1222) /* 主ボタンの上の文字 */
#define NK_SERIES    lv_color_hex(0x7788EE) /* グラフ */
#define NK_GOOD      lv_color_hex(0x5FD3A8) /* 良い・成功 */
#define NK_WARN      lv_color_hex(0xF2C26B) /* 注意 */
#define NK_SERIOUS   lv_color_hex(0xF09A6A) /* 整えどころ（赤は使わない） */
#define NK_SELECT    lv_color_hex(0x3A4596) /* 選んでいる項目 */
#define NK_TABBAR    lv_color_hex(0x121838) /* 下部タブ */
#define NK_WAKE      lv_color_hex(0xF2A58E) /* 睡眠段階：覚醒 */
#define NK_LIGHT     lv_color_hex(0x9AA6F5) /* 睡眠段階：浅い睡眠 */
#define NK_DEEP      lv_color_hex(0x4E5BC8) /* 睡眠段階：深い睡眠 */

/* 日本語フォント（TTF・TTC の中身）を受け取る。NULL なら内蔵の英字フォントで代用する */
void nk_theme_init(const void *regular, size_t regular_size, const void *bold, size_t bold_size);
void nk_theme_deinit(void);

/* px の大きさのフォント（初めて使う大きさのときに作る） */
const lv_font_t *nk_font(int32_t px, bool bold);

#endif /* NK_THEME_H */
