/*
 * 画面（LVGL）の公開窓口
 *
 * 画面 ID は docs/ux/UX-2_information-architecture.md と UX 班の改訂版（UX-1〜5）、
 * 状態 ID は docs/ux/UX-7_states.md と対応する。
 * いまは見た目を確かめるための試作で、表示する記録はデモ用の固定データ（ui_demo.c）。
 * LVGL の初期化と表示先（ディスプレイ・入力）の用意は呼び出し側（src/app/）が行う。
 */
#ifndef NK_UI_H
#define NK_UI_H

#include <stdbool.h>
#include <stddef.h>

/* 画面 ID（UX-2 の 2 章） */
typedef enum nk_ui_screen_id {
    NK_UI_ON01,  /* ようこそ */
    NK_UI_ON02,  /* データの使い方と同意 */
    NK_UI_ON03,  /* マイクと通知の許可 */
    NK_UI_ON04,  /* 置き方と試し計測 */
    NK_UI_ON05,  /* 目標と起きる時刻 */
    NK_UI_ON06,  /* アラームの許可 */
    NK_UI_ON07,  /* カーテンをつなぐ */
    NK_UI_ON08,  /* 自動モードの説明 */
    NK_UI_ON09,  /* 準備完了 */
    NK_UI_HM01,  /* ホーム */
    NK_UI_AD01,  /* アドバイスの一覧 */
    NK_UI_LG01,  /* 睡眠記録（日） */
    NK_UI_LG01W, /* 睡眠記録（週）。UX-4 改訂案の「週の表示」。UX-2 に ID がないため仮 */
    NK_UI_LG02,  /* 日別の詳細（くわしく見る） */
    NK_UI_NT01,  /* おやすみ */
    NK_UI_NT02,  /* 計測中 */
    NK_UI_NT03,  /* アラーム鳴動 */
    NK_UI_NT04,  /* おはよう */
    NK_UI_NT05,  /* アラームの設定（NT-01 の 4 から開く。UX-6 の 4 章） */
    NK_UI_BR01,  /* 寝室（設定の中） */
    NK_UI_BR02,  /* カーテンをつなぐ */
    NK_UI_BR03,  /* 自動モードを有効にする */
    NK_UI_ST01,  /* 設定 */
    NK_UI_ST02,  /* データとプライバシー */
    NK_UI_ST03,  /* この分析について */
    NK_UI_ST04,  /* ライセンス表示 */
    NK_UI_X01,   /* 状態表示の例（UX-7） */
    NK_UI_DEV,   /* 画面と状態の一覧（PC 版の確認用） */
    NK_UI_SCREEN_COUNT
} nk_ui_screen_id;

/* 状態 ID（UX-7 の 2 章）。NK_UI_ST_NONE は「通常」（分析できた夜、接続済みなど） */
typedef enum nk_ui_state {
    NK_UI_ST_NONE,
    NK_UI_ST_EMPTY_FIRST,
    NK_UI_ST_EMPTY_DAY,
    NK_UI_ST_SAVING,
    NK_UI_ST_ANALYZING,
    NK_UI_ST_ANALYSIS_FAILED,
    NK_UI_ST_FAILED,
    NK_UI_ST_INSUFFICIENT,
    NK_UI_ST_PARTIAL_GAP,
    NK_UI_ST_NOT_ENOUGH_NIGHTS,
    NK_UI_ST_TRACKING,
    NK_UI_ST_TRACKING_INTERRUPTED,
    NK_UI_ST_BATTERY_LOW,
    NK_UI_ST_PERM_MIC,
    NK_UI_ST_PERM_NOTIF,
    NK_UI_ST_PERM_EXACT_ALARM,
    NK_UI_ST_PERM_FSI,
    NK_UI_ST_CURTAIN_CHECKING,
    NK_UI_ST_CURTAIN_PENDING,
    NK_UI_ST_CURTAIN_OK,
    NK_UI_ST_CURTAIN_FAILED,
    NK_UI_ST_CURTAIN_UNCONFIRMED,
    NK_UI_ST_CURTAIN_OFFLINE,
    NK_UI_ST_CURTAIN_AUTH,
    NK_UI_ST_AUTO_PAUSED,
    NK_UI_STATE_COUNT
} nk_ui_state;

typedef struct nk_ui_config {
    /* 日本語フォント（TTF・TTC の中身）。NULL なら内蔵の英字フォントで代用する。
     * 呼び出し側が nk_ui_deinit まで保持する */
    const void *font_regular;
    size_t font_regular_size;
    const void *font_bold;
    size_t font_bold_size;
    float font_scale;     /* アプリ内の文字サイズ：1.0 標準、1.2 大、1.4 特大 */
    bool reduce_motion;   /* 動きを減らす（UX-8 の 4 章） */
    bool instant;         /* 画面の切り替えの動きをなくす（スクリーンショット用） */
    bool show_status_bar; /* 端末の時刻と電池の表示を描く（PC 版で端末らしく見せる） */
} nk_ui_config;

/* 画面を作り始める。lv_init とディスプレイの作成のあとに呼ぶ */
void nk_ui_init(const nk_ui_config *config);
void nk_ui_deinit(void);

/* 画面を開く（戻る操作の履歴は消える）。不正な ID なら false */
bool nk_ui_open(nk_ui_screen_id screen, nk_ui_state state);

/* 戻る（Android の戻るボタン、PC の Esc）。戻る先がない（ホームにいる）なら false */
bool nk_ui_back(void);

/* マウスホイールなどで、いまの画面を縦にスクロールする */
void nk_ui_scroll_by(int dy);
/* いまの画面を、上から y px の位置までスクロールする（スクリーンショット用） */
void nk_ui_scroll_to(int y);

/* いまの画面と状態 */
nk_ui_screen_id nk_ui_current_screen(void);
nk_ui_state nk_ui_current_state(void);

/* 名前（"HM-01"、"ST-ANALYZING"）との変換。見つからなければ *_COUNT を返す */
const char *nk_ui_screen_name(nk_ui_screen_id screen);
const char *nk_ui_screen_title(nk_ui_screen_id screen);
const char *nk_ui_state_name(nk_ui_state state);
nk_ui_screen_id nk_ui_screen_from_name(const char *name);
nk_ui_state nk_ui_state_from_name(const char *name);

/* スクリーンショットを撮る組み合わせ（全画面 × その画面で確認する状態）。ADR-0011 の段 4 */
size_t nk_ui_shot_count(void);
bool nk_ui_shot_get(size_t index, nk_ui_screen_id *screen, nk_ui_state *state);

#endif /* NK_UI_H */
