/*
 * ON-01〜ON-09 初回利用フロー（UX-1）
 *
 * 必須は 4 画面（ようこそ、同意、マイクと通知、置き方）。そのほかは「あとで」で飛ばせる。
 * PC 版では許可はすべて「許可」として進む（ADR-0004）。
 */
#include "ui_internal.h"

#include <stdint.h>
#include <stdio.h>

static void go_cb(lv_event_t *e) {
    nk_ui_replace((nk_ui_screen_id)(intptr_t)lv_event_get_user_data(e), NK_UI_ST_NONE);
}

static lv_obj_t *next_button(lv_obj_t *body, const char *text, nk_ui_screen_id next) {
    return nk_ui_btn_primary(body, text, NK_ICON_NONE, go_cb, (void *)(intptr_t)next);
}

static lv_obj_t *later_button(lv_obj_t *body, const char *text, nk_ui_screen_id next) {
    return nk_ui_btn_ghost(body, text, go_cb, (void *)(intptr_t)next);
}

/* 上の進み具合。必須の画面（1〜4）だけを数える */
static void progress(lv_obj_t *body, int step) {
    char label[192];
    if (step <= 4) {
        (void)snprintf(label, sizeof(label), "はじめの設定 %d / 4", step);
    } else {
        (void)snprintf(label, sizeof(label), "あとからでも設定できます");
    }
    nk_ui_label(body, label, NK_FB(12), NK_INK_3);
    lv_obj_t *bar = lv_bar_create(body);
    lv_obj_set_size(bar, LV_PCT(100), 6);
    lv_bar_set_range(bar, 0, 4);
    lv_bar_set_value(bar, step <= 4 ? step : 4, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, step <= 4 ? NK_BRAND : NK_SERIES, LV_PART_INDICATOR);
    lv_obj_set_style_margin_top(bar, -4, 0);
}

static void heading(lv_obj_t *body, const char *title, const char *text) {
    lv_obj_t *col = nk_ui_col(body, 6);
    lv_obj_set_style_pad_top(col, 4, 0);
    nk_ui_label(col, title, NK_FB(24), NK_INK);
    if (text != NULL) {
        nk_ui_text(col, text, NK_FR(14), NK_INK_2);
    }
}

/* ---------------------------------------------------------------- ON-01 */

static void welcome(lv_obj_t *body) {
    nk_ui_gap(body, 28);
    lv_obj_t *hero = nk_ui_col(body, 8);
    lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_ui_icon(hero, NK_ICON_LOGO, 88, NK_BRAND);
    nk_ui_label(hero, "ねむのき", NK_FB(30), NK_INK);
    nk_ui_label(hero, "眠りを知って、寝室を整える", NK_FR(15), NK_INK_2);
    nk_ui_gap(body, 8);

    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 0, 0);
    nk_ui_list_item(card, NK_ICON_CHART, "眠りを記録して振り返る",
                    "スマホを枕元に置くだけ。朝にスコアと眠りの帯が届きます");
    nk_ui_list_item(card, NK_ICON_CURTAIN, "寝室を整える",
                    "スマートカーテンを、アラームの前に静かに開けます");
    nk_ui_list_item(card, NK_ICON_MOON, "寝つきと目覚めを助ける",
                    "入眠サウンドとアラームを、ひとつの画面で");
    nk_ui_spacer(body);
    next_button(body, "はじめる", NK_UI_ON02);
}

/* ---------------------------------------------------------------- ON-02 */

static void policy_cb(lv_event_t *e) {
    (void)e;
    nk_ui_toast("PC 版ではブラウザを開きません");
}

static void decline_confirm_cb(lv_event_t *e) {
    (void)e;
    nk_demo_state_get()->consented = false;
    nk_ui_sheet_close();
    nk_ui_replace(NK_UI_ON09, NK_UI_ST_NONE);
}

static void decline_cb(lv_event_t *e) {
    (void)e;
    /* 責めずに、できなくなることと戻し方を伝える（UX-1 改訂版） */
    lv_obj_t *sheet = nk_ui_sheet_open(
        "計測なしで使いますか", "睡眠データを取るには同意が必要です。あとから設定で同意できます。");
    nk_ui_notice_row(sheet, NK_ICON_INFO, NK_SPIN, "記録とスコアは見られません");
    nk_ui_notice_row(sheet, NK_ICON_CHECK, NK_GOOD, "入眠サウンドとアラームは使えます");
    lv_obj_t *pair = nk_ui_btn_pair(sheet);
    lv_obj_t *back = nk_ui_btn_ghost(pair, "戻る", nk_ui_sheet_close_cb, NULL);
    lv_obj_set_flex_grow(back, 1);
    lv_obj_t *ok = nk_ui_btn_ghost(pair, "計測なしで使う", decline_confirm_cb, NULL);
    lv_obj_set_flex_grow(ok, 1);
}

static void consent(lv_obj_t *body) {
    progress(body, 2);
    heading(body, "データの使い方",
            "記録を始める前に、何を取得して、どう扱うかを確認してください。");
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 0, 0);
    nk_ui_list_item(card, NK_ICON_PHONE, "取得するもの",
                    "体の動き（加速度センサー）、寝室の音の大きさといびき（マイク）、部屋の明るさ"
                    "（照度センサー。ある端末のみ）");
    nk_ui_list_item(card, NK_ICON_SPARK, "使い道",
                    "眠りの記録、スコア、アドバイス、カーテンの設定の提案");
    nk_ui_list_item(card, NK_ICON_MIC, "録音",
                    "保存しません。音は端末の中ですぐ数値に変え、捨てます。残すのは、設定でオンに"
                    "したときだけ");
    nk_ui_list_item(card, NK_ICON_LOCK, "保存場所",
                    "この端末の中だけ。暗号化して保存し、サーバーには送りません");
    nk_ui_list_item(card, NK_ICON_CLOCK, "保存期間",
                    "毎日の結果は削除するまで。細かな計測値は 90 日");
    nk_ui_list_item(card, NK_ICON_TRASH, "削除・取り消し",
                    "設定の「データとプライバシー」から、いつでも削除・同意の取り消しができます");
    nk_ui_list_item(card, NK_ICON_INFO, "医療との関係", "医療機器ではなく、診断はしません");
    lv_obj_t *row = nk_ui_row(body, 4);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    nk_ui_label(row, "同意文 1.0", NK_FR(11), NK_INK_3);
    nk_ui_link(row, "プライバシーポリシーを読む", policy_cb, NULL);
    next_button(body, "同意して計測を使う", NK_UI_ON03);
    nk_ui_btn_ghost(body, "同意しないで続ける", decline_cb, NULL);
}

/* ---------------------------------------------------------------- ON-03 */

static void permissions(lv_obj_t *body, nk_ui_state state) {
    progress(body, 3);
    heading(body, "マイクと通知の許可",
            "このあと端末の確認画面が出ます。何のために使うかを、先に説明します。");
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 0, 0);
    nk_ui_list_item(card, NK_ICON_MIC, "マイク",
                    "いびきと寝室の音の大きさを記録します。録音は保存しません");
    nk_ui_list_item(card, NK_ICON_BELL, "通知",
                    "計測中であることの表示と、アラームを鳴らすために使います");
    nk_ui_notice_row(body, NK_ICON_INFO, NK_SPIN,
                     "計測中は、Android の「マイク使用中」の表示が出ます");
    if (state == NK_UI_ST_PERM_MIC) {
        nk_ui_banner(body, NK_TONE_WARN, NK_ICON_INFO, false,
                     "マイクがオフのため、いびきは記録されません",
                     "体の動きの記録とスコアは使えます。あとで設定の「データとプライバシー」から"
                     "許可できます",
                     NULL, NULL, NULL);
        next_button(body, "このまま進む", NK_UI_ON04);
        return;
    }
    nk_ui_spacer(body);
    next_button(body, "許可する", NK_UI_ON04);
    later_button(body, "あとで", NK_UI_ON04);
}

/* ---------------------------------------------------------------- ON-04 */

static void trial_tick_cb(lv_timer_t *timer) {
    lv_obj_t *bar = lv_timer_get_user_data(timer);
    int32_t v = lv_bar_get_value(bar) + 1;
    lv_bar_set_value(bar, v, LV_ANIM_ON);
    lv_obj_t *label = lv_obj_get_user_data(bar);
    char text[192];
    (void)snprintf(text, sizeof(text), "計測しています…あと %d 秒", (int)(10 - v));
    lv_label_set_text(label, text);
    if (v >= 10) {
        nk_ui_timer_stop(timer);
        lv_obj_t *slot = lv_obj_get_parent(bar);
        lv_obj_clean(slot);
        nk_ui_op_result(slot, NK_OP_OK, "置き方は OK です", "体の動きと音を計測できました", NULL,
                        NULL, NULL);
    }
}

static void trial_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_current_target_obj(e);
    lv_obj_t *slot = lv_event_get_user_data(e);
    lv_obj_set_hidden(btn, true);
    lv_obj_t *label = nk_ui_label(slot, "計測しています…あと 10 秒", NK_FR(13), NK_INK_2);
    lv_obj_t *bar = lv_bar_create(slot);
    lv_obj_set_size(bar, LV_PCT(100), 6);
    lv_bar_set_range(bar, 0, 10);
    lv_obj_set_style_bg_color(bar, NK_CARD_2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, NK_SERIES, LV_PART_INDICATOR);
    lv_obj_set_user_data(bar, label);
    (void)nk_ui_timer(trial_tick_cb, 1000, bar);
}

static void choice_cb(lv_event_t *e) {
    lv_obj_t *chip = lv_event_get_current_target_obj(e);
    lv_obj_t *row = lv_obj_get_parent(chip);
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
        lv_obj_remove_state(lv_obj_get_child(row, (int32_t)i), LV_STATE_CHECKED);
    }
    lv_obj_add_state(chip, LV_STATE_CHECKED);
}

static void placement(lv_obj_t *body) {
    progress(body, 4);
    heading(body, "置き方と試し計測", "毎晩同じ置き方にすると、記録が安定します。");
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *row = nk_ui_row(card, 8);
    static const nk_icon icons[] = {NK_ICON_PHONE, NK_ICON_SUN, NK_ICON_PLUG};
    static const char *const labels[] = {"枕元の\nマットレスの上", "画面を上に", "充電\nしながら"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *tile = nk_ui_tile(row);
        lv_obj_set_width(tile, 0);
        lv_obj_set_flex_grow(tile, 1);
        lv_obj_set_height(tile, 96);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(tile, 6, 0);
        nk_ui_icon(tile, icons[i], 28, NK_SERIES_LT);
        lv_obj_t *l = nk_ui_label(tile, labels[i], NK_FB(12), NK_INK);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_t *slot = nk_ui_col(card, 6);
    lv_obj_t *trial = nk_ui_btn_ghost(card, "試してみる（10 秒）", trial_cb, slot);
    lv_obj_move_to_index(trial, (int32_t)lv_obj_get_index(slot));

    nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN,
                     "同じベッドで寝る人がいると、記録の精度が下がることがあります");

    lv_obj_t *q = nk_ui_card(body);
    nk_ui_card_head(q, "一人で寝ていますか（任意）");
    nk_ui_text(q, "同じ寝具に人やペットがいると、体の動きの読み取りに影響します", NK_FR(12),
               NK_INK_3);
    lv_obj_t *chips = nk_ui_row(q, 8);
    nk_ui_chip(chips, "はい", false, choice_cb, NULL);
    nk_ui_chip(chips, "いいえ", false, choice_cb, NULL);
    nk_ui_chip(chips, "答えない", true, choice_cb, NULL);
    next_button(body, "次へ", NK_UI_ON05);
    later_button(body, "試し計測はあとで", NK_UI_ON05);
}

/* ---------------------------------------------------------------- ON-05 */

static lv_obj_t *s_suggest;

static void update_suggest(void) {
    nk_demo_state *st = nk_demo_state_get();
    char t[8];
    char text[192];
    nk_fmt_clock(t, sizeof(t), st->alarm_min - st->goal_min - 30);
    (void)snprintf(text, sizeof(text), "おすすめの寝る時刻：%s", t);
    if (s_suggest != NULL) {
        lv_label_set_text(s_suggest, text);
    }
}

static void goal_cb(lv_event_t *e) {
    uint32_t sel = lv_roller_get_selected(lv_event_get_current_target_obj(e));
    nk_demo_state_get()->goal_min = 5 * 60 + (int)sel * 15;
    update_suggest();
}

static void wake_hour_cb(lv_event_t *e) {
    nk_demo_state *st = nk_demo_state_get();
    uint32_t h = lv_roller_get_selected(lv_event_get_current_target_obj(e));
    st->alarm_min = (int)(h + 4) * 60 + st->alarm_min % 60;
    update_suggest();
}

static void wake_min_cb(lv_event_t *e) {
    nk_demo_state *st = nk_demo_state_get();
    uint32_t m = lv_roller_get_selected(lv_event_get_current_target_obj(e));
    st->alarm_min = (st->alarm_min / 60) * 60 + (int)m * 5;
    update_suggest();
}

static void goals(lv_obj_t *body) {
    nk_demo_state *st = nk_demo_state_get();
    progress(body, 5);
    heading(body, "目標と起きる時刻", "あとから設定で変えられます。");

    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_ui_card_head(card, "目標の睡眠時間");
    static const char goal_opts[] =
        "5時間\n5時間15分\n5時間30分\n5時間45分\n6時間\n6時間15分\n6時間30分\n6時間45分\n7時間\n"
        "7時間15分\n7時間30分\n7時間45分\n8時間\n8時間15分\n8時間30分\n8時間45分\n9時間";
    lv_obj_t *goal = nk_ui_roller(card, goal_opts, (uint32_t)((st->goal_min - 300) / 15), 200);
    lv_obj_add_event_cb(goal, goal_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *wake = nk_ui_card(body);
    lv_obj_t *head = nk_ui_card_head(wake, "起きる時刻");
    nk_ui_hint(head, "平日");
    lv_obj_t *rollers = nk_ui_row(wake, 10);
    lv_obj_set_flex_align(rollers, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    static const char hours[] = "4\n5\n6\n7\n8\n9\n10\n11";
    static const char minutes[] = "00\n05\n10\n15\n20\n25\n30\n35\n40\n45\n50\n55";
    lv_obj_t *hr = nk_ui_roller(rollers, hours, (uint32_t)(st->alarm_min / 60 - 4), 96);
    lv_obj_add_event_cb(hr, wake_hour_cb, LV_EVENT_VALUE_CHANGED, NULL);
    nk_ui_label(rollers, ":", NK_FB(28), NK_INK_2);
    lv_obj_t *mr = nk_ui_roller(rollers, minutes, (uint32_t)((st->alarm_min % 60) / 5), 96);
    lv_obj_add_event_cb(mr, wake_min_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *holiday = nk_ui_setrow(wake, "休日も同じ時刻", NULL, NULL);
    nk_ui_switch(holiday, true, NULL, NULL);

    lv_obj_t *tile = nk_ui_tile(body);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tile, 8, 0);
    nk_ui_icon(tile, NK_ICON_MOON, 18, NK_BRAND);
    s_suggest = nk_ui_label(tile, "", NK_FB(14), NK_INK);
    update_suggest();
    next_button(body, "次へ", NK_UI_ON06);
}

/* ---------------------------------------------------------------- ON-06 */

static void settings_cb(lv_event_t *e) {
    (void)e;
    nk_ui_toast("PC 版では端末の設定画面は開きません");
}

static void alarm_permissions(lv_obj_t *body, nk_ui_state state) {
    progress(body, 6);
    heading(body, "アラームの許可",
            "正確な時刻に鳴らす許可と、ロック中に画面を出す許可を使います。");
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_ver(card, 4, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    bool off = state == NK_UI_ST_PERM_EXACT_ALARM;
    lv_obj_t *r1 = nk_ui_setrow(card, "正確な時刻に鳴らす", "アラームと自動モードに必要です",
                                off ? NULL : "許可済み");
    if (off) {
        nk_ui_link(r1, "設定を開く", settings_cb, NULL);
    }
    nk_ui_setrow(card, "ロック中に画面を出す", "許可がなくても、通知と音で知らせます", "許可済み");
    if (off) {
        nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_ALARM, false,
                     "正確な時刻に鳴らすための許可がオフです",
                     "設定を開いて許可し、この画面に戻ると反映されます", NULL, NULL, NULL);
    }
    nk_ui_spacer(body);
    next_button(body, "次へ", NK_UI_ON07);
    later_button(body, "アラームを使わない", NK_UI_ON07);
}

/* ---------------------------------------------------------------- ON-07〜09 */

static void curtain(lv_obj_t *body, nk_ui_state state) {
    progress(body, 7);
    heading(body, "カーテンをつなぐ", "SwitchBot のカーテン（Curtain 3）とハブを使います。");
    nk_scr_connect_body(body, state, true);
}

static void enable_cb(lv_event_t *e) {
    (void)e;
    /* 「有効にする」は確認をはさむ（BR-03 と同じ画面） */
    nk_ui_push(NK_UI_BR03, NK_UI_ST_NONE);
}

static void automode(lv_obj_t *body) {
    nk_demo_state *st = nk_demo_state_get();
    progress(body, 8);
    heading(body, "自動モード", "毎朝のアラームとカーテンを、決めた条件で自動で動かします。");
    char weekday[8];
    char holiday[8];
    char item1[128];
    char item2[128];
    nk_fmt_clock(weekday, sizeof(weekday), st->alarm_min);
    nk_fmt_clock(holiday, sizeof(holiday), st->alarm_holiday_min);
    (void)snprintf(item1, sizeof(item1), "毎朝のアラームをセット（平日 %s、休日 %s）", weekday,
                   holiday);
    (void)snprintf(item2, sizeof(item2), "アラームの %d 分前に、静かにカーテンを開ける",
                   st->lead_min);
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 0, 0);
    nk_ui_list_item(card, NK_ICON_ALARM, item1, NULL);
    nk_ui_list_item(card, NK_ICON_CURTAIN, item2, NULL);
    nk_ui_list_item(card, NK_ICON_MOON, "「おやすみ」で計測を始めると、カーテンを閉める", NULL);
    nk_ui_list_item(card, NK_ICON_SUN, "アラームの前に「起きた」を押すと、その時点で開ける", NULL);
    nk_ui_text(body,
               "アドバイスの内容を、自動で設定に反映することはありません。いつでも止められます。",
               NK_FR(12), NK_INK_3);
    if (st->automode) {
        nk_ui_op_result(body, NK_OP_OK, "自動モードをオンにしました",
                        "寝室の画面から、いつでも止められます", NULL, NULL, NULL);
        nk_ui_spacer(body);
        next_button(body, "次へ", NK_UI_ON09);
        return;
    }
    nk_ui_notice_row(body, NK_ICON_INFO, NK_SPIN,
                     "初めはオフです。オンにする前に、もう一度内容を確かめます");
    nk_ui_spacer(body);
    nk_ui_btn_primary(body, "有効にする", NK_ICON_NONE, enable_cb, NULL);
    later_button(body, "今は使わない", NK_UI_ON09);
}

static void home_cb(lv_event_t *e) {
    (void)e;
    nk_ui_go_tab(NK_UI_HM01);
}

static void summary_row(lv_obj_t *list, const char *name, const char *value, bool set,
                        nk_ui_screen_id fix) {
    lv_obj_t *row = nk_ui_setrow(list, name, NULL, NULL);
    nk_ui_label(row, value, NK_FB(13), set ? NK_INK : NK_INK_3);
    nk_ui_icon(row, NK_ICON_RIGHT, 16, NK_INK_3);
    lv_obj_set_clickable(row, true);
    nk_ui_on_click_nav(row, fix, NK_UI_ST_NONE, NK_UI_NAV_PUSH);
}

static void done(lv_obj_t *body) {
    nk_demo_state *st = nk_demo_state_get();
    nk_ui_gap(body, 12);
    lv_obj_t *hero = nk_ui_col(body, 8);
    lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    nk_ui_icon(hero, NK_ICON_CHECK_CIRCLE, 60, NK_GOOD);
    nk_ui_label(hero, "準備ができました", NK_FB(24), NK_INK);
    nk_ui_label(hero, st->consented ? "今夜から記録できます" : "サウンドとアラームを使えます",
                NK_FR(14), NK_INK_2);

    lv_obj_t *card = nk_ui_card(body);
    nk_ui_card_head(card, "今夜の流れ");
    nk_ui_text(card,
               "寝る前に「睡眠モード」で「おやすみ」を押す → 朝にスコアとアドバイスが見られます",
               NK_FR(13), NK_INK);

    /* 設定のまとめ。「あとで」にしたものは「未設定」と分かるように出す */
    lv_obj_t *list = nk_ui_card(body);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_set_style_pad_ver(list, 6, 0);
    char goal[96];
    char t[8];
    char dur[64];
    nk_fmt_clock(t, sizeof(t), st->alarm_min);
    nk_fmt_duration(dur, sizeof(dur), st->goal_min);
    (void)snprintf(goal, sizeof(goal), "%s・平日 %s", dur, t);
    summary_row(list, "同意", st->consented ? "同意済み" : "同意しない", st->consented, NK_UI_ST02);
    summary_row(list, "マイク・通知", st->consented ? "許可済み" : "未設定", st->consented,
                NK_UI_ST02);
    summary_row(list, "目標と起きる時刻", goal, true, NK_UI_NT05);
    summary_row(list, "アラーム", st->alarm_on ? "設定済み" : "あとで", st->alarm_on, NK_UI_NT05);
    summary_row(list, "カーテン", st->curtain_connected ? "接続済み" : "未設定",
                st->curtain_connected, NK_UI_BR02);
    summary_row(list, "自動モード", st->automode ? "有効" : "無効", st->automode, NK_UI_BR01);
    nk_ui_text(body, "「あとで」にしたものは、設定からいつでも戻れます", NK_FR(12), NK_INK_3);
    nk_ui_btn_primary(body, "ホームへ", NK_ICON_HOME, home_cb, NULL);
}

void nk_scr_onboarding(lv_obj_t *body, nk_ui_screen_id screen, nk_ui_state state) {
    s_suggest = NULL;
    switch (screen) {
    case NK_UI_ON01:
        welcome(body);
        break;
    case NK_UI_ON02:
        consent(body);
        break;
    case NK_UI_ON03:
        permissions(body, state);
        break;
    case NK_UI_ON04:
        placement(body);
        break;
    case NK_UI_ON05:
        goals(body);
        break;
    case NK_UI_ON06:
        alarm_permissions(body, state);
        break;
    case NK_UI_ON07:
        curtain(body, state);
        break;
    case NK_UI_ON08:
        automode(body);
        break;
    case NK_UI_ON09:
    default:
        done(body);
        break;
    }
}
