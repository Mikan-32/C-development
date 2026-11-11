#include "ui_privacy_settings.h"

#include "data/data_consent.h"
#include "lvgl.h"

static lv_obj_t *screen_obj = NULL;
static lv_obj_t *status_label = NULL;
static ui_privacy_on_revoke_cb s_on_revoke_cb = NULL;

static void on_revoke_clicked(lv_event_t *e) {
    (void)e;
    // 1. 同意を撤回
    data_consent_set_agreed(false, 0);

    // 2. 画面上のステータスを更新して警告表示
    if (status_label != NULL) {
        lv_label_set_text(status_label,
                          "Consent Revoked.\nSleep tracking is disabled.\nPlease delete app data.");
    }

    // 3. 計測停止などのコールバックを実行
    if (s_on_revoke_cb != NULL) {
        s_on_revoke_cb();
    }
}

void ui_privacy_settings_create(ui_privacy_on_revoke_cb on_revoke) {
    s_on_revoke_cb = on_revoke;

    screen_obj = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(screen_obj);
    lv_label_set_text(title, "Data & Privacy (ST-02)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    status_label = lv_label_create(screen_obj);
    if (data_consent_is_agreed()) {
        lv_label_set_text(status_label, "Status: Agreed\nTracking Active");
    } else {
        lv_label_set_text(status_label, "Status: Not Agreed");
    }
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, -40);

    // 同意取り消しボタン
    lv_obj_t *revoke_btn = lv_btn_create(screen_obj);
    lv_obj_align(revoke_btn, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(revoke_btn, on_revoke_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(revoke_btn);
    lv_label_set_text(btn_lbl, "Withdraw Consent");
    lv_obj_center(btn_lbl);

    lv_scr_load(screen_obj);
}

void ui_privacy_settings_destroy(void) {
    if (screen_obj != NULL) {
        lv_obj_del(screen_obj);
        screen_obj = NULL;
        status_label = NULL;
    }
}

void ui_privacy_settings_click_revoke(void) {
    on_revoke_clicked(NULL);
}
