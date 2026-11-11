#pragma once

#include <stdbool.h>

#define NK_CURRENT_AGREEMENT_VERSION 1

// 同意画面のコールバック型（次へ進むときに呼ばれる）
typedef void (*ui_agreement_on_next_cb)(void);

// 画面の生成（次へ進むときのコールバック関数を渡す）
void ui_agreement_create(ui_agreement_on_next_cb on_next);

// 画面の破棄
void ui_agreement_destroy(void);

// テスト用
bool ui_agreement_is_checked(void);
bool ui_agreement_is_next_enabled(void);
void ui_agreement_toggle_checkbox(void);
void ui_agreement_click_next(void);
