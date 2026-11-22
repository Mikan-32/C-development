#include "nk_widgets.h"

static lv_point_precise_t s_detail_stage_pts[32];
static lv_obj_t *s_modal_root = NULL;

static void axis_lbl(lv_obj_t *parent, const char *text, int32_t x, int32_t y, int edge) {
    lv_obj_t *label = nk_text(parent, text, NK_FS_CAPTION, false, NK_INK_3);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_translate_x(label, LV_PCT(edge < 0 ? 0 : edge > 0 ? -100 : -50), 0);
}

static void draw_hour_axis(lv_obj_t *parent, int32_t x, int32_t y, int32_t w) {
    static const char *const k_hours[] = {"23時", "0時", "1時", "2時", "3時",
                                          "4時",  "5時", "6時", "7時"};
    for (int i = 0; i <= 8; i++) {
        int edge = (i == 0 && x == 0) ? -1 : i == 8 ? 1 : 0;
        axis_lbl(parent, k_hours[i], x + i * w / 8, y, edge);
    }
}

static lv_color_t marker_color(nk_marker_type type) {
    switch (type) {
    case NK_MARKER_TURN:
        return NK_SERIES;
    case NK_MARKER_SNORE:
        return NK_SERIOUS;
    case NK_MARKER_NOISE:
        return NK_WARN;
    case NK_MARKER_SOUND:
        return NK_LIGHT;
    case NK_MARKER_CURTAIN:
        return NK_GOOD;
    case NK_MARKER_ALARM:
    default:
        return NK_BRAND;
    }
}

/* ---------------------------------------------------------------- nk_banner */

lv_obj_t *nk_banner(lv_obj_t *parent, const char *text, bool warn) {
    lv_obj_t *bar = nk_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(bar, NK_CARD_2, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 10, 0);
    lv_obj_set_style_pad_all(bar, 10, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, warn ? NK_WARN : NK_GOOD, 0);

    lv_obj_t *dot = nk_rect(bar, 8, 8, warn ? NK_WARN : NK_GOOD, LV_RADIUS_CIRCLE);
    (void)dot;
    lv_obj_t *lbl = nk_wrap_text(bar, text, NK_FS_SMALL, false, NK_INK);
    lv_obj_set_flex_grow(lbl, 1);
    return bar;
}

/* ---------------------------------------------------------------- nk_hypnogram */

lv_obj_t *nk_hypnogram_band(lv_obj_t *parent, const nk_sleep_record *rec) {
    const int32_t w = NK_CARD_INNER_W;
    lv_obj_t *area = nk_rect(parent, w, 48, NK_CARD, 0);
    lv_obj_set_style_bg_opa(area, LV_OPA_TRANSP, 0);

    lv_obj_t *band = nk_rect(area, w, 28, NK_CARD_2, 4);
    if (rec != NULL) {
        int32_t cur = rec->in_bed_from;
        for (int i = 0; i < rec->segment_count; i++) {
            int32_t x0 = cur * w / NK_AXIS_MINUTES;
            int32_t x1 = (cur + rec->segments[i].minutes) * w / NK_AXIS_MINUTES;
            if (x1 > x0) {
                lv_obj_t *seg =
                    nk_rect(band, x1 - x0, 28, nk_stage_color(rec->segments[i].stage), 0);
                lv_obj_set_pos(seg, x0, 0);
            }
            cur += rec->segments[i].minutes;
        }
        /* 欠損区間（取得なし）を点線枠で重ねて表示 */
        if (rec->gap_from_min >= 0 && rec->gap_to_min > rec->gap_from_min) {
            int32_t gx0 = rec->gap_from_min * w / NK_AXIS_MINUTES;
            int32_t gx1 = rec->gap_to_min * w / NK_AXIS_MINUTES;
            if (gx1 > gx0) {
                lv_obj_t *gap = nk_rect(band, gx1 - gx0, 28, NK_CARD, 2);
                lv_obj_set_pos(gap, gx0, 0);
                lv_obj_set_style_border_width(gap, 1, 0);
                lv_obj_set_style_border_color(gap, NK_INK_3, 0);
            }
        }
    }
    draw_hour_axis(area, 0, 32, w);
    return area;
}

lv_obj_t *nk_hypnogram_detail(lv_obj_t *parent, const nk_sleep_record *rec) {
    static const char *const k_levels[] = {"覚醒", "浅い", "深い"};
    const int32_t left = 36;
    const int32_t w = NK_CARD_INNER_W - left;
    const int32_t level_y[] = {24, 58, 92};

    lv_obj_t *area = nk_rect(parent, NK_CARD_INNER_W, 126, NK_CARD, 0);
    lv_obj_set_style_bg_opa(area, LV_OPA_TRANSP, 0);

    for (int i = 0; i < 3; i++) {
        lv_obj_t *lbl = nk_text(area, k_levels[i], NK_FS_CAPTION, false, NK_INK_3);
        lv_obj_set_pos(lbl, 0, level_y[i] - 8);
        lv_obj_set_pos(nk_rect(area, w, 1, NK_CARD_2, 0), left, level_y[i]);
    }

    if (rec != NULL) {
        /* 帯の上のイベント印（寝返り・いびき・大きな音・サウンド・カーテン・アラーム） */
        for (int i = 0; i < rec->marker_count; i++) {
            int32_t mx = left + rec->markers[i].minute_from_23 * w / NK_AXIS_MINUTES;
            lv_obj_t *dot =
                nk_rect(area, 8, 8, marker_color(rec->markers[i].type), LV_RADIUS_CIRCLE);
            lv_obj_set_pos(dot, mx - 4, 4);
        }

        /* 睡眠段階の推移線 */
        int32_t cur = rec->in_bed_from;
        int pts = 0;
        for (int i = 0; i < rec->segment_count && pts + 2 <= 32; i++) {
            int32_t y = level_y[rec->segments[i].stage];
            s_detail_stage_pts[pts].x = left + cur * w / NK_AXIS_MINUTES;
            s_detail_stage_pts[pts].y = y;
            cur += rec->segments[i].minutes;
            s_detail_stage_pts[pts + 1].x = left + cur * w / NK_AXIS_MINUTES;
            s_detail_stage_pts[pts + 1].y = y;
            pts += 2;
        }
        lv_obj_t *line = lv_line_create(area);
        lv_line_set_points(line, s_detail_stage_pts, (uint32_t)pts);
        lv_obj_set_style_line_width(line, 3, 0);
        lv_obj_set_style_line_color(line, NK_SERIES, 0);

        /* 欠損区間がある場合は「取得なし」枠を重ねる */
        if (rec->gap_from_min >= 0 && rec->gap_to_min > rec->gap_from_min) {
            int32_t gx0 = left + rec->gap_from_min * w / NK_AXIS_MINUTES;
            int32_t gx1 = left + rec->gap_to_min * w / NK_AXIS_MINUTES;
            if (gx1 > gx0) {
                lv_obj_t *gap = nk_rect(area, gx1 - gx0, 76, NK_CARD, 4);
                lv_obj_set_pos(gap, gx0, 20);
                lv_obj_set_style_border_width(gap, 1, 0);
                lv_obj_set_style_border_color(gap, NK_INK_3, 0);
                lv_obj_center(nk_text(gap, "取得なし", NK_FS_CAPTION, false, NK_INK_3));
            }
        }
    }

    draw_hour_axis(area, left, 108, w);
    return area;
}

/* ---------------------------------------------------------------- nk_rhythm */

lv_obj_t *nk_rhythm(lv_obj_t *parent, const nk_sleep_record *const *week_days, int day_count) {
    static const char *const k_wd[] = {"月", "火", "水", "木", "金", "土", "日"};
    const int32_t left = 36;
    const int32_t w = NK_CARD_INNER_W - left;
    const int32_t h = 84; /* 21:00〜翌9:00（720分） */

    lv_obj_t *area = nk_rect(parent, NK_CARD_INNER_W, h + 22, NK_CARD, 0);
    lv_obj_set_style_bg_opa(area, LV_OPA_TRANSP, 0);

    axis_lbl(area, "21時", 0, 0, -1);
    axis_lbl(area, "3時", 0, h / 2 - 6, -1);
    axis_lbl(area, "9時", 0, h - 12, -1);

    for (int i = 0; i < day_count && i < 7; i++) {
        int32_t cx = left + (2 * i + 1) * w / 14;
        axis_lbl(area, k_wd[i], cx, h + 6, 0);

        const nk_sleep_record *rec = week_days != NULL ? week_days[i] : NULL;
        if (rec == NULL || rec->quality == NK_QUALITY_FAILED) {
            continue;
        }
        int32_t y0 = rec->bed_min_from_21 * h / 720;
        int32_t y1 = rec->wake_min_from_21 * h / 720;
        if (y0 < 0) {
            y0 = 0;
        }
        if (y1 > h) {
            y1 = h;
        }
        if (y1 > y0) {
            lv_obj_t *bar = nk_rect(area, 16, y1 - y0, NK_SERIES, 6);
            lv_obj_set_pos(bar, cx - 8, y0);
            if (rec->quality == NK_QUALITY_INSUFFICIENT) {
                lv_obj_set_style_bg_opa(bar, LV_OPA_50, 0);
            }
        }
    }
    return area;
}

/* ---------------------------------------------------------------- nk_advice_card */

static void on_open_advice_clicked(lv_event_t *e) {
    (void)e;
    nk_open_advice_list_ad01(nk_ui_store_latest());
}

lv_obj_t *nk_advice_card(lv_obj_t *parent, const char *caption, const char *text,
                         bool show_all_link) {
    lv_obj_t *card = nk_card(parent);
    lv_obj_t *head = nk_box(card, LV_FLEX_FLOW_ROW);
    lv_obj_t *cap = nk_text(head, caption, NK_FS_SMALL, false, NK_INK_3);
    lv_obj_set_flex_grow(cap, 1);

    if (show_all_link) {
        lv_obj_t *link = nk_text(head, "すべて見る >", NK_FS_CAPTION, true, NK_BRAND);
        lv_obj_set_clickable(link, true);
        lv_obj_set_ext_click_area(link, 12);
        lv_obj_add_event_cb(link, on_open_advice_clicked, LV_EVENT_CLICKED, NULL);
    }

    nk_wrap_text(card, text, NK_FS_BODY + 1, true, NK_INK);
    return card;
}

/* ---------------------------------------------------------------- モーダル・サブ画面 */

static void on_close_modal_clicked(lv_event_t *e) {
    (void)e;
    nk_modal_close();
}

void nk_modal_close(void) {
    if (s_modal_root != NULL) {
        lv_obj_del(s_modal_root);
        s_modal_root = NULL;
    }
}

bool nk_modal_is_open(void) {
    return s_modal_root != NULL;
}

static lv_obj_t *create_modal_sheet(const char *title_text) {
    nk_modal_close();
    s_modal_root = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_modal_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_modal_root, NK_BG, 0);
    lv_obj_set_style_bg_opa(s_modal_root, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_modal_root, 0, 0);
    lv_obj_set_style_pad_all(s_modal_root, NK_PAGE_PAD, 0);
    lv_obj_set_flex_flow(s_modal_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_modal_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *panel = nk_card(s_modal_root);
    nk_text(panel, title_text, NK_FS_TITLE, true, NK_INK);
    return panel;
}

void nk_open_help_st03(void) {
    lv_obj_t *panel = create_modal_sheet("睡眠段階とスコアの見方（ST-03）");
    nk_wrap_text(panel,
                 "・深い睡眠：体と脳をしっかり休める時間帯です（推定値）。\n"
                 "・浅い睡眠：眠りの大半を占める標準的な睡眠段階です。\n"
                 "・覚醒：夜中に目が覚めた、または寝返りが多い時間帯です。\n"
                 "・点線の枠：アプリ停止やセンサー未取得だった区間です。",
                 NK_FS_SMALL, false, NK_INK_2);
    lv_obj_t *close_btn = nk_button(panel, "閉じる", true);
    lv_obj_add_event_cb(close_btn, on_close_modal_clicked, LV_EVENT_CLICKED, NULL);
}

void nk_open_advice_list_ad01(const nk_sleep_record *rec) {
    lv_obj_t *panel = create_modal_sheet("今日のアドバイス一覧（AD-01）");
    if (rec != NULL) {
        nk_text(panel, "根拠", NK_FS_CAPTION, false, NK_INK_3);
        nk_wrap_text(panel, rec->advice_basis, NK_FS_BODY, false, NK_INK);
        nk_text(panel, "提案（睡眠習慣）", NK_FS_CAPTION, false, NK_INK_3);
        nk_wrap_text(panel, rec->advice_habit, NK_FS_BODY, false, NK_INK);
        nk_text(panel, "提案（カーテン設定）", NK_FS_CAPTION, false, NK_INK_3);
        nk_wrap_text(panel, rec->advice_curtain, NK_FS_BODY, false, NK_INK);
    } else {
        nk_wrap_text(panel, "今夜おやすみを記録すると、翌朝からアドバイスが表示されます。",
                     NK_FS_BODY, false, NK_INK_2);
    }
    lv_obj_t *close_btn = nk_button(panel, "ホームへ戻る", true);
    lv_obj_add_event_cb(close_btn, on_close_modal_clicked, LV_EVENT_CLICKED, NULL);
}

static void on_mood_choice_clicked(lv_event_t *e) {
    int mood = (int)(intptr_t)lv_event_get_user_data(e);
    nk_ui_store_set_mood(mood);
    nk_modal_close();
    nk_ui_open(nk_ui_current());
}

void nk_open_morning_summary_nt04(void) {
    lv_obj_t *panel = create_modal_sheet("おはようございます（NT-04）");
    nk_wrap_text(panel, "計測を終えました。今朝の目覚めはいかがですか？（飛ばすこともできます）",
                 NK_FS_BODY, false, NK_INK_2);
    lv_obj_t *row = nk_box(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, 0);

    static const char *const k_moods[3] = {"すっきり", "ふつう", "ねむい"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = nk_button(row, k_moods[i], false);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_event_cb(b, on_mood_choice_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
    }

    lv_obj_t *skip_btn = nk_button(panel, "ホームを見る", true);
    lv_obj_add_event_cb(skip_btn, on_close_modal_clicked, LV_EVENT_CLICKED, NULL);
}

void nk_open_mood_picker(void) {
    lv_obj_t *panel = create_modal_sheet("目覚めの気分を記録");
    nk_wrap_text(panel, "今朝の目覚めの気分を選べます（スコアには影響しません）。", NK_FS_SMALL,
                 false, NK_INK_2);
    lv_obj_t *row = nk_box(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, 0);

    static const char *const k_moods[3] = {"すっきり", "ふつう", "ねむい"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = nk_button(row, k_moods[i], false);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_event_cb(b, on_mood_choice_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
    }

    lv_obj_t *cancel_btn = nk_button(panel, "キャンセル", false);
    lv_obj_add_event_cb(cancel_btn, on_close_modal_clicked, LV_EVENT_CLICKED, NULL);
}

static void on_confirm_delete_clicked(lv_event_t *e) {
    (void)e;
    nk_ui_store_delete_selected();
    nk_modal_close();
    nk_ui_open(NK_UI_RECORD_DAY);
}

void nk_open_delete_confirm(void) {
    lv_obj_t *panel = create_modal_sheet("この夜の記録を削除しますか？");
    nk_wrap_text(panel, "削除すると元に戻せません。この夜の睡眠スコアと詳細データが消去されます。",
                 NK_FS_SMALL, false, NK_INK_2);

    lv_obj_t *del_btn = nk_button(panel, "削除する", true);
    lv_obj_add_event_cb(del_btn, on_confirm_delete_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_btn = nk_button(panel, "キャンセル", false);
    lv_obj_add_event_cb(cancel_btn, on_close_modal_clicked, LV_EVENT_CLICKED, NULL);
}

static void on_delete_all_records_clicked(lv_event_t *e) {
    (void)e;
    while (nk_ui_store_count() > 0) {
        nk_ui_store_delete_selected();
    }
    nk_modal_close();
    nk_ui_open(NK_UI_HOME);
}

void nk_open_privacy_modal(void) {
    lv_obj_t *panel = create_modal_sheet("データとプライバシー（ST-02）");
    nk_wrap_text(panel,
                 "・生の音声は端末内で解析後すぐに破棄され、外部へ送信されません。\n"
                 "・保存された睡眠記録はいつでも削除できます。",
                 NK_FS_SMALL, false, NK_INK_2);

    lv_obj_t *del_all = nk_button(panel, "すべての記録を削除する", false);
    lv_obj_add_event_cb(del_all, on_delete_all_records_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_btn = nk_button(panel, "閉じる", true);
    lv_obj_add_event_cb(close_btn, on_close_modal_clicked, LV_EVENT_CLICKED, NULL);
}

void nk_open_license_modal(void) {
    lv_obj_t *panel = create_modal_sheet("ライセンス表示（ST-04）");
    nk_wrap_text(panel,
                 "・LVGL (MIT License)\n"
                 "・SDL3 (zlib License)\n"
                 "・SQLite / SQLCipher (Public Domain / BSD-3-Clause)\n"
                 "・Noto Sans JP (SIL Open Font License 1.1)",
                 NK_FS_SMALL, false, NK_INK_2);
    lv_obj_t *close_btn = nk_button(panel, "閉じる", true);
    lv_obj_add_event_cb(close_btn, on_close_modal_clicked, LV_EVENT_CLICKED, NULL);
}
