#ifndef NK_SESSION_H
#define NK_SESSION_H

#include "data.h"
#include "result.h"

#include <stdint.h>

typedef struct nk_session nk_session;

/*
 * session管理オブジェクトを作成する。
 *
 * 成功時に返される*out_sessionは、
 * 呼び出し側がnk_session_destroy()で解放する。
 *
 * dataの所有権は移動しない。
 * sessionを破棄するまでdataを閉じてはいけない。
 */
nk_result nk_session_create(nk_session **out_session, nk_data *data);

/*
 * session管理オブジェクトを破棄する。
 * nk_data自体は閉じない。
 */
void nk_session_destroy(nk_session *session);

/*
 * 同意を確認してsessionを開始する。
 *
 * 同意がなければNK_ERROR_CONSENT_REQUIREDを返し、
 * DBへsessionを保存しない。
 */
nk_result nk_session_start(nk_session *session, const nk_session_record *record);

/*
 * sessionを終了する。
 *
 * night_dateはYYYY-MM-DD形式。
 * has_battery_endが0ならbattery_endはNULLとして保存する。
 */
nk_result nk_session_finish(nk_session *session, int64_t end_ms, const char *night_date,
                            nk_session_end_reason end_reason, int has_battery_end, int battery_end);

/*
 * 計測中のsessionへ30秒epochを保存する。
 *
 * epochのsession_idは、
 * 現在計測中のsession IDへ置き換えて保存する。
 */
nk_result nk_session_save_epoch(nk_session *session, const nk_epoch_record *epoch);

/*
 * 計測中のsessionへイベントを保存する。
 *
 * eventのsession_idは、
 * 現在計測中のsession IDへ置き換えて保存する。
 */
nk_result nk_session_save_event(nk_session *session, const nk_event_record *event);

/*
 * 計測中なら1、それ以外は0を返す。
 */
int nk_session_is_running(const nk_session *session);

/*
 * 計測中のsession IDを返す。
 * 計測中でなければ0を返す。
 */
int64_t nk_session_id(const nk_session *session);

#endif
