#pragma once

#include <stdbool.h>

typedef enum {
    NK_PERM_STATUS_UNKNOWN = 0,
    NK_PERM_STATUS_GRANTED,
    NK_PERM_STATUS_DENIED
} nk_perm_status_t;

typedef void (*nk_ui_permissions_on_done_cb)(void);
typedef void (*nk_ui_permissions_request_handler_cb)(void);

void nk_ui_permissions_create(nk_ui_permissions_on_done_cb on_done,
                              nk_ui_permissions_request_handler_cb on_request);
void nk_ui_permissions_destroy(void);

// プラットフォーム（SDL3 / Android）からの権限結果をUIへ反映
void nk_ui_permissions_set_mic_status(nk_perm_status_t status);
void nk_ui_permissions_set_notification_status(nk_perm_status_t status);

// 状態確認・テスト操作用 API
nk_perm_status_t nk_ui_permissions_get_mic_status(void);
nk_perm_status_t nk_ui_permissions_get_notification_status(void);
bool nk_ui_permissions_is_done_enabled(void);
bool nk_ui_permissions_is_mic_denied_shown(void);
void nk_ui_permissions_click_request(void);
void nk_ui_permissions_click_done(void);
