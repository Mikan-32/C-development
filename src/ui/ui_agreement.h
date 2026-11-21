#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*nk_ui_agreement_on_next_cb)(void);

// is_reagreement: true の場合は「規約改定による再同意」の案内文を表示
void nk_ui_agreement_create(nk_ui_agreement_on_next_cb on_next, bool is_reagreement);
void nk_ui_agreement_destroy(void);

// 状態確認・テスト操作用 API
bool nk_ui_agreement_is_checked(void);
bool nk_ui_agreement_is_next_enabled(void);
void nk_ui_agreement_toggle_checkbox(void);
void nk_ui_agreement_click_next(void);
