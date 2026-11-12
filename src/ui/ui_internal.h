/*
 * 画面の部品と、画面どうしで共有する関数（ui/ の中だけで使う）
 */
#ifndef NK_UI_INTERNAL_H
#define NK_UI_INTERNAL_H

#include "lvgl.h"
#include "nk_theme.h"
#include "ui.h"
#include "ui_demo.h"

#include <stdbool.h>
#include <stdint.h>

/* 部品（LV_PART_*）と状態（LV_STATE_*）を組み合わせたスタイルの対象。
 * 型の違う列挙どうしを | でつなぐ警告を避ける */
#define NK_SEL(part, state) ((lv_style_selector_t)(part) | (lv_style_selector_t)(state))

/* ================================================================
 * 画面の登録（ui.c）
 * ================================================================ */

typedef enum nk_ui_tab {
    NK_UI_TAB_NONE,     /* タブバーを出さない画面 */
    NK_UI_TAB_HOME,     /* ホーム */
    NK_UI_TAB_HISTORY,  /* 睡眠記録 */
    NK_UI_TAB_NIGHT,    /* 睡眠モード */
    NK_UI_TAB_SETTINGS, /* 設定 */
} nk_ui_tab;

/* 画面を作る関数。body は縦に並べる入れ物（スクロールする） */
typedef void (*nk_ui_build_fn)(lv_obj_t *body, nk_ui_state state);

/* 画面を作り直さずに状態だけ変える（例：寝室の操作結果）。いまの画面を作り直す */
void nk_ui_rebuild(void);

/* 下の階層へ進む（戻るで戻れる） */
void nk_ui_push(nk_ui_screen_id screen, nk_ui_state state);
/* いまの画面を置き換える（戻るの履歴を増やさない） */
void nk_ui_replace(nk_ui_screen_id screen, nk_ui_state state);
/* タブの最上位の画面へ（履歴を消す） */
void nk_ui_go_tab(nk_ui_screen_id screen);

/* 押したら画面を開く。how：0 進む、1 置き換える、2 タブへ */
#define NK_UI_NAV_PUSH    0
#define NK_UI_NAV_REPLACE 1
#define NK_UI_NAV_TAB     2
void nk_ui_on_click_nav(lv_obj_t *obj, nk_ui_screen_id screen, nk_ui_state state, int how);

/* 見出しの「‹ 戻る先」に出す名前（ひとつ前の画面。なければタブの名前） */
const char *nk_ui_back_label(void);

/* 設定の変更（文字サイズ・動きを減らす） */
void nk_ui_apply_font_scale(float scale);
bool nk_ui_reduce_motion(void);
void nk_ui_set_reduce_motion(bool on);
bool nk_ui_instant(void);

/* いまの画面が持つタイマー。画面を閉じると止まる。途中で止めるときは nk_ui_timer_stop */
lv_timer_t *nk_ui_timer(lv_timer_cb_t cb, uint32_t period_ms, void *user_data);
void nk_ui_timer_stop(lv_timer_t *timer);

/* 画面ごとの作る関数 */
void nk_scr_onboarding(lv_obj_t *body, nk_ui_screen_id screen, nk_ui_state state);
void nk_scr_home(lv_obj_t *body, nk_ui_state state);
void nk_scr_advice(lv_obj_t *body, nk_ui_state state);
void nk_scr_history_day(lv_obj_t *body, nk_ui_state state);
void nk_scr_history_week(lv_obj_t *body, nk_ui_state state);
void nk_scr_day(lv_obj_t *body, nk_ui_state state);
void nk_scr_night(lv_obj_t *body, nk_ui_state state);
void nk_scr_tracking(lv_obj_t *body, nk_ui_state state);
void nk_scr_ringing(lv_obj_t *body, nk_ui_state state);
void nk_scr_morning(lv_obj_t *body, nk_ui_state state);
void nk_scr_alarm_settings(lv_obj_t *body, nk_ui_state state);
void nk_scr_bedroom(lv_obj_t *body, nk_ui_state state);
void nk_scr_connect(lv_obj_t *body, nk_ui_state state);
/* BR-02 の中身（ON-07 でも使う） */
void nk_scr_connect_body(lv_obj_t *body, nk_ui_state state, bool onboarding);
void nk_scr_automode(lv_obj_t *body, nk_ui_state state);
void nk_scr_settings(lv_obj_t *body, nk_ui_state state);
void nk_scr_privacy(lv_obj_t *body, nk_ui_state state);
void nk_scr_about(lv_obj_t *body, nk_ui_state state);
void nk_scr_licenses(lv_obj_t *body, nk_ui_state state);
void nk_scr_states(lv_obj_t *body, nk_ui_state state);
void nk_scr_dev(lv_obj_t *body, nk_ui_state state);

/* ================================================================
 * アイコン（ui_icons.c）。画面モック v2 の SVG（24×24）を線と円弧で描く
 * ================================================================ */

typedef enum nk_icon {
    NK_ICON_NONE,
    NK_ICON_HOME,
    NK_ICON_CHART,
    NK_ICON_MOON,
    NK_ICON_CURTAIN,
    NK_ICON_GEAR,
    NK_ICON_PHONE,
    NK_ICON_SPARK,
    NK_ICON_MIC,
    NK_ICON_LOCK,
    NK_ICON_TRASH,
    NK_ICON_SUN,
    NK_ICON_INFO,
    NK_ICON_LEFT,
    NK_ICON_RIGHT,
    NK_ICON_CHECK,
    NK_ICON_CHECK_CIRCLE,
    NK_ICON_WAVE,
    NK_ICON_RAIN,
    NK_ICON_SEA,
    NK_ICON_MUSIC,
    NK_ICON_PAUSE,
    NK_ICON_PLAY,
    NK_ICON_VOL,
    NK_ICON_CLOCK,
    NK_ICON_ALARM,
    NK_ICON_BELL,
    NK_ICON_PLUG,
    NK_ICON_LOGO,
} nk_icon;

/* アイコンを 1 つ置く（size×size） */
lv_obj_t *nk_ui_icon(lv_obj_t *parent, nk_icon icon, int32_t size, lv_color_t color);
/* アイコンの色を変える */
void nk_ui_icon_set_color(lv_obj_t *icon_obj, lv_color_t color);
/* 描画イベントの中で、area にアイコンを描く */
void nk_ui_icon_draw(lv_layer_t *layer, nk_icon icon, const lv_area_t *area, lv_color_t color,
                     lv_opa_t opa);

/* ================================================================
 * 部品（ui_widgets.c）
 * ================================================================ */

typedef enum nk_tone {
    NK_TONE_NEUTRAL,
    NK_TONE_INFO,    /* 青紫（保存中・分析中など） */
    NK_TONE_GOOD,    /* 成功 */
    NK_TONE_WARN,    /* 未確認・注意 */
    NK_TONE_SERIOUS, /* 失敗（オレンジ） */
} nk_tone;

/* 操作結果の 4 状態（UX-5 の 2 章）。部品 nk_op_result */
typedef enum nk_op {
    NK_OP_PENDING,     /* 依頼中 */
    NK_OP_OK,          /* 成功（確認済み） */
    NK_OP_FAILED,      /* 失敗 */
    NK_OP_UNCONFIRMED, /* 未確認 */
} nk_op;

lv_color_t nk_tone_color(nk_tone tone);

/* 入れ物 */
lv_obj_t *nk_ui_box(lv_obj_t *parent);
lv_obj_t *nk_ui_row(lv_obj_t *parent, int32_t gap);
lv_obj_t *nk_ui_col(lv_obj_t *parent, int32_t gap);
lv_obj_t *nk_ui_spacer(lv_obj_t *row); /* 横並びの残りの幅を埋める */
lv_obj_t *nk_ui_gap(lv_obj_t *parent, int32_t height);

/* 文字 */
lv_obj_t *nk_ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);
/* 幅いっぱいで折り返す文字 */
lv_obj_t *nk_ui_text(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);
/* 文字の行間を詰める・広げる */
void nk_ui_line_space(lv_obj_t *label, int32_t space);

/* カード */
lv_obj_t *nk_ui_card(lv_obj_t *parent);
/* カードの見出し（左に見出し、右に right を置ける行）。行を返す */
lv_obj_t *nk_ui_card_head(lv_obj_t *card, const char *title);
/* 見出しの右の小さな注記 */
lv_obj_t *nk_ui_hint(lv_obj_t *parent, const char *text);
/* カードの中の区画（card-2） */
lv_obj_t *nk_ui_tile(lv_obj_t *parent);

/* 画面の見出し（h1 と右側の部品のための行） */
lv_obj_t *nk_ui_title(lv_obj_t *body, const char *title);
/* 下の階層の見出し（戻るボタン＋見出し） */
lv_obj_t *nk_ui_subhead(lv_obj_t *body, const char *title);

/* ボタン。cb が NULL なら押しても何もしない */
lv_obj_t *nk_ui_btn_primary(lv_obj_t *parent, const char *text, nk_icon icon, lv_event_cb_t cb,
                            void *user_data);
lv_obj_t *nk_ui_btn_ghost(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
lv_obj_t *nk_ui_btn_serious(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
/* 2 つ並べるボタンの行 */
lv_obj_t *nk_ui_btn_pair(lv_obj_t *parent);
lv_obj_t *nk_ui_icon_btn(lv_obj_t *parent, nk_icon icon, int32_t size, lv_event_cb_t cb,
                         void *user_data);
lv_obj_t *nk_ui_link(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
/* ボタンの文字を変える */
void nk_ui_btn_set_text(lv_obj_t *btn, const char *text);

/* 札 */
lv_obj_t *nk_ui_pill(lv_obj_t *parent, const char *text, nk_tone tone, bool dot);
lv_obj_t *nk_ui_tag_est(lv_obj_t *parent); /* 「推定」 */
lv_obj_t *nk_ui_tag_ref(lv_obj_t *parent); /* 「参考値」 */

/* 状態の表示（UX-7 の 4 章） */
lv_obj_t *nk_ui_banner(lv_obj_t *parent, nk_tone tone, nk_icon icon, bool spinner, const char *text,
                       const char *sub, const char *link, lv_event_cb_t cb, void *user_data);
lv_obj_t *nk_ui_op_result(lv_obj_t *parent, nk_op op, const char *text, const char *sub,
                          const char *link, lv_event_cb_t cb, void *user_data);
lv_obj_t *nk_ui_notice_row(lv_obj_t *parent, nk_icon icon, lv_color_t icon_color, const char *text);
lv_obj_t *nk_ui_empty(lv_obj_t *parent, const char *title, const char *text, const char *btn,
                      lv_event_cb_t cb, void *user_data);
lv_obj_t *nk_ui_spinner(lv_obj_t *parent, int32_t size, lv_color_t color);

/* 入力 */
lv_obj_t *nk_ui_switch(lv_obj_t *parent, bool on, lv_event_cb_t cb, void *user_data);
lv_obj_t *nk_ui_slider(lv_obj_t *parent, int32_t value, lv_event_cb_t cb, void *user_data);
lv_obj_t *nk_ui_chip(lv_obj_t *parent, const char *text, bool on, lv_event_cb_t cb,
                     void *user_data);
/* 横並びの選択肢（週／月 など）。user_data に (void *)(intptr_t) 番号を入れて cb を呼ぶ */
lv_obj_t *nk_ui_segmented(lv_obj_t *parent, const char *const *labels, int count, int selected,
                          lv_event_cb_t cb);
lv_obj_t *nk_ui_roller(lv_obj_t *parent, const char *options, uint32_t selected, int32_t width);

/* 一覧の行 */
lv_obj_t *nk_ui_divider(lv_obj_t *parent);
lv_obj_t *nk_ui_kv(lv_obj_t *parent, const char *key, const char *value, bool estimated);
lv_obj_t *nk_ui_list_item(lv_obj_t *parent, nk_icon icon, const char *title, const char *text);
lv_obj_t *nk_ui_setrow(lv_obj_t *parent, const char *title, const char *sub, const char *value);
lv_obj_t *nk_ui_icon_tile(lv_obj_t *parent, nk_icon icon, int32_t size, lv_color_t bg,
                          lv_opa_t bg_opa, lv_color_t fg);

/* シート（下から出る確認の画面）。中身を入れる入れ物を返す */
lv_obj_t *nk_ui_sheet_open(const char *title, const char *sub);
void nk_ui_sheet_close(void);
bool nk_ui_sheet_is_open(void);
/* シートを閉じる（押したとき） */
void nk_ui_sheet_close_cb(lv_event_t *e);

/* 画面の下に数秒だけ出る短い知らせ（成功は控えめに。失敗はこれだけで済ませない） */
void nk_ui_toast(const char *text);
/* シートとトーストを消す（終了時） */
void nk_ui_widgets_reset(void);

/* ================================================================
 * グラフと図（ui_charts.c）
 * ================================================================ */

/* スコアのリング */
lv_obj_t *nk_ui_ring(lv_obj_t *parent, int32_t size, int score, bool reference, bool animate);

/* 計測できた時間の帯（欠損は斜線） */
lv_obj_t *nk_ui_quality_bar(lv_obj_t *parent, const nk_demo_night *night);

/* 睡眠段階の概要（睡眠記録の日の表示）。1 時間ごとの目盛りの帯で、取得なしは点線の枠。
 * two_tone が true なら浅い・深いを分けない（データ不足の夜） */
lv_obj_t *nk_ui_stage_band(lv_obj_t *parent, const nk_demo_night *night, bool two_tone);

/* 睡眠段階の推移（LG-02）。覚醒・浅い・深いの上下と、出来事の印。取得なしは点線の枠 */
lv_obj_t *nk_ui_stage_steps(lv_obj_t *parent, const nk_demo_night *night);

/* 週の睡眠時間の棒グラフと、睡眠スコアの折れ線（月曜〜日曜。monday は 10 月の日）。
 * 棒・点を押すと LV_EVENT_VALUE_CHANGED。押した日は nk_ui_week_chart_day で取る */
lv_obj_t *nk_ui_week_bars(lv_obj_t *parent, int monday);
lv_obj_t *nk_ui_week_scores(lv_obj_t *parent, int monday);
int nk_ui_week_chart_day(lv_obj_t *chart);
/* 睡眠段階の推移の印の色（0 寝返り 1 いびき 2 大きな音 3 サウンド 4 カーテン 5 アラーム） */
lv_color_t nk_ui_mark_color(int kind);

/* カーテンの開き具合の図（0〜100%） */
lv_obj_t *nk_ui_curtain_view(lv_obj_t *parent, int32_t height, int position);
void nk_ui_curtain_view_set(lv_obj_t *view, int position);

/* 再生中の印（3 本の棒） */
lv_obj_t *nk_ui_eq(lv_obj_t *parent, lv_color_t color, bool animate);

/* 長押しの輪つきボタン（NT-02 の「起きた」）。1 秒押し続けると cb */
lv_obj_t *nk_ui_hold_button(lv_obj_t *parent, const char *text, lv_color_t color, lv_event_cb_t cb);

/* ================================================================
 * 書式（ui_demo.c）
 * ================================================================ */

/* 「6時間38分」「28分」 */
void nk_fmt_duration(char *buf, size_t size, int minutes);
/* 「6:30」「0:12」 */
void nk_fmt_clock(char *buf, size_t size, int minutes_of_day);
/* 「10月31日（土）」 */
void nk_fmt_date(char *buf, size_t size, int month, int day, int wday);

#endif /* NK_UI_INTERNAL_H */
