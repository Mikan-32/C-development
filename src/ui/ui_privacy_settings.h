#pragma once

#include <stdbool.h>

typedef void (*ui_privacy_on_revoke_cb)(void);

// 画面の生成
void ui_privacy_settings_create(ui_privacy_on_revoke_cb on_revoke);

// 画面の破棄
void ui_privacy_settings_destroy(void);

// テスト用
void ui_privacy_settings_click_revoke(void);
