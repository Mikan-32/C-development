/*
 * 色と文字（デザイントークン）。値は docs/ux/UX-8_expression-a11y.md と画面モック v2
 */
#ifndef NK_THEME_H
#define NK_THEME_H

#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- 色 ---- */
#define NK_BG        lv_color_hex(0x0E1330) /* 画面の背景 */
#define NK_BG_2      lv_color_hex(0x141A3D) /* シートの背景 */
#define NK_CARD      lv_color_hex(0x1A2147) /* カード */
#define NK_CARD_2    lv_color_hex(0x232B5A) /* カードの中の区画 */
#define NK_CARD_3    lv_color_hex(0x2C356E) /* スイッチ・スライダーの溝 */
#define NK_INK       lv_color_hex(0xF2F3FA) /* 主な文字 */
#define NK_INK_2     lv_color_hex(0xB8BCD8) /* 補助の文字 */
#define NK_INK_3     lv_color_hex(0x9298C2) /* 注記・目盛り */
#define NK_BRAND     lv_color_hex(0xF08CA8) /* 主ボタン、選択中のタブ、リンク */
#define NK_BRAND_INK lv_color_hex(0x2A1222) /* 主ボタンの上の文字 */
#define NK_SERIES    lv_color_hex(0x7788EE) /* グラフ（単一系列） */
#define NK_SERIES_LT lv_color_hex(0xC3CAFF) /* 薄い背景の上のアイコン */
#define NK_GOOD      lv_color_hex(0x5FD3A8) /* スコア 70 以上、成功 */
#define NK_WARN      lv_color_hex(0xF2C26B) /* スコア 55〜69、未確認 */
#define NK_SERIOUS   lv_color_hex(0xF09A6A) /* スコア 55 未満、失敗（赤は使わない） */
#define NK_SELECT    lv_color_hex(0x3A4596) /* 選んだ日付・切り替えの選択中 */
#define NK_TABBAR    lv_color_hex(0x121838) /* タブバー */
#define NK_ADVICE    lv_color_hex(0x24204A) /* アドバイスのカード */
#define NK_NIGHT     lv_color_hex(0x05070F) /* 計測中・鳴動の画面の背景 */
#define NK_NIGHT_INK lv_color_hex(0x8A7F9E) /* 計測中の文字（まぶしさを抑える） */
#define NK_NIGHT_SUB lv_color_hex(0x6E6585) /* 計測中の補助の文字 */
#define NK_SPIN      lv_color_hex(0xAEB7FF) /* くるくるの色 */
#define NK_WHITE     lv_color_hex(0xFFFFFF)

/* 眠りの帯の色 */
#define NK_COLOR_WAKE  lv_color_hex(0xF2C26B)
#define NK_COLOR_LIGHT lv_color_hex(0x9AA6F5)
#define NK_COLOR_DEEP  lv_color_hex(0x4E5BC8)
#define NK_COLOR_SLEEP lv_color_hex(0x8E99E8) /* 浅い・深いを分けないとき */

/* 白の透明度で表す線（--line、--line-2） */
#define NK_OPA_LINE   20 /* 8% */
#define NK_OPA_LINE_2 41 /* 16% */

/* ---- 形 ---- */
#define NK_RADIUS       20 /* カード */
#define NK_RADIUS_S     14 /* カードの中の区画 */
#define NK_PAD          16 /* 画面の左右の余白・カードの内側の余白 */
#define NK_GAP          12 /* カードどうしの間 */
#define NK_SCREEN_W     390
#define NK_SCREEN_H     844
#define NK_STATUS_H     44 /* 上の余白（PC 版はここに時刻と電池を描く） */
#define NK_TABBAR_H     82 /* タブバー（下の余白を含む） */
#define NK_HOME_IND_H   20 /* タブバーがない画面の下の余白 */
#define NK_CONTENT_W    (NK_SCREEN_W - 2 * NK_PAD)
#define NK_CARD_INNER_W (NK_CONTENT_W - 2 * NK_PAD - 2) /* カードの中の幅（枠線 1px を引く） */
#define NK_TOUCH_MIN    48 /* 押せるものの最小の大きさ（UX-8 の 3 章） */

/* ---- 文字 ---- */

/*
 * 文字の準備。regular・bold は TTF（または TTC）の中身で、呼び出し側が最後まで保持する。
 * NULL のときは LVGL 内蔵の Montserrat で代用する（日本語は表示されない。テスト用）。
 * scale はアプリ内の文字サイズ（1.0 標準、1.2 大、1.4 特大）
 */
void nk_theme_init(const void *regular, size_t regular_size, const void *bold, size_t bold_size,
                   float scale);

/* 文字サイズを変える。今のフォントは「引退」させておき、
 * 画面を作り直したあとで nk_theme_release_retired で捨てる */
void nk_theme_set_font_scale(float scale);
float nk_theme_font_scale(void);
void nk_theme_release_retired(void);
void nk_theme_deinit(void);

/* 論理 px の大きさのフォント。初めて使う大きさのときに作る */
const lv_font_t *nk_font(int32_t px, bool bold);

#define NK_FR(px) nk_font((px), false)
#define NK_FB(px) nk_font((px), true)

/* スコアの色（AI-4 の 2 章）。参考値は ink-3 */
lv_color_t nk_theme_score_color(int score, bool reference);
const char *nk_theme_score_label(int score);

#endif /* NK_THEME_H */
