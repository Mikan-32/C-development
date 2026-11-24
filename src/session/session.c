#include "session.h"

#include <stdlib.h>
#include <string.h>

struct nk_session {
    nk_data *data;

    int running;
    int64_t session_id;
    int64_t start_ms;

    /*
     * 連続している欠損epochの数。
     */
    int gap_epoch_count;

    /*
     * 欠損が始まったepoch番号。
     */
    int gap_start_idx;

    /*
     * GAP_STARTを保存済みなら1。
     */
    int gap_event_open;

    /*
     * GAPイベントへ保存するAI-2の理由コード。
     *
     * 加速度欠損はQ04、
     * マイク欠損はQ08を使用する。
     */
    const char *gap_reason;
};

static const char *epoch_gap_reason(const nk_epoch_record *epoch) {
    if (epoch == NULL) {
        return NULL;
    }

    /*
     * 加速度欠損は計測停止などのQ04として扱う。
     */
    if ((epoch->flags & NK_EPF_GAP_ACC) != 0U) {
        return "Q04";
    }

    /*
     * マイク欠損またはマイク使用中はQ08。
     */
    if ((epoch->flags & (NK_EPF_GAP_MIC | NK_EPF_MIC_BUSY)) != 0U) {
        return "Q08";
    }

    return NULL;
}

nk_result nk_session_create(nk_session **out_session, nk_data *data) {
    nk_session *session;

    if (out_session == NULL || data == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    *out_session = NULL;

    session = calloc(1, sizeof(*session));
    if (session == NULL) {
        return NK_ERROR_MEMORY;
    }

    session->data = data;

    session->running = 0;
    session->session_id = 0;
    session->start_ms = 0;

    session->gap_epoch_count = 0;
    session->gap_start_idx = 0;
    session->gap_event_open = 0;
    session->gap_reason = NULL;

    *out_session = session;

    return NK_OK;
}

void nk_session_destroy(nk_session *session) {
    if (session == NULL) {
        return;
    }

    /*
     * nk_dataの所有権は呼び出し側にあるため、
     * ここではnk_data_close()を呼ばない。
     */
    memset(session, 0, sizeof(*session));
    free(session);
}

nk_result nk_session_start(nk_session *session, const nk_session_record *record) {
    nk_result data_result;
    int has_consent = 0;
    int64_t session_id = 0;

    if (session == NULL || session->data == NULL || record == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (session->running) {
        return NK_ERROR_ALREADY_RUNNING;
    }

    /*
     * Issue #27の条件：
     * 計測同意がなければ開始しない。
     */
    data_result = nk_data_has_measurement_consent(session->data, &has_consent);

    if (data_result != NK_OK) {
        return NK_ERROR_DATABASE;
    }

    if (!has_consent) {
        return NK_ERROR_CONSENT_REQUIRED;
    }

    /*
     * 同意が確認できた場合だけ、
     * sessionをDBへ保存する。
     */
    data_result = nk_data_session_start(session->data, record, &session_id);

    if (data_result != NK_OK) {
        return NK_ERROR_DATABASE;
    }

    session->session_id = session_id;
    session->start_ms = record->start_ms;
    session->running = 1;

    session->gap_epoch_count = 0;
    session->gap_start_idx = 0;
    session->gap_event_open = 0;
    session->gap_reason = NULL;

    return NK_OK;
}

nk_result nk_session_finish(nk_session *session, int64_t end_ms, const char *night_date,
                            nk_session_end_reason end_reason, int has_battery_end,
                            int battery_end) {
    nk_event_record gap_end_event = {0};
    nk_result data_result;

    int64_t gap_start_ms;

    if (session == NULL || session->data == NULL || night_date == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (!session->running || session->session_id <= 0) {
        return NK_ERROR_NOT_RUNNING;
    }

    /*
     * GAP_STARTを保存済みのまま計測が終了する場合、
     * 終了時刻でGAP_ENDを保存する。
     */
    if (session->gap_event_open) {
        gap_start_ms = session->start_ms + (int64_t)session->gap_start_idx * INT64_C(30000);

        if (end_ms < gap_start_ms) {
            return NK_ERROR_ARGUMENT;
        }

        gap_end_event.session_id = session->session_id;

        gap_end_event.t_ms = end_ms;
        gap_end_event.dur_ms = end_ms - gap_start_ms;

        gap_end_event.type = NK_EVENT_GAP_END;
        gap_end_event.value.has_value = 0;
        gap_end_event.reason = session->gap_reason;

        data_result = nk_data_event_insert(session->data, &gap_end_event);

        if (data_result != NK_OK) {
            return NK_ERROR_DATABASE;
        }

        /*
         * GAP_ENDを二重保存しないよう、
         * 保存成功後に欠損状態を閉じる。
         */
        session->gap_epoch_count = 0;
        session->gap_start_idx = 0;
        session->gap_event_open = 0;
        session->gap_reason = NULL;
    }

    data_result = nk_data_session_finish(session->data, session->session_id, end_ms, night_date,
                                         end_reason, has_battery_end, battery_end);

    if (data_result != NK_OK) {
        return NK_ERROR_DATABASE;
    }

    session->running = 0;
    session->session_id = 0;
    session->start_ms = 0;

    session->gap_epoch_count = 0;
    session->gap_start_idx = 0;
    session->gap_event_open = 0;
    session->gap_reason = NULL;

    return NK_OK;
}

nk_result nk_session_save_epoch(nk_session *session, const nk_epoch_record *epoch) {
    nk_epoch_record record;
    nk_event_record gap_event = {0};

    nk_result data_result;
    const char *gap_reason;

    int64_t gap_start_ms;
    int64_t gap_end_ms;

    if (session == NULL || session->data == NULL || epoch == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (!session->running || session->session_id <= 0) {
        return NK_ERROR_NOT_RUNNING;
    }

    /*
     * 呼び出し側が指定したsession_idではなく、
     * 現在計測中のsession IDを使用する。
     */
    record = *epoch;
    record.session_id = session->session_id;

    /*
     * 先にepoch本体を保存する。
     */
    data_result = nk_data_epoch_insert(session->data, &record);

    if (data_result != NK_OK) {
        return NK_ERROR_DATABASE;
    }

    gap_reason = epoch_gap_reason(&record);

    /*
     * このepochが欠損の場合。
     */
    if (gap_reason != NULL) {
        if (session->gap_epoch_count == 0) {
            /*
             * 最初の欠損epochを記録する。
             * まだGAP_STARTは保存しない。
             */
            session->gap_start_idx = record.idx;
            session->gap_reason = gap_reason;
        }

        session->gap_epoch_count++;

        /*
         * 2エポック連続した時点でGAP_STARTを保存する。
         */
        if (session->gap_epoch_count == 2 && !session->gap_event_open) {
            gap_start_ms = session->start_ms + (int64_t)session->gap_start_idx * INT64_C(30000);

            gap_event.session_id = session->session_id;

            gap_event.t_ms = gap_start_ms;
            gap_event.dur_ms = 0;
            gap_event.type = NK_EVENT_GAP_START;

            gap_event.value.has_value = 0;
            gap_event.reason = session->gap_reason;

            data_result = nk_data_event_insert(session->data, &gap_event);

            if (data_result != NK_OK) {
                /*
                 * 次の欠損epochで再試行できるよう、
                 * 連続数を1へ戻す。
                 */
                session->gap_epoch_count = 1;
                return NK_ERROR_DATABASE;
            }

            session->gap_event_open = 1;
        }

        return NK_OK;
    }

    /*
     * 正常なepochへ戻った場合。
     */
    if (session->gap_event_open) {
        gap_start_ms = session->start_ms + (int64_t)session->gap_start_idx * INT64_C(30000);

        gap_end_ms = session->start_ms + (int64_t)record.idx * INT64_C(30000);

        gap_event.session_id = session->session_id;

        gap_event.t_ms = gap_end_ms;
        gap_event.dur_ms = gap_end_ms - gap_start_ms;

        gap_event.type = NK_EVENT_GAP_END;
        gap_event.value.has_value = 0;
        gap_event.reason = session->gap_reason;

        data_result = nk_data_event_insert(session->data, &gap_event);

        if (data_result != NK_OK) {
            /*
             * GAP_END保存に失敗した場合は、
             * open状態を残して次回再試行する。
             */
            return NK_ERROR_DATABASE;
        }
    }

    /*
     * 欠損状態をリセットする。
     *
     * 欠損が1エポックだけだった場合は、
     * GAP_STARTもGAP_ENDも作成されない。
     */
    session->gap_epoch_count = 0;
    session->gap_start_idx = 0;
    session->gap_event_open = 0;
    session->gap_reason = NULL;

    return NK_OK;
}

nk_result nk_session_save_event(nk_session *session, const nk_event_record *event) {
    nk_event_record record;
    nk_result data_result;

    if (session == NULL || session->data == NULL || event == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (!session->running || session->session_id <= 0) {
        return NK_ERROR_NOT_RUNNING;
    }

    /*
     * 呼び出し側が指定したsession_idをそのまま使わず、
     * 現在計測中のsession IDに置き換える。
     */
    record = *event;
    record.session_id = session->session_id;

    data_result = nk_data_event_insert(session->data, &record);

    if (data_result != NK_OK) {
        return NK_ERROR_DATABASE;
    }

    return NK_OK;
}

int nk_session_is_running(const nk_session *session) {
    if (session == NULL) {
        return 0;
    }

    return session->running;
}

int64_t nk_session_id(const nk_session *session) {
    if (session == NULL || !session->running) {
        return 0;
    }

    return session->session_id;
}
