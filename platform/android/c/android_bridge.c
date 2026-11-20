#include "android_bridge.h"

#include <SDL3/SDL_system.h>
#include <android/log.h>
#include <jni.h>
#include <stdatomic.h>
#include <stdbool.h>

#define NK_BRIDGE_TAG "NkAndroid"

typedef struct nk_android_bridge_cache {
    jclass bridge_class;
    jmethodID db_key;
    jmethodID night_start;
    jmethodID night_stop;
} nk_android_bridge_cache;

static nk_android_bridge_cache bridge;

static atomic_int night_state = NK_ANDROID_NIGHT_STOPPED;

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

    (void)__android_log_print(ANDROID_LOG_INFO, NK_BRIDGE_TAG, "NativeBridge ready");

    return JNI_VERSION_1_6;

failed:
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

    /* Fixed app text uses BMP UTF-8, without emoji. */
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

    (void)__android_log_print(ANDROID_LOG_INFO, NK_BRIDGE_TAG, "Night service state: %s",
                              names[state]);
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
