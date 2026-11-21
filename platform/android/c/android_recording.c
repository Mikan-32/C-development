#define _POSIX_C_SOURCE 200809L

#include "android_recording.h"

#include "android_bridge.h"
#include "data_consent.h"
#include "session.h"

#include <SDL3/SDL.h>
#include <android/log.h>
#include <sqlcipher/sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#include <time.h>

#define NK_RECORDING_TAG "NkAndroid"

struct nk_android_recording {
    nk_data *data;
    nk_session *session;
    nk_sensing *sensing;
    char *path;
    int64_t id;
    int64_t start_ms;
    int saved_epochs;
    int saved_events;
    int stop_event_saved;
    int consent_revoked;
    nk_result finish_result;
    int finished;
};

static const char *end_reason_name(nk_session_end_reason reason) {
    switch (reason) {
    case NK_SESSION_END_USER_STOP:
        return "USER_STOP";
    case NK_SESSION_END_ALARM_STOP:
        return "ALARM_STOP";
    case NK_SESSION_END_TIMEOUT_14H:
        return "TIMEOUT_14H";
    case NK_SESSION_END_SERVICE_KILLED:
        return "SERVICE_KILLED";
    case NK_SESSION_END_BATTERY_LOW:
        return "BATTERY_LOW";
    default:
        return "ERROR";
    }
}

static const char *event_name(nk_event_type type) {
    static const char *names[] = {"TURN",        "SNORE",      "NOISE",   "GAP_START", "GAP_END",
                                  "SOUND_START", "SOUND_STOP", "CURTAIN", "ALARM",     "USER"};
    return type >= NK_EVENT_TURN && type <= NK_EVENT_USER ? names[type] : "UNKNOWN";
}

static void log_error(nk_android_recording *recording, const char *operation, nk_result result) {
    (void)__android_log_print(ANDROID_LOG_ERROR, NK_RECORDING_TAG,
                              "Recording %s failed: result=%d detail=%s", operation, (int)result,
                              recording != NULL && recording->data != NULL
                                  ? nk_data_last_error(recording->data)
                                  : "unavailable");
}

static nk_result local_date(int64_t utc_ms, char date[11], int *tz_min) {
    time_t instant = (time_t)(utc_ms / 1000);
    struct tm local;
    char offset[8];
    if (utc_ms < 0 || (int64_t)instant != utc_ms / 1000 || localtime_r(&instant, &local) == NULL ||
        strftime(date, 11, "%Y-%m-%d", &local) != 10) {
        return NK_ERROR_IO;
    }
    if (tz_min != NULL) {
        if (strftime(offset, sizeof(offset), "%z", &local) != 5 ||
            (offset[0] != '+' && offset[0] != '-')) {
            return NK_ERROR_IO;
        }
        for (int i = 1; i <= 4; ++i) {
            if (offset[i] < '0' || offset[i] > '9') {
                return NK_ERROR_IO;
            }
        }
        int hours = (offset[1] - '0') * 10 + offset[2] - '0';
        int minutes = (offset[3] - '0') * 10 + offset[4] - '0';
        if (hours > 23 || minutes > 59) {
            return NK_ERROR_IO;
        }
        *tz_min = (offset[0] == '-' ? -1 : 1) * (hours * 60 + minutes);
    }
    return NK_OK;
}

static nk_result save_epoch(void *user_data, const nk_epoch_record *epoch) {
    nk_android_recording *recording = user_data;
    nk_result result = nk_session_save_epoch(recording->session, epoch);
    if (result == NK_OK) {
        ++recording->saved_epochs;
        (void)__android_log_print(
            ANDROID_LOG_INFO, NK_RECORDING_TAG,
            "Epoch saved: session=%lld idx=%d acc_cov=%.3f mic_cov=%.3f flags=%u",
            (long long)recording->id, epoch->idx, epoch->acc_cov, epoch->mic_cov, epoch->flags);
    } else {
        log_error(recording, "save epoch", result);
    }
    return result;
}

static nk_result save_event(void *user_data, const nk_event_record *event) {
    nk_android_recording *recording = user_data;
    nk_result result = nk_session_save_event(recording->session, event);
    if (result == NK_OK) {
        ++recording->saved_events;
        (void)__android_log_print(ANDROID_LOG_INFO, NK_RECORDING_TAG,
                                  "Event saved: session=%lld type=%s reason=%s",
                                  (long long)recording->id, event_name(event->type),
                                  event->reason != NULL ? event->reason : "NULL");
    } else {
        log_error(recording, "save event", result);
    }
    return result;
}

nk_result nk_android_recording_open(nk_android_recording **out_recording) {
    unsigned char key[NK_ANDROID_DB_KEY_SIZE] = {0};
    char *directory = NULL;
    nk_android_recording *recording;
    nk_result result = NK_ERROR_IO;
    int has_consent = 0;
    if (out_recording == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_recording = NULL;
    recording = calloc(1, sizeof(*recording));
    if (recording == NULL) {
        return NK_ERROR_MEMORY;
    }
    *out_recording = recording;
    directory = SDL_GetPrefPath("Mikan-32", "Nemunoki");
    if (directory == NULL) {
        goto cleanup;
    }
    if (SDL_asprintf(&recording->path, "%snemunoki.db", directory) < 0) {
        result = NK_ERROR_MEMORY;
        goto cleanup;
    }
    result = nk_android_bridge_db_key(key, sizeof(key));
    if (result != NK_OK) {
        goto cleanup;
    }
    result = nk_data_open(&recording->data, recording->path, key, sizeof(key));
    nk_android_bridge_clear_key(key, sizeof(key));
    if (result != NK_OK) {
        goto cleanup;
    }
    result = nk_data_create_schema(recording->data);
    if (result != NK_OK) {
        goto cleanup;
    }
    result = nk_data_has_versioned_consent(recording->data, NK_DATA_CONSENT_CURRENT_VERSION_TEXT,
                                           &has_consent);
    if (result != NK_OK) {
        goto cleanup;
    }
    if (!has_consent) {
        result = NK_ERROR_CONSENT_REQUIRED;
        goto cleanup;
    }
    result = nk_session_create(&recording->session, recording->data);
cleanup:
    nk_android_bridge_clear_key(key, sizeof(key));
    SDL_free(directory);
    if (result != NK_OK) {
        log_error(recording, "open", result);
    }
    return result;
}

nk_result nk_android_recording_check_consent(nk_android_recording *recording) {
    if (recording == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    int agreed = 0;
    nk_result result = nk_data_has_versioned_consent(recording->data,
                                                     NK_DATA_CONSENT_CURRENT_VERSION_TEXT, &agreed);
    if (result != NK_OK) {
        return result;
    }
    if (!agreed) {
        recording->consent_revoked = 1;
        return NK_ERROR_CONSENT_REQUIRED;
    }
    return NK_OK;
}

nk_result nk_android_recording_start(nk_android_recording *recording, int64_t start_ms) {
    nk_session_record record = {0};
    nk_sensing_config config = {0};
    nk_sensing_callbacks callbacks = {save_epoch, save_event};
    char date[11];
    char model[PROP_VALUE_MAX] = {0};
    int battery = -1;
    nk_result result;
    if (recording == NULL || recording->session == NULL || start_ms < 0) {
        return NK_ERROR_ARGUMENT;
    }
    if (recording->finished) {
        return NK_ERROR_NOT_RUNNING;
    }
    if (recording->sensing != NULL || nk_session_is_running(recording->session)) {
        return NK_ERROR_ALREADY_RUNNING;
    }
    result = nk_android_recording_check_consent(recording);
    if (result != NK_OK) {
        return result;
    }
    recording->consent_revoked = 0;
    result = local_date(start_ms, date, &record.tz_min);
    if (result != NK_OK) {
        return result;
    }
    record.start_ms = start_ms;
    record.target_sleep_min = 420;
    record.placement = "SKIP";
    /* Development build tags, not a claimed released APK/analysis version. */
    record.app_ver = "dev-issue27";
    record.algo_ver = "sensing-accel-v1";
    if (__system_property_get("ro.product.model", model) > 0) {
        record.device_model = model;
    }
    /* 戻り値の電源状態は使わず、残量だけ読む。不明な残量は以下でNULL扱いにする。 */
    (void)SDL_GetPowerInfo(NULL, &battery);
    if (battery >= 0 && battery <= 100) {
        record.has_battery_start = 1;
        record.battery_start = battery;
    }
    config.start_ms = start_ms;
    config.move_threshold_mg = 15.0;
    result = nk_sensing_create(&recording->sensing, &config, &callbacks, recording);
    if (result == NK_OK) {
        /* nk_session_start rechecks consent on this worker's DB connection. */
        result = nk_session_start(recording->session, &record);
    }
    if (result != NK_OK) {
        nk_sensing_destroy(recording->sensing);
        recording->sensing = NULL;
        log_error(recording, "start", result);
        return result;
    }
    recording->id = nk_session_id(recording->session);
    recording->start_ms = start_ms;
    (void)__android_log_print(ANDROID_LOG_INFO, NK_RECORDING_TAG,
                              "Session started: id=%lld mic_used=0 light_used=0",
                              (long long)recording->id);
    return NK_OK;
}

nk_result nk_android_recording_push_second(nk_android_recording *recording,
                                           const nk_sensing_second *second) {
    nk_sensing_second input;
    SDL_PowerState power;
    if (recording == NULL || second == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    if (recording->sensing == NULL || recording->finished) {
        return NK_ERROR_NOT_RUNNING;
    }
    input = *second;
    power = SDL_GetPowerInfo(NULL, NULL);
    input.charging = power == SDL_POWERSTATE_CHARGING || power == SDL_POWERSTATE_CHARGED;
    return nk_sensing_push_second(recording->sensing, &input);
}

static nk_result verify_saved(nk_android_recording *recording, int64_t end_ms) {
    static const char sql[] =
        "SELECT end_ms,"
        "(SELECT COUNT(*) FROM epoch WHERE session_id=s.id),"
        "(SELECT COUNT(*) FROM event WHERE session_id=s.id),start_ms,battery_start,battery_end,"
        "(SELECT COUNT(*) FROM epoch WHERE session_id=s.id AND acc_cov>=0.8),"
        "(SELECT COUNT(*) FROM epoch WHERE session_id=s.id AND (flags&16)!=0),"
        "(SELECT MIN(idx) FROM epoch WHERE session_id=s.id),"
        "(SELECT MAX(idx) FROM epoch WHERE session_id=s.id) "
        "FROM session AS s WHERE id=?1;";
    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    unsigned char key[NK_ANDROID_DB_KEY_SIZE] = {0};
    nk_result result = NK_ERROR_DATABASE;
    int events = 0;
    int good_epochs = 0;
    int charging_epochs = 0;
    int64_t duration_ms = 0;
    int64_t expected_epochs = 0;
    char battery_start[16] = "unknown";
    char battery_end[16] = "unknown";
    char battery_drop[16] = "unknown";
    if (sqlite3_open_v2(recording->path, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_busy_timeout(database, 5000) != SQLITE_OK ||
        nk_android_bridge_db_key(key, sizeof(key)) != NK_OK ||
        sqlite3_key(database, key, (int)sizeof(key)) != SQLITE_OK) {
        goto cleanup;
    }
    nk_android_bridge_clear_key(key, sizeof(key));
    if (sqlite3_prepare_v2(database, sql, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 1, (sqlite3_int64)recording->id) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW || sqlite3_column_type(statement, 0) == SQLITE_NULL ||
        sqlite3_column_int64(statement, 0) != end_ms ||
        sqlite3_column_int64(statement, 3) != recording->start_ms || end_ms < recording->start_ms ||
        sqlite3_column_int64(statement, 1) != recording->saved_epochs) {
        goto cleanup;
    }
    duration_ms = end_ms - recording->start_ms;
    expected_epochs = duration_ms / 30000 + (duration_ms % 30000 != 0);
    events = sqlite3_column_int(statement, 2);
    good_epochs = sqlite3_column_int(statement, 6);
    charging_epochs = sqlite3_column_int(statement, 7);
    if (events < recording->saved_events || recording->saved_epochs > expected_epochs ||
        (recording->saved_epochs > 0 && (sqlite3_column_int64(statement, 8) < 0 ||
                                         sqlite3_column_int64(statement, 9) >= expected_epochs))) {
        goto cleanup;
    }
    int has_start = sqlite3_column_type(statement, 4) != SQLITE_NULL;
    int has_end = sqlite3_column_type(statement, 5) != SQLITE_NULL;
    int start_percent = sqlite3_column_int(statement, 4);
    int end_percent = sqlite3_column_int(statement, 5);
    if (has_start && start_percent >= 0 && start_percent <= 100) {
        (void)snprintf(battery_start, sizeof(battery_start), "%d", start_percent);
    } else {
        has_start = 0;
    }
    if (has_end && end_percent >= 0 && end_percent <= 100) {
        (void)snprintf(battery_end, sizeof(battery_end), "%d", end_percent);
    } else {
        has_end = 0;
    }
    if (has_start && has_end) {
        (void)snprintf(battery_drop, sizeof(battery_drop), "%d", start_percent - end_percent);
    }
    result = NK_OK;
cleanup:
    nk_android_bridge_clear_key(key, sizeof(key));
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        result = NK_ERROR_DATABASE;
    }
    if (result != NK_OK && database != NULL) {
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_RECORDING_TAG,
                                  "DB readback failed: session=%lld expected_epochs=%d sqlite=%s",
                                  (long long)recording->id, recording->saved_epochs,
                                  sqlite3_errmsg(database));
    }
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) {
        result = NK_ERROR_DATABASE;
    }
    if (result == NK_OK) {
        (void)__android_log_print(ANDROID_LOG_INFO, NK_RECORDING_TAG,
                                  "DB verified: session=%lld finished=1 epochs=%d events=%d",
                                  (long long)recording->id, recording->saved_epochs, events);
        /* Include missing rows in the denominator; an absent epoch is not healthy data. */
        if (expected_epochs > 0) {
            (void)__android_log_print(
                ANDROID_LOG_INFO, NK_RECORDING_TAG,
                "TM02 summary: session=%lld duration_ms=%lld expected_epochs=%lld epochs=%d "
                "good_acc_epochs=%d good_acc_pct=%.3f missing_epochs=%lld charging_epochs=%d",
                (long long)recording->id, (long long)duration_ms, (long long)expected_epochs,
                recording->saved_epochs, good_epochs, 100.0 * good_epochs / expected_epochs,
                (long long)(expected_epochs - recording->saved_epochs), charging_epochs);
        } else {
            (void)__android_log_print(
                ANDROID_LOG_INFO, NK_RECORDING_TAG,
                "TM02 summary: session=%lld duration_ms=0 expected_epochs=0 epochs=0 "
                "good_acc_epochs=0 good_acc_pct=unknown missing_epochs=0 charging_epochs=0",
                (long long)recording->id);
        }
        (void)__android_log_print(ANDROID_LOG_INFO, NK_RECORDING_TAG,
                                  "TM02 battery: session=%lld start_pct=%s end_pct=%s drop_pp=%s",
                                  (long long)recording->id, battery_start, battery_end,
                                  battery_drop);
    } else {
        log_error(recording, "readback", result);
    }
    return result;
}

nk_result nk_android_recording_finish(nk_android_recording *recording, int64_t end_ms,
                                      nk_session_end_reason reason) {
    char date[11];
    nk_result result;
    nk_result next;
    int battery = -1;
    if (recording == NULL || end_ms < recording->start_ms) {
        return NK_ERROR_ARGUMENT;
    }
    if (recording->finished) {
        return recording->finish_result;
    }
    if (recording->session == NULL || !nk_session_is_running(recording->session)) {
        return NK_OK;
    }
    next = local_date(end_ms, date, NULL);
    if (next != NK_OK) {
        return next;
    }
    nk_result consent_result = nk_android_recording_check_consent(recording);
    if (consent_result != NK_OK && consent_result != NK_ERROR_CONSENT_REQUIRED) {
        reason = NK_SESSION_END_ERROR;
    }
    result = nk_sensing_finish(recording->sensing);
    if (result == NK_OK && consent_result != NK_OK && consent_result != NK_ERROR_CONSENT_REQUIRED) {
        result = consent_result;
    }
    if (result != NK_OK) {
        reason = NK_SESSION_END_ERROR;
    }
    if (reason == NK_SESSION_END_USER_STOP && !recording->stop_event_saved) {
        nk_event_record event = {0};
        event.type = NK_EVENT_USER;
        event.t_ms = end_ms;
        event.reason = recording->consent_revoked ? "CONSENT_REVOKED" : "USER_STOP";
        next = save_event(recording, &event);
        if (next != NK_OK) {
            result = next;
            reason = NK_SESSION_END_ERROR;
        } else {
            recording->stop_event_saved = 1;
        }
    }
    /* 戻り値の電源状態は使わず、残量だけ読む。不明な残量は以下でNULL扱いにする。 */
    (void)SDL_GetPowerInfo(NULL, &battery);
    next = nk_session_finish(recording->session, end_ms, date, reason,
                             battery >= 0 && battery <= 100, battery);
    if (next != NK_OK) {
        log_error(recording, "finish", next);
        return result != NK_OK ? result : next;
    }
    recording->finished = 1;
    (void)__android_log_print(ANDROID_LOG_INFO, NK_RECORDING_TAG,
                              "Session finished: id=%lld end_reason=%s", (long long)recording->id,
                              end_reason_name(reason));
    next = verify_saved(recording, end_ms);
    recording->finish_result = result != NK_OK ? result : next;
    return recording->finish_result;
}

nk_result nk_android_recording_close(nk_android_recording **in_out_recording) {
    nk_android_recording *recording;
    nk_result result;
    if (in_out_recording == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    recording = *in_out_recording;
    if (recording == NULL) {
        return NK_OK;
    }
    nk_sensing_destroy(recording->sensing);
    recording->sensing = NULL;
    nk_session_destroy(recording->session);
    recording->session = NULL;
    result = nk_data_close(recording->data);
    if (result != NK_OK) {
        log_error(recording, "close", result);
        return result;
    }
    SDL_free(recording->path);
    free(recording);
    *in_out_recording = NULL;
    return NK_OK;
}

/* END NK_ANDROID_RECORDING_C */
