#include "android_bridge.h"
#include "android_consent.h"
#include "android_night.h"
#include "data.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <android/log.h>
#include <jni.h>
#include <sqlcipher/sqlite3.h>
#include <stdbool.h>
#include <stdio.h>

#define NK_LOG_TAG "NkAndroid"

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
    result = nk_data_has_measurement_consent(*out_data, out_has_consent);

cleanup:
    nk_android_bridge_clear_key(key, sizeof(key));
    SDL_free(path);
    SDL_free(directory);
    if (result != NK_OK) {
        *out_has_consent = 0;
    }
    return result;
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
            nk_android_night_ui_draw(renderer) != NK_OK || !SDL_RenderPresent(renderer)) {
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
