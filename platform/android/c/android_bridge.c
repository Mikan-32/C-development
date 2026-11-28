#include "android_bridge.h"

#include "android_accel.h"
#include "curtain.h"

#include <SDL3/SDL_system.h>
#include <android/log.h>
#include <jni.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>

#define NK_BRIDGE_TAG "NkAndroid"

typedef struct nk_android_bridge_cache {
    jclass bridge_class;
    jmethodID db_key;
    jmethodID night_start;
    jmethodID night_stop;
    jmethodID schedule_fn;
    jmethodID cancel_fn;
} nk_android_bridge_cache;

static nk_android_bridge_cache bridge;
static atomic_int night_state = NK_ANDROID_NIGHT_STOPPED;

static void sync_curtain_schedule_to_android(const nk_curtain_schedule *sched) {
    if (sched == NULL) {
        return;
    }
    int64_t now_ms = (int64_t)time(NULL) * 1000LL;
    if (sched->open_enabled) {
        int64_t next_ms =
            nk_curtain_next_trigger_ms(sched->open_hour, sched->open_min, sched->repeat, now_ms);
        (void)nk_android_bridge_schedule_curtain(NK_CURTAIN_SCHED_ID_OPEN, next_ms, 1, 20000,
                                                 "カーテンを開けます", "予約時刻です");
    } else {
        (void)nk_android_bridge_cancel_curtain(NK_CURTAIN_SCHED_ID_OPEN);
    }

    if (sched->close_enabled) {
        int64_t next_ms =
            nk_curtain_next_trigger_ms(sched->close_hour, sched->close_min, sched->repeat, now_ms);
        (void)nk_android_bridge_schedule_curtain(NK_CURTAIN_SCHED_ID_CLOSE, next_ms, 2, 20000,
                                                 "カーテンを閉めます", "予約時刻です");
    } else {
        (void)nk_android_bridge_cancel_curtain(NK_CURTAIN_SCHED_ID_CLOSE);
    }
}

static bool clear_exception(JNIEnv *env, const char *operation) {
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_BRIDGE_TAG, "JNI operation failed: %s",
                                  operation);
        return true;
    }
    return false;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    jclass local_class;

    (void)reserved;
    if (vm == NULL || (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK || env == NULL) {
        return JNI_ERR;
    }

    local_class = (*env)->FindClass(env, "io/github/mikan32/nemunoki/platform/NativeBridge");
    if (local_class == NULL) {
        goto failed;
    }
    bridge.bridge_class = (jclass)(*env)->NewGlobalRef(env, local_class);
    (*env)->DeleteLocalRef(env, local_class);
    if (bridge.bridge_class == NULL) {
        goto failed;
    }

    bridge.db_key = (*env)->GetStaticMethodID(env, bridge.bridge_class, "dbKey", "()[B");
    if (bridge.db_key == NULL) {
        goto failed;
    }
    bridge.night_start = (*env)->GetStaticMethodID(env, bridge.bridge_class, "nightServiceStart",
                                                   "(ILjava/lang/String;Ljava/lang/String;)Z");
    if (bridge.night_start == NULL) {
        goto failed;
    }
    bridge.night_stop =
        (*env)->GetStaticMethodID(env, bridge.bridge_class, "nightServiceStop", "()V");
    if (bridge.night_stop == NULL) {
        goto failed;
    }
    bridge.schedule_fn = (*env)->GetStaticMethodID(env, bridge.bridge_class, "schedule",
                                                   "(IJIIILjava/lang/String;Ljava/lang/String;)Z");
    if (bridge.schedule_fn == NULL) {
        goto failed;
    }
    bridge.cancel_fn = (*env)->GetStaticMethodID(env, bridge.bridge_class, "cancel", "(I)V");
    if (bridge.cancel_fn == NULL) {
        goto failed;
    }

    nk_curtain_set_platform_scheduler(sync_curtain_schedule_to_android);
    (void)__android_log_print(ANDROID_LOG_INFO, NK_BRIDGE_TAG, "NativeBridge ready");
    return JNI_VERSION_1_6;

failed:
    /* 初期化失敗はJNI_ERRで返すため、後片付けの結果では上書きしない。 */
    (void)clear_exception(env, "JNI_OnLoad");
    if (bridge.bridge_class != NULL) {
        (*env)->DeleteGlobalRef(env, bridge.bridge_class);
    }
    bridge = (nk_android_bridge_cache){0};
    return JNI_ERR;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;

    (void)reserved;
    if (vm != NULL && (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) == JNI_OK && env != NULL) {
        if (bridge.bridge_class != NULL) {
            (*env)->DeleteGlobalRef(env, bridge.bridge_class);
        }
        bridge = (nk_android_bridge_cache){0};
    }
}

static JNIEnv *current_env(void) {
    if (bridge.bridge_class == NULL) {
        return NULL;
    }
    return (JNIEnv *)SDL_GetAndroidJNIEnv();
}

void nk_android_bridge_clear_key(void *key, size_t key_size) {
    volatile unsigned char *bytes = key;

    if (bytes == NULL) {
        return;
    }
    while (key_size > 0) {
        *bytes++ = 0;
        --key_size;
    }
}

static bool clear_java_key(JNIEnv *env, jbyteArray key, jsize length) {
    const jbyte zeros[NK_ANDROID_DB_KEY_SIZE] = {0};
    jsize offset = 0;

    while (offset < length) {
        jsize count = length - offset;
        if (count > NK_ANDROID_DB_KEY_SIZE) {
            count = NK_ANDROID_DB_KEY_SIZE;
        }
        (*env)->SetByteArrayRegion(env, key, offset, count, zeros);
        if (clear_exception(env, "clear Java key")) {
            return false;
        }
        offset += count;
    }
    return true;
}

nk_result nk_android_bridge_db_key(unsigned char *out_key, size_t key_size) {
    JNIEnv *env;
    jbyteArray key;
    jsize length;
    nk_result result = NK_ERROR_IO;

    if (out_key == NULL || key_size != NK_ANDROID_DB_KEY_SIZE) {
        return NK_ERROR_ARGUMENT;
    }
    nk_android_bridge_clear_key(out_key, key_size);
    env = current_env();
    if (env == NULL) {
        return NK_ERROR_IO;
    }

    key = (jbyteArray)(*env)->CallStaticObjectMethod(env, bridge.bridge_class, bridge.db_key);
    if (clear_exception(env, "dbKey") || key == NULL) {
        if (key != NULL) {
            (*env)->DeleteLocalRef(env, key);
        }
        return NK_ERROR_IO;
    }
    length = (*env)->GetArrayLength(env, key);
    if (!clear_exception(env, "key length")) {
        if (length == NK_ANDROID_DB_KEY_SIZE) {
            (*env)->GetByteArrayRegion(env, key, 0, length, (jbyte *)out_key);
            if (!clear_exception(env, "copy key")) {
                result = NK_OK;
            }
        }
        if (!clear_java_key(env, key, length)) {
            result = NK_ERROR_IO;
        }
    }
    (*env)->DeleteLocalRef(env, key);
    if (result != NK_OK) {
        nk_android_bridge_clear_key(out_key, key_size);
    }
    return result;
}

nk_result nk_android_bridge_night_start(unsigned int flags, const char *title, const char *body) {
    const unsigned int allowed = NK_ANDROID_NIGHT_MIC | NK_ANDROID_NIGHT_PLAYBACK;
    JNIEnv *env;
    jstring java_title = NULL;
    jstring java_body = NULL;
    jboolean accepted;
    nk_result result = NK_ERROR_IO;

    if ((flags & ~allowed) != 0U || title == NULL || body == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    env = current_env();
    if (env == NULL) {
        return NK_ERROR_IO;
    }

    /* Fixed app text uses BMP UTF-8, without emoji (platform boundary rules). */
    java_title = (*env)->NewStringUTF(env, title);
    if (clear_exception(env, "notification title") || java_title == NULL) {
        goto cleanup;
    }
    java_body = (*env)->NewStringUTF(env, body);
    if (clear_exception(env, "notification body") || java_body == NULL) {
        goto cleanup;
    }
    accepted = (*env)->CallStaticBooleanMethod(env, bridge.bridge_class, bridge.night_start,
                                               (jint)flags, java_title, java_body);
    if (!clear_exception(env, "nightServiceStart") && accepted == JNI_TRUE) {
        result = NK_OK;
    }

cleanup:
    if (java_body != NULL) {
        (*env)->DeleteLocalRef(env, java_body);
    }
    if (java_title != NULL) {
        (*env)->DeleteLocalRef(env, java_title);
    }
    return result;
}

nk_result nk_android_bridge_night_stop(void) {
    nk_android_accel_request_user_stop();
    if (nk_android_accel_is_active()) {
        (void)__android_log_print(ANDROID_LOG_INFO, NK_BRIDGE_TAG,
                                  "Night service stop deferred until DB flush");
        return NK_OK;
    }
    return nk_android_bridge_night_stop_now();
}

nk_result nk_android_bridge_night_stop_now(void) {
    JNIEnv *env = current_env();

    if (env == NULL) {
        return NK_ERROR_IO;
    }
    (*env)->CallStaticVoidMethod(env, bridge.bridge_class, bridge.night_stop);
    return clear_exception(env, "nightServiceStop") ? NK_ERROR_IO : NK_OK;
}

nk_android_night_state nk_android_bridge_night_state(void) {
    return (nk_android_night_state)atomic_load(&night_state);
}

nk_result nk_android_bridge_notifications_allowed(int *out_allowed) {
    JNIEnv *env;
    jmethodID method;
    jboolean allowed;

    if (out_allowed == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_allowed = 0;
    env = current_env();
    if (env == NULL) {
        return NK_ERROR_IO;
    }
    method = (*env)->GetStaticMethodID(env, bridge.bridge_class, "capability", "(I)Z");
    if (clear_exception(env, "notification capability") || method == NULL) {
        return NK_ERROR_IO;
    }
    /* NativeBridge.CAP_NOTIFICATIONS = 2. */
    allowed = (*env)->CallStaticBooleanMethod(env, bridge.bridge_class, method, (jint)2);
    if (clear_exception(env, "notification capability")) {
        return NK_ERROR_IO;
    }
    *out_allowed = allowed == JNI_TRUE;
    return NK_OK;
}

nk_result nk_android_bridge_schedule_curtain(int schedule_id, int64_t at_ms, int kind,
                                             int timeout_ms, const char *title, const char *body) {
    JNIEnv *env;
    jstring j_title = NULL;
    jstring j_body = NULL;
    jboolean ok;
    nk_result result = NK_ERROR_IO;

    if (title == NULL || body == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    env = current_env();
    if (env == NULL) {
        return NK_ERROR_IO;
    }

    j_title = (*env)->NewStringUTF(env, title);
    if (clear_exception(env, "schedule title") || j_title == NULL) {
        goto cleanup;
    }
    j_body = (*env)->NewStringUTF(env, body);
    if (clear_exception(env, "schedule body") || j_body == NULL) {
        goto cleanup;
    }
    ok = (*env)->CallStaticBooleanMethod(env, bridge.bridge_class, bridge.schedule_fn,
                                         (jint)schedule_id, (jlong)at_ms, (jint)kind, (jint)0,
                                         (jint)timeout_ms, j_title, j_body);
    if (!clear_exception(env, "schedule") && ok == JNI_TRUE) {
        result = NK_OK;
    }

cleanup:
    if (j_body != NULL) {
        (*env)->DeleteLocalRef(env, j_body);
    }
    if (j_title != NULL) {
        (*env)->DeleteLocalRef(env, j_title);
    }
    return result;
}

nk_result nk_android_bridge_cancel_curtain(int schedule_id) {
    JNIEnv *env = current_env();
    if (env == NULL) {
        return NK_ERROR_IO;
    }
    (*env)->CallStaticVoidMethod(env, bridge.bridge_class, bridge.cancel_fn, (jint)schedule_id);
    return clear_exception(env, "cancel") ? NK_ERROR_IO : NK_OK;
}

JNIEXPORT void JNICALL Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnNightService(
    JNIEnv *env, jclass bridge_class, jint state);

JNIEXPORT void JNICALL Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnNightService(
    JNIEnv *env, jclass bridge_class, jint state) {
    static const char *names[] = {"started", "stopped", "failed"};
    (void)env;
    (void)bridge_class;
    if (state < 0 || state > 2) {
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_BRIDGE_TAG,
                                  "Invalid night service state: %d", (int)state);
        return;
    }
    atomic_store(&night_state, (int)state);
    if (state != NK_ANDROID_NIGHT_STARTED) {
        nk_android_accel_request_stop();
    }
    (void)__android_log_print(ANDROID_LOG_INFO, NK_BRIDGE_TAG, "Night service state: %s",
                              names[state]);
}

JNIEXPORT void JNICALL Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnScheduled(
    JNIEnv *env, jclass bridge_class, jint id, jint kind, jint timeout_ms);

JNIEXPORT void JNICALL Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnScheduled(
    JNIEnv *env, jclass bridge_class, jint id, jint kind, jint timeout_ms) {
    (void)env;
    (void)bridge_class;
    (void)id;

    /* NativeBridge.SCHED_CURTAIN_OPEN = 1, SCHED_CURTAIN_CLOSE = 2 */
    if (kind == 1 || kind == 2) {
        bool open_action = (kind == 1);
        time_t now = time(NULL);
        struct tm tm_val;
        (void)localtime_r(&now, &tm_val);
        bool is_weekday = (tm_val.tm_wday >= 1 && tm_val.tm_wday <= 5);

        (void)nk_curtain_init();
        if (timeout_ms > 0 && timeout_ms < 45000) {
            nk_curtain_set_timing_ms(2000, timeout_ms / 2, 3000, 5000);
        }
        nk_result rc = nk_curtain_execute_scheduled(open_action, is_weekday);
        (void)__android_log_print(rc == NK_OK ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, NK_BRIDGE_TAG,
                                  "Scheduled curtain action (open=%d) result=%d",
                                  open_action ? 1 : 0, (int)rc);

        /* 次回の予約を翌日に再登録する */
        nk_curtain_snapshot snap;
        nk_curtain_get_snapshot(&snap);
        sync_curtain_schedule_to_android(&snap.schedule);
    }
}

JNIEXPORT void JNICALL
Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnNotificationAction(
    JNIEnv *env, jclass bridge_class, jint action, jint id, jint timeout_ms);

JNIEXPORT void JNICALL
Java_io_github_mikan32_nemunoki_platform_NativeBridge_nativeOnNotificationAction(
    JNIEnv *env, jclass bridge_class, jint action, jint id, jint timeout_ms) {
    (void)env;
    (void)bridge_class;
    (void)id;
    (void)timeout_ms;
    /* NativeBridge.ACT_STOP_TRACKING = 2. */
    if (action == 2) {
        nk_result result = nk_android_bridge_night_stop();
        (void)__android_log_print(result == NK_OK ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                                  NK_BRIDGE_TAG, "Notification stop request: %d", (int)result);
    } else {
        (void)__android_log_print(ANDROID_LOG_INFO, NK_BRIDGE_TAG,
                                  "Notification action not implemented here: %d", (int)action);
    }
}

/* END NK_ANDROID_BRIDGE_C */
