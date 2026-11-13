#include "ui_agreement.h"

#include "data_consent.h"
#include "lvgl.h"

static lv_obj_t *s_screen_obj = NULL;
static lv_obj_t *s_next_btn = NULL;
static lv_obj_t *s_agree_checkbox = NULL;
static nk_ui_agreement_on_next_cb s_on_next_cb = NULL;

static void on_checkbox_changed(lv_event_t *e) {
    lv_obj_t *cb = lv_event_get_target(e);
    bool is_checked = lv_obj_has_state(cb, LV_STATE_CHECKED);

    if (is_checked) {
        lv_obj_clear_state(s_next_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_next_btn, LV_STATE_DISABLED);
    }
}

static void on_next_clicked(lv_event_t *e) {
    (void)e;
    nk_data_consent_set_agreed(true, NK_DATA_CONSENT_CURRENT_VERSION);
    if (s_on_next_cb != NULL) {
        s_on_next_cb();
    }
}

void nk_ui_agreement_create(nk_ui_agreement_on_next_cb on_next, bool is_reagreement) {
    s_on_next_cb = on_next;
    s_screen_obj = lv_obj_create(NULL);

    // ステップ表示 (ON-02: Step 1/2)
    lv_obj_t *step = lv_label_create(s_screen_obj);
    lv_label_set_text(step, "Step 1 / 2");
    lv_obj_align(step, LV_ALIGN_TOP_MID, 0, 20);

    // タイトル
    lv_obj_t *title = lv_label_create(s_screen_obj);
    lv_label_set_text(title, is_reagreement ? "利用規約の更新（再同意）"
                                            : "利用規約・プライバシーポリシー");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    // 説明文（再同意か初回かで文言を切り替え）
    lv_obj_t *desc = lv_label_create(s_screen_obj);
    lv_obj_set_width(desc, 300);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    if (is_reagreement) {
        lv_label_set_text(desc, "利用規約が更新されました。\n"
                                "引き続きねむのきをご利用いただくには、再度同意してください。");
    } else {
        lv_label_set_text(desc, "ねむのきは、お使いのデバイス上で睡眠中の音を解析します。\n"
                                "生の音声はアップロードされません。同意してください。");
    }
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 95);

    // チェックボックス
    s_agree_checkbox = lv_checkbox_create(s_screen_obj);
    lv_checkbox_set_text(s_agree_checkbox, "利用規約とプライバシーポリシーに同意する (v1)");
    lv_obj_align(s_agree_checkbox, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(s_agree_checkbox, on_checkbox_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // 「同意して次へ」ボタン
    s_next_btn = lv_btn_create(s_screen_obj);
    lv_obj_set_width(s_next_btn, 240);
    lv_obj_align(s_next_btn, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_obj_add_state(s_next_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_next_btn, on_next_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(s_next_btn);
    lv_label_set_text(btn_label, "同意して次へ");
    lv_obj_center(btn_label);

    lv_scr_load(s_screen_obj);
}

void nk_ui_agreement_destroy(void) {
    if (s_screen_obj != NULL) {
        lv_obj_del(s_screen_obj);
        s_screen_obj = NULL;
        s_next_btn = NULL;
        s_agree_checkbox = NULL;
    }
}

bool nk_ui_agreement_is_checked(void) {
    return (s_agree_checkbox != NULL) ? lv_obj_has_state(s_agree_checkbox, LV_STATE_CHECKED)
                                      : false;
}

bool nk_ui_agreement_is_next_enabled(void) {
    return (s_next_btn != NULL) ? !lv_obj_has_state(s_next_btn, LV_STATE_DISABLED) : false;
}

void nk_ui_agreement_toggle_checkbox(void) {
    if (s_agree_checkbox == NULL) {
        return;
    }
    if (nk_ui_agreement_is_checked()) {
        lv_obj_clear_state(s_agree_checkbox, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(s_agree_checkbox, LV_STATE_CHECKED);
    }
    lv_obj_send_event(s_agree_checkbox, LV_EVENT_VALUE_CHANGED, NULL);
}

void nk_ui_agreement_click_next(void) {
    if (nk_ui_agreement_is_next_enabled()) {
        on_next_clicked(NULL);
    }
}
