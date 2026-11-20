#define _POSIX_C_SOURCE 200809L

#include "android_accel.h"

#include "android_bridge.h"

#include <android/log.h>
#include <android/looper.h>
#include <android/sensor.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
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

static void log_count(int64_t second, unsigned int count, bool partial) {
    if (count != 0U) {
        (void)__android_log_print(ANDROID_LOG_INFO, NK_ACCEL_TAG,
                                  "Accelerometer: sensor_second=%lld samples=%u partial=%d",
                                  (long long)second, count, partial ? 1 : 0);
    }
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
    bool enabled = false;
    bool failed = false;
    bool first_second = true;
    int64_t second = -1;
    int64_t last_timestamp = -1;
    unsigned int count = 0;
    uint64_t total = 0;

    (void)unused;

    if (!wait_for_service()) {
        failed = !atomic_load(&stop_requested);
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
    if (queue == NULL || ASensorEventQueue_enableSensor(queue, sensor) < 0) {
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
        int poll_result = ALooper_pollOnce(NK_ACCEL_POLL_MS, NULL, NULL, NULL);

        if (poll_result == ALOOPER_POLL_ERROR) {
            failed = true;
            break;
        }
        /* Bounded batches keep stop requests responsive even under a backlog. */
        while (!atomic_load(&stop_requested) &&
               (received = ASensorEventQueue_getEvents(queue, events, 32)) > 0) {
            for (ssize_t i = 0; i < received; ++i) {
                int64_t event_second;

                if (events[i].type != ASENSOR_TYPE_ACCELEROMETER ||
                    events[i].timestamp <= last_timestamp) {
                    continue;
                }
                last_timestamp = events[i].timestamp;
                event_second = events[i].timestamp / INT64_C(1000000000);
                if (second >= 0 && event_second != second) {
                    log_count(second, count, first_second);
                    first_second = false;
                    count = 0;
                }
                second = event_second;
                ++count;
                ++total;
            }
        }
        if (!atomic_load(&stop_requested) && received < 0 && received != -EAGAIN &&
            received != -EINTR) {
            failed = true;
            break;
        }
    }

cleanup:
    log_count(second, count, true);
    if (enabled && ASensorEventQueue_disableSensor(queue, sensor) < 0) {
        failed = true;
    }
    if (queue != NULL && ASensorManager_destroyEventQueue(manager, queue) < 0) {
        failed = true;
    }
    atomic_store(&status, failed ? ACCEL_FAILED : ACCEL_STOPPED);
    (void)__android_log_print(failed ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, NK_ACCEL_TAG,
                              "Accelerometer %s: total_samples=%llu", failed ? "failed" : "stopped",
                              (unsigned long long)total);
    if (failed) {
        /* Avoid leaving the foreground service running after a sensor failure. */
        (void)nk_android_bridge_night_stop();
    }
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

nk_result nk_android_accel_shutdown(void) {
    nk_android_accel_request_stop();
    if (joinable) {
        if (pthread_join(worker_thread, NULL) != 0) {
            return NK_ERROR_IO;
        }
        joinable = false;
    }
    return NK_OK;
}

const char *nk_android_accel_status(void) {
    switch (atomic_load(&status)) {
    case ACCEL_WAITING:
        return "Accelerometer: waiting for service";
    case ACCEL_RUNNING:
        return "Accelerometer: receiving samples";
    case ACCEL_FAILED:
        return "Accelerometer: error; check log";
    default:
        return "Accelerometer: stopped";
    }
}

/* END NK_ANDROID_ACCEL_C */
