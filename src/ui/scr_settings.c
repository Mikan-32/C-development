/*
 * ST-01 設定、BR-01 寝室、BR-02 カーテンをつなぐ（UX-5 改訂版準拠）
 */
#include "curtain.h"
#include "ui_internal.h"

#include <SDL3/SDL.h>
#include <string.h>

static bool s_curtain_ready = false;
static lv_timer_t *s_poll_timer = NULL;
static nk_curtain_op_state s_last_rendered_op = NK_CURTAIN_OP_IDLE;
static nk_curtain_conn_state s_last_rendered_conn = NK_CURTAIN_CONN_DISCONNECTED;
static int s_last_rendered_pct = -1;
static bool s_last_rendered_accepted = false;

static nk_curtain_api_device s_found_devices[NK_CURTAIN_API_MAX_DEVICES];
static size_t s_found_count = 0;
static lv_obj_t *s_ta_token = NULL;
static lv_obj_t *s_ta_secret = NULL;
static lv_obj_t *s_ta_url = NULL;

static lv_obj_t *s_time_modal = NULL;
static lv_obj_t *s_lbl_edit_hour = NULL;
static lv_obj_t *s_lbl_edit_min = NULL;
static bool s_editing_open = true;
static int s_edit_hour = 6;
static int s_edit_min = 20;

static void ensure_curtain(void) {
    if (!s_curtain_ready) {
        (void)nk_curtain_init();
        s_curtain_ready = true;
    }
}

/* 設定の 1 行：名前（と説明）、右に値と「›」 */
static lv_obj_t *item(lv_obj_t *card, const char *name, const char *note, const char *value) {
    lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_ver(row, 12, 0);
    if (lv_obj_get_child_count(card) > 1) {
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, NK_CARD_2, 0);
    }
    lv_obj_t *left = nk_box(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(left, 1);
    nk_text(left, name, NK_FS_BODY, false, NK_INK);
    if (note != NULL) {
        nk_text(left, note, NK_FS_CAPTION, false, NK_INK_3);
    }
    if (value != NULL) {
        nk_text(row, value, NK_FS_SMALL + 1, false, NK_INK_2);
    }
    nk_text(row, "›", NK_FS_TITLE + 3, false, NK_INK_3);
    return row;
}

static lv_obj_t *group(lv_obj_t *page, const char *title) {
    lv_obj_set_style_pad_top(nk_text(page, title, NK_FS_SMALL, true, NK_INK_3), 4, 0);
    lv_obj_t *card = nk_card(page);
    lv_obj_set_style_pad_ver(card, 4, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    return card;
}

void nk_build_settings(lv_obj_t *page) {
    ensure_curtain();
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);

    const char *conn_lbl = "未接続";
    if (snap.conn_state == NK_CURTAIN_CONN_CONNECTED) {
        conn_lbl = "接続済み";
    } else if (snap.conn_state == NK_CURTAIN_CONN_OFFLINE) {
        conn_lbl = "つながりません";
    }

    nk_text(page, "設定", NK_FS_HEAD + 4, true, NK_INK);

    lv_obj_t *room = group(page, "寝室");
    nk_on_click_open(item(room, "寝室", "カーテンと自動モード", conn_lbl), NK_UI_BEDROOM);

    lv_obj_t *sleep_goal = group(page, "睡眠");
    item(sleep_goal, "目標の就寝時刻", NULL, NK_DEMO_GOAL);
    item(sleep_goal, "起きる時刻", NULL, "平日 6:30・休日 8:00");

    lv_obj_t *other = group(page, "そのほか");
    item(other, "データとプライバシー", "同意、マイクの許可、記録の削除", NULL);
    item(other, "この分析について", NULL, NULL);
    item(other, "ライセンス表示", NULL, NULL);

    lv_obj_t *version =
        nk_text(page, "ねむのき 0.1.0（画面の試作）", NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_set_width(version, LV_PCT(100));
    lv_obj_set_style_text_align(version, LV_TEXT_ALIGN_CENTER, 0);
}

/* ---------------------------------------------------------------- 時刻設定ピッカー */

static void close_time_picker(void) {
    if (s_time_modal != NULL) {
        lv_obj_delete(s_time_modal);
        s_time_modal = NULL;
        s_lbl_edit_hour = NULL;
        s_lbl_edit_min = NULL;
    }
}

static void refresh_time_picker_labels(void) {
    if (s_lbl_edit_hour != NULL) {
        char h_buf[16];
        lv_snprintf(h_buf, sizeof(h_buf), "%02d 時", s_edit_hour);
        lv_label_set_text(s_lbl_edit_hour, h_buf);
    }
    if (s_lbl_edit_min != NULL) {
        char m_buf[16];
        lv_snprintf(m_buf, sizeof(m_buf), "%02d 分", s_edit_min);
        lv_label_set_text(s_lbl_edit_min, m_buf);
    }
}

static void on_hour_step_clicked(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    s_edit_hour = (s_edit_hour + delta + 24) % 24;
    refresh_time_picker_labels();
}

static void on_min_step_clicked(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    s_edit_min = (s_edit_min + delta + 60) % 60;
    refresh_time_picker_labels();
}

static void on_time_cancel_clicked(lv_event_t *e) {
    (void)e;
    close_time_picker();
}

static void on_time_save_clicked(lv_event_t *e) {
    (void)e;
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    if (s_editing_open) {
        snap.schedule.open_hour = s_edit_hour;
        snap.schedule.open_min = s_edit_min;
    } else {
        snap.schedule.close_hour = s_edit_hour;
        snap.schedule.close_min = s_edit_min;
    }
    (void)nk_curtain_set_schedule(&snap.schedule);
    close_time_picker();
    nk_ui_open(NK_UI_BEDROOM);
}

static void open_time_picker(bool is_open_time) {
    close_time_picker();

    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    s_editing_open = is_open_time;
    s_edit_hour = is_open_time ? snap.schedule.open_hour : snap.schedule.close_hour;
    s_edit_min = is_open_time ? snap.schedule.open_min : snap.schedule.close_min;

    s_time_modal = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_time_modal);
    lv_obj_set_size(s_time_modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_time_modal, NK_BG, 0);
    lv_obj_set_style_bg_opa(s_time_modal, LV_OPA_90, 0);
    lv_obj_set_style_pad_all(s_time_modal, NK_PAGE_PAD, 0);
    lv_obj_set_flex_flow(s_time_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_time_modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *panel = nk_card(s_time_modal);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_border_color(panel, NK_SELECT, 0);
    lv_obj_set_style_pad_row(panel, 12, 0);

    nk_text(panel, is_open_time ? "開ける時刻を設定" : "閉める時刻を設定", NK_FS_TITLE, true,
            NK_INK);

    /* 時の調整行 */
    lv_obj_t *h_row = nk_box(panel, LV_FLEX_FLOW_ROW);
    lv_obj_t *h_minus = nk_button(h_row, "-1時間", false);
    lv_obj_set_width(h_minus, 84);
    lv_obj_set_height(h_minus, 40);
    lv_obj_add_event_cb(h_minus, on_hour_step_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

    s_lbl_edit_hour = nk_text(h_row, "06 時", NK_FS_TITLE + 2, true, NK_INK);

    lv_obj_t *h_plus = nk_button(h_row, "+1時間", false);
    lv_obj_set_width(h_plus, 84);
    lv_obj_set_height(h_plus, 40);
    lv_obj_add_event_cb(h_plus, on_hour_step_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    /* 分の調整行（1分単位と5分単位の両方を用意） */
    lv_obj_t *m_row = nk_box(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(m_row, 4, 0);

    lv_obj_t *m_m5 = nk_button(m_row, "-5", false);
    lv_obj_set_width(m_m5, 48);
    lv_obj_set_height(m_m5, 40);
    lv_obj_add_event_cb(m_m5, on_min_step_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)-5);

    lv_obj_t *m_m1 = nk_button(m_row, "-1", false);
    lv_obj_set_width(m_m1, 44);
    lv_obj_set_height(m_m1, 40);
    lv_obj_add_event_cb(m_m1, on_min_step_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

    s_lbl_edit_min = nk_text(m_row, "20 分", NK_FS_TITLE + 2, true, NK_INK);

    lv_obj_t *m_p1 = nk_button(m_row, "+1", false);
    lv_obj_set_width(m_p1, 44);
    lv_obj_set_height(m_p1, 40);
    lv_obj_add_event_cb(m_p1, on_min_step_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lv_obj_t *m_p5 = nk_button(m_row, "+5", false);
    lv_obj_set_width(m_p5, 48);
    lv_obj_set_height(m_p5, 40);
    lv_obj_add_event_cb(m_p5, on_min_step_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)5);

    refresh_time_picker_labels();

    /* 保存・キャンセルボタン */
    lv_obj_t *act = nk_box(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(act, 10, 0);
    lv_obj_t *cancel_btn = nk_button(act, "キャンセル", false);
    lv_obj_set_flex_grow(cancel_btn, 1);
    lv_obj_add_event_cb(cancel_btn, on_time_cancel_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_btn = nk_button(act, "保存", true);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_add_event_cb(save_btn, on_time_save_clicked, LV_EVENT_CLICKED, NULL);
}

static void on_edit_open_time_clicked(lv_event_t *e) {
    (void)e;
    open_time_picker(true);
}

static void on_edit_close_time_clicked(lv_event_t *e) {
    (void)e;
    open_time_picker(false);
}

/* ---------------------------------------------------------------- BR-01 寝室 */

static void stop_bedroom_timer(void) {
    if (s_poll_timer != NULL) {
        lv_timer_delete(s_poll_timer);
        s_poll_timer = NULL;
    }
}

static void on_bedroom_deleted(lv_event_t *e) {
    (void)e;
    stop_bedroom_timer();
    close_time_picker();
}

static void on_bedroom_timer(lv_timer_t *timer) {
    (void)timer;
    if (nk_ui_current() != NK_UI_BEDROOM) {
        stop_bedroom_timer();
        return;
    }
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    if (snap.op_state != s_last_rendered_op || snap.conn_state != s_last_rendered_conn ||
        snap.open_pct != s_last_rendered_pct || snap.command_accepted != s_last_rendered_accepted) {
        nk_ui_open(NK_UI_BEDROOM);
    }
}

static void on_open_clicked(lv_event_t *e) {
    (void)e;
    (void)nk_curtain_open_async();
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_close_clicked(lv_event_t *e) {
    (void)e;
    (void)nk_curtain_close_async();
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_slider_released(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target_obj(e);
    int32_t val = lv_slider_get_value(slider);
    (void)nk_curtain_set_position_async((int)val);
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_refresh_clicked(lv_event_t *e) {
    (void)e;
    (void)nk_curtain_refresh_status();
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_toggle_open_sched(lv_event_t *e) {
    (void)e;
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    snap.schedule.open_enabled = !snap.schedule.open_enabled;
    (void)nk_curtain_set_schedule(&snap.schedule);
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_toggle_close_sched(lv_event_t *e) {
    (void)e;
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    snap.schedule.close_enabled = !snap.schedule.close_enabled;
    (void)nk_curtain_set_schedule(&snap.schedule);
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_repeat_everyday(lv_event_t *e) {
    (void)e;
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    snap.schedule.repeat = NK_CURTAIN_REPEAT_EVERYDAY;
    (void)nk_curtain_set_schedule(&snap.schedule);
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_repeat_weekdays(lv_event_t *e) {
    (void)e;
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);
    snap.schedule.repeat = NK_CURTAIN_REPEAT_WEEKDAYS;
    (void)nk_curtain_set_schedule(&snap.schedule);
    nk_ui_open(NK_UI_BEDROOM);
}

/* カーテンの開き具合の図：窓の両側から、閉じている分だけ布をかける */
static void curtain_picture(lv_obj_t *parent, int32_t open_pct) {
    const int32_t w = 112;
    const int32_t h = 84;
    lv_obj_t *window = nk_rect(parent, w, h, NK_SELECT, 6);
    lv_obj_set_pos(nk_rect(window, 2, h, NK_CARD, 0), w / 2 - 1, 0);
    int32_t cloth = (100 - open_pct) * w / 200;
    lv_obj_set_pos(nk_rect(window, cloth, h, NK_INK_3, 0), 0, 0);
    lv_obj_set_pos(nk_rect(window, cloth, h, NK_INK_3, 0), w - cloth, 0);
}

static void plan_row(lv_obj_t *card, const char *name, const char *time_text, bool on,
                     lv_event_cb_t edit_cb, lv_event_cb_t toggle_cb) {
    lv_obj_t *row = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_t *left = nk_box(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_clickable(left, true);
    lv_obj_add_event_cb(left, edit_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_grow(nk_text(left, name, NK_FS_BODY, false, NK_INK), 1);

    lv_obj_t *time_box = nk_box(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(time_box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(time_box, NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(time_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(time_box, 8, 0);
    lv_obj_set_style_pad_hor(time_box, 10, 0);
    lv_obj_set_style_pad_ver(time_box, 4, 0);
    nk_text(time_box, time_text, NK_FS_TITLE + 1, true, NK_INK);

    nk_text(row, on ? "オン" : "オフ", NK_FS_SMALL, true, on ? NK_GOOD : NK_INK_3);
    lv_obj_t *sw = nk_switch(row, on);
    lv_obj_add_event_cb(sw, toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void nk_build_bedroom(lv_obj_t *page) {
    ensure_curtain();
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);

    s_last_rendered_op = snap.op_state;
    s_last_rendered_conn = snap.conn_state;
    s_last_rendered_pct = snap.open_pct;
    s_last_rendered_accepted = snap.command_accepted;

    lv_obj_add_event_cb(page, on_bedroom_deleted, LV_EVENT_DELETE, NULL);
    if (s_poll_timer == NULL) {
        s_poll_timer = lv_timer_create(on_bedroom_timer, 500, NULL);
    }

    /* 見出し */
    nk_header(page, "設定", "寝室");

    /* 1 接続の札（タップで BR-02 カーテンをつなぐへ） */
    lv_obj_t *link_card = nk_card(page);
    lv_obj_set_flex_flow(link_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(link_card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(link_card, 10, 0);

    const char *badge_txt = "未接続";
    lv_color_t badge_col = NK_INK_3;
    if (snap.conn_state == NK_CURTAIN_CONN_CONNECTED) {
        badge_txt = "接続済み";
        badge_col = NK_GOOD;
    } else if (snap.conn_state == NK_CURTAIN_CONN_CHECKING) {
        badge_txt = "確認中";
        badge_col = NK_WARN;
    } else if (snap.conn_state == NK_CURTAIN_CONN_OFFLINE) {
        badge_txt = "つながりません";
        badge_col = NK_SERIOUS;
    }
    nk_pill(link_card, badge_txt, badge_col, true);

    char link_lbl[64];
    if (snap.device_name[0] != '\0') {
        lv_snprintf(link_lbl, sizeof(link_lbl), "%s（%s） ›", snap.device_name, snap.masked_token);
    } else {
        lv_snprintf(link_lbl, sizeof(link_lbl), "カーテンをつなぐ ›");
    }
    nk_text(link_card, link_lbl, NK_FS_SMALL, false, NK_INK_2);
    nk_on_click_open(link_card, NK_UI_CURTAIN_CONNECT);

    /* 2 カーテンの状態 */
    lv_obj_t *state = nk_card(page);
    lv_obj_set_flex_flow(state, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(state, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(state, 16, 0);
    curtain_picture(state, snap.open_pct);
    lv_obj_t *words = nk_box(state, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(words, 1);

    char pct_buf[32];
    if (snap.open_pct >= 100) {
        lv_snprintf(pct_buf, sizeof(pct_buf), "全開（100%%）");
    } else if (snap.open_pct <= 0) {
        lv_snprintf(pct_buf, sizeof(pct_buf), "全閉（0%%）");
    } else {
        lv_snprintf(pct_buf, sizeof(pct_buf), "%d%% 開いています", snap.open_pct);
    }
    nk_text(words, pct_buf, NK_FS_TITLE + 1, true, NK_INK);

    char chk_buf[32];
    lv_snprintf(chk_buf, sizeof(chk_buf), "最終確認 %s", snap.last_checked_time);
    nk_text(words, chk_buf, NK_FS_SMALL, false, NK_INK_2);
    nk_wrap_text(words, "（0%：全部閉／100%：全開）", NK_FS_CAPTION, false, NK_INK_3);

    /* 3 操作（依頼中は二重送信防止のため無効化：UX-5 2章） */
    bool is_pending = (snap.op_state == NK_CURTAIN_OP_PENDING);
    lv_obj_t *ops = nk_card(page);
    lv_obj_set_style_pad_row(ops, 12, 0);
    nk_card_title(ops, "操作", NULL);
    lv_obj_t *buttons = nk_box(ops, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(buttons, 12, 0);

    lv_obj_t *btn_open = nk_button(buttons, "開ける", false);
    lv_obj_set_flex_grow(btn_open, 1);
    if (is_pending) {
        lv_obj_add_state(btn_open, LV_STATE_DISABLED);
        lv_obj_set_style_opa(btn_open, LV_OPA_40, 0);
    } else {
        lv_obj_add_event_cb(btn_open, on_open_clicked, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *btn_close = nk_button(buttons, "閉める", false);
    lv_obj_set_flex_grow(btn_close, 1);
    if (is_pending) {
        lv_obj_add_state(btn_close, LV_STATE_DISABLED);
        lv_obj_set_style_opa(btn_close, LV_OPA_40, 0);
    } else {
        lv_obj_add_event_cb(btn_close, on_close_clicked, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *slider = lv_slider_create(ops);
    lv_obj_set_size(slider, LV_PCT(100), 6);
    lv_slider_set_value(slider, snap.open_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, NK_SERIES, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, NK_INK, LV_PART_KNOB);
    if (is_pending) {
        lv_obj_add_state(slider, LV_STATE_DISABLED);
    } else {
        lv_obj_add_event_cb(slider, on_slider_released, LV_EVENT_RELEASED, NULL);
    }

    lv_obj_t *scale = nk_box(ops, LV_FLEX_FLOW_ROW);
    nk_text(scale, "0%", NK_FS_CAPTION, false, NK_INK_3);
    nk_text(scale, "離したときに命令を送ります", NK_FS_CAPTION, false, NK_INK_3);
    nk_text(scale, "100%", NK_FS_CAPTION, false, NK_INK_3);

    /* 4 結果の行（nk_op_result：依頼中・成功・失敗・未確認の 4 状態） */
    lv_obj_t *result = nk_card(page);
    nk_text(result, "直近の結果", NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_t *done = nk_box(result, LV_FLEX_FLOW_ROW);
    if (snap.op_state == NK_CURTAIN_OP_PENDING) {
        lv_obj_t *sp = lv_spinner_create(done);
        lv_obj_set_size(sp, 22, 22);
        lv_obj_set_flex_grow(nk_wrap_text(done, snap.status_message, NK_FS_BODY, true, NK_INK), 1);
        nk_pill(done, snap.command_accepted ? "動作中" : "送信中", NK_WARN, false);
    } else if (snap.op_state == NK_CURTAIN_OP_CONFIRMED) {
        lv_obj_set_flex_grow(nk_wrap_text(done, snap.status_message, NK_FS_BODY + 1, true, NK_INK),
                             1);
        nk_pill(done, "確認済み", NK_GOOD, false);
    } else if (snap.op_state == NK_CURTAIN_OP_FAILED) {
        lv_obj_set_flex_grow(
            nk_wrap_text(done, snap.status_message, NK_FS_SMALL + 1, true, NK_SERIOUS), 1);
        nk_pill(done, "失敗", NK_SERIOUS, false);
        lv_obj_t *act = nk_box(result, LV_FLEX_FLOW_ROW);
        lv_obj_t *retry = nk_button(act, "もう一度", false);
        lv_obj_set_flex_grow(retry, 1);
        lv_obj_add_event_cb(retry, snap.target_open_pct >= 50 ? on_open_clicked : on_close_clicked,
                            LV_EVENT_CLICKED, NULL);
        lv_obj_t *chk = nk_button(act, "接続を確かめる", false);
        lv_obj_set_flex_grow(chk, 1);
        nk_on_click_open(chk, NK_UI_CURTAIN_CONNECT);
    } else if (snap.op_state == NK_CURTAIN_OP_UNCONFIRMED) {
        lv_obj_set_flex_grow(
            nk_wrap_text(done, snap.status_message, NK_FS_SMALL + 1, true, NK_WARN), 1);
        nk_pill(done, "未確認", NK_WARN, false);
        lv_obj_t *ver = nk_button(result, "状態を確かめる", false);
        lv_obj_add_event_cb(ver, on_refresh_clicked, LV_EVENT_CLICKED, NULL);
    } else {
        nk_text(done, "まだ操作していません", NK_FS_BODY, false, NK_INK_2);
    }

    /* 5 予約（時刻部分をタップして時刻設定ピッカーを開く） */
    lv_obj_t *plan = nk_card(page);
    lv_obj_set_style_pad_row(plan, 10, 0);
    nk_card_title(plan, "予約", "時刻を押して変更");
    char open_t[16], close_t[16];
    lv_snprintf(open_t, sizeof(open_t), "%d:%02d", snap.schedule.open_hour, snap.schedule.open_min);
    lv_snprintf(close_t, sizeof(close_t), "%d:%02d", snap.schedule.close_hour,
                snap.schedule.close_min);
    plan_row(plan, "開ける時刻", open_t, snap.schedule.open_enabled, on_edit_open_time_clicked,
             on_toggle_open_sched);
    plan_row(plan, "閉める時刻", close_t, snap.schedule.close_enabled, on_edit_close_time_clicked,
             on_toggle_close_sched);
    lv_obj_t *repeat = nk_box(plan, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(repeat, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *chip_all =
        nk_chip(repeat, "毎日", snap.schedule.repeat == NK_CURTAIN_REPEAT_EVERYDAY);
    lv_obj_set_clickable(chip_all, true);
    lv_obj_add_event_cb(chip_all, on_repeat_everyday, LV_EVENT_CLICKED, NULL);
    lv_obj_t *chip_wd =
        nk_chip(repeat, "平日のみ", snap.schedule.repeat == NK_CURTAIN_REPEAT_WEEKDAYS);
    lv_obj_set_clickable(chip_wd, true);
    lv_obj_add_event_cb(chip_wd, on_repeat_weekdays, LV_EVENT_CLICKED, NULL);

    /* 6 自動モード（オフのときは説明と「有効にする」） */
    lv_obj_t *mode = nk_card(page);
    lv_obj_t *mode_head = nk_box(mode, LV_FLEX_FLOW_ROW);
    nk_text(mode_head, "自動モード", NK_FS_TITLE, true, NK_INK);
    nk_pill(mode_head, "オフ", NK_INK_3, false);
    lv_obj_t *mode_body = nk_box(mode, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mode_body, 12, 0);
    lv_obj_set_flex_grow(nk_wrap_text(mode_body,
                                      "決めた時刻に、カーテンとアラームを自動で動かします",
                                      NK_FS_SMALL, false, NK_INK_2),
                         1);
    lv_obj_t *enable = nk_button(mode_body, "有効にする", false);
    lv_obj_set_size(enable, 120, 44);

    /* 7 動作の記録（直近 5 件。失敗も残す） */
    lv_obj_t *history = nk_card(page);
    nk_card_title(history, "動作の記録", "直近5件");
    if (snap.log_count == 0) {
        nk_text(history, "まだ動作の記録はありません", NK_FS_SMALL, false, NK_INK_3);
    } else {
        for (int i = 0; i < snap.log_count; i++) {
            lv_obj_t *row = nk_box(history, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_width(nk_text(row, snap.logs[i].when_text, NK_FS_SMALL, false, NK_INK_3),
                             84);
            lv_obj_set_flex_grow(nk_wrap_text(row, snap.logs[i].message, NK_FS_SMALL + 1, false,
                                              snap.logs[i].is_error ? NK_SERIOUS : NK_INK),
                                 1);
        }
    }
}

/* ---------------------------------------------------------------- BR-02 カーテンをつなぐ */

static void on_open_guide_url(lv_event_t *e) {
    (void)e;
    (void)SDL_OpenURL("https://support.switch-bot.com/");
}

static void on_paste_token(lv_event_t *e) {
    (void)e;
    char *clip = SDL_GetClipboardText();
    if (clip != NULL && s_ta_token != NULL) {
        lv_textarea_set_text(s_ta_token, clip);
        SDL_free(clip);
    }
}

static void on_paste_secret(lv_event_t *e) {
    (void)e;
    char *clip = SDL_GetClipboardText();
    if (clip != NULL && s_ta_secret != NULL) {
        lv_textarea_set_text(s_ta_secret, clip);
        SDL_free(clip);
    }
}

static void on_paste_url(lv_event_t *e) {
    (void)e;
    char *clip = SDL_GetClipboardText();
    if (clip != NULL && s_ta_url != NULL) {
        lv_textarea_set_text(s_ta_url, clip);
        SDL_free(clip);
    }
}

static void on_connect_submit(lv_event_t *e) {
    (void)e;
    if (s_ta_token == NULL || s_ta_secret == NULL) {
        return;
    }
    if (s_ta_url != NULL) {
        const char *u = lv_textarea_get_text(s_ta_url);
        if (u != NULL && u[0] != '\0') {
            (void)nk_curtain_set_base_url(u);
        }
    }
    const char *tok = lv_textarea_get_text(s_ta_token);
    const char *sec = lv_textarea_get_text(s_ta_secret);
    s_found_count = 0;
    (void)nk_curtain_connect(tok, sec, s_found_devices, NK_CURTAIN_API_MAX_DEVICES, &s_found_count);
    nk_ui_open(NK_UI_CURTAIN_CONNECT);
}

static void on_device_picked(lv_event_t *e) {
    size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx < s_found_count) {
        (void)nk_curtain_select_device(s_found_devices[idx].device_id,
                                       s_found_devices[idx].device_name);
        nk_ui_open(NK_UI_BEDROOM);
    }
}

static void on_nudge_clicked(lv_event_t *e) {
    (void)e;
    (void)nk_curtain_test_nudge_async();
    nk_ui_open(NK_UI_BEDROOM);
}

static void on_disconnect_clicked(lv_event_t *e) {
    (void)e;
    s_found_count = 0;
    (void)nk_curtain_disconnect();
    nk_ui_open(NK_UI_CURTAIN_CONNECT);
}

static lv_obj_t *make_input(lv_obj_t *parent, const char *placeholder, bool password) {
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, LV_PCT(100), 42);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, password);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_style_text_font(ta, nk_font(NK_FS_SMALL + 1, false), 0);
    lv_obj_set_style_text_font(ta, nk_font(NK_FS_SMALL + 1, false), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(ta, NK_CARD_2, 0);
    lv_obj_set_style_text_color(ta, NK_INK, 0);
    lv_obj_set_style_text_color(ta, NK_INK_3, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(ta, NK_SELECT, 0);
    return ta;
}

void nk_build_curtain_connect(lv_obj_t *page) {
    ensure_curtain();
    nk_curtain_snapshot snap;
    nk_curtain_get_snapshot(&snap);

    nk_header(page, "寝室", "カーテンをつなぐ");

    /* 1 説明と取得のしかた */
    lv_obj_t *guide = nk_card(page);
    nk_wrap_text(guide, "SwitchBot アプリの開発者向け設定でトークンとシークレットを取得します。",
                 NK_FS_SMALL, false, NK_INK_2);
    lv_obj_t *how = nk_button(guide, "取得のしかた", false);
    lv_obj_add_event_cb(how, on_open_guide_url, LV_EVENT_CLICKED, NULL);

    /* 2 トークン・シークレット（保存後は末尾4文字のみ表示：ADR-0006） */
    lv_obj_t *cred = nk_card(page);
    nk_card_title(cred, "認証情報", NULL);

    char tok_cap[48];
    lv_snprintf(tok_cap, sizeof(tok_cap), "トークン（保存済み：%s）", snap.masked_token);
    lv_obj_t *tok_row = nk_box(cred, LV_FLEX_FLOW_ROW);
    nk_text(tok_row, tok_cap, NK_FS_SMALL, false, NK_INK_2);
    lv_obj_t *paste_tok = nk_chip(tok_row, "貼り付け", false);
    lv_obj_set_clickable(paste_tok, true);
    lv_obj_add_event_cb(paste_tok, on_paste_token, LV_EVENT_CLICKED, NULL);
    s_ta_token = make_input(cred, "トークンを貼り付け", false);

    char sec_cap[48];
    lv_snprintf(sec_cap, sizeof(sec_cap), "シークレット（保存済み：%s）", snap.masked_secret);
    lv_obj_t *sec_row = nk_box(cred, LV_FLEX_FLOW_ROW);
    nk_text(sec_row, sec_cap, NK_FS_SMALL, false, NK_INK_2);
    lv_obj_t *paste_sec = nk_chip(sec_row, "貼り付け", false);
    lv_obj_set_clickable(paste_sec, true);
    lv_obj_add_event_cb(paste_sec, on_paste_secret, LV_EVENT_CLICKED, NULL);
    s_ta_secret = make_input(cred, "シークレットを貼り付け", true);

    lv_obj_t *url_row = nk_box(cred, LV_FLEX_FLOW_ROW);
    nk_text(url_row, "接続先 URL（検証用）", NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_t *paste_url = nk_chip(url_row, "貼り付け", false);
    lv_obj_set_clickable(paste_url, true);
    lv_obj_add_event_cb(paste_url, on_paste_url, LV_EVENT_CLICKED, NULL);
    s_ta_url = make_input(cred, NK_CURTAIN_API_DEFAULT_URL, false);
    lv_textarea_set_text(s_ta_url, snap.base_url);

    lv_obj_t *btn_conn = nk_button(cred, "つなぐ", true);
    lv_obj_add_event_cb(btn_conn, on_connect_submit, LV_EVENT_CLICKED, NULL);

    if (snap.status_message[0] != '\0') {
        nk_wrap_text(cred, snap.status_message, NK_FS_SMALL, false,
                     snap.conn_state == NK_CURTAIN_CONN_CONNECTED ? NK_GOOD : NK_SERIOUS);
    }

    /* 3 見つかったカーテン一覧（Curtain / Curtain3 のみ） */
    if (s_found_count > 0) {
        lv_obj_t *dev_card = nk_card(page);
        nk_card_title(dev_card, "見つかったカーテン", "1つ選ぶ");
        for (size_t i = 0; i < s_found_count; i++) {
            char label[192];
            lv_snprintf(label, sizeof(label), "%s（%s）", s_found_devices[i].device_name,
                        s_found_devices[i].device_type);
            lv_obj_t *pick = nk_button(dev_card, label, false);
            lv_obj_add_event_cb(pick, on_device_picked, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        }
    }

    /* 4 試しに少し動かす・接続を解除 */
    if (snap.conn_state == NK_CURTAIN_CONN_CONNECTED) {
        lv_obj_t *ops = nk_card(page);
        lv_obj_t *nudge = nk_button(ops, "試しに少し動かす", false);
        lv_obj_add_event_cb(nudge, on_nudge_clicked, LV_EVENT_CLICKED, NULL);

        lv_obj_t *disc = nk_button(ops, "接続を解除", false);
        lv_obj_set_style_border_color(disc, NK_SERIOUS, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(disc, 0), NK_SERIOUS, 0);
        lv_obj_add_event_cb(disc, on_disconnect_clicked, LV_EVENT_CLICKED, NULL);
    }
}
