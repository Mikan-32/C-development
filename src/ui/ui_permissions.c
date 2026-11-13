#include "ui_permissions.h"

#include "lvgl.h"

static lv_obj_t *s_screen_obj = NULL;
static lv_obj_t *s_mic_status_label = NULL;
static lv_obj_t *s_notif_status_label = NULL;
static lv_obj_t *s_error_box = NULL;
static lv_obj_t *s_req_btn = NULL;
static lv_obj_t *s_req_btn_label = NULL;
static lv_obj_t *s_done_btn = NULL;

static nk_ui_permissions_on_done_cb s_on_done_cb = NULL;
static nk_ui_permissions_request_handler_cb s_on_request_cb = NULL;

static nk_perm_status_t s_mic_status = NK_PERM_STATUS_UNKNOWN;
static nk_perm_status_t s_notif_status = NK_PERM_STATUS_UNKNOWN;

static void refresh_ui(void) {
    if (s_screen_obj == NULL) {
        return;
    }

    // マイク権限表示（必須）
    if (s_mic_status == NK_PERM_STATUS_GRANTED) {
        lv_label_set_text(s_mic_status_label, "マイク（必須）：許可済み");
        lv_obj_add_flag(s_error_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(s_done_btn, LV_STATE_DISABLED);
        lv_label_set_text(s_req_btn_label, "権限が設定されました");
    } else if (s_mic_status == NK_PERM_STATUS_DENIED) {
        lv_label_set_text(s_mic_status_label, "マイク（必須）：拒否されました");
        // ST-PERM-MIC 拒否時の表示（UX-1 / Issue #28 要件）
        lv_obj_clear_flag(s_error_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(s_done_btn, LV_STATE_DISABLED);
        lv_label_set_text(s_req_btn_label, "権限リクエストを再試行");
    } else {
        lv_label_set_text(s_mic_status_label, "マイク（必須）：リクエストされていません");
        lv_obj_add_flag(s_error_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(s_done_btn, LV_STATE_DISABLED);
        lv_label_set_text(s_req_btn_label, "マイクと通知を許可");
    }

    // 通知権限表示（任意）
    if (s_notif_status == NK_PERM_STATUS_GRANTED) {
        lv_label_set_text(s_notif_status_label, "通知（任意）：許可済み");
    } else if (s_notif_status == NK_PERM_STATUS_DENIED) {
        lv_label_set_text(s_notif_status_label,
                          "通知（任意）：拒否されました（後で有効化できます）");
    } else {
        lv_label_set_text(s_notif_status_label, "通知（任意）：リクエストされていません");
    }
}

static void on_request_clicked(lv_event_t *e) {
    (void)e;
    if (s_on_request_cb != NULL) {
        s_on_request_cb();
    } else {
        // ハンドラ未設定時のデフォルト挙動
        nk_ui_permissions_set_mic_status(NK_PERM_STATUS_GRANTED);
        nk_ui_permissions_set_notification_status(NK_PERM_STATUS_GRANTED);
    }
}

static void on_done_clicked(lv_event_t *e) {
    (void)e;
    if (nk_ui_permissions_is_done_enabled() && s_on_done_cb != NULL) {
        s_on_done_cb();
    }
}

void nk_ui_permissions_create(nk_ui_permissions_on_done_cb on_done,
                              nk_ui_permissions_request_handler_cb on_request) {
    s_on_done_cb = on_done;
    s_on_request_cb = on_request;
    s_mic_status = NK_PERM_STATUS_UNKNOWN;
    s_notif_status = NK_PERM_STATUS_UNKNOWN;

    s_screen_obj = lv_obj_create(NULL);

    lv_obj_t *step = lv_label_create(s_screen_obj);
    lv_label_set_text(step, "ステップ 2 / 2");
    lv_obj_align(step, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *title = lv_label_create(s_screen_obj);
    lv_label_set_text(title, "デバイス権限");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *desc = lv_label_create(s_screen_obj);
    lv_obj_set_width(desc, 300);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_label_set_text(desc, "マイクは呼吸と睡眠段階を検出するために必要です。");
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 90);

    s_mic_status_label = lv_label_create(s_screen_obj);
    lv_obj_align(s_mic_status_label, LV_ALIGN_TOP_MID, 0, 150);

    s_notif_status_label = lv_label_create(s_screen_obj);
    lv_obj_align(s_notif_status_label, LV_ALIGN_TOP_MID, 0, 180);

    // ST-PERM-MIC 拒否時の警告コンテナ（初期は非表示）
    s_error_box = lv_obj_create(s_screen_obj);
    lv_obj_set_size(s_error_box, 310, 95);
    lv_obj_align(s_error_box, LV_ALIGN_CENTER, 0, -15);
    lv_obj_add_flag(s_error_box, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *err_lbl = lv_label_create(s_error_box);
    lv_obj_set_width(err_lbl, 280);
    lv_label_set_long_mode(err_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(err_lbl, "[ST-PERM-MIC] マイクのアクセスが拒否されました。\n"
                               "OS設定でマイクを許可するか、以下のボタンから再試行してください。");
    lv_obj_center(err_lbl);

    // 権限リクエストボタン
    s_req_btn = lv_btn_create(s_screen_obj);
    lv_obj_set_width(s_req_btn, 270);
    lv_obj_align(s_req_btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_event_cb(s_req_btn, on_request_clicked, LV_EVENT_CLICKED, NULL);

    s_req_btn_label = lv_label_create(s_req_btn);
    lv_obj_center(s_req_btn_label);

    // 完了ボタン（マイク許可まで無効）
    s_done_btn = lv_btn_create(s_screen_obj);
    lv_obj_set_width(s_done_btn, 240);
    lv_obj_align(s_done_btn, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_obj_add_state(s_done_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_done_btn, on_done_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *done_lbl = lv_label_create(s_done_btn);
    lv_label_set_text(done_lbl, "ねむのきを開始");
    lv_obj_center(done_lbl);

    refresh_ui();
    lv_scr_load(s_screen_obj);
}

void nk_ui_permissions_destroy(void) {
    if (s_screen_obj != NULL) {
        lv_obj_del(s_screen_obj);
        s_screen_obj = NULL;
        s_mic_status_label = NULL;
        s_notif_status_label = NULL;
        s_error_box = NULL;
        s_req_btn = NULL;
        s_req_btn_label = NULL;
        s_done_btn = NULL;
    }
}

void nk_ui_permissions_set_mic_status(nk_perm_status_t status) {
    s_mic_status = status;
    refresh_ui();
}

void nk_ui_permissions_set_notification_status(nk_perm_status_t status) {
    s_notif_status = status;
    refresh_ui();
}

nk_perm_status_t nk_ui_permissions_get_mic_status(void) {
    return s_mic_status;
}

nk_perm_status_t nk_ui_permissions_get_notification_status(void) {
    return s_notif_status;
}

bool nk_ui_permissions_is_done_enabled(void) {
    return (s_done_btn != NULL) ? !lv_obj_has_state(s_done_btn, LV_STATE_DISABLED) : false;
}

bool nk_ui_permissions_is_mic_denied_shown(void) {
    return (s_error_box != NULL) ? !lv_obj_has_flag(s_error_box, LV_OBJ_FLAG_HIDDEN) : false;
}

void nk_ui_permissions_click_request(void) {
    on_request_clicked(NULL);
}

void nk_ui_permissions_click_done(void) {
    on_done_clicked(NULL);
}
