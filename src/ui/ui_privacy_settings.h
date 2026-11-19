#pragma once

typedef void (*nk_ui_privacy_on_revoke_cb)(void);

void nk_ui_privacy_create(nk_ui_privacy_on_revoke_cb on_revoke);
void nk_ui_privacy_destroy(void);
void nk_ui_privacy_click_revoke(void);
