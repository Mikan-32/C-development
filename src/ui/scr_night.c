/*
 * NT-01 睡眠モード（おやすみ）、NT-02 計測中、NT-03 アラーム鳴動、NT-04 おはよう、
 * NT-05 アラームの設定（UX-6。タブの名前は UX-3 改訂案の「睡眠モード」）
 */
#include "ui_internal.h"

#include <stdint.h>
#include <stdio.h>

static const char *const k_sounds[] = {"ホワイトノイズ", "雨",  "ピンクノイズ", "波",
                                       "ブラウンノイズ", "音楽"};
static const nk_icon k_sound_icons[] = {NK_ICON_WAVE, NK_ICON_RAIN, NK_ICON_WAVE,
                                        NK_ICON_SEA,  NK_ICON_WAVE, NK_ICON_MUSIC};
static const char *const k_timers[] = {"15分", "30分", "60分", "朝まで"};

/* 次のアラーム（デモの「あす」は日曜なので休日の時刻） */
static void alarm_text(char *buf, size_t size) {
    nk_fmt_clock(buf, size, nk_demo_next_alarm());
}

/* ---------------------------------------------------------------- NT-01 */

static void sound_cb(lv_event_t *e) {
    nk_demo_state *st = nk_demo_state_get();
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (st->sound == idx) {
        st->playing = !st->playing;
    } else {
        st->sound = idx;
        st->playing = true; /* タップで試聴（UX-6 の 1 章） */
    }
    nk_ui_rebuild();
}

static void play_cb(lv_event_t *e) {
    (void)e;
    nk_demo_state *st = nk_demo_state_get();
    if (st->sound < 0) {
        st->sound = 0;
    }
    st->playing = !st->playing;
    nk_ui_rebuild();
}

static void volume_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        nk_demo_state_get()->volume = lv_slider_get_value(lv_event_get_current_target_obj(e));
    }
}

static void timer_cb(lv_event_t *e) {
    nk_demo_state_get()->timer_choice = (int)(intptr_t)lv_event_get_user_data(e);
    nk_ui_rebuild();
}

static void alarm_switch_cb(lv_event_t *e) {
    nk_demo_state_get()->alarm_on =
        lv_obj_has_state(lv_event_get_current_target_obj(e), LV_STATE_CHECKED);
    nk_ui_rebuild();
}

static void start_tracking_cb(lv_event_t *e) {
    (void)e;
    nk_ui_push(NK_UI_NT02, NK_UI_ST_TRACKING);
}

static void settings_cb(lv_event_t *e) {
    (void)e;
    nk_ui_toast("PC 版では端末の設定画面は開きません");
}

static void sound_tile(lv_obj_t *grid, int idx, bool on, bool playing) {
    lv_obj_t *tile = lv_button_create(grid);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, (NK_CARD_INNER_W - 8) / 2, 54);
    lv_obj_set_style_radius(tile, NK_RADIUS_S, 0);
    lv_obj_set_style_bg_color(tile, on ? lv_color_mix(NK_BRAND, NK_CARD_2, 33) : NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_border_color(tile, on ? NK_BRAND : NK_CARD_2, 0);
    lv_obj_set_style_pad_hor(tile, 12, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tile, 8, 0);
    lv_obj_set_style_bg_color(tile, NK_CARD_3, LV_STATE_PRESSED);
    nk_ui_icon_tile(tile, k_sound_icons[idx], 28, NK_WHITE, 15, on ? NK_BRAND : NK_INK_2);
    lv_obj_t *col = nk_ui_col(tile, 0);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    nk_ui_label(col, k_sounds[idx], NK_FB(13), NK_INK);
    if (on) {
        nk_ui_label(col, playing ? "再生中" : "一時停止中", NK_FR(11), NK_INK_3);
        if (playing) {
            nk_ui_eq(tile, NK_BRAND, !nk_ui_reduce_motion());
        }
    }
    lv_obj_add_event_cb(tile, sound_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
}

static void night_checks(lv_obj_t *body, nk_ui_state state) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 6, 0);
    if (state == NK_UI_ST_BATTERY_LOW) {
        nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN,
                         "電池が残り 15% です。充電しながら計測すると、朝まで記録できます");
    } else {
        nk_ui_notice_row(card, NK_ICON_CHECK, NK_GOOD, "電池 64%・充電中");
    }
    if (state == NK_UI_ST_PERM_MIC) {
        nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN, "マイクがオフのため、いびきは記録されません");
        nk_ui_link(card, "マイクを許可", settings_cb, NULL);
    } else {
        nk_ui_notice_row(card, NK_ICON_CHECK, NK_GOOD,
                         "マイク：いびきも記録（録音は保存しません）");
    }
    if (state == NK_UI_ST_PERM_NOTIF) {
        nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN,
                         "通知がオフのため、計測中の表示とアラームの通知が出ません");
        nk_ui_link(card, "通知を許可", settings_cb, NULL);
    }
    nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN, "枕元のマットレスの上に置いてください");
}

static void sound_card(lv_obj_t *body) {
    nk_demo_state *st = nk_demo_state_get();
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, "入眠サウンド");
    char hint[192];
    (void)snprintf(hint, sizeof(hint), "タイマー %s", k_timers[st->timer_choice]);
    nk_ui_hint(head, hint);

    lv_obj_t *grid = nk_ui_row(card, 8);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 8, 0);
    for (int i = 0; i < 6; i++) {
        sound_tile(grid, i, st->sound == i, st->playing);
    }

    lv_obj_t *ctrl = nk_ui_row(card, 14);
    lv_obj_set_style_pad_top(ctrl, 4, 0);
    lv_obj_t *play = lv_button_create(ctrl);
    lv_obj_remove_style_all(play);
    lv_obj_set_size(play, 52, 52);
    lv_obj_set_style_radius(play, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(play, NK_BRAND, 0);
    lv_obj_set_style_bg_opa(play, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(play, lv_color_hex(0xE36F91), LV_STATE_PRESSED);
    lv_obj_t *pi = nk_ui_icon(play, st->playing ? NK_ICON_PAUSE : NK_ICON_PLAY, 22, NK_BRAND_INK);
    lv_obj_center(pi);
    lv_obj_add_event_cb(play, play_cb, LV_EVENT_CLICKED, NULL);
    nk_ui_icon(ctrl, NK_ICON_VOL, 20, NK_INK_3);
    lv_obj_t *vol = nk_ui_slider(ctrl, st->volume, volume_cb, NULL);
    lv_obj_set_width(vol, 0);
    lv_obj_set_flex_grow(vol, 1);
    lv_obj_set_style_margin_right(vol, 10, 0);

    lv_obj_t *chips = nk_ui_row(card, 8);
    for (int i = 0; i < 4; i++) {
        nk_ui_chip(chips, k_timers[i], st->timer_choice == i, timer_cb, (void *)(intptr_t)i);
    }
}

static void alarm_card(lv_obj_t *body, bool disabled) {
    nk_demo_state *st = nk_demo_state_get();
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, "アラーム");
    char hint[192];
    (void)snprintf(hint, sizeof(hint), "スヌーズ %d分・%d回まで", st->snooze_len, st->snooze_max);
    nk_ui_hint(head, hint);

    lv_obj_t *row = nk_ui_row(card, 8);
    lv_obj_t *time_btn = nk_ui_box(row);
    lv_obj_set_flex_flow(time_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(time_btn, 6, 0);
    lv_obj_set_clickable(time_btn, true);
    nk_ui_on_click_nav(time_btn, NK_UI_NT05, NK_UI_ST_NONE, NK_UI_NAV_PUSH);
    char t[8];
    alarm_text(t, sizeof(t));
    lv_obj_t *big = nk_ui_label(time_btn, t, NK_FB(40), st->alarm_on ? NK_INK : NK_INK_3);
    lv_obj_set_style_text_opa(big, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_t *day = nk_ui_label(time_btn, "あす（日）・休日の時刻", NK_FB(13), NK_INK_3);
    lv_obj_set_style_pad_bottom(day, 6, 0);
    nk_ui_spacer(row);
    lv_obj_t *sw = nk_ui_switch(row, st->alarm_on && !disabled, alarm_switch_cb, NULL);
    if (disabled) {
        lv_obj_add_state(sw, LV_STATE_DISABLED);
    }
    if (st->automode && st->alarm_on && !disabled) {
        char text[192];
        (void)snprintf(text, sizeof(text),
                       "自動モードで %s にセット済み・%d分前にカーテンを開けます", t, st->lead_min);
        nk_ui_text(card, text, NK_FR(12), NK_INK_3);
    }
}

void nk_scr_night(lv_obj_t *body, nk_ui_state state) {
    lv_obj_t *title = nk_ui_title(body, "睡眠モード");
    lv_obj_t *date = nk_ui_label(title, "10月31日（土）\n22:41", NK_FR(12), NK_INK_2);
    lv_obj_set_style_text_align(date, LV_TEXT_ALIGN_RIGHT, 0);

    bool no_exact = state == NK_UI_ST_PERM_EXACT_ALARM;
    if (no_exact) {
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_ALARM, false,
                     "正確な時刻に鳴らすための許可がオフです", "アラームと自動モードが使えません",
                     "設定を開く", settings_cb, NULL);
    }
    night_checks(body, state);
    sound_card(body);
    alarm_card(body, no_exact);
    nk_ui_btn_primary(body, "計測をはじめる", NK_ICON_MOON, start_tracking_cb, NULL);
}

/* ---------------------------------------------------------------- NT-02 */

static int s_track_seconds;

static void night_row(lv_obj_t *body, const char *left, const char *right) {
    lv_obj_t *row = nk_ui_row(body, 8);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, NK_NIGHT_INK, 0);
    lv_obj_set_style_border_opa(row, 56, 0);
    lv_obj_set_style_radius(row, 16, 0);
    lv_obj_set_style_pad_hor(row, 14, 0);
    lv_obj_set_style_pad_ver(row, 12, 0);
    nk_ui_label(row, left, NK_FR(14), NK_NIGHT_INK);
    nk_ui_label(row, right, NK_FB(14), NK_NIGHT_INK);
}

static void woke_cb(lv_event_t *e) {
    (void)e;
    nk_ui_replace(NK_UI_NT04, NK_UI_ST_SAVING);
}

static void tracking_tick_cb(lv_timer_t *timer) {
    lv_obj_t *clock = lv_timer_get_user_data(timer);
    lv_obj_t *sub = lv_obj_get_user_data(clock);
    s_track_seconds++;
    int now = 1 * 60 + 24 + s_track_seconds / 60;
    int elapsed = 2 * 60 + 43 + s_track_seconds / 60;
    char buf[8];
    char dur[192];
    char text[192];
    nk_fmt_clock(buf, sizeof(buf), now);
    lv_label_set_text(clock, buf);
    nk_fmt_duration(dur, sizeof(dur), elapsed);
    (void)snprintf(text, sizeof(text), "計測中・%s", dur);
    lv_label_set_text(sub, text);
}

static void resume_cb(lv_event_t *e) {
    (void)e;
    nk_ui_replace(NK_UI_NT02, NK_UI_ST_TRACKING);
}

void nk_scr_tracking(lv_obj_t *body, nk_ui_state state) {
    nk_demo_state *st = nk_demo_state_get();
    s_track_seconds = 0;
    bool stopped = state == NK_UI_ST_TRACKING_INTERRUPTED;
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_ui_gap(body, stopped ? 24 : 64);
    lv_obj_t *clock = nk_ui_label(body, stopped ? "2:15" : "1:24", NK_FR(84), NK_NIGHT_INK);
    lv_obj_t *sub = nk_ui_label(body, stopped ? "計測が止まっています" : "計測中・2時間43分",
                                NK_FR(14), stopped ? NK_WARN : NK_NIGHT_SUB);
    lv_obj_set_user_data(clock, sub);
    nk_ui_gap(body, stopped ? 24 : 56);

    if (stopped) {
        lv_obj_t *row = nk_ui_col(body, 8);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, NK_WARN, 0);
        lv_obj_set_style_border_opa(row, 90, 0);
        lv_obj_set_style_radius(row, 16, 0);
        lv_obj_set_style_pad_all(row, 14, 0);
        nk_ui_text(row, "2:15 に計測が止まりました（アプリが停止しました）", NK_FR(14),
                   NK_NIGHT_INK);
        nk_ui_text(row, "再開すると、止まっていた時間は「計測できなかった時間」として記録します",
                   NK_FR(12), NK_NIGHT_SUB);
        lv_obj_t *btn = nk_ui_btn_ghost(row, "計測を再開する", resume_cb, NULL);
        lv_obj_set_style_border_color(btn, NK_NIGHT_INK, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), NK_NIGHT_INK, 0);
    }

    char sound[192];
    (void)snprintf(sound, sizeof(sound), "%s・残り 0 分",
                   st->sound >= 0 ? k_sounds[st->sound] : "サウンド");
    night_row(body, sound, "停止しました");
    char alarm[192];
    char t[8];
    alarm_text(t, sizeof(t));
    (void)snprintf(alarm, sizeof(alarm), "%s に起こします", t);
    night_row(body, "アラーム", st->alarm_on ? alarm : "オフ");
    night_row(body, "カーテン", st->automode ? "閉めました（23:00 確認）" : "手動");
    nk_ui_gap(body, 16);
    nk_ui_hold_button(body, "起きた（長押し）", NK_NIGHT_INK, woke_cb);
    lv_obj_t *note =
        nk_ui_label(body, "画面は 30 秒で消えます。計測は続きます", NK_FR(12), NK_NIGHT_SUB);
    lv_obj_set_style_margin_top(note, 6, 0);

    if (!nk_ui_instant() && !stopped) {
        (void)nk_ui_timer(tracking_tick_cb, 1000, clock);
    }
}

/* ---------------------------------------------------------------- NT-03 */

static void stop_alarm_cb(lv_event_t *e) {
    (void)e;
    nk_ui_replace(NK_UI_NT04, NK_UI_ST_SAVING);
}

static void snooze_back_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    nk_ui_replace(NK_UI_NT02, NK_UI_ST_TRACKING);
}

static void snooze_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_current_target_obj(e);
    char t[8];
    char text[192];
    nk_fmt_clock(t, sizeof(t), nk_demo_next_alarm() + nk_demo_state_get()->snooze_len);
    (void)snprintf(text, sizeof(text), "%s にもう一度鳴ります", t);
    nk_ui_btn_set_text(btn, text);
    lv_obj_set_clickable(btn, false);
    (void)nk_ui_timer(snooze_back_cb, 1500, NULL);
}

static void curtain_done_cb(lv_timer_t *timer) {
    nk_ui_timer_stop(timer);
    nk_ui_replace(NK_UI_NT03, NK_UI_ST_CURTAIN_OK);
}

void nk_scr_ringing(lv_obj_t *body, nk_ui_state state) {
    nk_demo_state *st = nk_demo_state_get();
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_ui_gap(body, 40);
    char t[8];
    alarm_text(t, sizeof(t));
    nk_ui_label(body, t, NK_FR(96), NK_INK);
    nk_ui_label(body, "11月1日（日）", NK_FR(14), NK_INK_2);
    nk_ui_gap(body, 12);

    lv_obj_t *res = NULL;
    switch (state) {
    case NK_UI_ST_CURTAIN_OK:
        res = nk_ui_op_result(body, NK_OP_OK, "開けました（6:20 確認）",
                              "自動モード・アラームの 10 分前", NULL, NULL, NULL);
        break;
    case NK_UI_ST_CURTAIN_FAILED:
        res = nk_ui_op_result(body, NK_OP_FAILED, "開けられませんでした：カーテンが応答しません",
                              "ハブがオフラインの可能性があります。アラームはこのまま鳴ります",
                              NULL, NULL, NULL);
        break;
    case NK_UI_ST_CURTAIN_UNCONFIRMED:
        res = nk_ui_op_result(body, NK_OP_UNCONFIRMED,
                              "開ける命令は届きましたが、動いたか確認できませんでした",
                              "あとで寝室の画面から状態を確かめられます", NULL, NULL, NULL);
        break;
    case NK_UI_ST_CURTAIN_PENDING:
    default:
        res = nk_ui_op_result(body, NK_OP_PENDING, "カーテンを開けています",
                              "自動モード・静音で開けています", NULL, NULL, NULL);
        if (!nk_ui_instant()) {
            (void)nk_ui_timer(curtain_done_cb, 3000, NULL);
        }
        break;
    }
    lv_obj_set_style_bg_color(res, NK_WHITE, 0);
    lv_obj_set_style_bg_opa(res, 13, 0);

    char snooze[192];
    (void)snprintf(snooze, sizeof(snooze), "%d分後にもう一度（あと %d 回）", st->snooze_len,
                   st->snooze_max);
    lv_obj_t *sz = lv_button_create(body);
    lv_obj_remove_style_all(sz);
    lv_obj_set_size(sz, 300, 56);
    lv_obj_set_style_margin_top(sz, 14, 0);
    lv_obj_set_style_radius(sz, 28, 0);
    lv_obj_set_style_bg_color(sz, NK_WHITE, 0);
    lv_obj_set_style_bg_opa(sz, 20, 0);
    lv_obj_set_style_bg_opa(sz, 40, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(sz, 1, 0);
    lv_obj_set_style_border_color(sz, NK_WHITE, 0);
    lv_obj_set_style_border_opa(sz, NK_OPA_LINE_2, 0);
    lv_obj_t *szl = nk_ui_label(sz, snooze, NK_FB(16), NK_INK);
    lv_obj_center(szl);
    lv_obj_add_event_cb(sz, snooze_cb, LV_EVENT_CLICKED, NULL);

    nk_ui_spacer(body);
    lv_obj_set_width(lv_obj_get_child(body, -1), 1);

    /* 画面の下半分を占める大きな「止める」（UX-6 の 3 章） */
    lv_obj_t *stop = lv_button_create(body);
    lv_obj_remove_style_all(stop);
    lv_obj_set_size(stop, LV_PCT(100), 220);
    lv_obj_set_style_radius(stop, 32, 0);
    lv_obj_set_style_bg_color(stop, NK_BRAND, 0);
    lv_obj_set_style_bg_opa(stop, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(stop, lv_color_hex(0xE36F91), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(stop, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(stop, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(stop, 6, 0);
    nk_ui_label(stop, "止める", NK_FB(28), NK_BRAND_INK);
    lv_obj_t *small = nk_ui_label(stop, "計測を終えて、記録を分析します", NK_FB(13), NK_BRAND_INK);
    lv_obj_set_style_text_opa(small, LV_OPA_80, 0);
    lv_obj_add_event_cb(stop, stop_alarm_cb, LV_EVENT_CLICKED, NULL);
}

/* ---------------------------------------------------------------- NT-04 */

static void morning_next_cb(lv_timer_t *timer) {
    nk_ui_state next = (nk_ui_state)(intptr_t)lv_timer_get_user_data(timer);
    nk_ui_timer_stop(timer);
    nk_ui_replace(NK_UI_NT04, next);
}

static void mood_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_current_target_obj(e);
    lv_obj_t *row = lv_obj_get_parent(btn);
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
        lv_obj_remove_state(lv_obj_get_child(row, (int32_t)i), LV_STATE_CHECKED);
    }
    lv_obj_add_state(btn, LV_STATE_CHECKED);
}

static void home_cb(lv_event_t *e) {
    (void)e;
    nk_ui_go_tab(NK_UI_HM01);
}

static void step_row(lv_obj_t *card, int phase, int done_at, const char *doing, const char *done) {
    lv_obj_t *row = nk_ui_row(card, 10);
    if (phase > done_at) {
        nk_ui_icon(row, NK_ICON_CHECK_CIRCLE, 20, NK_GOOD);
        nk_ui_label(row, done, NK_FR(14), NK_INK);
    } else if (phase == done_at) {
        nk_ui_spinner(row, 20, NK_SPIN);
        nk_ui_label(row, doing, NK_FR(14), NK_INK);
    } else {
        lv_obj_t *dot = nk_ui_box(row);
        lv_obj_set_size(dot, 20, 20);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 2, 0);
        lv_obj_set_style_border_color(dot, NK_INK_3, 0);
        lv_obj_set_style_border_opa(dot, LV_OPA_50, 0);
        nk_ui_label(row, doing, NK_FR(14), NK_INK_3);
    }
}

void nk_scr_morning(lv_obj_t *body, nk_ui_state state) {
    int phase = state == NK_UI_ST_SAVING ? 0 : (state == NK_UI_ST_ANALYZING ? 1 : 2);
    nk_ui_gap(body, 24);
    nk_ui_icon(body, NK_ICON_SUN, 56, NK_WARN);
    nk_ui_label(body, "おはようございます", NK_FB(26), NK_INK);
    nk_ui_text(body, "計測を終えました。記録を保存して、昨夜の眠りを分析します。", NK_FR(14),
               NK_INK_2);

    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 12, 0);
    step_row(card, phase, 0, "記録を保存しています", "記録を保存しました");
    step_row(card, phase, 1, "昨夜の記録を分析しています", "分析が終わりました");

    if (phase < 2) {
        if (!nk_ui_instant()) {
            (void)nk_ui_timer(morning_next_cb, phase == 0 ? 1500 : 2000,
                              (void *)(intptr_t)(phase == 0 ? NK_UI_ST_ANALYZING : NK_UI_ST_NONE));
        }
        return;
    }

    /* 任意の 1 タップ（UX-3 の 3 章）。飛ばせる */
    lv_obj_t *mood = nk_ui_card(body);
    nk_ui_card_head(mood, "今朝の目覚めは？");
    lv_obj_t *row = nk_ui_row(mood, 8);
    static const char *const moods[] = {"すっきり", "ふつう", "ねむい"};
    static const nk_icon icons[] = {NK_ICON_SUN, NK_ICON_CLOCK, NK_ICON_MOON};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_button_create(row);
        lv_obj_remove_style_all(b);
        lv_obj_set_height(b, 76);
        lv_obj_set_width(b, 0);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_radius(b, NK_RADIUS_S, 0);
        lv_obj_set_style_bg_color(b, NK_CARD_2, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, NK_CARD_2, 0);
        lv_obj_set_style_border_color(b, NK_BRAND, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(b, lv_color_mix(NK_BRAND, NK_CARD_2, 41), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(b, NK_INK_2, 0);
        lv_obj_set_style_text_color(b, NK_INK, LV_STATE_CHECKED);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(b, 6, 0);
        lv_obj_t *ic = nk_ui_icon(b, icons[i], 26, NK_INK_2);
        lv_obj_remove_local_style_prop(ic, LV_STYLE_TEXT_COLOR, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, moods[i]);
        lv_obj_set_style_text_font(l, NK_FB(13), 0);
        lv_obj_add_event_cb(b, mood_cb, LV_EVENT_CLICKED, NULL);
    }
    nk_ui_text(mood, "答えなくてもかまいません。設定で、この質問を出さないようにできます",
               NK_FR(12), NK_INK_3);
    nk_ui_spacer(body);
    nk_ui_btn_primary(body, "ホームへ", NK_ICON_HOME, home_cb, NULL);
}

/* ---------------------------------------------------------------- NT-05 */

static void hour_cb(lv_event_t *e) {
    nk_demo_state *st = nk_demo_state_get();
    uint32_t h = lv_roller_get_selected(lv_event_get_current_target_obj(e));
    st->alarm_min = (int)h * 60 + st->alarm_min % 60;
}

static void minute_cb(lv_event_t *e) {
    nk_demo_state *st = nk_demo_state_get();
    uint32_t m = lv_roller_get_selected(lv_event_get_current_target_obj(e));
    st->alarm_min = (st->alarm_min / 60) * 60 + (int)m * 5;
}

static void snooze_len_cb(lv_event_t *e) {
    nk_demo_state_get()->snooze_len = (int)(intptr_t)lv_event_get_user_data(e);
    nk_ui_rebuild();
}

static void snooze_max_cb(lv_event_t *e) {
    nk_demo_state_get()->snooze_max = (int)(intptr_t)lv_event_get_user_data(e);
    nk_ui_rebuild();
}

static void vibration_cb(lv_event_t *e) {
    nk_demo_state_get()->vibration =
        lv_obj_has_state(lv_event_get_current_target_obj(e), LV_STATE_CHECKED);
}

static void save_alarm_cb(lv_event_t *e) {
    (void)e;
    char t[8];
    char text[192];
    nk_fmt_clock(t, sizeof(t), nk_demo_state_get()->alarm_min);
    (void)snprintf(text, sizeof(text), "平日のアラームを %s にセットしました", t);
    nk_ui_toast(text);
    (void)nk_ui_back();
}

static void preview_cb(lv_event_t *e) {
    (void)e;
    nk_ui_toast("試聴しています（PC 版では音は出ません）");
}

void nk_scr_alarm_settings(lv_obj_t *body, nk_ui_state state) {
    nk_demo_state *st = nk_demo_state_get();
    nk_ui_subhead(body, "アラーム");
    if (state == NK_UI_ST_PERM_EXACT_ALARM) {
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_ALARM, false,
                     "正確な時刻に鳴らすための許可がオフです", "アラームと自動モードが使えません",
                     "設定を開く", settings_cb, NULL);
    } else if (state == NK_UI_ST_PERM_FSI) {
        nk_ui_banner(body, NK_TONE_WARN, NK_ICON_INFO, false,
                     "ロック中は画面が出ず、通知で知らせます（音は鳴ります）",
                     "全画面で知らせる許可がオフです", "設定を開く", settings_cb, NULL);
    } else if (state == NK_UI_ST_PERM_NOTIF) {
        nk_ui_banner(body, NK_TONE_WARN, NK_ICON_BELL, false,
                     "通知がオフのため、計測中の表示とアラームの通知が出ません", NULL, "通知を許可",
                     settings_cb, NULL);
    }

    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(card, "起きる時刻");
    nk_ui_hint(head, "平日");
    lv_obj_t *rollers = nk_ui_row(card, 10);
    lv_obj_set_flex_align(rollers, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    static const char hours[] =
        "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";
    static const char minutes[] = "00\n05\n10\n15\n20\n25\n30\n35\n40\n45\n50\n55";
    lv_obj_t *hr = nk_ui_roller(rollers, hours, (uint32_t)(st->alarm_min / 60), 96);
    lv_obj_add_event_cb(hr, hour_cb, LV_EVENT_VALUE_CHANGED, NULL);
    nk_ui_label(rollers, ":", NK_FB(28), NK_INK_2);
    lv_obj_t *mr = nk_ui_roller(rollers, minutes, (uint32_t)((st->alarm_min % 60) / 5), 96);
    lv_obj_add_event_cb(mr, minute_cb, LV_EVENT_VALUE_CHANGED, NULL);
    char holiday[8];
    nk_fmt_clock(holiday, sizeof(holiday), st->alarm_holiday_min);
    nk_ui_setrow(card, "休日の起きる時刻", "土曜・日曜・祝日の朝", holiday);
    char goal[64];
    nk_fmt_duration(goal, sizeof(goal), st->goal_min);
    nk_ui_setrow(card, "目標の睡眠時間", "今夜の目標の就寝時刻は、ここから決めます", goal);

    lv_obj_t *sound = nk_ui_card(body);
    nk_ui_card_head(sound, "音");
    static const char *const tones[] = {"やわらかいチャイム", "鳥のさえずり", "電子音"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *row = nk_ui_setrow(sound, tones[i], NULL, NULL);
        nk_ui_link(row, "試聴", preview_cb, NULL);
        lv_obj_t *radio = nk_ui_box(row);
        lv_obj_set_size(radio, 22, 22);
        lv_obj_set_style_radius(radio, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(radio, i == 0 ? 6 : 2, 0);
        lv_obj_set_style_border_color(radio, i == 0 ? NK_BRAND : NK_INK_3, 0);
    }

    lv_obj_t *snooze = nk_ui_card(body);
    nk_ui_card_head(snooze, "スヌーズ");
    nk_ui_label(snooze, "長さ", NK_FR(13), NK_INK_2);
    lv_obj_t *lens = nk_ui_row(snooze, 8);
    static const int len_values[] = {5, 10, 15};
    for (int i = 0; i < 3; i++) {
        char t[8];
        (void)snprintf(t, sizeof(t), "%d分", len_values[i]);
        nk_ui_chip(lens, t, st->snooze_len == len_values[i], snooze_len_cb,
                   (void *)(intptr_t)len_values[i]);
    }
    nk_ui_label(snooze, "回数", NK_FR(13), NK_INK_2);
    lv_obj_t *maxes = nk_ui_row(snooze, 8);
    for (int i = 0; i <= 5; i++) {
        char t[8];
        (void)snprintf(t, sizeof(t), "%d回", i);
        nk_ui_chip(maxes, t, st->snooze_max == i, snooze_max_cb, (void *)(intptr_t)i);
    }
    lv_obj_t *vib = nk_ui_setrow(snooze, "バイブレーション", NULL, NULL);
    nk_ui_switch(vib, st->vibration, vibration_cb, NULL);

    nk_ui_btn_primary(body, "保存する", NK_ICON_NONE, save_alarm_cb, NULL);
}
