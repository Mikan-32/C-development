#pragma once

#include <stdbool.h>

typedef void (*ui_permissions_on_done_cb)(void);

// 画面の生成
void ui_permissions_create(ui_permissions_on_done_cb on_done);

// 画面の破棄
void ui_permissions_destroy(void);

// 外部（SDL3 / プラットフォーム）から権限結果を反映する関数
void ui_permissions_set_mic_granted(bool granted);
void ui_permissions_set_notification_granted(bool granted);

// テスト用
bool ui_permissions_is_done_enabled(void);
void ui_permissions_click_request(void);
