///////////////////////////////////////
/*動作検証のためのAIが書いてくれたコード*/
///////////////////////////////////////

#include "ui_agreement.h"

#include "lvgl.h"

// 画面全体の親オブジェクト
static lv_obj_t *screen_obj = NULL;
// 「次へ」ボタン
static lv_obj_t *next_btn = NULL;
// チェックボックス
static lv_obj_t *agree_checkbox = NULL;

// チェックボックスがタップされたときの処理
static void on_checkbox_changed(lv_event_t *e) {
    lv_obj_t *cb = lv_event_get_target(e);
    bool is_checked = lv_obj_has_state(cb, LV_STATE_CHECKED);

    // チェックが入ったら「次へ」ボタンを押せるようにし、外れたら無効化（グレーアウト）する
    if (is_checked) {
        lv_obj_clear_state(next_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(next_btn, LV_STATE_DISABLED);
    }
}

// 「次へ」ボタンが押されたときの処理
static void on_next_clicked(lv_event_t *e) {
    // 1. 同意したことをデータ層（src/data）に保存する関数を呼ぶ
    // 2. 次の画面（ON-03 権限リクエスト）へ切り替える
}

void ui_agreement_create(void) {
    // 1. 新しい画面のベースを作る
    screen_obj = lv_obj_create(NULL);

    // 2. タイトルラベル（文字）を作る
    lv_obj_t *title = lv_label_create(screen_obj);
    lv_label_set_text(title, "利用規約とプライバシーポリシー");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40); // 画面上部中央から下に40px

    // 3. 説明文ラベルを作る
    lv_obj_t *desc = lv_label_create(screen_obj);
    lv_label_set_text(desc, "ねむのきを利用するには、規約への同意が必要です。");
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 80);

    // 4. チェックボックスを作る
    agree_checkbox = lv_checkbox_create(screen_obj);
    lv_checkbox_set_text(agree_checkbox, "利用規約に同意する");
    lv_obj_align(agree_checkbox, LV_ALIGN_CENTER, 0, 0);
    // タップイベントを検知するコールバック関数を登録
    lv_obj_add_event_cb(agree_checkbox, on_checkbox_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // 5. 「次へ」ボタンを作る
    next_btn = lv_btn_create(screen_obj);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    // 最初はチェックされていないのでボタンを押せない状態にしておく
    lv_obj_add_state(next_btn, LV_STATE_DISABLED);
    lv_obj_add_event_cb(next_btn, on_next_clicked, LV_EVENT_CLICKED, NULL);

    // ボタンの中に文字ラベルを入れる
    lv_obj_t *btn_label = lv_label_create(next_btn);
    lv_label_set_text(btn_label, "次へ進む");
    lv_obj_center(btn_label);

    // 作った画面をアクティブにする（表示する）
    lv_scr_load(screen_obj);
}

void ui_agreement_destroy(void) {
    if (screen_obj != NULL) {
        lv_obj_del(screen_obj); // LVGLが子オブジェクト（ボタンやラベル）もまとめて解放してくれる
        screen_obj = NULL;
    }
}
