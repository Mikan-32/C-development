#include "android_night.h"

#include "android_accel.h"
#include "android_bridge.h"

#include <SDL3/SDL_system.h>
#include <android/log.h>

static const SDL_FRect start_button = {20.0f, 300.0f, 150.0f, 50.0f};
static const SDL_FRect stop_button = {190.0f, 300.0f, 150.0f, 50.0f};
static const char *action_status = "Tap START to record accelerometer";
static bool start_requested;

static void SDLCALL notification_permission_result(void *userdata, const char *permission,
                                                   bool granted) {
    (void)userdata;
    (void)__android_log_print(ANDROID_LOG_INFO, "NkAndroid", "Permission result: %s granted=%d",
                              permission, granted ? 1 : 0);
    /* A permission result never starts the service automatically. */
}

void nk_android_night_ui_init(void) {
    action_status = "Tap START to record accelerometer";
    start_requested = false;
    (void)SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
}

static nk_result request_start(nk_data *data) {
    int has_consent = 0;
    int notifications_allowed = 0;
    nk_result result;

    if (data == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    result = nk_data_has_measurement_consent(data, &has_consent);
    if (result != NK_OK) {
        return result;
    }
    if (!has_consent) {
        return NK_ERROR_CONSENT_REQUIRED;
    }
    if (nk_android_bridge_night_state() == NK_ANDROID_NIGHT_STARTED) {
        return NK_ERROR_ALREADY_RUNNING;
    }
    result = nk_android_bridge_notifications_allowed(&notifications_allowed);
    if (result != NK_OK) {
        return result;
    }
    if (!notifications_allowed) {
        if (SDL_GetAndroidSDKVersion() >= 33) {
            if (!SDL_RequestAndroidPermission("android.permission.POST_NOTIFICATIONS",
                                              notification_permission_result, NULL)) {
                return NK_ERROR_IO;
            }
        }
        return NK_ERROR_PERMISSION_REQUIRED;
    }
    result = nk_android_accel_prepare();
    if (result != NK_OK) {
        return result;
    }
    result =
        nk_android_bridge_night_start(0, "ねむのき", "加速度の計測と暗号化保存中（マイクなし）");
    if (result == NK_OK) {
        start_requested = true;
    } else {
        nk_android_accel_request_stop();
    }
    return result;
}

static bool contains(const SDL_FRect *button, float x, float y) {
    return x >= button->x && x < button->x + button->w && y >= button->y &&
           y < button->y + button->h;
}

nk_result nk_android_night_ui_event(nk_data *data, SDL_Renderer *renderer, const SDL_Event *input) {
    SDL_Event event;
    nk_result result;

    if (renderer == NULL || input == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    if (input->type != SDL_EVENT_MOUSE_BUTTON_UP || input->button.button != SDL_BUTTON_LEFT) {
        return NK_OK;
    }
    event = *input;
    if (!SDL_ConvertEventToRenderCoordinates(renderer, &event)) {
        return NK_ERROR_IO;
    }
    if (contains(&start_button, event.button.x, event.button.y)) {
        result = request_start(data);
        switch (result) {
        case NK_OK:
            action_status = "Start requested";
            break;
        case NK_ERROR_CONSENT_REQUIRED:
            action_status = "Measurement consent is required";
            break;
        case NK_ERROR_PERMISSION_REQUIRED:
            action_status = "Allow notifications; tap START again";
            break;
        case NK_ERROR_ALREADY_RUNNING:
            action_status = "Starting or already running";
            break;
        case NK_ERROR_ARGUMENT:
            action_status = "Database is unavailable";
            break;
        default:
            action_status = "Start failed; check log";
            break;
        }
    } else if (contains(&stop_button, event.button.x, event.button.y)) {
        result = nk_android_bridge_night_stop();
        action_status = result == NK_OK ? "Stop requested" : "Stop failed; check log";
    } else {
        return NK_OK;
    }
    (void)__android_log_print(ANDROID_LOG_INFO, "NkAndroid", "Night service action result: %d",
                              (int)result);
    return result;
}

static bool draw_button(SDL_Renderer *renderer, const SDL_FRect *button, const char *label) {
    return SDL_SetRenderDrawColor(renderer, 45, 80, 120, 255) &&
           SDL_RenderFillRect(renderer, button) &&
           SDL_SetRenderDrawColor(renderer, 235, 240, 245, 255) &&
           SDL_RenderDebugText(renderer, button->x + 12.0f, button->y + 20.0f, label);
}

nk_result nk_android_night_ui_draw(SDL_Renderer *renderer) {
    const char *state_text;
    if (renderer == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    switch (nk_android_bridge_night_state()) {
    case NK_ANDROID_NIGHT_STARTED:
        state_text = "Night service: running";
        break;
    case NK_ANDROID_NIGHT_STOPPED:
        state_text = "Night service: stopped";
        break;
    default:
        state_text = "Night service: failed";
        break;
    }
    if (!SDL_SetRenderDrawColor(renderer, 235, 240, 245, 255) ||
        !SDL_RenderDebugText(renderer, 20.0f, 240.0f, state_text) ||
        !draw_button(renderer, &start_button, "START service") ||
        !draw_button(renderer, &stop_button, "STOP service") ||
        !SDL_RenderDebugText(renderer, 20.0f, 380.0f, action_status) ||
        !SDL_RenderDebugText(renderer, 20.0f, 420.0f, nk_android_accel_status())) {
        return NK_ERROR_IO;
    }
    return NK_OK;
}

nk_result nk_android_night_ui_shutdown(void) {
    nk_result accel_result = nk_android_accel_shutdown();
    nk_result service_result = NK_OK;
    if (start_requested || nk_android_bridge_night_state() == NK_ANDROID_NIGHT_STARTED) {
        service_result = nk_android_bridge_night_stop();
    }
    return accel_result != NK_OK ? accel_result : service_result;
}

/* END NK_ANDROID_NIGHT_C */
