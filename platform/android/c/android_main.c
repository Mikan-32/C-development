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

/* Read the actual SQLCipher version from an in-memory database. */
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

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    char cipher_version[96];
    bool running = true;
    int exit_code = 0;

    (void)argc;
    (void)argv;

    if (!read_cipher_version(cipher_version, sizeof(cipher_version))) {
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_LOG_TAG, "SQLCipher version check failed");
        return 1;
    }

    (void)__android_log_print(ANDROID_LOG_INFO, NK_LOG_TAG, "%s", cipher_version);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!SDL_CreateWindowAndRenderer("Nemunoki", 720, 1280, SDL_WINDOW_RESIZABLE, &window,
                                     &renderer)) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!SDL_SetRenderLogicalPresentation(renderer, 360, 640, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SDL_Log("Display setup failed: %s", SDL_GetError());
        exit_code = 1;
        running = false;
    }

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }

        if (!running) {
            break;
        }

        if (!SDL_SetRenderDrawColor(renderer, 20, 25, 35, 255) || !SDL_RenderClear(renderer) ||
            !SDL_SetRenderDrawColor(renderer, 235, 240, 245, 255) ||
            !SDL_RenderDebugText(renderer, 20.0f, 40.0f, "Nemunoki - Android startup check") ||
            !SDL_RenderDebugText(renderer, 20.0f, 80.0f, cipher_version) ||
            !SDL_RenderDebugText(renderer, 20.0f, 120.0f, "Measurement is not running.") ||
            !SDL_RenderPresent(renderer)) {
            SDL_Log("Rendering failed: %s", SDL_GetError());
            exit_code = 1;
            break;
        }

        SDL_Delay(100);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return exit_code;
}

/* Called by the existing Kotlin MainActivity through NativeBridge. */
JNIEXPORT void JNICALL Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnLaunch(
    JNIEnv *env, jclass bridge_class, jint reason, jint id) {
    (void)env;
    (void)bridge_class;

    (void)__android_log_print(ANDROID_LOG_INFO, NK_LOG_TAG, "Application launched: reason=%d id=%d",
                              (int)reason, (int)id);
}
