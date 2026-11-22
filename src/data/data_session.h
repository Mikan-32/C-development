#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t id;
    int64_t started_at;
    int64_t ended_at;
    int sleep_score;
} nk_sleep_session_t;

// 1晩分の睡眠セッションを保存し、採番されたIDを out_id に返す
bool nk_data_session_insert(int64_t started_at, int64_t ended_at, int sleep_score, int64_t *out_id);

// 保存されている睡眠セッションの総件数を取得（エラー時は -1）
int nk_data_session_count(void);

// 指定したIDが存在するか確認
bool nk_data_session_exists(int64_t session_id);

// 【Issue #28 完了条件】1夜の削除（指定したIDの履歴を1件削除）
bool nk_data_session_delete_by_id(int64_t session_id);

// 【Issue #28 完了条件】全削除（すべての履歴を削除）
bool nk_data_session_delete_all(void);

// 保存済みの睡眠セッションを新しい順で最大 max_count 件取得し、取得件数を返す（エラー時は -1）
int nk_data_session_list(nk_sleep_session_t *out_list, int max_count);
