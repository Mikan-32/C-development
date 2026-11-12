/*
 * 画面の切り替え、戻る操作、タブバー（UX-2）
 *
 * 画面は「開くたびに作り、閉じたら捨てる」。
 * タブバーは最上位の層（lv_layer_top）に 1 つだけ置く。
 * イベントの中から画面を切り替えるときは、lv_async_call で次の周回に回す
 *（押したボタンを消さないため）。
 */
#include "ui.h"

#include "ui_internal.h"

#include <stdint.h>
#include <string.h>

#define LEN(a)     (sizeof(a) / sizeof((a)[0]))
#define STACK_MAX  16
#define TIMERS_MAX 16
#define MORNING    (7 * 60 + 2)   /* デモの「今」：10 月 31 日（土）7:02 */
#define EVENING    (22 * 60 + 41) /* おやすみの画面の時刻 */
#define MIDNIGHT   (1 * 60 + 24)  /* 計測中の画面の時刻 */
#define ALARM_TIME (6 * 60 + 30)

/* ---------------------------------------------------------------- 画面の一覧 */

typedef struct screen_desc {
    const char *name;
    const char *title;
    const char *back; /* 次の画面の見出しで「‹ ○○」と出す名前 */
    nk_ui_tab tab;
    int clock;
    bool night;
    bool scroll;
    nk_ui_build_fn build;
    const nk_ui_state *states; /* スクリーンショットを撮る状態（先頭は通常） */
    size_t state_count;
} screen_desc;

#define ON_BUILDER(id, screen)                                                                     \
    static void build_##id(lv_obj_t *body, nk_ui_state state) {                                    \
        nk_scr_onboarding(body, screen, state);                                                    \
    }
ON_BUILDER(on01, NK_UI_ON01)
ON_BUILDER(on02, NK_UI_ON02)
ON_BUILDER(on03, NK_UI_ON03)
ON_BUILDER(on04, NK_UI_ON04)
ON_BUILDER(on05, NK_UI_ON05)
ON_BUILDER(on06, NK_UI_ON06)
ON_BUILDER(on07, NK_UI_ON07)
ON_BUILDER(on08, NK_UI_ON08)
ON_BUILDER(on09, NK_UI_ON09)

#define S(x) NK_UI_ST_##x
static const nk_ui_state k_st_none[] = {S(NONE)};
static const nk_ui_state k_st_on03[] = {S(NONE), S(PERM_MIC)};
static const nk_ui_state k_st_on06[] = {S(NONE), S(PERM_EXACT_ALARM)};
static const nk_ui_state k_st_on07[] = {S(NONE), S(CURTAIN_OK), S(CURTAIN_AUTH)};
static const nk_ui_state k_st_home[] = {
    S(NONE),
    S(EMPTY_FIRST),
    S(EMPTY_DAY),
    S(SAVING),
    S(ANALYZING),
    S(ANALYSIS_FAILED),
    S(FAILED),
    S(INSUFFICIENT),
    S(PARTIAL_GAP),
    S(TRACKING_INTERRUPTED),
    S(CURTAIN_PENDING),
    S(CURTAIN_FAILED),
    S(CURTAIN_UNCONFIRMED),
    S(CURTAIN_OFFLINE),
};
static const nk_ui_state k_st_advice[] = {S(NONE), S(NOT_ENOUGH_NIGHTS)};
static const nk_ui_state k_st_history[] = {S(NONE), S(EMPTY_FIRST), S(EMPTY_DAY), S(INSUFFICIENT),
                                           S(FAILED)};
static const nk_ui_state k_st_week[] = {S(NONE), S(EMPTY_FIRST), S(NOT_ENOUGH_NIGHTS)};
static const nk_ui_state k_st_day[] = {S(NONE), S(INSUFFICIENT), S(PARTIAL_GAP), S(ANALYSIS_FAILED),
                                       S(FAILED)};
static const nk_ui_state k_st_night[] = {S(NONE), S(BATTERY_LOW), S(PERM_MIC), S(PERM_NOTIF),
                                         S(PERM_EXACT_ALARM)};
static const nk_ui_state k_st_tracking[] = {S(TRACKING), S(TRACKING_INTERRUPTED)};
static const nk_ui_state k_st_ringing[] = {S(CURTAIN_PENDING), S(CURTAIN_OK), S(CURTAIN_FAILED),
                                           S(CURTAIN_UNCONFIRMED)};
static const nk_ui_state k_st_morning[] = {S(NONE), S(SAVING), S(ANALYZING)};
static const nk_ui_state k_st_alarm[] = {S(NONE), S(PERM_EXACT_ALARM), S(PERM_FSI), S(PERM_NOTIF)};
static const nk_ui_state k_st_bedroom[] = {
    S(NONE),           S(CURTAIN_CHECKING),    S(CURTAIN_PENDING),
    S(CURTAIN_FAILED), S(CURTAIN_UNCONFIRMED), S(CURTAIN_OFFLINE),
    S(AUTO_PAUSED),    S(PERM_EXACT_ALARM),    S(EMPTY_FIRST),
};
static const nk_ui_state k_st_connect[] = {S(NONE), S(EMPTY_FIRST), S(CURTAIN_AUTH)};
static const nk_ui_state k_st_automode[] = {S(NONE), S(PERM_EXACT_ALARM), S(EMPTY_FIRST)};
static const nk_ui_state k_st_privacy[] = {S(NONE), S(PERM_MIC)};
#undef S

#define STATES(a) a, LEN(a)

static const screen_desc k_screens[NK_UI_SCREEN_COUNT] = {
    /* 初回利用フロー（UX-1 改訂版） */
    [NK_UI_ON01] = {"ON-01", "ようこそ", "戻る", NK_UI_TAB_NONE, MORNING, false, true, build_on01,
                    STATES(k_st_none)},
    [NK_UI_ON02] = {"ON-02", "データの使い方と同意", "戻る", NK_UI_TAB_NONE, MORNING, false, true,
                    build_on02, STATES(k_st_none)},
    [NK_UI_ON03] = {"ON-03", "マイクと通知の許可", "戻る", NK_UI_TAB_NONE, MORNING, false, true,
                    build_on03, STATES(k_st_on03)},
    [NK_UI_ON04] = {"ON-04", "置き方と試し計測", "戻る", NK_UI_TAB_NONE, MORNING, false, true,
                    build_on04, STATES(k_st_none)},
    [NK_UI_ON05] = {"ON-05", "目標と起きる時刻", "戻る", NK_UI_TAB_NONE, MORNING, false, true,
                    build_on05, STATES(k_st_none)},
    [NK_UI_ON06] = {"ON-06", "アラームの許可", "戻る", NK_UI_TAB_NONE, MORNING, false, true,
                    build_on06, STATES(k_st_on06)},
    [NK_UI_ON07] = {"ON-07", "カーテンをつなぐ", "戻る", NK_UI_TAB_NONE, MORNING, false, true,
                    build_on07, STATES(k_st_on07)},
    [NK_UI_ON08] = {"ON-08", "自動モードの説明", "戻る", NK_UI_TAB_NONE, MORNING, false, true,
                    build_on08, STATES(k_st_none)},
    [NK_UI_ON09] = {"ON-09", "準備完了", "戻る", NK_UI_TAB_NONE, MORNING, false, true, build_on09,
                    STATES(k_st_none)},
    /* ホーム（UX-3 改訂案） */
    [NK_UI_HM01] = {"HM-01", "ホーム", "ホーム", NK_UI_TAB_HOME, MORNING, false, true, nk_scr_home,
                    STATES(k_st_home)},
    [NK_UI_AD01] = {"AD-01", "アドバイスの一覧", "アドバイス", NK_UI_TAB_HOME, MORNING, false, true,
                    nk_scr_advice, STATES(k_st_advice)},
    /* 睡眠記録（UX-4 改訂案） */
    [NK_UI_LG01] = {"LG-01", "睡眠記録（日）", "睡眠記録", NK_UI_TAB_HISTORY, MORNING, false, true,
                    nk_scr_history_day, STATES(k_st_history)},
    [NK_UI_LG01W] = {"LG-01W", "睡眠記録（週）", "睡眠記録", NK_UI_TAB_HISTORY, MORNING, false,
                     true, nk_scr_history_week, STATES(k_st_week)},
    [NK_UI_LG02] = {"LG-02", "日別の詳細", "日別の詳細", NK_UI_TAB_HISTORY, MORNING, false, true,
                    nk_scr_day, STATES(k_st_day)},
    /* 睡眠モード（UX-6） */
    [NK_UI_NT01] = {"NT-01", "睡眠モード", "睡眠モード", NK_UI_TAB_NIGHT, EVENING, false, true,
                    nk_scr_night, STATES(k_st_night)},
    [NK_UI_NT02] = {"NT-02", "計測中", "計測中", NK_UI_TAB_NONE, MIDNIGHT, true, false,
                    nk_scr_tracking, STATES(k_st_tracking)},
    [NK_UI_NT03] = {"NT-03", "アラーム鳴動", "アラーム", NK_UI_TAB_NONE, ALARM_TIME, true, false,
                    nk_scr_ringing, STATES(k_st_ringing)},
    [NK_UI_NT04] = {"NT-04", "おはよう", "おはよう", NK_UI_TAB_NONE, ALARM_TIME + 1, false, false,
                    nk_scr_morning, STATES(k_st_morning)},
    [NK_UI_NT05] = {"NT-05", "アラームの設定", "アラーム", NK_UI_TAB_NIGHT, EVENING, false, true,
                    nk_scr_alarm_settings, STATES(k_st_alarm)},
    /* 設定と寝室（UX-5 改訂版。寝室は設定の中に置く仮の形） */
    [NK_UI_BR01] = {"BR-01", "寝室", "寝室", NK_UI_TAB_SETTINGS, MORNING, false, true,
                    nk_scr_bedroom, STATES(k_st_bedroom)},
    [NK_UI_BR02] = {"BR-02", "カーテンをつなぐ", "カーテン", NK_UI_TAB_SETTINGS, MORNING, false,
                    true, nk_scr_connect, STATES(k_st_connect)},
    [NK_UI_BR03] = {"BR-03", "自動モードを有効にする", "自動モード", NK_UI_TAB_SETTINGS, MORNING,
                    false, true, nk_scr_automode, STATES(k_st_automode)},
    [NK_UI_ST01] = {"ST-01", "設定", "設定", NK_UI_TAB_SETTINGS, MORNING, false, true,
                    nk_scr_settings, STATES(k_st_none)},
    [NK_UI_ST02] = {"ST-02", "データとプライバシー", "データ", NK_UI_TAB_SETTINGS, MORNING, false,
                    true, nk_scr_privacy, STATES(k_st_privacy)},
    [NK_UI_ST03] = {"ST-03", "この分析について", "分析", NK_UI_TAB_SETTINGS, MORNING, false, true,
                    nk_scr_about, STATES(k_st_none)},
    [NK_UI_ST04] = {"ST-04", "ライセンス表示", "ライセンス", NK_UI_TAB_SETTINGS, MORNING, false,
                    true, nk_scr_licenses, STATES(k_st_none)},
    [NK_UI_X01] = {"X-01", "状態表示の例", "状態表示", NK_UI_TAB_SETTINGS, MORNING, false, true,
                   nk_scr_states, STATES(k_st_none)},
    [NK_UI_DEV] = {"DEV", "画面と状態の一覧", "一覧", NK_UI_TAB_NONE, MORNING, false, true,
                   nk_scr_dev, STATES(k_st_none)},
};

static const char *const k_state_names[NK_UI_STATE_COUNT] = {
    "ST-NONE",
    "ST-EMPTY-FIRST",
    "ST-EMPTY-DAY",
    "ST-SAVING",
    "ST-ANALYZING",
    "ST-ANALYSIS-FAILED",
    "ST-FAILED",
    "ST-INSUFFICIENT",
    "ST-PARTIAL-GAP",
    "ST-NOT-ENOUGH-NIGHTS",
    "ST-TRACKING",
    "ST-TRACKING-INTERRUPTED",
    "ST-BATTERY-LOW",
    "ST-PERM-MIC",
    "ST-PERM-NOTIF",
    "ST-PERM-EXACT-ALARM",
    "ST-PERM-FSI",
    "ST-CURTAIN-CHECKING",
    "ST-CURTAIN-PENDING",
    "ST-CURTAIN-OK",
    "ST-CURTAIN-FAILED",
    "ST-CURTAIN-UNCONFIRMED",
    "ST-CURTAIN-OFFLINE",
    "ST-CURTAIN-AUTH",
    "ST-AUTO-PAUSED",
};

/* タブの最上位の画面 */
static const nk_ui_screen_id k_tab_root[] = {NK_UI_HM01, NK_UI_HM01, NK_UI_LG01, NK_UI_NT01,
                                             NK_UI_ST01};

/* ---------------------------------------------------------------- 状態 */

typedef struct nav_entry {
    nk_ui_screen_id screen;
    nk_ui_state state;
} nav_entry;

typedef enum nav_mode {
    NAV_OPEN,    /* 履歴を消して開く */
    NAV_PUSH,    /* 進む */
    NAV_REPLACE, /* 置き換える */
    NAV_TAB,     /* タブの最上位へ */
    NAV_BACK,    /* 戻る */
    NAV_REBUILD, /* 作り直す */
} nav_mode;

static struct {
    bool ready;
    nk_ui_config config;
    nav_entry stack[STACK_MAX];
    int depth;
    lv_obj_t *screen;
    lv_obj_t *body;
    lv_obj_t *tabbar;
    lv_obj_t *tab_buttons[4];
    lv_obj_t *home_indicator;
    bool pending;
    nav_entry pending_entry;
    nav_mode pending_mode;
    struct {
        lv_timer_t *timer;
        lv_obj_t *owner;
    } timers[TIMERS_MAX];
} s_ui;

/* ---------------------------------------------------------------- 名前 */

static int ascii_casecmp(const char *a, const char *b) {
    for (;; a++, b++) {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') {
            ca -= 'a' - 'A';
        }
        if (cb >= 'a' && cb <= 'z') {
            cb -= 'a' - 'A';
        }
        if (ca != cb || ca == 0) {
            return ca - cb;
        }
    }
}

const char *nk_ui_screen_name(nk_ui_screen_id screen) {
    return screen < NK_UI_SCREEN_COUNT ? k_screens[screen].name : "?";
}

const char *nk_ui_screen_title(nk_ui_screen_id screen) {
    return screen < NK_UI_SCREEN_COUNT ? k_screens[screen].title : "?";
}

const char *nk_ui_state_name(nk_ui_state state) {
    return state < NK_UI_STATE_COUNT ? k_state_names[state] : "?";
}

nk_ui_screen_id nk_ui_screen_from_name(const char *name) {
    if (name == NULL) {
        return NK_UI_SCREEN_COUNT;
    }
    for (int i = 0; i < NK_UI_SCREEN_COUNT; i++) {
        if (ascii_casecmp(name, k_screens[i].name) == 0) {
            return (nk_ui_screen_id)i;
        }
    }
    return NK_UI_SCREEN_COUNT;
}

nk_ui_state nk_ui_state_from_name(const char *name) {
    if (name == NULL) {
        return NK_UI_STATE_COUNT;
    }
    for (int i = 0; i < NK_UI_STATE_COUNT; i++) {
        /* 「ST-」は省略してもよい */
        if (ascii_casecmp(name, k_state_names[i]) == 0 ||
            ascii_casecmp(name, k_state_names[i] + 3) == 0) {
            return (nk_ui_state)i;
        }
    }
    return NK_UI_STATE_COUNT;
}

size_t nk_ui_shot_count(void) {
    size_t count = 0;
    for (int i = 0; i < NK_UI_SCREEN_COUNT; i++) {
        count += k_screens[i].state_count;
    }
    return count;
}

bool nk_ui_shot_get(size_t index, nk_ui_screen_id *screen, nk_ui_state *state) {
    for (int i = 0; i < NK_UI_SCREEN_COUNT; i++) {
        if (index < k_screens[i].state_count) {
            *screen = (nk_ui_screen_id)i;
            *state = k_screens[i].states[index];
            return true;
        }
        index -= k_screens[i].state_count;
    }
    return false;
}

/* ---------------------------------------------------------------- タイマー */

lv_timer_t *nk_ui_timer(lv_timer_cb_t cb, uint32_t period_ms, void *user_data) {
    lv_timer_t *timer = lv_timer_create(cb, period_ms, user_data);
    for (int i = 0; i < TIMERS_MAX; i++) {
        if (s_ui.timers[i].timer == NULL) {
            s_ui.timers[i].timer = timer;
            s_ui.timers[i].owner = s_ui.screen;
            return timer;
        }
    }
    LV_LOG_WARN("too many screen timers");
    return timer;
}

void nk_ui_timer_stop(lv_timer_t *timer) {
    for (int i = 0; i < TIMERS_MAX; i++) {
        if (s_ui.timers[i].timer == timer) {
            s_ui.timers[i].timer = NULL;
            s_ui.timers[i].owner = NULL;
        }
    }
    lv_timer_delete(timer);
}

static void delete_timers_of(const lv_obj_t *owner) {
    for (int i = 0; i < TIMERS_MAX; i++) {
        if (s_ui.timers[i].timer != NULL && s_ui.timers[i].owner == owner) {
            lv_timer_delete(s_ui.timers[i].timer);
            s_ui.timers[i].timer = NULL;
            s_ui.timers[i].owner = NULL;
        }
    }
}

static void screen_delete_cb(lv_event_t *e) {
    lv_obj_t *scr = lv_event_get_current_target_obj(e);
    delete_timers_of(scr);
    if (s_ui.screen == scr) {
        s_ui.screen = NULL;
        s_ui.body = NULL;
    }
}

/* ---------------------------------------------------------------- 端末の上の帯（PC 版） */

static void status_bar(lv_obj_t *scr, int clock, bool night) {
    lv_color_t color = night ? NK_INK_2 : NK_INK;
    lv_obj_t *bar = nk_ui_row(scr, 6);
    lv_obj_set_size(bar, NK_SCREEN_W, NK_STATUS_H);
    lv_obj_set_style_pad_hor(bar, 30, 0);
    lv_obj_set_style_pad_top(bar, 14, 0);
    char buf[8];
    nk_fmt_clock(buf, sizeof(buf), clock);
    nk_ui_label(bar, buf, NK_FB(15), color);
    nk_ui_spacer(bar);
    lv_obj_t *dots = nk_ui_box(bar);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots, 2, 0);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *dot = nk_ui_box(dots);
        lv_obj_set_size(dot, 9, 9);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, color, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }
    nk_ui_label(bar, "64%", NK_FB(15), color);
}

/* ---------------------------------------------------------------- タブバー */

static void tab_click_cb(lv_event_t *e) {
    int tab = (int)(intptr_t)lv_event_get_user_data(e);
    nk_ui_go_tab(k_tab_root[tab]);
}

static void tabbar_create(void) {
    static const char *const labels[] = {"ホーム", "睡眠記録", "睡眠モード", "設定"};
    static const nk_icon icons[] = {NK_ICON_HOME, NK_ICON_CHART, NK_ICON_MOON, NK_ICON_GEAR};

    lv_obj_t *bar = nk_ui_box(lv_layer_top());
    lv_obj_set_size(bar, NK_SCREEN_W, NK_TABBAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, NK_TABBAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, NK_WHITE, 0);
    lv_obj_set_style_border_opa(bar, NK_OPA_LINE, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(bar, 9, 0);
    lv_obj_set_style_pad_hor(bar, 10, 0);
    lv_obj_set_clickable(bar, true); /* タブバーの下の画面を押させない */

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_button_create(bar);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 76, 52);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(btn, 3, 0);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_bg_color(btn, NK_WHITE, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(btn, 12, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, NK_INK_3, 0);
        lv_obj_set_style_text_color(btn, NK_BRAND, LV_STATE_CHECKED);
        lv_obj_t *ic = nk_ui_icon(btn, icons[i], 26, NK_INK_3);
        lv_obj_remove_local_style_prop(ic, LV_STYLE_TEXT_COLOR, 0); /* ボタンの色を引き継ぐ */
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, labels[i]);
        lv_obj_set_style_text_font(label, NK_FB(11), 0);
        lv_obj_add_event_cb(btn, tab_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
        s_ui.tab_buttons[i] = btn;
    }
    s_ui.tabbar = bar;

    if (s_ui.config.show_status_bar) {
        lv_obj_t *ind = nk_ui_box(lv_layer_top());
        lv_obj_set_size(ind, 134, 5);
        lv_obj_set_style_radius(ind, 3, 0);
        lv_obj_set_style_bg_color(ind, NK_WHITE, 0);
        lv_obj_set_style_bg_opa(ind, 89, 0);
        lv_obj_align(ind, LV_ALIGN_BOTTOM_MID, 0, -8);
        s_ui.home_indicator = ind;
    }
}

static void tabbar_destroy(void) {
    if (s_ui.tabbar != NULL) {
        lv_obj_delete(s_ui.tabbar);
        s_ui.tabbar = NULL;
    }
    if (s_ui.home_indicator != NULL) {
        lv_obj_delete(s_ui.home_indicator);
        s_ui.home_indicator = NULL;
    }
}

static void tabbar_update(nk_ui_tab tab) {
    if (s_ui.tabbar == NULL) {
        return;
    }
    if (tab == NK_UI_TAB_NONE) {
        lv_obj_set_hidden(s_ui.tabbar, true);
    } else {
        lv_obj_set_hidden(s_ui.tabbar, false);
    }
    for (int i = 0; i < 4; i++) {
        lv_obj_set_state(s_ui.tab_buttons[i], LV_STATE_CHECKED, (int)tab == i + 1);
    }
}

/* ---------------------------------------------------------------- 画面を作る */

static lv_screen_load_anim_t anim_for(nav_mode mode) {
    if (s_ui.config.instant || s_ui.config.reduce_motion) {
        return LV_SCREEN_LOAD_ANIM_NONE;
    }
    switch (mode) {
    case NAV_PUSH:
        return LV_SCREEN_LOAD_ANIM_MOVE_LEFT;
    case NAV_BACK:
        return LV_SCREEN_LOAD_ANIM_MOVE_RIGHT;
    case NAV_REBUILD:
        return LV_SCREEN_LOAD_ANIM_NONE;
    case NAV_OPEN:
    case NAV_REPLACE:
    case NAV_TAB:
    default:
        return LV_SCREEN_LOAD_ANIM_FADE_IN;
    }
}

static void show(nav_entry entry, nav_mode mode) {
    const screen_desc *desc = &k_screens[entry.screen];
    int32_t keep_scroll = 0;
    if (mode == NAV_REBUILD && s_ui.body != NULL) {
        keep_scroll = lv_obj_get_scroll_y(s_ui.body);
    }
    nk_ui_sheet_close();

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, NK_SCREEN_W, NK_SCREEN_H);
    lv_obj_set_style_bg_color(scr, desc->night ? NK_NIGHT : NK_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);
    lv_obj_add_event_cb(scr, screen_delete_cb, LV_EVENT_DELETE, NULL);

    if (s_ui.config.show_status_bar) {
        status_bar(scr, desc->clock, desc->night);
    }

    int32_t bottom = desc->tab != NK_UI_TAB_NONE ? NK_TABBAR_H : NK_HOME_IND_H;
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_pos(body, 0, NK_STATUS_H);
    lv_obj_set_size(body, NK_SCREEN_W, NK_SCREEN_H - NK_STATUS_H - bottom);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, NK_GAP, 0);
    lv_obj_set_style_pad_hor(body, NK_PAD, 0);
    lv_obj_set_style_pad_top(body, 6, 0);
    lv_obj_set_style_pad_bottom(body, desc->scroll ? 28 : 12, 0);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_ACTIVE);
    if (!desc->scroll) {
        lv_obj_set_scrollable(body, false);
    }

    s_ui.screen = scr;
    s_ui.body = body;
    desc->build(body, entry.state);
    tabbar_update(desc->tab);

    if (keep_scroll > 0) {
        lv_obj_update_layout(body);
        lv_obj_scroll_to_y(body, keep_scroll, LV_ANIM_OFF);
    }

    lv_screen_load_anim_t anim = anim_for(mode);
    uint32_t time =
        anim == LV_SCREEN_LOAD_ANIM_NONE ? 0 : (mode == NAV_PUSH || mode == NAV_BACK ? 220 : 160);
    lv_screen_load_anim(scr, anim, time, 0, true);
}

static void navigate(nav_entry entry, nav_mode mode) {
    switch (mode) {
    case NAV_OPEN:
    case NAV_TAB:
        s_ui.depth = 0;
        s_ui.stack[s_ui.depth++] = entry;
        break;
    case NAV_PUSH:
        if (s_ui.depth < STACK_MAX) {
            s_ui.stack[s_ui.depth++] = entry;
        } else {
            s_ui.stack[STACK_MAX - 1] = entry;
        }
        break;
    case NAV_REPLACE:
        if (s_ui.depth == 0) {
            s_ui.depth = 1;
        }
        s_ui.stack[s_ui.depth - 1] = entry;
        break;
    case NAV_BACK:
        if (s_ui.depth > 1) {
            s_ui.depth--;
        }
        entry = s_ui.stack[s_ui.depth - 1];
        break;
    case NAV_REBUILD:
    default:
        if (s_ui.depth > 0) {
            entry = s_ui.stack[s_ui.depth - 1];
        }
        break;
    }
    show(entry, mode);
}

static void pending_cb(void *user_data) {
    (void)user_data;
    if (!s_ui.ready || !s_ui.pending) {
        return;
    }
    s_ui.pending = false;
    navigate(s_ui.pending_entry, s_ui.pending_mode);
}

static void request(nk_ui_screen_id screen, nk_ui_state state, nav_mode mode) {
    if (!s_ui.ready || screen >= NK_UI_SCREEN_COUNT) {
        return;
    }
    s_ui.pending_entry.screen = screen;
    s_ui.pending_entry.state = state;
    s_ui.pending_mode = mode;
    if (!s_ui.pending) {
        s_ui.pending = true;
        (void)lv_async_call(pending_cb, NULL);
    }
}

/* ---------------------------------------------------------------- 公開する関数 */

void nk_ui_init(const nk_ui_config *config) {
    memset(&s_ui, 0, sizeof(s_ui));
    if (config != NULL) {
        s_ui.config = *config;
    }
    if (s_ui.config.font_scale <= 0.5f) {
        s_ui.config.font_scale = 1.0f;
    }
    nk_demo_state_reset();
    nk_theme_init(s_ui.config.font_regular, s_ui.config.font_regular_size, s_ui.config.font_bold,
                  s_ui.config.font_bold_size, s_ui.config.font_scale);
    lv_obj_set_style_bg_color(lv_screen_active(), NK_BG, 0);
    tabbar_create();
    s_ui.ready = true;
}

void nk_ui_deinit(void) {
    if (!s_ui.ready) {
        return;
    }
    s_ui.ready = false;
    s_ui.pending = false;
    (void)lv_async_call_cancel(pending_cb, NULL);
    nk_ui_widgets_reset();
    tabbar_destroy();
    /* いまの画面を空の画面に置き換えて消す（フォントを捨てる前に） */
    lv_obj_t *blank = lv_obj_create(NULL);
    lv_screen_load(blank);
    if (s_ui.screen != NULL) {
        lv_obj_delete(s_ui.screen);
    }
    for (int i = 0; i < TIMERS_MAX; i++) {
        if (s_ui.timers[i].timer != NULL) {
            lv_timer_delete(s_ui.timers[i].timer);
            s_ui.timers[i].timer = NULL;
        }
    }
    nk_theme_deinit();
}

bool nk_ui_open(nk_ui_screen_id screen, nk_ui_state state) {
    if (!s_ui.ready || screen >= NK_UI_SCREEN_COUNT || state >= NK_UI_STATE_COUNT) {
        return false;
    }
    s_ui.pending = false;
    if (s_ui.config.instant) {
        /* スクリーンショットとテストでは、毎回同じデモの状態から作る */
        nk_demo_state_reset();
    }
    nav_entry entry = {screen, state};
    navigate(entry, NAV_OPEN);
    return true;
}

bool nk_ui_back(void) {
    if (!s_ui.ready) {
        return false;
    }
    if (nk_ui_sheet_is_open()) {
        nk_ui_sheet_close();
        return true;
    }
    nk_ui_screen_id cur = nk_ui_current_screen();
    if (cur == NK_UI_NT02 || cur == NK_UI_NT03) {
        return true; /* 計測中と鳴動中は戻らない（UX-2 の 4 章） */
    }
    if (s_ui.depth > 1) {
        request(cur, NK_UI_ST_NONE, NAV_BACK);
        return true;
    }
    if (k_screens[cur].tab != NK_UI_TAB_NONE && cur != NK_UI_HM01) {
        request(NK_UI_HM01, NK_UI_ST_NONE, NAV_TAB);
        return true;
    }
    return false;
}

void nk_ui_scroll_by(int dy) {
    if (s_ui.body != NULL && lv_obj_is_scrollable(s_ui.body)) {
        lv_obj_scroll_by_bounded(s_ui.body, 0, dy, LV_ANIM_ON);
    }
}

void nk_ui_scroll_to(int y) {
    if (s_ui.body != NULL && lv_obj_is_scrollable(s_ui.body)) {
        lv_obj_update_layout(s_ui.body);
        lv_obj_scroll_to_y(s_ui.body, y, LV_ANIM_OFF);
    }
}

const char *nk_ui_back_label(void) {
    if (s_ui.depth >= 2) {
        return k_screens[s_ui.stack[s_ui.depth - 2].screen].back;
    }
    nk_ui_tab tab = s_ui.depth > 0 ? k_screens[s_ui.stack[0].screen].tab : NK_UI_TAB_HOME;
    if (tab == NK_UI_TAB_NONE) {
        return "戻る";
    }
    return k_screens[k_tab_root[tab]].back;
}

nk_ui_screen_id nk_ui_current_screen(void) {
    return s_ui.depth > 0 ? s_ui.stack[s_ui.depth - 1].screen : NK_UI_HM01;
}

nk_ui_state nk_ui_current_state(void) {
    return s_ui.depth > 0 ? s_ui.stack[s_ui.depth - 1].state : NK_UI_ST_NONE;
}

/* ---------------------------------------------------------------- ui/ の中で使う関数 */

void nk_ui_rebuild(void) {
    request(nk_ui_current_screen(), nk_ui_current_state(), NAV_REBUILD);
}

void nk_ui_push(nk_ui_screen_id screen, nk_ui_state state) {
    request(screen, state, NAV_PUSH);
}

void nk_ui_replace(nk_ui_screen_id screen, nk_ui_state state) {
    request(screen, state, NAV_REPLACE);
}

void nk_ui_go_tab(nk_ui_screen_id screen) {
    request(screen, NK_UI_ST_NONE, NAV_TAB);
}

static void nav_click_cb(lv_event_t *e) {
    uintptr_t packed = (uintptr_t)lv_event_get_user_data(e);
    nk_ui_screen_id screen = (nk_ui_screen_id)(packed & 0xFFu);
    nk_ui_state state = (nk_ui_state)((packed >> 8) & 0xFFu);
    int how = (int)((packed >> 16) & 0xFFu);
    if (how == NK_UI_NAV_TAB) {
        request(screen, state, NAV_TAB);
    } else if (how == NK_UI_NAV_REPLACE) {
        request(screen, state, NAV_REPLACE);
    } else {
        request(screen, state, NAV_PUSH);
    }
}

void nk_ui_on_click_nav(lv_obj_t *obj, nk_ui_screen_id screen, nk_ui_state state, int how) {
    uintptr_t packed = (uintptr_t)screen | ((uintptr_t)state << 8) | ((uintptr_t)how << 16);
    lv_obj_add_event_cb(obj, nav_click_cb, LV_EVENT_CLICKED, (void *)packed);
}

static void apply_font_scale_cb(void *user_data) {
    float scale = (float)(intptr_t)user_data / 100.0f;
    nk_ui_widgets_reset();
    tabbar_destroy();
    nk_theme_set_font_scale(scale);
    tabbar_create();
    s_ui.pending = false;
    nav_entry cur = {nk_ui_current_screen(), nk_ui_current_state()};
    bool instant = s_ui.config.instant;
    s_ui.config.instant = true;
    show(cur, NAV_REBUILD);
    s_ui.config.instant = instant;
    nk_theme_release_retired();
}

void nk_ui_apply_font_scale(float scale) {
    s_ui.config.font_scale = scale;
    (void)lv_async_call(apply_font_scale_cb, (void *)(intptr_t)(scale * 100.0f + 0.5f));
}

bool nk_ui_reduce_motion(void) {
    return s_ui.config.reduce_motion || s_ui.config.instant;
}

void nk_ui_set_reduce_motion(bool on) {
    s_ui.config.reduce_motion = on;
}

bool nk_ui_instant(void) {
    return s_ui.config.instant;
}
