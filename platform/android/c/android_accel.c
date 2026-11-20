#define _POSIX_C_SOURCE 200809L

#include "android_accel.h"

#include "accel_features.h"
#include "android_bridge.h"
#include "android_recording.h"

#include <android/log.h>
#include <android/looper.h>
#include <android/sensor.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define NK_ACCEL_TAG         "NkAndroid"
#define NK_ACCEL_INTERVAL_US 20000
#define NK_ACCEL_POLL_MS     100
#define NK_ACCEL_QUEUE_ID    1

enum { ACCEL_STOPPED, ACCEL_WAITING, ACCEL_RUNNING, ACCEL_FAILED };

/* thread/joinable/manager/sensor are owned by the SDL main thread. */
static pthread_t worker_thread;
static bool joinable;
static ASensorManager *manager;
static const ASensor *sensor;
static atomic_bool stop_requested = true;
static atomic_bool worker_finished = true;
static atomic_int status = ACCEL_STOPPED;
static atomic_int stop_reason = NK_SESSION_END_SERVICE_KILLED;
static atomic_int worker_result = NK_OK;

typedef struct nk_android_accel_context {
    nk_android_recording *recording;
    int next_idx;
} nk_android_accel_context;

#define NK_ACC_NS              INT64_C(1000000000)
#define NK_ACC_MAX_DELIVERY_NS (INT64_C(10) * NK_ACC_NS)
#define NK_ACC_LIMIT_NS ((int64_t)NK_SENSING_MAX_EPOCHS * NK_SENSING_EPOCH_SECONDS * NK_ACC_NS)

static nk_result boot_time_ns(int64_t *out_ns) {
    struct timespec value;
    if (clock_gettime(CLOCK_BOOTTIME, &value) != 0 || value.tv_sec < 0 ||
        value.tv_sec >= INT64_MAX / NK_ACC_NS || value.tv_nsec < 0 || value.tv_nsec >= NK_ACC_NS) {
        return NK_ERROR_IO;
    }
    *out_ns = (int64_t)value.tv_sec * NK_ACC_NS + value.tv_nsec;
    return NK_OK;
}

static nk_result realtime_ms(int64_t *out_ms) {
    struct timespec value;
    if (clock_gettime(CLOCK_REALTIME, &value) != 0 || value.tv_sec < 0 ||
        value.tv_sec >= (INT64_MAX - NK_ACC_LIMIT_NS / INT64_C(1000000)) / 1000 ||
        value.tv_nsec < 0 || value.tv_nsec >= NK_ACC_NS) {
        return NK_ERROR_IO;
    }
    *out_ms = (int64_t)value.tv_sec * 1000 + value.tv_nsec / INT64_C(1000000);
    return NK_OK;
}

static nk_result receive_second(void *user_data, const nk_sensing_second *second) {
    nk_android_accel_context *context = user_data;
    nk_result result = nk_android_recording_push_second(context->recording, second);
    if (result != NK_OK) {
        return result;
    }
    char hp_text[48] = "NULL";
    char gravity_text[96] = "NULL";
    const char *moving = "NULL";
    int length;

    if (second->acc_hp.has_value) {
        length = snprintf(hp_text, sizeof(hp_text), "%.3f", second->acc_hp.value);
        if (length < 0 || (size_t)length >= sizeof(hp_text)) {
            return NK_ERROR_FORMAT;
        }
        moving = second->acc_hp.value > 15.0 ? "1" : "0";
    }
    if (second->gravity.has_value) {
        length = snprintf(gravity_text, sizeof(gravity_text), "(%.4f,%.4f,%.4f)", second->gravity.x,
                          second->gravity.y, second->gravity.z);
        if (length < 0 || (size_t)length >= sizeof(gravity_text)) {
            return NK_ERROR_FORMAT;
        }
    }
    (void)__android_log_print(ANDROID_LOG_INFO, NK_ACCEL_TAG,
                              "ACC 1s: idx=%d samples=%d acc_hp_mg=%s moving=%s gravity=%s",
                              second->idx, second->acc_sample_count, hp_text, moving, gravity_text);
    context->next_idx = second->idx + 1;
    return NK_OK;
}

static bool wait_for_service(void) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        struct timespec delay = {0, 100000000L};

        if (atomic_load(&stop_requested)) {
            return false;
        }
        if (nk_android_bridge_night_state() == NK_ANDROID_NIGHT_STARTED) {
            return true;
        }
        while (nanosleep(&delay, &delay) != 0) {
            if (errno != EINTR || atomic_load(&stop_requested)) {
                return false;
            }
        }
    }
    return false;
}

static void *read_accelerometer(void *unused) {
    ALooper *looper;
    ASensorEventQueue *queue = NULL;
    nk_accel_features *features = NULL;
    bool enabled = false;
    bool failed = false;
    nk_android_accel_context context = {0};
    bool recording_started = false;
    bool timed_out = false;
    int64_t start_ms = 0;
    int64_t origin_ns = 0;
    int64_t last_timestamp = -1;
    uint64_t total = 0;
    uint64_t late_samples = 0;
    uint64_t invalid_samples = 0;

    (void)unused;

    if (!wait_for_service()) {
        failed = !atomic_load(&stop_requested) ||
                 nk_android_bridge_night_state() == NK_ANDROID_NIGHT_FAILED;
        if (failed) {
            (void)__android_log_print(ANDROID_LOG_ERROR, NK_ACCEL_TAG,
                                      "Accelerometer: foreground service start timed out");
        }
        goto cleanup;
    }
    if (atomic_load(&stop_requested)) {
        goto cleanup;
    }

    looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (looper == NULL) {
        failed = true;
        goto cleanup;
    }
    queue = ASensorManager_createEventQueue(manager, looper, NK_ACCEL_QUEUE_ID, NULL, NULL);
    if (queue == NULL || nk_android_recording_open(&context.recording) != NK_OK ||
        boot_time_ns(&origin_ns) != NK_OK || realtime_ms(&start_ms) != NK_OK ||
        nk_accel_features_create(&features, origin_ns, receive_second, &context) != NK_OK ||
        nk_android_recording_start(context.recording, start_ms) != NK_OK) {
        failed = true;
        goto cleanup;
    }
    recording_started = true;
    if (atomic_load(&stop_requested)) {
        goto cleanup;
    }
    if (ASensorEventQueue_enableSensor(queue, sensor) < 0) {
        failed = true;
        goto cleanup;
    }
    enabled = true;
    if (ASensorEventQueue_setEventRate(queue, sensor, NK_ACCEL_INTERVAL_US) < 0) {
        failed = true;
        goto cleanup;
    }
    atomic_store(&status, ACCEL_RUNNING);
    (void)__android_log_print(
        ANDROID_LOG_INFO, NK_ACCEL_TAG,
        "Accelerometer started: requested_hz=50 interval_us=%d min_delay_us=%d",
        NK_ACCEL_INTERVAL_US, ASensor_getMinDelay(sensor));

    while (!atomic_load(&stop_requested)) {
        ASensorEvent events[32];
        ssize_t received = 0;
        int64_t now_ns;
        int poll_result = ALooper_pollOnce(NK_ACCEL_POLL_MS, NULL, NULL, NULL);

        if (poll_result == ALOOPER_POLL_ERROR) {
            failed = true;
            break;
        }
        while (!atomic_load(&stop_requested) &&
               (received = ASensorEventQueue_getEvents(queue, events, 32)) > 0) {
            for (ssize_t i = 0; i < received; ++i) {
                const ASensorEvent *event = &events[i];
                if (event->type != ASENSOR_TYPE_ACCELEROMETER) {
                    continue;
                }
                if (event->timestamp < origin_ns || event->timestamp <= last_timestamp ||
                    (event->timestamp - origin_ns) / NK_ACC_NS < context.next_idx) {
                    ++late_samples;
                    continue;
                }
                if (event->timestamp - origin_ns >= NK_ACC_LIMIT_NS) {
                    timed_out = true;
                    goto cleanup;
                }
                if (!isfinite(event->acceleration.x) || !isfinite(event->acceleration.y) ||
                    !isfinite(event->acceleration.z)) {
                    ++invalid_samples;
                    continue;
                }
                if (nk_accel_features_push(features, event->timestamp, event->acceleration.x,
                                           event->acceleration.y, event->acceleration.z) != NK_OK) {
                    failed = true;
                    goto cleanup;
                }
                last_timestamp = event->timestamp;
                ++total;
            }
        }
        if (!atomic_load(&stop_requested) && received < 0 && received != -EAGAIN &&
            received != -EINTR) {
            failed = true;
            break;
        }
        if (boot_time_ns(&now_ns) != NK_OK) {
            failed = true;
            break;
        }
        if (now_ns - origin_ns >= NK_ACC_LIMIT_NS) {
            timed_out = true;
            goto cleanup;
        }
        /* Leave up to 10 seconds for sensor delivery before declaring silence. */
        if (now_ns - origin_ns > NK_ACC_MAX_DELIVERY_NS &&
            nk_accel_features_advance(features, now_ns - NK_ACC_MAX_DELIVERY_NS) != NK_OK) {
            failed = true;
            break;
        }
    }

cleanup:
    if (enabled && ASensorEventQueue_disableSensor(queue, sensor) < 0) {
        failed = true;
    }
    if (queue != NULL && ASensorManager_destroyEventQueue(manager, queue) < 0) {
        failed = true;
    }
    if (recording_started) {
        int64_t end_ns;
        nk_session_end_reason reason;
        if (boot_time_ns(&end_ns) != NK_OK) {
            end_ns = last_timestamp >= origin_ns ? last_timestamp + 1 : origin_ns;
            failed = true;
        }
        if (end_ns - origin_ns > NK_ACC_LIMIT_NS) {
            end_ns = origin_ns + NK_ACC_LIMIT_NS;
        }
        if (nk_accel_features_finish(features, end_ns) != NK_OK) {
            failed = true;
        }
        if (nk_android_bridge_night_state() == NK_ANDROID_NIGHT_FAILED) {
            failed = true;
        }
        reason = failed      ? NK_SESSION_END_ERROR
                 : timed_out ? NK_SESSION_END_TIMEOUT_14H
                             : (nk_session_end_reason)atomic_load(&stop_reason);
        if (nk_android_recording_finish(context.recording,
                                        start_ms + (end_ns - origin_ns) / INT64_C(1000000),
                                        reason) != NK_OK) {
            failed = true;
        }
    }
    nk_accel_features_destroy(features);
    if (nk_android_recording_close(&context.recording) != NK_OK) {
        failed = true;
        /* nk_data_close retains the object on failure, allowing one retry. */
        (void)nk_android_recording_close(&context.recording);
    }
    if (late_samples != 0 || invalid_samples != 0) {
        (void)__android_log_print(
            ANDROID_LOG_INFO, NK_ACCEL_TAG, "Accelerometer skipped: late=%llu invalid=%llu",
            (unsigned long long)late_samples, (unsigned long long)invalid_samples);
    }
    /* DB flush/readback and DB close finish before the FGS releases its wake lock. */
    if (nk_android_bridge_night_stop_now() != NK_OK) {
        failed = true;
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_ACCEL_TAG,
                                  "Night service final stop failed after DB cleanup");
    }
    atomic_store(&status, failed ? ACCEL_FAILED : ACCEL_STOPPED);
    atomic_store(&worker_result, failed ? NK_ERROR_IO : NK_OK);
    (void)__android_log_print(failed ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, NK_ACCEL_TAG,
                              "Accelerometer %s: total_samples=%llu", failed ? "failed" : "stopped",
                              (unsigned long long)total);
    atomic_store(&worker_finished, true);
    return NULL;
}

nk_result nk_android_accel_prepare(void) {
    if (joinable) {
        if (!atomic_load(&worker_finished)) {
            return NK_ERROR_ALREADY_RUNNING;
        }
        if (pthread_join(worker_thread, NULL) != 0) {
            return NK_ERROR_IO;
        }
        joinable = false;
    }
    manager = ASensorManager_getInstanceForPackage("io.github.mikan32.nemunoki");
    sensor = manager != NULL ? ASensorManager_getDefaultSensor(manager, ASENSOR_TYPE_ACCELEROMETER)
                             : NULL;
    if (sensor == NULL) {
        atomic_store(&status, ACCEL_FAILED);
        (void)__android_log_print(ANDROID_LOG_ERROR, NK_ACCEL_TAG,
                                  "Accelerometer unavailable: reason=Q02; start rejected");
        return NK_ERROR_IO;
    }
    atomic_store(&stop_requested, false);
    atomic_store(&stop_reason, NK_SESSION_END_SERVICE_KILLED);
    atomic_store(&worker_result, NK_OK);
    atomic_store(&worker_finished, false);
    atomic_store(&status, ACCEL_WAITING);
    if (pthread_create(&worker_thread, NULL, read_accelerometer, NULL) != 0) {
        atomic_store(&stop_requested, true);
        atomic_store(&worker_finished, true);
        atomic_store(&status, ACCEL_FAILED);
        return NK_ERROR_IO;
    }
    joinable = true;
    return NK_OK;
}

void nk_android_accel_request_stop(void) {
    atomic_store(&stop_requested, true);
}

void nk_android_accel_request_user_stop(void) {
    atomic_store(&stop_reason, NK_SESSION_END_USER_STOP);
    nk_android_accel_request_stop();
}

bool nk_android_accel_is_active(void) {
    return !atomic_load(&worker_finished);
}

nk_result nk_android_accel_shutdown(void) {
    nk_android_accel_request_user_stop();
    if (joinable) {
        if (pthread_join(worker_thread, NULL) != 0) {
            return NK_ERROR_IO;
        }
        joinable = false;
    }
    return (nk_result)atomic_load(&worker_result);
}

const char *nk_android_accel_status(void) {
    switch (atomic_load(&status)) {
    case ACCEL_WAITING:
        return "Recording: waiting for service";
    case ACCEL_RUNNING:
        return "Recording: saving epochs";
    case ACCEL_FAILED:
        return "Recording: error; check log";
    default:
        return "Recording: stopped";
    }
}

/* END NK_ANDROID_ACCEL_C */
