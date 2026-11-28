/*
 * 画面（LVGL）：改訂版の画面図（UX-1・3・4・5）に合わせた構成
 */
#ifndef NK_UI_H
#define NK_UI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum nk_ui_screen {
    NK_UI_HOME,            /* HM-01 ホーム */
    NK_UI_RECORD_DAY,      /* LG-01 睡眠記録（日） */
    NK_UI_RECORD_WEEK,     /* LG-01 睡眠記録（週） */
    NK_UI_RECORD_DETAIL,   /* LG-02 日別の詳細 */
    NK_UI_NIGHT,           /* NT-01 睡眠モード */
    NK_UI_SETTINGS,        /* ST-01 設定 */
    NK_UI_BEDROOM,         /* BR-01 寝室（設定の中） */
    NK_UI_CURTAIN_CONNECT, /* BR-02 カーテンをつなぐ */
    NK_UI_SCREEN_COUNT
} nk_ui_screen;

/* 表示先（lv_display）を作り、nk_theme_init を呼んだあとで呼ぶ。ホームを開く */
void nk_ui_init(void);
void nk_ui_deinit(void);

void nk_ui_open(nk_ui_screen screen);
nk_ui_screen nk_ui_current(void);

/* 一つ上の画面へ戻る（日別の詳細→睡眠記録、寝室→設定、カーテン接続→寝室）。戻る先がなければ false
 */
bool nk_ui_back(void);

/* 画面を上下にずらす（マウスのホイール用） */
void nk_ui_scroll_by(int32_t dy);

#endif /* NK_UI_H */
