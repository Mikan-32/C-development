#include "ui_privacy_settings.h"

#include "data_consent.h"
#include "data_session.h"
#include "lvgl.h"

static lv_obj_t *s_screen_obj = NULL;
static lv_obj_t *s_status_label = NULL;
static nk_ui_privacy_on_revoke_cb s_on_revoke_cb = NULL;

static void on_revoke_clicked(lv_event_t *e) {
    (void)e;
    if (!nk_data_consent_set_agreed(false, 0)) {
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "同意の取り消しを保存できませんでした。");
        }
        return;
    }

    if (s_status_label != NULL) {
        lv_label_set_text(s_status_label, "同意を取り消しました。\n"
                                          "新しい計測は開始できません。\n"
                                          "下のボタンから保存データを削除できます。");
    }

    if (s_on_revoke_cb != NULL) {
        s_on_revoke_cb();
    }
}

static void on_delete_all_clicked(lv_event_t *e) {
    (void)e;
    if (nk_data_session_delete_all()) {
        if (s_status_label != NULL) {
            lv_label_set_text(s_status_label, "睡眠記録を削除しました。");
        }
    } else if (s_status_label != NULL) {
        lv_label_set_text(s_status_label,
                          "削除できませんでした。\n計測が停止していることを確認してください。");
    }
}

void nk_ui_privacy_create(nk_ui_privacy_on_revoke_cb on_revoke) {
    s_on_revoke_cb = on_revoke;
    s_screen_obj = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(s_screen_obj);
    lv_label_set_text(title, "データとプライバシー (ST-02)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);

    s_status_label = lv_label_create(s_screen_obj);
    lv_obj_set_width(s_status_label, 300);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    if (nk_data_consent_is_agreed()) {
        lv_label_set_text(s_status_label, "状態: 同意済み (v1)\n睡眠計測は有効です。");
    } else {
        lv_label_set_text(s_status_label, "状態: 未同意（計測停止中）");
    }
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 90);

    // 同意取り消しボタン
    lv_obj_t *revoke_btn = lv_btn_create(s_screen_obj);
    lv_obj_set_width(revoke_btn, 250);
    lv_obj_align(revoke_btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(revoke_btn, on_revoke_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(revoke_btn);
    lv_label_set_text(btn_lbl, "同意を取り消す");
    lv_obj_center(btn_lbl);

    // 全データ削除ボタン
    lv_obj_t *del_btn = lv_btn_create(s_screen_obj);
    lv_obj_set_width(del_btn, 250);
    lv_obj_align(del_btn, LV_ALIGN_CENTER, 0, 85);
    lv_obj_add_event_cb(del_btn, on_delete_all_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *del_lbl = lv_label_create(del_btn);
    lv_label_set_text(del_lbl, "睡眠記録をすべて削除する");
    lv_obj_center(del_lbl);

    lv_scr_load(s_screen_obj);
}

void nk_ui_privacy_destroy(void) {
    if (s_screen_obj != NULL) {
        lv_obj_del(s_screen_obj);
        s_screen_obj = NULL;
        s_status_label = NULL;
    }
}

void nk_ui_privacy_click_revoke(void) {
    on_revoke_clicked(NULL);
}
