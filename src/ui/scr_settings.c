/*
 * ST-01 設定（下部タブ）、ST-02 データとプライバシー、ST-03 この分析について、
 * ST-04 ライセンス表示、X-01 状態表示の例（UX-7）、
 * DEV 画面と状態の一覧（PC 版の確認用。ADR-0011 の段 4）
 */
#include "ui_internal.h"

#include <stdint.h>
#include <stdio.h>

static lv_obj_t *chevron_row(lv_obj_t *card, const char *title, const char *sub,
                             nk_ui_screen_id target) {
    lv_obj_t *row = nk_ui_setrow(card, title, sub, NULL);
    nk_ui_icon(row, NK_ICON_RIGHT, 18, NK_INK_3);
    lv_obj_set_clickable(row, true);
    lv_obj_set_style_bg_color(row, NK_WHITE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, 10, LV_STATE_PRESSED);
    nk_ui_on_click_nav(row, target, NK_UI_ST_NONE, NK_UI_NAV_PUSH);
    return row;
}

static lv_obj_t *list_card(lv_obj_t *body) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_ver(card, 4, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    return card;
}

static void section(lv_obj_t *body, const char *text) {
    lv_obj_t *l = nk_ui_label(body, text, NK_FB(12), NK_INK_3);
    lv_obj_set_style_margin_top(l, 4, 0);
    lv_obj_set_style_margin_bottom(l, -6, 0);
}

/* ---------------------------------------------------------------- ST-01 */

static void font_scale_cb(lv_event_t *e) {
    static const float scales[] = {1.0f, 1.2f, 1.4f};
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    nk_ui_apply_font_scale(scales[idx]);
}

static void reduce_motion_cb(lv_event_t *e) {
    nk_ui_set_reduce_motion(lv_obj_has_state(lv_event_get_current_target_obj(e), LV_STATE_CHECKED));
}

static void morning_q_cb(lv_event_t *e) {
    nk_demo_state_get()->morning_question =
        lv_obj_has_state(lv_event_get_current_target_obj(e), LV_STATE_CHECKED);
}

void nk_scr_settings(lv_obj_t *body, nk_ui_state state) {
    (void)state;
    /* 設定は下部タブから開く（UX-3 改訂案）。寝室（カーテン）もここに置く（UX-5 改訂版の仮の形） */
    nk_ui_title(body, "設定");

    section(body, "寝室と機器");
    lv_obj_t *room = list_card(body);
    chevron_row(room, "寝室", "カーテンの操作・予約・自動モード", NK_UI_BR01);
    chevron_row(room, "カーテンをつなぐ", "寝室のカーテン（Curtain 3）・接続済み", NK_UI_BR02);

    section(body, "目標とアラーム");
    lv_obj_t *alarm = list_card(body);
    char sub[128];
    char weekday[8];
    char holiday[8];
    nk_fmt_clock(weekday, sizeof(weekday), nk_demo_state_get()->alarm_min);
    nk_fmt_clock(holiday, sizeof(holiday), nk_demo_state_get()->alarm_holiday_min);
    (void)snprintf(sub, sizeof(sub), "目標 7時間・平日 %s・休日 %s・スヌーズ 5分", weekday,
                   holiday);
    chevron_row(alarm, "目標とアラーム", sub, NK_UI_NT05);

    section(body, "記録とデータ");
    lv_obj_t *data = list_card(body);
    chevron_row(data, "データとプライバシー", "同意、マイク、保存している記録", NK_UI_ST02);
    chevron_row(data, "この分析について", "何をどう推定しているか", NK_UI_ST03);

    section(body, "表示");
    lv_obj_t *view = list_card(body);
    lv_obj_t *size_row =
        nk_ui_setrow(view, "文字の大きさ", "端末の設定ではなく、ここで変えます", NULL);
    static const char *const sizes[] = {"標準", "大", "特大"};
    float scale = nk_theme_font_scale();
    int sel = scale > 1.3f ? 2 : (scale > 1.1f ? 1 : 0);
    nk_ui_segmented(size_row, sizes, 3, sel, font_scale_cb);
    lv_obj_t *motion =
        nk_ui_setrow(view, "動きを減らす", "スコアのリングなどの動きを止めます", NULL);
    nk_ui_switch(motion, nk_ui_reduce_motion() && !nk_ui_instant(), reduce_motion_cb, NULL);
    lv_obj_t *mq = nk_ui_setrow(view, "朝の気分を聞く", "起きたときに 1 タップで答える質問", NULL);
    nk_ui_switch(mq, nk_demo_state_get()->morning_question, morning_q_cb, NULL);
    lv_obj_t *clock = nk_ui_setrow(view, "計測中も時計を表示する",
                                   "オフなら 30 秒で画面を消します（電池のため）", NULL);
    nk_ui_switch(clock, false, NULL, NULL);

    section(body, "そのほか");
    lv_obj_t *other = list_card(body);
    chevron_row(other, "ライセンス表示", NULL, NK_UI_ST04);
    chevron_row(other, "状態表示の例", "UX-7 の帯・行・操作結果", NK_UI_X01);
    chevron_row(other, "画面と状態の一覧", "開発用：すべての画面を開く", NK_UI_DEV);

    lv_obj_t *ver =
        nk_ui_text(body, "ねむのき 0.1.0（画面の試作。表示している記録はデモのデータです）",
                   NK_FR(11), NK_INK_3);
    lv_obj_set_style_text_align(ver, LV_TEXT_ALIGN_CENTER, 0);
}

/* ---------------------------------------------------------------- ST-02 */

static void toast_cb(lv_event_t *e) {
    nk_ui_toast(lv_event_get_user_data(e));
}

static void keep_snore_cb(lv_event_t *e) {
    nk_demo_state_get()->keep_snore =
        lv_obj_has_state(lv_event_get_current_target_obj(e), LV_STATE_CHECKED);
}

static void delete_all_cb(lv_event_t *e) {
    (void)e;
    lv_obj_t *sheet =
        nk_ui_sheet_open("記録をすべて削除しますか",
                         "14 夜分の記録と設定を、この端末から削除します。元に戻せません。");
    lv_obj_t *pair = nk_ui_btn_pair(sheet);
    lv_obj_t *cancel = nk_ui_btn_ghost(pair, "やめる", nk_ui_sheet_close_cb, NULL);
    lv_obj_set_flex_grow(cancel, 1);
    lv_obj_t *ok =
        nk_ui_btn_serious(pair, "すべて削除する", toast_cb, (void *)"削除しました（デモ）");
    lv_obj_set_flex_grow(ok, 1);
    lv_obj_add_event_cb(ok, nk_ui_sheet_close_cb, LV_EVENT_CLICKED, NULL);
}

static void withdraw_cb(lv_event_t *e) {
    (void)e;
    lv_obj_t *sheet = nk_ui_sheet_open("同意を取り消しますか",
                                       "計測を止めます。サウンドとアラームはこのまま使えます。");
    lv_obj_t *col = nk_ui_col(sheet, 8);
    nk_ui_btn_serious(col, "取り消して、記録も削除する", nk_ui_sheet_close_cb, NULL);
    nk_ui_btn_ghost(col, "取り消して、記録は残す", nk_ui_sheet_close_cb, NULL);
    nk_ui_btn_ghost(col, "やめる", nk_ui_sheet_close_cb, NULL);
}

void nk_scr_privacy(lv_obj_t *body, nk_ui_state state) {
    nk_ui_subhead(body, "データとプライバシー");
    lv_obj_t *card = list_card(body);
    nk_ui_setrow(card, "計測への同意", "同意文 1.0・2026/10/20 に同意", "同意済み");
    if (state == NK_UI_ST_PERM_MIC) {
        lv_obj_t *row = nk_ui_setrow(card, "マイク", "オフのため、いびきは記録されません", NULL);
        nk_ui_link(row, "マイクを許可", toast_cb, (void *)"PC 版では許可の確認は出ません");
    } else {
        nk_ui_setrow(card, "マイク", "いびきと寝室の音の大きさに使います", "許可済み");
    }
    lv_obj_t *rec =
        nk_ui_setrow(card, "いびきの録音を残す",
                     "オンにすると、1 回 10 秒・1 晩 10 件まで暗号化して保存し、7 日で自動で"
                     "消します。同室の人の声が入ることがあります",
                     NULL);
    nk_ui_switch(rec, nk_demo_state_get()->keep_snore, keep_snore_cb, NULL);

    lv_obj_t *data = list_card(body);
    nk_ui_setrow(data, "保存している記録", "2026/10/18 から 14 夜分・この端末の中だけ", "1.4 MB");
    nk_ui_setrow(data, "細かな計測値の保存期間", "毎日の結果は削除するまで残ります", "90 日");
    chevron_row(data, "この分析について", "何をどう推定しているか", NK_UI_ST03);

    nk_ui_btn_serious(body, "記録をすべて削除する", delete_all_cb, NULL);
    nk_ui_btn_ghost(body, "同意を取り消す", withdraw_cb, NULL);
    nk_ui_text(body, "同意を取り消すと計測は止まり、記録を削除するかを選べます。", NK_FR(11),
               NK_INK_3);
}

/* ---------------------------------------------------------------- ST-03 */

static void info_card(lv_obj_t *body, nk_icon icon, const char *title, const char *text) {
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_row(card, 10);
    nk_ui_icon_tile(head, icon, 34, NK_SERIES, 56, NK_SERIES_LT);
    nk_ui_label(head, title, NK_FB(15), NK_INK);
    nk_ui_text(card, text, NK_FR(13), NK_INK_2);
}

void nk_scr_about(lv_obj_t *body, nk_ui_state state) {
    (void)state;
    nk_ui_subhead(body, "この分析について");
    info_card(body, NK_ICON_PHONE, "何を計測しているか",
              "体の動き（加速度センサー）と、寝室の音の大きさ・いびき（マイク）から、眠りの様子を"
              "推定します。音は端末の中ですぐ数値に変え、録音は保存しません。");
    info_card(body, NK_ICON_MOON, "眠りの段階は推定です",
              "覚醒・浅い眠り・深い眠りは、体の動きの少なさと続いた長さから推定しています。"
              "脳波を測る検査とは違い、目安としてご覧ください。");

    lv_obj_t *card = nk_ui_card(body);
    lv_obj_t *head = nk_ui_row(card, 10);
    nk_ui_icon_tile(head, NK_ICON_CHART, 34, NK_SERIES, 56, NK_SERIES_LT);
    nk_ui_label(head, "スコアの決め方（100 点）", NK_FB(15), NK_INK);
    nk_ui_kv(card, "睡眠時間（目標との差）", "30 点", false);
    nk_ui_kv(card, "寝つき", "15 点", false);
    nk_ui_kv(card, "連続性（夜中に目覚めた時間）", "25 点", false);
    nk_ui_kv(card, "効率（寝床で眠っていた割合）", "15 点", false);
    nk_ui_kv(card, "規則性（いつもの時刻との差）", "15 点", false);
    lv_obj_t *band = nk_ui_row(card, 6);
    lv_obj_set_flex_flow(band, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(band, 6, 0);
    nk_ui_pill(band, "85〜 とても良い", NK_TONE_GOOD, false);
    nk_ui_pill(band, "70〜 良い", NK_TONE_GOOD, false);
    nk_ui_pill(band, "55〜 まずまず", NK_TONE_WARN, false);
    nk_ui_pill(band, "〜54 回復を優先", NK_TONE_SERIOUS, false);

    info_card(body, NK_ICON_INFO, "参考値になるとき",
              "計測できた時間が全体の 7 割に届かない夜や、3 時間に届かない夜は、スコアを「参考値」"
              "として表示し、浅い眠りと深い眠りを分けずに表示します。");
    info_card(body, NK_ICON_LOCK, "医療機器ではありません",
              "ねむのきは診断をしません。つらさが続くときは、医療機関（睡眠外来など）で相談する"
              "こともできます。");
}

/* ---------------------------------------------------------------- ST-04 */

void nk_scr_licenses(lv_obj_t *body, nk_ui_state state) {
    (void)state;
    static const char *const libs[][3] = {
        {"SDL3", "release-3.4.18", "zlib License"},
        {"LVGL", "v9.6.0", "MIT License"},
        {"SQLCipher", "v4.19.0", "BSD 系のライセンス"},
        {"curl", "8.23.0", "curl License"},
        {"cJSON", "v1.7.19", "MIT License"},
        {"Noto Sans JP", "—", "SIL Open Font License 1.1"},
    };
    nk_ui_subhead(body, "ライセンス表示");
    nk_ui_text(body, "ねむのきは、次のオープンソースソフトウェアを使っています。", NK_FR(13),
               NK_INK_2);
    lv_obj_t *card = list_card(body);
    for (size_t i = 0; i < sizeof(libs) / sizeof(libs[0]); i++) {
        char sub[192];
        (void)snprintf(sub, sizeof(sub), "%s・%s", libs[i][1], libs[i][2]);
        nk_ui_setrow(card, libs[i][0], sub, NULL);
    }
    nk_ui_text(body, "ライセンスの全文は、リリースまでにこの画面で読めるようにします。", NK_FR(11),
               NK_INK_3);
}

/* ---------------------------------------------------------------- X-01 */

static void catalog(lv_obj_t *body, const char *text) {
    lv_obj_t *l = nk_ui_label(body, text, NK_FB(12), NK_INK_3);
    lv_obj_set_style_margin_top(l, 6, 0);
    lv_obj_set_style_margin_bottom(l, -4, 0);
}

void nk_scr_states(lv_obj_t *body, nk_ui_state state) {
    (void)state;
    nk_ui_subhead(body, "状態表示の例");

    catalog(body, "保存中・分析中（ST-SAVING / ST-ANALYZING）");
    nk_ui_banner(body, NK_TONE_INFO, NK_ICON_NONE, true, "昨夜の記録を分析しています",
                 "前回の結果を表示しています", NULL, NULL, NULL);

    catalog(body, "計測が止まった（ST-TRACKING-INTERRUPTED）");
    nk_ui_banner(body, NK_TONE_WARN, NK_ICON_INFO, false,
                 "計測が止まっています（アプリが停止しました）", NULL, "計測を再開する", NULL,
                 NULL);

    catalog(body, "許可がない（ST-PERM-EXACT-ALARM）");
    nk_ui_banner(body, NK_TONE_SERIOUS, NK_ICON_ALARM, false,
                 "正確な時刻に鳴らすための許可がオフです", "アラームと自動モードが使えません",
                 "設定を開く", NULL, NULL);

    catalog(body, "カーテンの操作結果（nk_op_result の 4 状態）");
    nk_ui_op_result(body, NK_OP_PENDING, "カーテンに伝えています", NULL, NULL, NULL, NULL);
    nk_ui_op_result(body, NK_OP_OK, "開けました（6:21 確認）", "自動モード・アラームの 10 分前",
                    NULL, NULL, NULL);
    nk_ui_op_result(body, NK_OP_UNCONFIRMED, "命令は届きましたが、動いたか確認できませんでした",
                    NULL, "状態を確かめる", NULL, NULL);
    nk_ui_op_result(body, NK_OP_FAILED, "開けられませんでした：カーテンが応答しません",
                    "ハブがオフラインの可能性があります", "もう一度", NULL, NULL);

    catalog(body, "注意の行（nk_notice_row）");
    lv_obj_t *card = nk_ui_card(body);
    lv_obj_set_style_pad_row(card, 6, 0);
    nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN,
                     "電池が残り 15% です。充電しながら計測すると、朝まで記録できます");
    nk_ui_notice_row(card, NK_ICON_INFO, NK_WARN, "マイクがオフのため、いびきは記録されません");

    catalog(body, "札（接続・品質）");
    lv_obj_t *pills = nk_ui_row(body, 6);
    lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(pills, 6, 0);
    nk_ui_pill(pills, "接続済み", NK_TONE_GOOD, true);
    nk_ui_pill(pills, "確認中", NK_TONE_INFO, true);
    nk_ui_pill(pills, "つながりません", NK_TONE_SERIOUS, true);
    nk_ui_pill(pills, "未接続", NK_TONE_NEUTRAL, true);
    nk_ui_tag_ref(pills);
    nk_ui_tag_est(pills);

    catalog(body, "記録がまだない（ST-EMPTY-FIRST）");
    nk_ui_empty(body, "今夜から記録できます", "寝る前に「おやすみ」を押してください", NULL, NULL,
                NULL);
}

/* ---------------------------------------------------------------- DEV */

static void dev_open_cb(lv_event_t *e) {
    uintptr_t packed = (uintptr_t)lv_event_get_user_data(e);
    nk_ui_push((nk_ui_screen_id)(packed & 0xFFu), (nk_ui_state)(packed >> 8));
}

void nk_scr_dev(lv_obj_t *body, nk_ui_state state) {
    (void)state;
    nk_ui_title(body, "画面と状態の一覧");
    nk_ui_text(body,
               "PC 版の確認用です。押すと、その画面と状態を開きます（Esc で戻る）。"
               "起動時の --screen と --state でも開けます。",
               NK_FR(13), NK_INK_2);
    size_t count = nk_ui_shot_count();
    lv_obj_t *card = NULL;
    lv_obj_t *chips = NULL;
    nk_ui_screen_id last = NK_UI_SCREEN_COUNT;
    for (size_t i = 0; i < count; i++) {
        nk_ui_screen_id screen;
        nk_ui_state st;
        if (!nk_ui_shot_get(i, &screen, &st) || screen == NK_UI_DEV) {
            continue;
        }
        if (screen != last) {
            last = screen;
            card = nk_ui_card(body);
            lv_obj_set_style_pad_all(card, 12, 0);
            lv_obj_set_style_pad_row(card, 8, 0);
            char title[192];
            (void)snprintf(title, sizeof(title), "%s  %s", nk_ui_screen_name(screen),
                           nk_ui_screen_title(screen));
            nk_ui_label(card, title, NK_FB(14), NK_INK);
            chips = nk_ui_row(card, 6);
            lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW_WRAP);
            lv_obj_set_style_pad_row(chips, 6, 0);
        }
        const char *name = st == NK_UI_ST_NONE ? "通常" : nk_ui_state_name(st) + 3;
        uintptr_t packed = (uintptr_t)screen | ((uintptr_t)st << 8);
        lv_obj_t *chip = nk_ui_chip(chips, name, false, dev_open_cb, (void *)packed);
        lv_obj_set_style_text_font(lv_obj_get_child(chip, 0), NK_FB(12), 0);
        lv_obj_set_style_pad_ver(chip, 6, 0);
        lv_obj_set_style_pad_hor(chip, 10, 0);
    }
    (void)card;
}
