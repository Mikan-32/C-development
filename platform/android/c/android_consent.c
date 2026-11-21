#include "android_consent.h"

#include "data_consent.h"

#include <android/log.h>
#include <stdint.h>

#define NK_CONSENT_VERSION NK_DATA_CONSENT_CURRENT_VERSION_TEXT
#define NK_CONSENT_LOG_TAG "NkAndroid"

nk_result nk_android_consent_request(SDL_Window *window, nk_data *data, int *out_has_consent) {
    const SDL_MessageBoxButtonData buttons[] = {
        {.flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
         .buttonID = 0,
         .text = "今回は同意しない"},
        {.flags = 0, .buttonID = 1, .text = "同意する"}};

    const SDL_MessageBoxData dialog = {
        .flags = SDL_MESSAGEBOX_INFORMATION,
        .window = window,
        .title = "計測への同意（開発テスト用）",
        .message = "加速度センサーで体の動きを計測する、開発テスト用の同意です。\n\n"
                   "計測した場合、集計した記録をこの端末内に暗号化して保存します。"
                   "サーバーには送りません。生の加速度は保存しません。\n\n"
                   "マイクの使用と録音は、この同意には含みません。\n\n"
                   "同意の取消・記録削除は、起動画面のPRIVACYから行えます。\n\n"
                   "同意を保存するだけでは、計測は始まりません。",
        .numbuttons = 2,
        .buttons = buttons,
        .colorScheme = NULL};

    SDL_Time now = 0;
    nk_consent_record record = {0};
    nk_result result;
    int selected = -1;

    if (data == NULL || out_has_consent == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    *out_has_consent = 0;

    result = nk_data_has_versioned_consent(data, NK_CONSENT_VERSION, out_has_consent);

    if (result != NK_OK) {
        *out_has_consent = 0;
        return result;
    }

    if (*out_has_consent) {
        return NK_OK;
    }

    if (!SDL_ShowMessageBox(&dialog, &selected)) {
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_CONSENT_LOG_TAG,
                                  "Consent dialog failed: %s", SDL_GetError());
        return NK_ERROR_IO;
    }

    if (selected != 1) {
        (void)__android_log_print(ANDROID_LOG_INFO, NK_CONSENT_LOG_TAG,
                                  "Measurement consent not granted; no record written");
        return NK_OK;
    }

    if (!SDL_GetCurrentTime(&now) || now <= 0) {
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_CONSENT_LOG_TAG,
                                  "Consent time unavailable");
        return NK_ERROR_IO;
    }

    record.version = NK_CONSENT_VERSION;
    record.measurement = 1;
    record.microphone = 0;
    record.consented_ms = (int64_t)(now / INT64_C(1000000));

    result = nk_data_consent_set(data, &record);

    if (result != NK_OK) {
        return result;
    }

    result = nk_data_has_versioned_consent(data, NK_CONSENT_VERSION, out_has_consent);

    if (result != NK_OK || !*out_has_consent) {
        *out_has_consent = 0;
        return result != NK_OK ? result : NK_ERROR_DATABASE;
    }

    (void)__android_log_print(ANDROID_LOG_INFO, NK_CONSENT_LOG_TAG,
                              "Consent saved: version=%s measurement=1 microphone=0",
                              NK_CONSENT_VERSION);

    return NK_OK;
}

/* END NK_ANDROID_CONSENT_C */
