#define _POSIX_C_SOURCE 200809L

#include "android_accel.h"
#include "android_bridge.h"
#include "android_consent.h"
#include "android_night.h"
#include "android_saved_report.h"
#include "data.h"
#include "data_consent.h"
#include "data_recovery.h"
#include "nk_db.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <android/log.h>
#include <jni.h>
#include <sqlcipher/sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NK_LOG_TAG "NkAndroid"

static nk_data_recovery_report startup_recovery;

/* Diagnostic launch bypasses schema creation, recovery, consent writes, and measurement. */
static int show_saved_report(void) {
    unsigned char key[NK_ANDROID_DB_KEY_SIZE] = {0};
    char *directory = SDL_GetPrefPath("Mikan-32", "Nemunoki");
    char *path = NULL;
    nk_android_saved_report report = {0};
    char text[1024];
    nk_result result = NK_ERROR_IO;
    if (directory != NULL && SDL_asprintf(&path, "%snemunoki.db", directory) >= 0) {
        result = nk_android_bridge_db_key(key, sizeof(key));
        if (result == NK_OK) {
            result = nk_android_saved_report_read(path, key, sizeof(key), &report);
        }
    }
    nk_android_bridge_clear_key(key, sizeof(key));
    SDL_free(path);
    SDL_free(directory);
    if (result == NK_OK) {
        result = nk_android_saved_report_format(&report, text, sizeof(text));
    }
    if (result != NK_OK) {
        (void)snprintf(text, sizeof(text), "TM02 stored report failed: result=%d", (int)result);
    }
    (void)__android_log_print(result == NK_OK ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, NK_LOG_TAG,
                              "%s", text);
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "TM-02 saved measurement", text,
                                  NULL)) {
        SDL_Log("Saved report dialog failed: %s", SDL_GetError());
    }
    return result == NK_OK ? 0 : 1;
}

JNIEXPORT void JNICALL Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnLaunch(
    JNIEnv *env, jclass bridge_class, jint reason, jint id);

static bool read_cipher_version(char *buffer, size_t buffer_size) {
    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    bool success = false;

    if (sqlite3_open(":memory:", &database) == SQLITE_OK &&
        sqlite3_prepare_v2(database, "PRAGMA cipher_version;", -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char *version = sqlite3_column_text(statement, 0);
        if (version != NULL && version[0] != '\0') {
            int length = snprintf(buffer, buffer_size, "SQLCipher: %s", (const char *)version);
            success = length >= 0 && (size_t)length < buffer_size;
        }
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        success = false;
    }
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) {
        success = false;
    }
    return success;
}

/* The caller owns any returned database, including on schema/read failure. */
static nk_result open_application_database(nk_data **out_data, int *out_has_consent) {
    unsigned char key[NK_ANDROID_DB_KEY_SIZE] = {0};
    char *directory = NULL;
    char *path = NULL;
    nk_result result;

    if (out_data == NULL || out_has_consent == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_data = NULL;
    *out_has_consent = 0;
    startup_recovery = (nk_data_recovery_report){0};

    directory = SDL_GetPrefPath("Mikan-32", "Nemunoki");
    if (directory == NULL) {
        result = NK_ERROR_IO;
        goto cleanup;
    }
    if (SDL_asprintf(&path, "%snemunoki.db", directory) < 0) {
        result = NK_ERROR_MEMORY;
        goto cleanup;
    }

    result = nk_android_bridge_db_key(key, sizeof(key));
    if (result != NK_OK) {
        goto cleanup;
    }
    result = nk_data_open(out_data, path, key, sizeof(key));
    nk_android_bridge_clear_key(key, sizeof(key));
    if (result != NK_OK) {
        goto cleanup;
    }
    result = nk_data_create_schema(*out_data);
    if (result != NK_OK) {
        goto cleanup;
    }
    /* A live worker must never be mistaken for a process that died. */
    if (!nk_android_accel_is_active()) {
        SDL_Time now;
        if (!SDL_GetCurrentTime(&now) || now < 0) {
            result = NK_ERROR_IO;
            goto cleanup;
        }
        result = nk_data_recover_unfinished(*out_data, (int64_t)(now / 1000000), &startup_recovery);
        if (result != NK_OK) {
            goto cleanup;
        }
        (void)__android_log_print(
            ANDROID_LOG_INFO, NK_LOG_TAG,
            "Recovery check: recovered=%d interrupted=%d user_stops=%d epochs_added=%d q04=%d",
            startup_recovery.recovered_sessions, startup_recovery.interrupted_sessions,
            startup_recovery.completed_user_stops, startup_recovery.added_epochs,
            startup_recovery.q04_sessions);
        if (startup_recovery.has_interruption) {
            (void)__android_log_print(
                ANDROID_LOG_INFO, NK_LOG_TAG,
                "Q04 stored: session=%lld end_reason=SERVICE_KILLED gap_start_ms=%lld "
                "gap_end_ms=%lld duration_ms=%lld",
                (long long)startup_recovery.interrupted_session_id,
                (long long)startup_recovery.gap_start_ms, (long long)startup_recovery.gap_end_ms,
                (long long)(startup_recovery.gap_end_ms - startup_recovery.gap_start_ms));
        }
    }
    result = nk_db_bind(*out_data);
    if (result == NK_OK) {
        result = nk_data_has_versioned_consent(*out_data, NK_DATA_CONSENT_CURRENT_VERSION_TEXT,
                                               out_has_consent);
    }

cleanup:
    nk_android_bridge_clear_key(key, sizeof(key));
    SDL_free(path);
    SDL_free(directory);
    if (result != NK_OK) {
        *out_has_consent = 0;
    }
    return result;
}

static void recovery_time_text(char *buffer, size_t size, const char *label, int64_t utc_ms) {
    time_t instant = (time_t)(utc_ms / 1000);
    struct tm local;
    char time_text[32];
    if ((int64_t)instant == utc_ms / 1000 && localtime_r(&instant, &local) != NULL &&
        strftime(time_text, sizeof(time_text), "%m-%d %H:%M:%S", &local) != 0) {
        (void)snprintf(buffer, size, "%s%s", label, time_text);
    } else {
        (void)snprintf(buffer, size, "%sUTC ms %lld", label, (long long)utc_ms);
    }
}

static bool draw_recovery_status(SDL_Renderer *renderer) {
    char id[64];
    char from[64];
    char to[64];
    if (!startup_recovery.has_interruption) {
        return startup_recovery.recovered_sessions == 0 ||
               SDL_RenderDebugText(renderer, 20.0f, 470.0f, "Previous recording finalized");
    }
    (void)snprintf(id, sizeof(id), "Previous Q04 session: %lld",
                   (long long)startup_recovery.interrupted_session_id);
    recovery_time_text(from, sizeof(from), "From: ", startup_recovery.gap_start_ms);
    recovery_time_text(to, sizeof(to), "To:   ", startup_recovery.gap_end_ms);
    return SDL_RenderDebugText(renderer, 20.0f, 470.0f, id) &&
           SDL_RenderDebugText(renderer, 20.0f, 495.0f, "Recording was interrupted") &&
           SDL_RenderDebugText(renderer, 20.0f, 520.0f, from) &&
           SDL_RenderDebugText(renderer, 20.0f, 545.0f, to) &&
           SDL_RenderDebugText(renderer, 20.0f, 575.0f, "Start: last valid saved epoch");
}

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    nk_data *database = NULL;
    char cipher_version[96];
    const char *database_status;
    const char *consent_status;
    nk_result database_result;
    bool running = true;
    int has_consent = 0;
    int exit_code = 0;

    (void)argc;
    (void)argv;

    if (!read_cipher_version(cipher_version, sizeof(cipher_version))) {
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_LOG_TAG, "SQLCipher version check failed");
        return 1;
    }
    (void)__android_log_print(ANDROID_LOG_INFO, NK_LOG_TAG, "%s", cipher_version);

    nk_android_night_ui_init();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (argc == 2 && strcmp(argv[1], "--tm02-report-only") == 0) {
        int report_result = show_saved_report();
        SDL_Quit();
        return report_result;
    }

    database_result = open_application_database(&database, &has_consent);
    if (database_result == NK_OK) {
        database_status = "Encrypted DB: ready";
        consent_status =
            has_consent ? "Measurement consent: granted" : "Measurement consent: not granted";
        (void)__android_log_print(ANDROID_LOG_INFO, NK_LOG_TAG,
                                  "Encrypted DB ready; measurement consent=%d", has_consent);
    } else {
        database_status = "Encrypted DB: error";
        consent_status = "Measurement consent: unavailable";
        exit_code = 1;
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_LOG_TAG,
                                  "Database initialization failed: %d", (int)database_result);
        if (database != NULL) {
            (void)__android_log_print(ANDROID_LOG_ERROR, NK_LOG_TAG, "Database detail: %s",
                                      nk_data_last_error(database));
        }
    }

    if (!SDL_CreateWindowAndRenderer("Nemunoki", 720, 1280, SDL_WINDOW_RESIZABLE, &window,
                                     &renderer)) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        exit_code = 1;
        goto cleanup;
    }
    if (!SDL_SetRenderLogicalPresentation(renderer, 360, 640, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SDL_Log("Display setup failed: %s", SDL_GetError());
        exit_code = 1;
        goto cleanup;
    }

    if (database_result == NK_OK && !has_consent) {
        nk_result consent_result = nk_android_consent_request(window, database, &has_consent);
        if (consent_result == NK_OK) {
            consent_status =
                has_consent ? "Measurement consent: granted" : "Measurement consent: not granted";
        } else {
            consent_status = "Measurement consent: unavailable";
            exit_code = 1;
            (void)__android_log_print(ANDROID_LOG_ERROR, NK_LOG_TAG,
                                      "Consent confirmation failed: %d", (int)consent_result);
        }
    }

    bool drawing_enabled = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
            if (!running) {
                break;
            }
            if (event.type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
                drawing_enabled = false;
            } else if (event.type == SDL_EVENT_DID_ENTER_FOREGROUND) {
                drawing_enabled = true;
            }
            if (drawing_enabled) {
                (void)nk_android_night_ui_event(database_result == NK_OK ? database : NULL,
                                                renderer, &event);
                if (nk_android_night_ui_take_records_deleted()) {
                    startup_recovery = (nk_data_recovery_report){0};
                }
                if (database_result == NK_OK) {
                    int agreed = 0;
                    nk_result consent_read = nk_data_has_versioned_consent(
                        database, NK_DATA_CONSENT_CURRENT_VERSION_TEXT, &agreed);
                    consent_status = consent_read == NK_OK && agreed
                                         ? "Measurement consent: granted"
                                         : "Measurement consent: not granted";
                }
            }
        }
        if (!running) {
            break;
        }
        if (!drawing_enabled) {
            SDL_Delay(100);
            continue;
        }
        if (!SDL_SetRenderDrawColor(renderer, 20, 25, 35, 255) || !SDL_RenderClear(renderer) ||
            !SDL_SetRenderDrawColor(renderer, 235, 240, 245, 255) ||
            !SDL_RenderDebugText(renderer, 20.0f, 40.0f, "Nemunoki - Android startup check") ||
            !SDL_RenderDebugText(renderer, 20.0f, 80.0f, cipher_version) ||
            !SDL_RenderDebugText(renderer, 20.0f, 120.0f, database_status) ||
            !SDL_RenderDebugText(renderer, 20.0f, 160.0f, consent_status) ||
            !SDL_RenderDebugText(renderer, 20.0f, 200.0f, "Accelerometer mode; encrypted DB") ||
            nk_android_night_ui_draw(renderer) != NK_OK || !draw_recovery_status(renderer) ||
            !SDL_RenderPresent(renderer)) {
            SDL_Log("Rendering failed: %s", SDL_GetError());
            exit_code = 1;
            break;
        }
        SDL_Delay(100);
    }

cleanup:
    if (nk_android_night_ui_shutdown() != NK_OK) {
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_LOG_TAG, "Night service shutdown failed");
        exit_code = 1;
    }
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
    }
    if (window != NULL) {
        SDL_DestroyWindow(window);
    }
    nk_db_close(); /* Unbind the borrowed UI connection before closing it. */
    if (database != NULL) {
        nk_result close_result = nk_data_close(database);
        if (close_result != NK_OK) {
            (void)__android_log_print(ANDROID_LOG_ERROR, NK_LOG_TAG, "Database close failed: %d",
                                      (int)close_result);
            exit_code = 1;
        }
    }
    SDL_Quit();
    return exit_code;
}

JNIEXPORT void JNICALL Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnLaunch(
    JNIEnv *env, jclass bridge_class, jint reason, jint id) {
    (void)env;
    (void)bridge_class;
    (void)__android_log_print(ANDROID_LOG_INFO, NK_LOG_TAG, "Application launched: reason=%d id=%d",
                              (int)reason, (int)id);
}

/* END NK_ANDROID_MAIN_C */
