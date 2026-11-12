/*
 * BR-01 寝室、BR-02 カーテンをつなぐ、BR-03 自動モードを有効にする（UX-5 改訂版）
 *
 * 寝室は「設定」の中に置く（改訂版の仮の形）。カーテンの操作はデモで、命令の受け付けと
 * 実際の動作を分け、確かめてから「成功」と出す（依頼中 → 動いています → 成功（確認済み））。
 */
#include "ui_internal.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK_TIME "7:03" /* デモの「今」 */

static void connect_cb(lv_event_t *e) {
    (void)e;
    nk_ui_push(NK_UI_BR02, NK_UI_ST_NONE);
}

static void toast_cb(lv_event_t *e) {
    nk_ui_toast(lv_event_get_user_data(e));
}

/* 太い枠のボタン（改訂版の図の白いボタン） */
static lv_obj_t *outline_strong(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                                void *user_data) {
    lv_obj_t *btn = nk_ui_btn_ghost(parent, text, cb, user_data);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, NK_INK_2, 0);
    lv_obj_set_style_border_opa(btn, 150, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), NK_INK, 0);
    return btn;
}

/* ---------------------------------------------------------------- カーテンの操作（デモ） */

static struct {
    lv_obj_t *view;
    lv_obj_t *pos_label;
    lv_obj_t *slider;
    lv_obj_t *slot;
    lv_obj_t *btn_open;
    lv_obj_t *btn_close;
    int position;
    int target;
} s_br;

static void position_text(int pos, char *buf, size_t size) {
    if (pos >= 100) {
        (void)snprintf(buf, size, "全開");
    } else if (pos <= 0) {
        (void)snprintf(buf, size, "全閉");
    } else {
        (void)snprintf(buf, size, "%d%% 開いています", pos);
    }
}

static void show_position(int pos) {
    char label[64];
    position_text(pos, label, sizeof(label));
    lv_label_set_text(s_br.pos_label, label);
    nk_ui_curtain_view_set(s_br.view, pos);
    lv_slider_set_value(s_br.slider, pos, LV_ANIM_OFF);
}

static void set_busy(bool busy) {
    lv_obj_set_disabled(s_br.btn_open, busy);
    lv_obj_set_disabled(s_br.btn_close, busy);
    lv_obj_set_disabled(s_br.slider, busy);
}

/* 4 結果の行。成功と出すのは「成功（確認済み）」だけ */
static void result(nk_op op, const char *text, const char *sub, const char *link,
                   lv_event_cb_t cb) {
    lv_obj_clean(s_br.slot);
    nk_ui_label(s_br.slot, "直近の結果", NK_FR(12), NK_INK_3);
    if (op != NK_OP_OK) {
        nk_ui_op_result(s_br.slot, op, text, sub, link, cb, NULL);
        return;
    }
    lv_obj_t *row = nk_ui_row(s_br.slot, 8);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(row, 4, 0);
    nk_ui_label(row, text, NK_FB(15), NK_INK);
    nk_ui_pill(row, "確認済み", NK_TONE_GOOD, false);
    if (sub != NULL) {
        nk_ui_label(s_br.slot, sub, NK_FR(12), NK_INK_3);
    }
}

static void move_tick_cb(lv_timer_t *timer) {
    int step = s_br.target > s_br.position ? 4 : -4;
    int next = s_br.position + step;
    if ((step > 0 && next >= s_br.target) || (step < 0 && next <= s_br.target)) {
        next = s_br.target;
    }
    s_br.position = next;
    show_position(next);
    if (next != s_br.target) {
        return;
    }
    nk_ui_timer_stop(timer);
    nk_demo_state_get()->curtain_pos = next;
    set_busy(false);
    char text[96];
    if (next >= 100) {
        (void)snprintf(text, sizeof(text), "開けました（%s 確認）", CHECK_TIME);
    } else if (next <= 0) {
        (void)snprintf(text, sizeof(text), "閉めました（%s 確認）", CHECK_TIME);
    } else {
        (void)snprintf(text, sizeof(text), "%d%% にしました（%s 確認）", next, CHECK_TIME);
    }
    result(NK_OP_OK, text, "手動の操作", NULL, NULL);
}

static void start_moving(void) {
    result(NK_OP_PENDING, "動いています", "開き具合を 3 秒ごとに確かめています", NULL, NULL);
    (void)nk_ui_timer(move_tick_cb, 90, NULL);
}

static void accepted_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    start_moving();
}

static void command(int target) {
    if (target == s_br.position) {
        return;
    }
    s_br.target = target;
    set_busy(true);
    result(NK_OP_PENDING, "カーテンに伝えています", NULL, NULL, NULL);
    (void)nk_ui_timer(accepted_cb, 900, NULL);
}

static void open_cb(lv_event_t *e) {
    (void)e;
    command(100);
}

static void close_cb(lv_event_t *e) {
    (void)e;
    command(0);
}

static void slider_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *sl = lv_event_get_current_target_obj(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        /* 動かしている間は命令を送らず、図だけ変える */
        nk_ui_curtain_view_set(s_br.view, lv_slider_get_value(sl));
    } else if (code == LV_EVENT_RELEASED) {
        command(lv_slider_get_value(sl));
    }
}

static void retry_cb(lv_event_t *e) {
    (void)e;
    nk_ui_replace(NK_UI_BR01, NK_UI_ST_CURTAIN_PENDING);
}

static void check_cb(lv_event_t *e) {
    (void)e;
    nk_ui_replace(NK_UI_BR01, NK_UI_ST_CURTAIN_CHECKING);
}

static void checked_done_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    nk_ui_replace(NK_UI_BR01, NK_UI_ST_NONE);
}

/* 「もう一度」で開け直すデモ：閉じた状態から開ける */
static void pending_start_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    s_br.target = 100;
    start_moving();
}

/* ---------------------------------------------------------------- BR-01 */

/* 1 接続の札。押すと BR-02 */
static void connection_card(lv_obj_t *body, nk_ui_state state) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(card, 10, 0);
    lv_obj_set_clickable(card, true);
    lv_obj_set_style_bg_color(card, NK_CARD_2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, connect_cb, LV_EVENT_CLICKED, NULL);
    if (state == NK_UI_ST_EMPTY_FIRST) {
        nk_ui_pill(card, "未接続", NK_TONE_NEUTRAL, true);
        nk_ui_label(card, "つなぐ ›", NK_FB(13), NK_BRAND);
    } else if (state == NK_UI_ST_CURTAIN_OFFLINE || state == NK_UI_ST_AUTO_PAUSED) {
        nk_ui_pill(card, "つながりません", NK_TONE_SERIOUS, true);
        nk_ui_label(card, "つなぎ直す ›", NK_FB(13), NK_BRAND);
    } else if (state == NK_UI_ST_CURTAIN_CHECKING) {
        nk_ui_pill(card, "確認中", NK_TONE_INFO, true);
        nk_ui_label(card, "つなぎ直す ›", NK_FR(13), NK_INK_3);
    } else {
        nk_ui_pill(card, "接続済み", NK_TONE_GOOD, true);
        nk_ui_label(card, "つなぎ直す ›", NK_FR(13), NK_INK_3);
    }
}

/* 2 カーテンの状態 */
static void state_card(lv_obj_t *body, nk_ui_state state) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 14, 0);
    s_br.view = nk_ui_curtain_view(card, 100, s_br.position);
    lv_obj_set_width(s_br.view, 150);
    nk_ui_curtain_view_set(s_br.view, s_br.position);
    if (state == NK_UI_ST_CURTAIN_CHECKING || state == NK_UI_ST_CURTAIN_OFFLINE) {
        lv_obj_set_style_opa(s_br.view, LV_OPA_50, 0);
    }
    lv_obj_t *col = nk_ui_col(card, 4);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    char label[64];
    position_text(s_br.position, label, sizeof(label));
    s_br.pos_label = nk_ui_text(col, label, NK_FB(17), NK_INK);
    nk_ui_label(col, state == NK_UI_ST_CURTAIN_CHECKING ? "状態を確かめています" : "最終確認 6:21",
                NK_FR(12), NK_INK_3);
    nk_ui_text(col, "（0%：全部閉／100%：全開）", NK_FR(11), NK_INK_3);
}

/* 3 操作と 4 結果の行 */
static void control_card(lv_obj_t *body, nk_ui_state state) {
    lv_obj_t *card = nk_ui_card(body);
    nk_ui_card_head(card, "操作");
    lv_obj_t *pair = nk_ui_btn_pair(card);
    s_br.btn_open = outline_strong(pair, "開ける", open_cb, NULL);
    lv_obj_set_height(s_br.btn_open, 52);
    lv_obj_set_flex_grow(s_br.btn_open, 1);
    s_br.btn_close = outline_strong(pair, "閉める", close_cb, NULL);
    lv_obj_set_height(s_br.btn_close, 52);
    lv_obj_set_flex_grow(s_br.btn_close, 1);

    lv_obj_t *wrap = nk_ui_box(card);
    lv_obj_set_width(wrap, LV_PCT(100));
    lv_obj_set_style_pad_hor(wrap, 10, 0);
    lv_obj_set_style_pad_top(wrap, 12, 0);
    lv_obj_set_style_pad_bottom(wrap, 2, 0);
    s_br.slider = nk_ui_slider(wrap, s_br.position, slider_cb, NULL);
    lv_obj_t *scale = nk_ui_row(card, 4);
    lv_obj_set_flex_align(scale, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_ui_label(scale, "0%", NK_FR(11), NK_INK_3);
    nk_ui_label(scale, "離したときに命令を送ります", NK_FR(11), NK_INK_3);
    nk_ui_label(scale, "100%", NK_FR(11), NK_INK_3);

    lv_obj_t *res = nk_ui_card(body);
    lv_obj_set_style_pad_row(res, 4, 0);
    lv_obj_set_style_pad_ver(res, 12, 0);
    s_br.slot = res;
    switch (state) {
    case NK_UI_ST_CURTAIN_CHECKING:
        set_busy(true);
        result(NK_OP_PENDING, "カーテンの状態を確かめています", NULL, NULL, NULL);
        if (!nk_ui_instant()) {
            (void)nk_ui_timer(checked_done_cb, 1800, NULL);
        }
        break;
    case NK_UI_ST_CURTAIN_PENDING:
        set_busy(true);
        result(NK_OP_PENDING, "カーテンに伝えています", NULL, NULL, NULL);
        if (!nk_ui_instant()) {
            (void)nk_ui_timer(pending_start_cb, 900, NULL);
        }
        break;
    case NK_UI_ST_CURTAIN_FAILED:
        result(NK_OP_FAILED, "開けられませんでした：ハブがオフラインです",
               "ハブの電源とインターネット接続を確かめると直ります", "もう一度", retry_cb);
        break;
    case NK_UI_ST_CURTAIN_UNCONFIRMED:
        result(NK_OP_UNCONFIRMED, "開ける命令は届きましたが、動いたか確認できませんでした", NULL,
               "状態を確かめる", check_cb);
        break;
    case NK_UI_ST_CURTAIN_OFFLINE:
    case NK_UI_ST_AUTO_PAUSED:
        set_busy(true);
        result(NK_OP_FAILED, "カーテンにつながりません", "最後に確認できたのは 6:21 です",
               "もう一度試す", check_cb);
        break;
    default: {
        char text[96];
        (void)snprintf(text, sizeof(text), "%d%% にしました（6:21 確認）", s_br.position);
        result(NK_OP_OK, text, "自動モードを使わない手動の操作", NULL, NULL);
        break;
    }
    }
}

static void schedule_card(lv_obj_t *body) {
    nk_demo_state *st = nk_demo_state_get();
    static const char *const kinds[] = {"毎日", "平日のみ"};
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_t *head = nk_ui_card_head(card, "予約");
    lv_obj_set_style_pad_bottom(head, 4, 0);
    if (st->automode) {
        /* 自動モードの間は手動の予約を使えなくする（UX-5 の 1 章） */
        nk_ui_hint(head, "自動モードで管理しています");
        lv_obj_set_style_opa(card, 115, 0);
    }
    static const char *const names[] = {"開ける時刻", "閉める時刻"};
    static const char *const times[] = {"6:20", "23:00"};
    for (int i = 0; i < 2; i++) {
        bool on = i == 0;
        lv_obj_t *row = nk_ui_setrow(card, names[i], NULL, NULL);
        nk_ui_label(row, st->automode ? "—" : times[i], NK_FB(18), NK_INK);
        nk_ui_label(row, on ? "オン" : "オフ", NK_FB(12), NK_INK_3);
        lv_obj_t *sw = nk_ui_switch(row, on && !st->automode, NULL, NULL);
        if (st->automode) {
            lv_obj_set_disabled(sw, true);
        }
    }
    lv_obj_t *row = nk_ui_row(card, 8);
    lv_obj_set_style_pad_top(row, 8, 0);
    nk_ui_segmented(row, kinds, 2, 0, NULL);
}

static void enable_cb(lv_event_t *e) {
    (void)e;
    nk_ui_push(NK_UI_BR03, NK_UI_ST_NONE);
}

static void skip_cb(lv_event_t *e) {
    (void)e;
    nk_demo_state_get()->automode_skip = !nk_demo_state_get()->automode_skip;
    nk_ui_rebuild();
}

static void stop_auto_cb(lv_event_t *e) {
    (void)e;
    nk_demo_state_get()->automode = false;
    nk_demo_state_get()->automode_skip = false;
    nk_ui_toast("自動モードを止めました");
    nk_ui_rebuild();
}

/* 6 自動モードのカード */
static void automode_card(lv_obj_t *body, nk_ui_state state) {
    nk_demo_state *st = nk_demo_state_get();
    bool connected = state != NK_UI_ST_EMPTY_FIRST;
    bool on = st->automode && connected;
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, "自動モード");
    nk_ui_pill(head, on ? "オン" : "オフ", on ? NK_TONE_GOOD : NK_TONE_NEUTRAL, false);

    if (!on) {
        lv_obj_t *row = nk_ui_row(card, 10);
        lv_obj_t *t = nk_ui_text(row, "決めた時刻に、カーテンとアラームを自動で動かします",
                                 NK_FR(13), NK_INK_2);
        lv_obj_set_width(t, 0);
        lv_obj_set_flex_grow(t, 1);
        lv_obj_t *btn = outline_strong(row, "有効にする", enable_cb, NULL);
        lv_obj_set_size(btn, 120, 48);
        if (!connected) {
            lv_obj_set_disabled(btn, true);
            nk_ui_text(card, "カーテンをつなぐと使えます", NK_FR(12), NK_INK_3);
        }
        return;
    }

    if (state == NK_UI_ST_AUTO_PAUSED) {
        nk_ui_banner(card, NK_TONE_WARN, NK_ICON_INFO, false,
                     "自動モードを一時停止しています：カーテンにつながりません",
                     "つながると自動で再開します。アラームはこのまま鳴ります", "直し方を見る",
                     connect_cb, NULL);
    } else if (st->automode_skip) {
        nk_ui_banner(card, NK_TONE_WARN, NK_ICON_CLOCK, false, "明日の朝は開けません（スキップ）",
                     NULL, "取り消す", skip_cb, NULL);
    } else {
        char sub[128];
        char open_t[8];
        char alarm_t[8];
        int alarm = nk_demo_next_alarm();
        nk_fmt_clock(open_t, sizeof(open_t), alarm - st->lead_min);
        nk_fmt_clock(alarm_t, sizeof(alarm_t), alarm);
        (void)snprintf(sub, sizeof(sub), "あす（日）%s カーテンを開ける・%s アラーム", open_t,
                       alarm_t);
        nk_ui_banner(card, NK_TONE_INFO, NK_ICON_CLOCK, false, "次に自動で行うこと", sub, NULL,
                     NULL, NULL);
    }
    lv_obj_t *pair = nk_ui_btn_pair(card);
    lv_obj_t *a = nk_ui_btn_ghost(
        pair, st->automode_skip ? "スキップを取り消す" : "次の 1 回だけスキップ", skip_cb, NULL);
    lv_obj_set_height(a, 44);
    lv_obj_set_flex_grow(a, 1);
    lv_obj_t *b = nk_ui_btn_ghost(pair, "止める", stop_auto_cb, NULL);
    lv_obj_set_height(b, 44);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_text_font(lv_obj_get_child(a, 0), NK_FB(13), 0);
}

/* 7 動作の記録（直近 5 件。失敗も残す） */
static void log_card(lv_obj_t *body) {
    static const char *const rows[][2] = {
        {"今朝 6:21", "手動で 60% にしました（確認済み）"},
        {"昨夜 23:00", "手動で閉めました（確認済み）"},
        {"昨朝 6:20", "開けられませんでした（失敗）"},
        {"10/29 6:20", "予約で開けました（確認済み）"},
        {"10/28 23:10", "手動で閉めました（確認済み）"},
    };
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, "動作の記録");
    nk_ui_hint(head, "直近5件");
    lv_obj_t *list = nk_ui_col(card, 6);
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        lv_obj_t *row = nk_ui_row(list, 8);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t *t = nk_ui_label(row, rows[i][0], NK_FR(12), NK_INK_3);
        lv_obj_set_width(t, 80);
        lv_obj_t *text = nk_ui_text(row, rows[i][1], NK_FR(12), NK_INK_2);
        lv_obj_set_width(text, 0);
        lv_obj_set_flex_grow(text, 1);
    }
}

static void settings_toast_cb(lv_event_t *e) {
    (void)e;
    nk_ui_toast("PC 版では端末の設定画面は開きません");
}

void nk_scr_bedroom(lv_obj_t *body, nk_ui_state state) {
    nk_ui_subhead(body, "寝室");
    s_br.position = state == NK_UI_ST_CURTAIN_PENDING ? 0 : nk_demo_state_get()->curtain_pos;
    s_br.target = s_br.position;
    connection_card(body, state);

    if (state == NK_UI_ST_PERM_EXACT_ALARM) {
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_ALARM, false,
                     "正確な時刻に鳴らすための許可がオフです", "アラームと自動モードが使えません",
                     "設定を開く", settings_toast_cb, NULL);
    } else if (state == NK_UI_ST_CURTAIN_OFFLINE) {
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_INFO, false, "カーテンにつながりません",
                     "ハブの電源とインターネット接続を確かめてください", "接続を確かめる",
                     connect_cb, NULL);
    }

    if (state == NK_UI_ST_EMPTY_FIRST) {
        lv_obj_t *card = nk_ui_card(body);
        nk_ui_label(card, "カーテンがまだつながっていません", NK_FB(16), NK_INK);
        nk_ui_text(card,
                   "SwitchBot のカーテンをつなぐと、ここから開け閉めでき、アラームの前に自動で"
                   "開けられます。",
                   NK_FR(13), NK_INK_2);
        nk_ui_btn_primary(card, "カーテンをつなぐ", NK_ICON_CURTAIN, connect_cb, NULL);
        automode_card(body, state);
        return;
    }
    state_card(body, state);
    control_card(body, state);
    schedule_card(body);
    automode_card(body, state);
    log_card(body);
}

/* ---------------------------------------------------------------- BR-03 */

static void enable_confirm_cb(lv_event_t *e) {
    (void)e;
    nk_demo_state_get()->automode = true;
    nk_demo_state_get()->automode_skip = false;
    nk_ui_toast("自動モードをオンにしました");
    (void)nk_ui_back();
}

static void requirement(lv_obj_t *card, bool ok, const char *ok_text, const char *ng_text,
                        nk_ui_screen_id fix, nk_ui_state fix_state) {
    lv_obj_t *row = nk_ui_row(card, 8);
    nk_ui_icon(row, ok ? NK_ICON_CHECK : NK_ICON_INFO, 18, ok ? NK_GOOD : NK_WARN);
    lv_obj_t *t = nk_ui_text(row, ok ? ok_text : ng_text, NK_FR(13), ok ? NK_INK : NK_WARN);
    lv_obj_set_width(t, 0);
    lv_obj_set_flex_grow(t, 1);
    if (!ok) {
        lv_obj_t *go = outline_strong(row, "設定へ ›", NULL, NULL);
        lv_obj_set_size(go, 96, 36);
        lv_obj_set_style_border_color(go, NK_BRAND, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(go, 0), NK_BRAND, 0);
        lv_obj_set_style_text_font(lv_obj_get_child(go, 0), NK_FB(13), 0);
        nk_ui_on_click_nav(go, fix, fix_state, NK_UI_NAV_PUSH);
    }
}

void nk_scr_automode(lv_obj_t *body, nk_ui_state state) {
    nk_demo_state *st = nk_demo_state_get();
    nk_ui_subhead(body, "自動モードを有効にする");

    lv_obj_t *intro = nk_ui_card(body);
    lv_obj_set_style_pad_ver(intro, 12, 0);
    nk_ui_text(intro, "自動モードをオンにすると、次のことを自動で行います", NK_FB(14), NK_INK);

    /* ADR-0009 の 4 項目を、実際の時刻を入れた文で出す */
    char weekday[8];
    char holiday[8];
    char item1[128];
    char item2[128];
    nk_fmt_clock(weekday, sizeof(weekday), st->alarm_min);
    nk_fmt_clock(holiday, sizeof(holiday), st->alarm_holiday_min);
    (void)snprintf(item1, sizeof(item1), "毎朝のアラームをセットします（平日 %s、休日 %s）",
                   weekday, holiday);
    (void)snprintf(item2, sizeof(item2), "アラームの %d 分前に、静かにカーテンを開けます",
                   st->lead_min);
    const char *items[] = {item1, item2, "「おやすみ」で計測を始めると、カーテンを閉めます",
                           "アラームの前に「起きた」を押すと、その時点でカーテンを開けます"};
    lv_obj_t *list = nk_ui_card(body);
    lv_obj_set_style_pad_row(list, 10, 0);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *row = nk_ui_row(list, 10);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t *num = nk_ui_box(row);
        lv_obj_set_size(num, 24, 24);
        lv_obj_set_style_radius(num, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(num, 2, 0);
        lv_obj_set_style_border_color(num, NK_SERIES, 0);
        char n[4];
        (void)snprintf(n, sizeof(n), "%d", i + 1);
        lv_obj_t *nl = nk_ui_label(num, n, NK_FB(12), NK_SERIES_LT);
        lv_obj_center(nl);
        lv_obj_t *text = nk_ui_text(row, items[i], NK_FR(14), NK_INK);
        lv_obj_set_width(text, 0);
        lv_obj_set_flex_grow(text, 1);
    }

    lv_obj_t *note = nk_ui_card(body);
    nk_ui_text(note,
               "アドバイスの内容を、自動で設定に反映することはありません。いつでも手動で操作でき、"
               "この画面や通知から止められます。",
               NK_FR(13), NK_INK_2);

    bool curtain_ok = state != NK_UI_ST_EMPTY_FIRST;
    bool alarm_ok = state != NK_UI_ST_PERM_EXACT_ALARM;
    lv_obj_t *need = nk_ui_card(body);
    nk_ui_card_head(need, "有効にする前に必要なもの");
    requirement(need, curtain_ok, "カーテンに接続済み", "カーテンにつないでいません", NK_UI_BR02,
                NK_UI_ST_EMPTY_FIRST);
    requirement(need, alarm_ok, "正確なアラームの許可：許可済み", "正確なアラームの許可：未設定",
                NK_UI_NT05, NK_UI_ST_PERM_EXACT_ALARM);

    lv_obj_t *ok = nk_ui_btn_primary(body, "有効にする", NK_ICON_NONE, enable_confirm_cb, NULL);
    if (!curtain_ok || !alarm_ok) {
        /* 足りないときは、先にその設定へ案内する（押せない見た目にする） */
        lv_obj_set_disabled(ok, true);
    }
}

/* ---------------------------------------------------------------- BR-02 */

static struct {
    lv_obj_t *token;
    lv_obj_t *secret;
    lv_obj_t *connect;
    bool has_token;
    bool has_secret;
} s_conn;

static void update_connect_button(void) {
    if (s_conn.connect != NULL) {
        lv_obj_set_disabled(s_conn.connect, !(s_conn.has_token && s_conn.has_secret));
    }
}

static void paste_cb(lv_event_t *e) {
    lv_obj_t *value = lv_event_get_user_data(e);
    bool token = value == s_conn.token;
    lv_label_set_text(value, token ? "••••••••••••a1b2" : "••••••••••••9f0c");
    lv_obj_set_style_text_color(value, NK_INK, 0);
    if (token) {
        s_conn.has_token = true;
    } else {
        s_conn.has_secret = true;
    }
    update_connect_button();
}

/* 2・3 トークンとシークレット（貼り付けボタンと、手入力の欄） */
static lv_obj_t *secret_card(lv_obj_t *body, const char *title, bool filled, const char *value) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 8, 0);
    nk_ui_label(card, title, NK_FB(14), NK_INK);
    lv_obj_t *row = nk_ui_row(card, 10);
    lv_obj_t *field = nk_ui_box(row);
    lv_obj_set_height(field, 44);
    lv_obj_set_width(field, 0);
    lv_obj_set_flex_grow(field, 1);
    lv_obj_set_style_bg_color(field, NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(field, 1, 0);
    lv_obj_set_style_border_color(field, NK_WHITE, 0);
    lv_obj_set_style_border_opa(field, NK_OPA_LINE_2, 0);
    lv_obj_set_style_radius(field, 10, 0);
    lv_obj_set_style_pad_hor(field, 12, 0);
    lv_obj_t *val = nk_ui_label(field, filled ? value : "貼り付けか、手入力", NK_FR(14),
                                filled ? NK_INK : NK_INK_3);
    lv_obj_set_style_text_letter_space(val, 1, 0);
    lv_obj_align(val, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *paste = outline_strong(row, "貼り付け", paste_cb, val);
    lv_obj_set_size(paste, 92, 44);
    lv_obj_set_style_text_font(lv_obj_get_child(paste, 0), NK_FB(14), 0);
    return val;
}

static void connected_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    if (nk_ui_current_screen() == NK_UI_ON07) {
        nk_ui_replace(NK_UI_ON07, NK_UI_ST_CURTAIN_OK);
    } else {
        nk_ui_replace(NK_UI_BR02, NK_UI_ST_NONE);
    }
}

static void do_connect_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_current_target_obj(e);
    nk_ui_btn_set_text(btn, "つないでいます…");
    lv_obj_set_disabled(btn, true);
    (void)nk_ui_timer(connected_cb, 1200, NULL);
}

static void radio_cb(lv_event_t *e) {
    nk_demo_state_get()->curtain_choice = (int)(intptr_t)lv_event_get_user_data(e);
    nk_ui_rebuild();
}

static void next_cb(lv_event_t *e) {
    (void)e;
    nk_demo_state_get()->curtain_connected = true;
    nk_ui_replace(NK_UI_ON08, NK_UI_ST_NONE);
}

void nk_scr_connect_body(lv_obj_t *body, nk_ui_state state, bool onboarding) {
    nk_demo_state *st = nk_demo_state_get();
    /* BR-02 は「接続済み」から、ON-07 は「未入力」から始める */
    bool filled = onboarding ? (state == NK_UI_ST_CURTAIN_OK || state == NK_UI_ST_CURTAIN_AUTH)
                             : state != NK_UI_ST_EMPTY_FIRST;
    bool connected = filled && state != NK_UI_ST_CURTAIN_AUTH;
    s_conn.has_token = filled;
    s_conn.has_secret = filled;

    /* 1 説明 */
    lv_obj_t *card = nk_ui_card(body);
    nk_ui_text(card, "SwitchBot アプリで、トークンとシークレットを取得します", NK_FR(14), NK_INK);
    lv_obj_t *how =
        nk_ui_btn_ghost(card, "取得のしかた ›", toast_cb, (void *)"PC 版ではブラウザを開きません");
    lv_obj_set_size(how, 150, 36);
    lv_obj_set_style_radius(how, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(how, NK_BRAND, 0);
    lv_obj_set_style_border_opa(how, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(how, 0), NK_BRAND, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(how, 0), NK_FB(13), 0);

    s_conn.token = secret_card(body, "トークン", filled, "••••••••••••a1b2");
    s_conn.secret = secret_card(body, "シークレット", filled, "••••••••••••9f0c");
    nk_ui_text(body, "手入力は、英数字のキーボードだけです", NK_FR(11), NK_INK_3);

    if (state == NK_UI_ST_CURTAIN_AUTH) {
        /* つなげない理由ごとに、別の文で出す（UX-5 改訂版） */
        nk_ui_op_result(body, NK_OP_FAILED, "つなげませんでした",
                        "トークンかシークレットが違うようです。もう一度確かめてください",
                        "貼り付け直す", NULL, NULL);
    }
    s_conn.connect = nk_ui_btn_primary(body, connected ? "つなぎ直す" : "つなぐ", NK_ICON_NONE,
                                       do_connect_cb, NULL);
    update_connect_button();
    if (!connected) {
        if (onboarding) {
            nk_ui_text(body,
                       "あとでにすると、カーテンの開け閉めと自動の開閉は、つなぐまで使えません",
                       NK_FR(12), NK_INK_3);
            lv_obj_t *later = nk_ui_btn_ghost(body, "あとで", NULL, NULL);
            nk_ui_on_click_nav(later, NK_UI_ON09, NK_UI_ST_NONE, NK_UI_NAV_REPLACE);
        }
        return;
    }

    /* 4 見つかったカーテン（Curtain・Curtain 3 だけ）から 1 台選ぶ */
    lv_obj_t *list = nk_ui_card(body);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_t *head = nk_ui_card_head(list, "見つかったカーテン");
    lv_obj_set_style_pad_bottom(head, 6, 0);
    static const char *const names[] = {"寝室のカーテン（Curtain 3）", "居間のカーテン（Curtain）"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *row = nk_ui_setrow(list, names[i], NULL, NULL);
        lv_obj_set_clickable(row, true);
        lv_obj_add_event_cb(row, radio_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *radio = nk_ui_box(row);
        lv_obj_set_size(radio, 22, 22);
        lv_obj_set_style_radius(radio, LV_RADIUS_CIRCLE, 0);
        bool on = st->curtain_choice == i;
        lv_obj_set_style_border_width(radio, on ? 6 : 2, 0);
        lv_obj_set_style_border_color(radio, on ? NK_BRAND : NK_INK_3, 0);
        lv_obj_move_to_index(radio, 0);
    }
    nk_ui_text(list, "カーテン（Curtain・Curtain 3）だけを表示しています", NK_FR(11), NK_INK_3);

    /* 5 試しに少し動かす（任意） */
    outline_strong(body, "試しに少し動かす（任意）", toast_cb,
                   (void *)"10% だけ動かして、元に戻しました");

    if (onboarding) {
        nk_ui_btn_primary(body, "このカーテンで次へ", NK_ICON_NONE, next_cb, NULL);
        return;
    }
    /* 6 保存したトークン（末尾 4 文字だけ） */
    lv_obj_t *saved = nk_ui_card(body);
    lv_obj_set_flex_flow(saved, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(saved, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    nk_ui_label(saved, "保存したトークン　…a1b2", NK_FR(13), NK_INK_2);
    lv_obj_t *off =
        nk_ui_btn_ghost(saved, "接続を解除", toast_cb, (void *)"接続を解除しました（デモ）");
    lv_obj_set_size(off, 112, 40);
    lv_obj_set_style_text_font(lv_obj_get_child(off, 0), NK_FB(13), 0);
}

void nk_scr_connect(lv_obj_t *body, nk_ui_state state) {
    nk_ui_subhead(body, "カーテンをつなぐ");
    nk_scr_connect_body(body, state, false);
}
