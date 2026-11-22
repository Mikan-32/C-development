#ifndef NK_WIDGETS_H
#define NK_WIDGETS_H

#include "nk_ui_store.h"

/* 1. nk_banner: 状態の帯（必要なときだけ・1行＋次の行動） */
lv_obj_t *nk_banner(lv_obj_t *parent, const char *text, bool warn);

/* 2. nk_hypnogram: 睡眠段階の帯（LG-01 概要 / LG-02 詳細・欠損点線枠＋イベント印） */
lv_obj_t *nk_hypnogram_band(lv_obj_t *parent, const nk_sleep_record *rec);
lv_obj_t *nk_hypnogram_detail(lv_obj_t *parent, const nk_sleep_record *rec);

/* 3. nk_rhythm: 就寝〜起床時刻のリズム帯グラフ（UX-4 1章 #5） */
lv_obj_t *nk_rhythm(lv_obj_t *parent, const nk_sleep_record *const *week_days, int day_count);

/* 4. nk_advice_card: AIフィードバック・アドバイスカード（UX-3 #7 / UX-4 #6） */
lv_obj_t *nk_advice_card(lv_obj_t *parent, const char *caption, const char *text,
                         bool show_all_link);

/* サブ画面・モーダル表示（ST-03 / AD-01 / NT-04 / ST-02 / ST-04 / 気分記録 / 削除確認） */
void nk_modal_close(void);
bool nk_modal_is_open(void);
void nk_open_help_st03(void);
void nk_open_advice_list_ad01(const nk_sleep_record *rec);
void nk_open_morning_summary_nt04(void);
void nk_open_mood_picker(void);
void nk_open_delete_confirm(void);
void nk_open_privacy_modal(void);
void nk_open_license_modal(void);

#endif /* NK_WIDGETS_H */
