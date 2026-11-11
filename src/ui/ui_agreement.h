#pragma once

#include <stdbool.h>

// 同意画面のUIを作成して表示する関数
void ui_agreement_create(void);

// 画面を破棄（片付け）する関数
void ui_agreement_destroy(void);

// 【追加】テスト・検証用：チェックボックスの状態を取得
bool ui_agreement_is_checked(void);

// 【追加】テスト・検証用：次へ進むボタンが押せる状態（有効）か取得
bool ui_agreement_is_next_enabled(void);

// 【追加】テスト・検証用：外部からチェックボックスをトグル（切り替え）する
void ui_agreement_toggle_checkbox(void);
