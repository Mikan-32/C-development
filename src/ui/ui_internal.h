/*
 * 画面どうしで共有する部品（ui.c）と、各画面を組み立てる関数（scr_*.c）
 */
#ifndef NK_UI_INTERNAL_H
#define NK_UI_INTERNAL_H

#include "nk_theme.h"
#include "ui.h"

/* 部品の位置を計算で決めるときの幅（px） */
#define NK_SCREEN_W     390
#define NK_PAGE_PAD     16
#define NK_CARD_PAD     16
#define NK_CARD_INNER_W (NK_SCREEN_W - 2 * NK_PAGE_PAD - 2 * NK_CARD_PAD)

/* 部品（part）と状態（state）を合わせたスタイルの指定 */
#define NK_SEL(part, state) ((lv_style_selector_t)(part) | (lv_style_selector_t)(state))

/* 文字の大きさ（px） */
#define NK_FS_CAPTION 12
#define NK_FS_SMALL   13
#define NK_FS_BODY    15
#define NK_FS_TITLE   17
#define NK_FS_HEAD    22
#define NK_FS_BIG     40

/* 見た目の確認用の固定の値（10月30日（金）の夜〜31日（土）の朝） */
#define NK_DEMO_SCORE       82
#define NK_DEMO_LABEL       "良い"
#define NK_DEMO_SLEEP       "6時間38分"
#define NK_DEMO_BEDTIME     "23:48"
#define NK_DEMO_WAKE        "6:48"
#define NK_DEMO_DIFF        "+3点"
#define NK_DEMO_FEEDBACK    "寝つきまで 28 分でした。今夜は 23:30 ごろに布団に入ってみませんか。"
#define NK_DEMO_GOAL        "23:30"
#define NK_DEMO_CURTAIN_PCT 60

/* 睡眠段階の区間（寝床に入った 23:20 から順に、分） */
typedef enum nk_stage { NK_STAGE_WAKE, NK_STAGE_LIGHT, NK_STAGE_DEEP } nk_stage;
typedef struct nk_segment {
    nk_stage stage;
    int32_t minutes;
} nk_segment;
extern const nk_segment nk_demo_segments[];
extern const int nk_demo_segment_count;
#define NK_DEMO_IN_BED_FROM 20  /* 23:00 から何分後に寝床に入ったか */
#define NK_AXIS_MINUTES     480 /* 時刻の軸：23:00〜7:00 */

lv_color_t nk_stage_color(nk_stage stage);

/* 部品。どれも親の幅いっぱいに広がり、高さは中身に合わせる */
lv_obj_t *nk_box(lv_obj_t *parent, lv_flex_flow_t flow);
lv_obj_t *nk_rect(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t color, int32_t radius);
lv_obj_t *nk_card(lv_obj_t *parent);
lv_obj_t *nk_text(lv_obj_t *parent, const char *text, int32_t px, bool bold, lv_color_t color);
lv_obj_t *nk_wrap_text(lv_obj_t *parent, const char *text, int32_t px, bool bold, lv_color_t color);
lv_obj_t *nk_card_title(lv_obj_t *card, const char *title, const char *note);
lv_obj_t *nk_button(lv_obj_t *parent, const char *text, bool primary);
lv_obj_t *nk_pill(lv_obj_t *parent, const char *text, lv_color_t color, bool dot);
lv_obj_t *nk_chip(lv_obj_t *parent, const char *text, bool selected);
lv_obj_t *nk_stat(lv_obj_t *parent, const char *caption, const char *value, bool center);
lv_obj_t *nk_ring(lv_obj_t *parent, int32_t size, int32_t score);
lv_obj_t *nk_switch(lv_obj_t *parent, bool on);
lv_obj_t *nk_pager(lv_obj_t *parent, const char *text);
void nk_header(lv_obj_t *parent, const char *back, const char *title);
void nk_record_toggle(lv_obj_t *parent, bool week);
void nk_on_click_open(lv_obj_t *obj, nk_ui_screen screen);

/* 各画面。page はスクロールする縦並びの入れ物 */
void nk_build_home(lv_obj_t *page);
void nk_build_record_day(lv_obj_t *page);
void nk_build_record_week(lv_obj_t *page);
void nk_build_record_detail(lv_obj_t *page);
void nk_build_night(lv_obj_t *page);
void nk_build_settings(lv_obj_t *page);
void nk_build_bedroom(lv_obj_t *page);
void nk_build_curtain_connect(lv_obj_t *page);

#endif /* NK_UI_INTERNAL_H */
