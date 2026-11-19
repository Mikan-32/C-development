/*
 * LVGL の内部の確認（LV_ASSERT）に失敗したときの動き。
 * lv_conf.h の LV_ASSERT_CUSTOM_INCLUDE で読み込む。
 * 既定の無限ループだと、テストや PC 版が止まったままになるため、異常終了させる
 */
#ifndef NK_LV_ASSERT_H
#define NK_LV_ASSERT_H

#include <stdlib.h>

#define LV_ASSERT_HANDLER abort();

#endif /* NK_LV_ASSERT_H */
