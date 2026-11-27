#ifndef NK_DATA_SESSION_H
#define NK_DATA_SESSION_H

#include <stdbool.h>
#include <stdint.h>

/* History refers to finished AI-1 session rows written by the acquisition code.
 * Use data.h/session.h for insertion. Scores belong to the analysis component. */
int nk_data_session_count(void);
bool nk_data_session_exists(int64_t session_id);
bool nk_data_session_delete_by_id(int64_t session_id);
bool nk_data_session_delete_all(void);

// 保存済みの睡眠セッションを新しい順で最大 max_count 件取得し、取得件数を返す（エラー時は -1）
int nk_data_session_list(nk_sleep_session_t *out_list, int max_count);
#endif
