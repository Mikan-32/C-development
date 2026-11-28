#include "curtain.h"

#include "nk_db.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOKEN_MAX  256
#define SECRET_MAX 256

typedef struct {
    SDL_Mutex *lock;
    nk_curtain_api *api;
    char token[TOKEN_MAX];
    char secret[SECRET_MAX];
    nk_curtain_snapshot snap;

    int poll_interval_ms;
    int confirm_timeout_ms;
    int retry1_delay_ms;
    int retry2_delay_ms;
    bool worker_busy;
} curtain_ctx;

typedef struct {
    nk_curtain_api_command cmd;
    int target_open_pct;
    bool is_nudge;
} worker_job;

static curtain_ctx s_ctx;
static bool s_initialized = false;
static nk_curtain_platform_schedule_fn s_platform_sched_fn = NULL;

static void copy_str(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    (void)snprintf(dst, dst_size, "%s", src);
}

static int clamp_pct(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 100) {
        return 100;
    }
    return v;
}

static void mask_tail4(const char *raw, char out_masked[16]) {
    if (raw == NULL || raw[0] == '\0') {
        copy_str(out_masked, 16, "未設定");
        return;
    }
    size_t len = strlen(raw);
    if (len <= 4) {
        (void)snprintf(out_masked, 16, "…%s", raw);
    } else {
        (void)snprintf(out_masked, 16, "…%s", &raw[len - 4]);
    }
}

static void format_hhmm_now(char out_buf[16]) {
    time_t now = time(NULL);
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
#if defined(_WIN32)
    (void)localtime_s(&tm_val, &now);
#else
    const struct tm *ptm = localtime(&now);
    if (ptm != NULL) {
        tm_val = *ptm;
    }
#endif
    (void)snprintf(out_buf, 16, "%d:%02d", tm_val.tm_hour, tm_val.tm_min);
}

static void push_log_locked(const char *message, bool is_error) {
    char now_hm[16];
    format_hhmm_now(now_hm);

    char when_lbl[24];
    (void)snprintf(when_lbl, sizeof(when_lbl), "今日 %s", now_hm);

    int limit = s_ctx.snap.log_count < NK_CURTAIN_MAX_LOGS ? s_ctx.snap.log_count
                                                           : (NK_CURTAIN_MAX_LOGS - 1);
    for (int i = limit; i > 0; i--) {
        s_ctx.snap.logs[i] = s_ctx.snap.logs[i - 1];
    }
    copy_str(s_ctx.snap.logs[0].when_text, sizeof(s_ctx.snap.logs[0].when_text), when_lbl);
    copy_str(s_ctx.snap.logs[0].message, sizeof(s_ctx.snap.logs[0].message), message);
    s_ctx.snap.logs[0].is_error = is_error;
    if (s_ctx.snap.log_count < NK_CURTAIN_MAX_LOGS) {
        s_ctx.snap.log_count++;
    }
}

static void format_api_failure_msg(nk_curtain_api_error err, bool opening, char *out_buf,
                                   size_t buf_size) {
    const char *verb = opening ? "開けられませんでした" : "閉められませんでした";
    switch (err) {
    case NK_CURTAIN_API_ERR_NETWORK:
        (void)snprintf(out_buf, buf_size, "%s：インターネットにつながりません", verb);
        break;
    case NK_CURTAIN_API_ERR_AUTH:
        (void)snprintf(out_buf, buf_size,
                       "%s：接続の設定を確かめてください（トークン・シークレット・端末の時刻）",
                       verb);
        break;
    case NK_CURTAIN_API_ERR_RATE_LIMIT:
        (void)snprintf(out_buf, buf_size, "%s：今日の操作回数の上限に達しました", verb);
        break;
    case NK_CURTAIN_API_ERR_DEVICE:
    case NK_CURTAIN_API_ERR_PARSE:
    default:
        (void)snprintf(out_buf, buf_size,
                       "%s：カーテンが応答しません（ハブがオフラインの可能性があります）", verb);
        break;
    }
}

static void ensure_db_table(void) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return;
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS curtain_config ("
                      "    id INTEGER PRIMARY KEY CHECK (id = 1),"
                      "    token TEXT NOT NULL,"
                      "    secret TEXT NOT NULL,"
                      "    device_id TEXT NOT NULL,"
                      "    device_name TEXT NOT NULL,"
                      "    base_url TEXT NOT NULL,"
                      "    open_enabled INTEGER NOT NULL,"
                      "    open_hour INTEGER NOT NULL,"
                      "    open_min INTEGER NOT NULL,"
                      "    close_enabled INTEGER NOT NULL,"
                      "    close_hour INTEGER NOT NULL,"
                      "    close_min INTEGER NOT NULL,"
                      "    repeat_rule INTEGER NOT NULL"
                      ");";
    (void)sqlite3_exec(db, sql, NULL, NULL, NULL);
}

static void save_to_db_locked(void) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return;
    }
    ensure_db_table();

    const char *sql =
        "INSERT OR REPLACE INTO curtain_config ("
        "id, token, secret, device_id, device_name, base_url, "
        "open_enabled, open_hour, open_min, close_enabled, close_hour, close_min, repeat_rule"
        ") VALUES (1, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, s_ctx.token, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s_ctx.secret, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s_ctx.snap.device_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, s_ctx.snap.device_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, s_ctx.snap.base_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, s_ctx.snap.schedule.open_enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 7, s_ctx.snap.schedule.open_hour);
    sqlite3_bind_int(stmt, 8, s_ctx.snap.schedule.open_min);
    sqlite3_bind_int(stmt, 9, s_ctx.snap.schedule.close_enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 10, s_ctx.snap.schedule.close_hour);
    sqlite3_bind_int(stmt, 11, s_ctx.snap.schedule.close_min);
    sqlite3_bind_int(stmt, 12, (int)s_ctx.snap.schedule.repeat);

    (void)sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void load_from_db_locked(void) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return;
    }
    ensure_db_table();

    const char *sql =
        "SELECT token, secret, device_id, device_name, base_url, "
        "open_enabled, open_hour, open_min, close_enabled, close_hour, close_min, repeat_rule "
        "FROM curtain_config WHERE id = 1;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *tok = sqlite3_column_text(stmt, 0);
        const unsigned char *sec = sqlite3_column_text(stmt, 1);
        const unsigned char *did = sqlite3_column_text(stmt, 2);
        const unsigned char *dnm = sqlite3_column_text(stmt, 3);
        const unsigned char *url = sqlite3_column_text(stmt, 4);

        copy_str(s_ctx.token, sizeof(s_ctx.token), (const char *)tok);
        copy_str(s_ctx.secret, sizeof(s_ctx.secret), (const char *)sec);
        copy_str(s_ctx.snap.device_id, sizeof(s_ctx.snap.device_id), (const char *)did);
        copy_str(s_ctx.snap.device_name, sizeof(s_ctx.snap.device_name), (const char *)dnm);
        if (url != NULL && url[0] != '\0') {
            copy_str(s_ctx.snap.base_url, sizeof(s_ctx.snap.base_url), (const char *)url);
            (void)nk_curtain_api_set_base_url(s_ctx.api, s_ctx.snap.base_url);
        }

        s_ctx.snap.schedule.open_enabled = sqlite3_column_int(stmt, 5) != 0;
        s_ctx.snap.schedule.open_hour = sqlite3_column_int(stmt, 6);
        s_ctx.snap.schedule.open_min = sqlite3_column_int(stmt, 7);
        s_ctx.snap.schedule.close_enabled = sqlite3_column_int(stmt, 8) != 0;
        s_ctx.snap.schedule.close_hour = sqlite3_column_int(stmt, 9);
        s_ctx.snap.schedule.close_min = sqlite3_column_int(stmt, 10);
        s_ctx.snap.schedule.repeat = (nk_curtain_repeat_rule)sqlite3_column_int(stmt, 11);

        mask_tail4(s_ctx.token, s_ctx.snap.masked_token);
        mask_tail4(s_ctx.secret, s_ctx.snap.masked_secret);
        (void)nk_curtain_api_set_credentials(s_ctx.api, s_ctx.token, s_ctx.secret);

        if (s_ctx.token[0] != '\0' && s_ctx.snap.device_id[0] != '\0') {
            s_ctx.snap.conn_state = NK_CURTAIN_CONN_CONNECTED;
        }
    }
    sqlite3_finalize(stmt);
}

static nk_result execute_command_with_poll(nk_curtain_api_command cmd, int target_open_pct,
                                           bool is_auto) {
    char device_id[NK_CURTAIN_API_ID_MAX];
    int poll_ms = 3000;
    int timeout_ms = 45000;
    bool opening = (cmd == NK_CURTAIN_API_CMD_OPEN) ||
                   (cmd == NK_CURTAIN_API_CMD_SET_POSITION && target_open_pct >= 50);

    SDL_LockMutex(s_ctx.lock);
    if (s_ctx.snap.device_id[0] == '\0' || s_ctx.token[0] == '\0') {
        s_ctx.snap.conn_state = NK_CURTAIN_CONN_DISCONNECTED;
        s_ctx.snap.op_state = NK_CURTAIN_OP_FAILED;
        copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message),
                 "カーテンが未接続です。接続設定を確認してください");
        SDL_UnlockMutex(s_ctx.lock);
        return NK_ERROR_PERMISSION_REQUIRED;
    }

    copy_str(device_id, sizeof(device_id), s_ctx.snap.device_id);
    poll_ms = s_ctx.poll_interval_ms;
    timeout_ms = s_ctx.confirm_timeout_ms;

    s_ctx.snap.op_state = NK_CURTAIN_OP_PENDING;
    s_ctx.snap.command_accepted = false;
    s_ctx.snap.target_open_pct = clamp_pct(target_open_pct);
    copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message),
             "カーテンに伝えています");
    SDL_UnlockMutex(s_ctx.lock);

    /* 1. 命令送信 */
    nk_result send_rc =
        nk_curtain_api_send_command(s_ctx.api, device_id, cmd, clamp_pct(target_open_pct));
    if (send_rc != NK_OK) {
        nk_curtain_api_error api_err = nk_curtain_api_last_error(s_ctx.api);
        char err_msg[128];
        format_api_failure_msg(api_err, opening, err_msg, sizeof(err_msg));

        SDL_LockMutex(s_ctx.lock);
        s_ctx.snap.op_state = NK_CURTAIN_OP_FAILED;
        if (api_err == NK_CURTAIN_API_ERR_NETWORK || api_err == NK_CURTAIN_API_ERR_DEVICE) {
            s_ctx.snap.conn_state = NK_CURTAIN_CONN_OFFLINE;
        }
        copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message), err_msg);
        push_log_locked(err_msg, true);
        SDL_UnlockMutex(s_ctx.lock);
        return send_rc;
    }

    /* 2. 命令受付完了 -> 実際の動作をポーリング確認（3秒おき・最大45秒） */
    SDL_LockMutex(s_ctx.lock);
    s_ctx.snap.conn_state = NK_CURTAIN_CONN_CONNECTED;
    s_ctx.snap.command_accepted = true;
    copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message), "動いています");
    SDL_UnlockMutex(s_ctx.lock);

    int elapsed_ms = 0;
    while (elapsed_ms <= timeout_ms) {
        if (elapsed_ms > 0) {
            SDL_Delay((Uint32)poll_ms);
        }

        nk_curtain_api_status st;
        nk_result st_rc = nk_curtain_api_get_status(s_ctx.api, device_id, &st);
        if (st_rc == NK_OK) {
            int diff = st.open_pct - clamp_pct(target_open_pct);
            if (diff < 0) {
                diff = -diff;
            }
            SDL_LockMutex(s_ctx.lock);
            s_ctx.snap.open_pct = st.open_pct;
            format_hhmm_now(s_ctx.snap.last_checked_time);
            if (!st.moving && diff <= 5) {
                s_ctx.snap.op_state = NK_CURTAIN_OP_CONFIRMED;
                char ok_msg[128];
                const char *done_word =
                    (target_open_pct >= 95)
                        ? "開けました"
                        : (target_open_pct <= 5 ? "閉めました" : "位置を合わせました");
                (void)snprintf(ok_msg, sizeof(ok_msg), "%s（%s 確認）", done_word,
                               s_ctx.snap.last_checked_time);
                copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message), ok_msg);

                char log_msg[96];
                (void)snprintf(log_msg, sizeof(log_msg), "%s%s（確認済み）",
                               is_auto ? "予約で" : "手動で", done_word);
                push_log_locked(log_msg, false);
                SDL_UnlockMutex(s_ctx.lock);
                return NK_OK;
            }
            SDL_UnlockMutex(s_ctx.lock);
        }

        elapsed_ms += poll_ms;
    }

    /* 45秒以内に動いたか確認できなければ「未確認」 */
    SDL_LockMutex(s_ctx.lock);
    s_ctx.snap.op_state = NK_CURTAIN_OP_UNCONFIRMED;
    const char *unconf_msg = opening ? "開ける命令は届きましたが、動いたか確認できませんでした"
                                     : "閉める命令は届きましたが、動いたか確認できませんでした";
    copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message), unconf_msg);
    push_log_locked("命令は届きましたが、動作を確認できませんでした（未確認）", true);
    SDL_UnlockMutex(s_ctx.lock);
    return NK_ERROR_IO;
}

static int SDLCALL curtain_worker_thread(void *userdata) {
    worker_job *job = (worker_job *)userdata;
    if (job == NULL) {
        return 0;
    }

    if (job->is_nudge) {
        int orig_pct = 0;
        SDL_LockMutex(s_ctx.lock);
        orig_pct = s_ctx.snap.open_pct;
        SDL_UnlockMutex(s_ctx.lock);

        int nudge_pct = (orig_pct <= 85) ? (orig_pct + 10) : (orig_pct - 10);
        if (execute_command_with_poll(NK_CURTAIN_API_CMD_SET_POSITION, nudge_pct, false) == NK_OK) {
            (void)execute_command_with_poll(NK_CURTAIN_API_CMD_SET_POSITION, orig_pct, false);
        }
    } else {
        (void)execute_command_with_poll(job->cmd, job->target_open_pct, false);
    }

    free(job);
    SDL_LockMutex(s_ctx.lock);
    s_ctx.worker_busy = false;
    SDL_UnlockMutex(s_ctx.lock);
    return 0;
}

static nk_result launch_worker(nk_curtain_api_command cmd, int target_open_pct, bool is_nudge) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }
    SDL_LockMutex(s_ctx.lock);
    if (s_ctx.worker_busy) {
        SDL_UnlockMutex(s_ctx.lock);
        return NK_ERROR_ALREADY_RUNNING;
    }
    worker_job *job = (worker_job *)calloc(1, sizeof(worker_job));
    if (job == NULL) {
        SDL_UnlockMutex(s_ctx.lock);
        return NK_ERROR_MEMORY;
    }
    job->cmd = cmd;
    job->target_open_pct = clamp_pct(target_open_pct);
    job->is_nudge = is_nudge;
    s_ctx.worker_busy = true;
    SDL_UnlockMutex(s_ctx.lock);

    SDL_Thread *th = SDL_CreateThread(curtain_worker_thread, "nk-curtain", job);
    if (th == NULL) {
        free(job);
        SDL_LockMutex(s_ctx.lock);
        s_ctx.worker_busy = false;
        SDL_UnlockMutex(s_ctx.lock);
        return NK_ERROR_IO;
    }
    SDL_DetachThread(th);
    return NK_OK;
}

void nk_curtain_set_platform_scheduler(nk_curtain_platform_schedule_fn fn) {
    s_platform_sched_fn = fn;
}

nk_result nk_curtain_init(void) {
    if (s_initialized) {
        return NK_OK;
    }
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.lock = SDL_CreateMutex();
    if (s_ctx.lock == NULL) {
        return NK_ERROR_MEMORY;
    }
    nk_result rc = nk_curtain_api_create(&s_ctx.api);
    if (rc != NK_OK) {
        SDL_DestroyMutex(s_ctx.lock);
        s_ctx.lock = NULL;
        return rc;
    }

    copy_str(s_ctx.snap.base_url, sizeof(s_ctx.snap.base_url), NK_CURTAIN_API_DEFAULT_URL);
    copy_str(s_ctx.snap.masked_token, sizeof(s_ctx.snap.masked_token), "未設定");
    copy_str(s_ctx.snap.masked_secret, sizeof(s_ctx.snap.masked_secret), "未設定");
    copy_str(s_ctx.snap.last_checked_time, sizeof(s_ctx.snap.last_checked_time), "--:--");
    s_ctx.snap.open_pct = 60;
    s_ctx.snap.target_open_pct = 60;
    s_ctx.snap.schedule.open_enabled = true;
    s_ctx.snap.schedule.open_hour = 6;
    s_ctx.snap.schedule.open_min = 20;
    s_ctx.snap.schedule.close_enabled = false;
    s_ctx.snap.schedule.close_hour = 22;
    s_ctx.snap.schedule.close_min = 30;
    s_ctx.snap.schedule.repeat = NK_CURTAIN_REPEAT_EVERYDAY;

    s_ctx.poll_interval_ms = 3000;
    s_ctx.confirm_timeout_ms = 45000;
    s_ctx.retry1_delay_ms = 15000;
    s_ctx.retry2_delay_ms = 60000;

    load_from_db_locked();
    s_initialized = true;

    if (s_platform_sched_fn != NULL) {
        s_platform_sched_fn(&s_ctx.snap.schedule);
    }
    return NK_OK;
}

void nk_curtain_deinit(void) {
    if (!s_initialized) {
        return;
    }
    volatile unsigned char *tok = (volatile unsigned char *)s_ctx.token;
    volatile unsigned char *sec = (volatile unsigned char *)s_ctx.secret;
    for (size_t i = 0; i < sizeof(s_ctx.token); i++) {
        tok[i] = 0;
        sec[i] = 0;
    }
    nk_curtain_api_destroy(s_ctx.api);
    s_ctx.api = NULL;
    if (s_ctx.lock != NULL) {
        SDL_DestroyMutex(s_ctx.lock);
        s_ctx.lock = NULL;
    }
    s_initialized = false;
}

void nk_curtain_set_timing_ms(int poll_interval_ms, int confirm_timeout_ms, int retry1_delay_ms,
                              int retry2_delay_ms) {
    if (!s_initialized) {
        return;
    }
    SDL_LockMutex(s_ctx.lock);
    if (poll_interval_ms > 0) {
        s_ctx.poll_interval_ms = poll_interval_ms;
    }
    if (confirm_timeout_ms > 0) {
        s_ctx.confirm_timeout_ms = confirm_timeout_ms;
    }
    if (retry1_delay_ms >= 0) {
        s_ctx.retry1_delay_ms = retry1_delay_ms;
    }
    if (retry2_delay_ms >= 0) {
        s_ctx.retry2_delay_ms = retry2_delay_ms;
    }
    SDL_UnlockMutex(s_ctx.lock);
}

nk_result nk_curtain_set_base_url(const char *base_url) {
    if (!s_initialized || base_url == NULL || base_url[0] == '\0') {
        return NK_ERROR_ARGUMENT;
    }
    SDL_LockMutex(s_ctx.lock);
    nk_result rc = nk_curtain_api_set_base_url(s_ctx.api, base_url);
    if (rc == NK_OK) {
        copy_str(s_ctx.snap.base_url, sizeof(s_ctx.snap.base_url),
                 nk_curtain_api_get_base_url(s_ctx.api));
        save_to_db_locked();
    }
    SDL_UnlockMutex(s_ctx.lock);
    return rc;
}

nk_result nk_curtain_connect(const char *token, const char *secret,
                             nk_curtain_api_device *out_devices, size_t max_devices,
                             size_t *out_count) {
    if (!s_initialized || token == NULL || secret == NULL || token[0] == '\0' ||
        secret[0] == '\0' || out_devices == NULL || max_devices == 0 || out_count == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    SDL_LockMutex(s_ctx.lock);
    copy_str(s_ctx.token, sizeof(s_ctx.token), token);
    copy_str(s_ctx.secret, sizeof(s_ctx.secret), secret);
    mask_tail4(s_ctx.token, s_ctx.snap.masked_token);
    mask_tail4(s_ctx.secret, s_ctx.snap.masked_secret);
    s_ctx.snap.conn_state = NK_CURTAIN_CONN_CHECKING;
    (void)nk_curtain_api_set_credentials(s_ctx.api, s_ctx.token, s_ctx.secret);
    SDL_UnlockMutex(s_ctx.lock);

    size_t count = 0;
    nk_result rc = nk_curtain_api_list_devices(s_ctx.api, out_devices, max_devices, &count);

    SDL_LockMutex(s_ctx.lock);
    if (rc != NK_OK) {
        nk_curtain_api_error err = nk_curtain_api_last_error(s_ctx.api);
        s_ctx.snap.conn_state = (err == NK_CURTAIN_API_ERR_NETWORK) ? NK_CURTAIN_CONN_OFFLINE
                                                                    : NK_CURTAIN_CONN_DISCONNECTED;
        format_api_failure_msg(err, true, s_ctx.snap.status_message,
                               sizeof(s_ctx.snap.status_message));
        SDL_UnlockMutex(s_ctx.lock);
        return rc;
    }

    if (count == 0) {
        s_ctx.snap.conn_state = NK_CURTAIN_CONN_DISCONNECTED;
        copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message),
                 "カーテンが見つかりませんでした（ハブとの連携を確認してください）");
        SDL_UnlockMutex(s_ctx.lock);
        *out_count = 0;
        return NK_ERROR_FORMAT;
    }

    /* 1台目を自動選択して保存 */
    copy_str(s_ctx.snap.device_id, sizeof(s_ctx.snap.device_id), out_devices[0].device_id);
    copy_str(s_ctx.snap.device_name, sizeof(s_ctx.snap.device_name), out_devices[0].device_name);
    s_ctx.snap.conn_state = NK_CURTAIN_CONN_CONNECTED;
    copy_str(s_ctx.snap.status_message, sizeof(s_ctx.snap.status_message),
             "カーテンと接続しました");
    save_to_db_locked();
    SDL_UnlockMutex(s_ctx.lock);

    *out_count = count;
    (void)nk_curtain_refresh_status();
    return NK_OK;
}

nk_result nk_curtain_select_device(const char *device_id, const char *device_name) {
    if (!s_initialized || device_id == NULL || device_id[0] == '\0') {
        return NK_ERROR_ARGUMENT;
    }
    SDL_LockMutex(s_ctx.lock);
    copy_str(s_ctx.snap.device_id, sizeof(s_ctx.snap.device_id), device_id);
    copy_str(s_ctx.snap.device_name, sizeof(s_ctx.snap.device_name),
             device_name != NULL ? device_name : "SwitchBot Curtain");
    s_ctx.snap.conn_state = NK_CURTAIN_CONN_CONNECTED;
    save_to_db_locked();
    SDL_UnlockMutex(s_ctx.lock);
    return nk_curtain_refresh_status();
}

nk_result nk_curtain_disconnect(void) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }
    SDL_LockMutex(s_ctx.lock);
    memset(s_ctx.token, 0, sizeof(s_ctx.token));
    memset(s_ctx.secret, 0, sizeof(s_ctx.secret));
    s_ctx.snap.device_id[0] = '\0';
    s_ctx.snap.device_name[0] = '\0';
    copy_str(s_ctx.snap.masked_token, sizeof(s_ctx.snap.masked_token), "未設定");
    copy_str(s_ctx.snap.masked_secret, sizeof(s_ctx.snap.masked_secret), "未設定");
    s_ctx.snap.conn_state = NK_CURTAIN_CONN_DISCONNECTED;
    s_ctx.snap.op_state = NK_CURTAIN_OP_IDLE;
    s_ctx.snap.status_message[0] = '\0';
    (void)nk_curtain_api_set_credentials(s_ctx.api, "", "");
    save_to_db_locked();
    SDL_UnlockMutex(s_ctx.lock);
    return NK_OK;
}

void nk_curtain_get_snapshot(nk_curtain_snapshot *out_snapshot) {
    if (out_snapshot == NULL) {
        return;
    }
    if (!s_initialized) {
        memset(out_snapshot, 0, sizeof(*out_snapshot));
        return;
    }
    SDL_LockMutex(s_ctx.lock);
    *out_snapshot = s_ctx.snap;
    SDL_UnlockMutex(s_ctx.lock);
}

nk_result nk_curtain_refresh_status(void) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }
    char device_id[NK_CURTAIN_API_ID_MAX];
    SDL_LockMutex(s_ctx.lock);
    if (s_ctx.snap.device_id[0] == '\0') {
        SDL_UnlockMutex(s_ctx.lock);
        return NK_ERROR_PERMISSION_REQUIRED;
    }
    copy_str(device_id, sizeof(device_id), s_ctx.snap.device_id);
    s_ctx.snap.conn_state = NK_CURTAIN_CONN_CHECKING;
    SDL_UnlockMutex(s_ctx.lock);

    nk_curtain_api_status st;
    nk_result rc = nk_curtain_api_get_status(s_ctx.api, device_id, &st);

    SDL_LockMutex(s_ctx.lock);
    if (rc == NK_OK) {
        s_ctx.snap.open_pct = st.open_pct;
        s_ctx.snap.conn_state = NK_CURTAIN_CONN_CONNECTED;
        format_hhmm_now(s_ctx.snap.last_checked_time);
    } else {
        s_ctx.snap.conn_state = NK_CURTAIN_CONN_OFFLINE;
    }
    SDL_UnlockMutex(s_ctx.lock);
    return rc;
}

nk_result nk_curtain_open_sync(void) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }
    return execute_command_with_poll(NK_CURTAIN_API_CMD_OPEN, 100, false);
}

nk_result nk_curtain_close_sync(void) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }
    return execute_command_with_poll(NK_CURTAIN_API_CMD_CLOSE, 0, false);
}

nk_result nk_curtain_set_position_sync(int open_pct) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }
    return execute_command_with_poll(NK_CURTAIN_API_CMD_SET_POSITION, clamp_pct(open_pct), false);
}

nk_result nk_curtain_test_nudge_sync(void) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }
    int orig_pct = 0;
    SDL_LockMutex(s_ctx.lock);
    orig_pct = s_ctx.snap.open_pct;
    SDL_UnlockMutex(s_ctx.lock);

    int nudge_pct = (orig_pct <= 85) ? (orig_pct + 10) : (orig_pct - 10);
    nk_result rc = execute_command_with_poll(NK_CURTAIN_API_CMD_SET_POSITION, nudge_pct, false);
    if (rc != NK_OK) {
        return rc;
    }
    return execute_command_with_poll(NK_CURTAIN_API_CMD_SET_POSITION, orig_pct, false);
}

nk_result nk_curtain_open_async(void) {
    return launch_worker(NK_CURTAIN_API_CMD_OPEN, 100, false);
}

nk_result nk_curtain_close_async(void) {
    return launch_worker(NK_CURTAIN_API_CMD_CLOSE, 0, false);
}

nk_result nk_curtain_set_position_async(int open_pct) {
    return launch_worker(NK_CURTAIN_API_CMD_SET_POSITION, clamp_pct(open_pct), false);
}

nk_result nk_curtain_test_nudge_async(void) {
    return launch_worker(NK_CURTAIN_API_CMD_SET_POSITION, 50, true);
}

nk_result nk_curtain_set_schedule(const nk_curtain_schedule *sched) {
    if (!s_initialized || sched == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    SDL_LockMutex(s_ctx.lock);
    s_ctx.snap.schedule = *sched;
    save_to_db_locked();
    SDL_UnlockMutex(s_ctx.lock);

    if (s_platform_sched_fn != NULL) {
        s_platform_sched_fn(sched);
    }
    return NK_OK;
}

nk_result nk_curtain_execute_scheduled(bool open_action, bool is_weekday) {
    if (!s_initialized) {
        return NK_ERROR_NOT_RUNNING;
    }

    nk_curtain_schedule sched;
    int r1 = 15000;
    int r2 = 60000;
    SDL_LockMutex(s_ctx.lock);
    sched = s_ctx.snap.schedule;
    r1 = s_ctx.retry1_delay_ms;
    r2 = s_ctx.retry2_delay_ms;
    SDL_UnlockMutex(s_ctx.lock);

    if (open_action && !sched.open_enabled) {
        return NK_OK;
    }
    if (!open_action && !sched.close_enabled) {
        return NK_OK;
    }
    if (sched.repeat == NK_CURTAIN_REPEAT_WEEKDAYS && !is_weekday) {
        return NK_OK;
    }

    nk_curtain_api_command cmd = open_action ? NK_CURTAIN_API_CMD_OPEN : NK_CURTAIN_API_CMD_CLOSE;
    int target = open_action ? 100 : 0;

    /* 1回目（即時）、2回目（15秒後）、3回目（60秒後）の再試行（TM-04 No.6） */
    const int delays[3] = {0, r1, r2};
    nk_result last_rc = NK_ERROR_IO;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (delays[attempt] > 0) {
            SDL_Delay((Uint32)delays[attempt]);
        }
        last_rc = execute_command_with_poll(cmd, target, true);
        if (last_rc == NK_OK) {
            return NK_OK;
        }
    }
    return last_rc;
}

int64_t nk_curtain_next_trigger_ms(int hour, int minute, nk_curtain_repeat_rule repeat,
                                   int64_t now_ms) {
    time_t now_sec = (time_t)(now_ms / 1000LL);
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
#if defined(_WIN32)
    (void)localtime_s(&tm_val, &now_sec);
#else
    const struct tm *ptm = localtime(&now_sec);
    if (ptm != NULL) {
        tm_val = *ptm;
    }
#endif

    tm_val.tm_hour = hour;
    tm_val.tm_min = minute;
    tm_val.tm_sec = 0;
    time_t cand = mktime(&tm_val);
    if ((int64_t)cand * 1000LL <= now_ms) {
        tm_val.tm_mday += 1;
        cand = mktime(&tm_val);
    }

    if (repeat == NK_CURTAIN_REPEAT_WEEKDAYS) {
        while (tm_val.tm_wday == 0 || tm_val.tm_wday == 6) {
            tm_val.tm_mday += 1;
            cand = mktime(&tm_val);
        }
    }
    return (int64_t)cand * 1000LL;
}
