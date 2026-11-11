#include "ui_permissions.h"

#include "lvgl.h"

static lv_obj_t *screen_obj = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *req_btn = NULL;
static lv_obj_t *done_btn = NULL;
static ui_permissions_on_done_cb s_on_done_cb = NULL;

static bool s_mic_granted = false;
static bool s_notif_granted = false;

static void update_ui_state(void) {
    if (s_mic_granted && s_notif_granted) {
        lv_label_set_text(status_label, "All permissions granted!");
        lv_obj_clear_state(done_btn, LV_STATE_DISABLED);
    } else if (!s_mic_granted && status_label) {
        // マイクが拒否された場合の警告表示（Issue #28 要件）
        lv_label_set_text(status_label,
                          "ST-PERM-MIC:\nMicrophone permission required\nfor sleep tracking.");
    }
}

static void on_request_clicked(lv_event_t *e) {
    (void)e;
    // 本来はここで SDL3_RequestPermission などを呼ぶ
    // シミュレータ / テスト用に一旦「許可された」とみなす
    ui_permissions_set_mic_granted(true);
    ui_permissions_set_notification_granted(true);
}

static void on_done_clicked(lv_event_t *e) {
    (void)e;
    if (s_on_done_cb != NULL) {
        s_on_done_cb();
    }
}

void ui_permissions_create(ui_permissions_on_done_cb on_done) {
    s_on_done_cb = on_done;
    s_mic_granted = false;
    s_notif_granted = false;

    screen_obj = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(screen_obj);
    lv_label_set_text(title, "Required Permissions");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *desc = lv_label_create(screen_obj);
    lv_label_set_text(desc,
                      "nemunoki needs Microphone & Notification\nto record and evaluate sleep.");
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 80);

    // 権限ステータス表示
    status_label = lv_label_create(screen_obj);
    lv_label_set_text(status_label, "Waiting for permission...");
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, -20);

    // 権限リクエストボタン
    req_btn = lv_btn_create(screen_obj);
    lv_obj_align(req_btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(req_btn, on_request_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *req_lbl = lv_label_create(req_btn);
    lv_label_set_text(req_lbl, "Grant Permissions");
    lv_obj_center(req_lbl);

    // 完了ボタン（初期は無効）
    done_btn = lv_btn_create(screen_obj);
    lv_obj_align(done_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_add_state(done_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(done_btn, on_done_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *done_lbl = lv_label_create(done_btn);
    lv_label_set_text(done_lbl, "Start nemunoki");
    lv_obj_center(done_lbl);

    lv_scr_load(screen_obj);
}

void ui_permissions_destroy(void) {
    if (screen_obj != NULL) {
        lv_obj_del(screen_obj);
        screen_obj = NULL;
    }
}

void ui_permissions_set_mic_granted(bool granted) {
    s_mic_granted = granted;
    update_ui_state();
}

void ui_permissions_set_notification_granted(bool granted) {
    s_notif_granted = granted;
    update_ui_state();
}

bool ui_permissions_is_done_enabled(void) {
    return done_btn ? !lv_obj_has_state(done_btn, LV_STATE_DISABLED) : false;
}

void ui_permissions_click_request(void) {
    on_request_clicked(NULL);
}
