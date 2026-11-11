#include "ui_agreement.h"

#include "data/data_consent.h"
#include "lvgl.h"

static lv_obj_t *screen_obj = NULL;
static lv_obj_t *next_btn = NULL;
static lv_obj_t *agree_checkbox = NULL;
static ui_agreement_on_next_cb s_on_next_cb = NULL;

static void on_checkbox_changed(lv_event_t *e) {
    lv_obj_t *cb = lv_event_get_target(e);
    bool is_checked = lv_obj_has_state(cb, LV_STATE_CHECKED);

    if (is_checked) {
        lv_obj_clear_state(next_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(next_btn, LV_STATE_DISABLED);
    }
}

static void on_next_clicked(lv_event_t *e) {
    (void)e;
    // 1. 同意した規約バージョンを保存
    data_consent_set_agreed(true, NK_CURRENT_AGREEMENT_VERSION);

    // 2. 次の画面（ON-03）へ進むコールバックを実行
    if (s_on_next_cb != NULL) {
        s_on_next_cb();
    }
}

void ui_agreement_create(ui_agreement_on_next_cb on_next) {
    s_on_next_cb = on_next;

    screen_obj = lv_obj_create(NULL);

    // タイトル
    lv_obj_t *title = lv_label_create(screen_obj);
    lv_label_set_text(title, "Terms of Service");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // 説明文
    lv_obj_t *desc = lv_label_create(screen_obj);
    lv_label_set_text(desc, "To use nemunoki,\nplease agree to the terms.");
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 80);

    // チェックボックス
    agree_checkbox = lv_checkbox_create(screen_obj);
    lv_checkbox_set_text(agree_checkbox, "I agree to the terms (v1.0)");
    lv_obj_align(agree_checkbox, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(agree_checkbox, on_checkbox_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // 次へボタン
    next_btn = lv_btn_create(screen_obj);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_add_state(next_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(next_btn, on_next_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(next_btn);
    lv_label_set_text(btn_label, "Next");
    lv_obj_center(btn_label);

    lv_scr_load(screen_obj);
}

void ui_agreement_destroy(void) {
    if (screen_obj != NULL) {
        lv_obj_del(screen_obj);
        screen_obj = NULL;
        next_btn = NULL;
        agree_checkbox = NULL;
    }
}

bool ui_agreement_is_checked(void) {
    return agree_checkbox ? lv_obj_has_state(agree_checkbox, LV_STATE_CHECKED) : false;
}

bool ui_agreement_is_next_enabled(void) {
    return next_btn ? !lv_obj_has_state(next_btn, LV_STATE_DISABLED) : false;
}

void ui_agreement_toggle_checkbox(void) {
    if (!agree_checkbox) {
        return;
    }
    if (ui_agreement_is_checked()) {
        lv_obj_clear_state(agree_checkbox, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(agree_checkbox, LV_STATE_CHECKED);
    }
    lv_obj_send_event(agree_checkbox, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_agreement_click_next(void) {
    if (ui_agreement_is_next_enabled()) {
        on_next_clicked(NULL);
    }
}
