#include "accel_features.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

#define NK_ACC_SECOND_NS         INT64_C(1000000000)
#define NK_ACC_GRAVITY_WINDOW_NS INT64_C(500000000)
#define NK_ACC_WINDOW_CAPACITY   512
#define NK_ACC_STANDARD_GRAVITY  9.80665
#define NK_ACC_MAX_SECONDS       (NK_SENSING_MAX_EPOCHS * NK_SENSING_EPOCH_SECONDS)

typedef struct nk_accel_point {
    int64_t timestamp_ns;
    float x;
    float y;
    float z;
} nk_accel_point;

struct nk_accel_features {
    int64_t start_ns;
    int64_t last_ns;
    int64_t warm_start_ns;
    nk_accel_second_callback callback;
    void *user_data;
    nk_accel_point window[NK_ACC_WINDOW_CAPACITY];
    int head;
    int window_count;
    double window_x;
    double window_y;
    double window_z;
    nk_sensing_second second;
    int hp_count;
    double hp_sum;
    double gravity_x;
    double gravity_y;
    double gravity_z;
    nk_result failure;
    int finished;
};

static nk_result emit_second(nk_accel_features *features) {
    nk_sensing_second output = features->second;
    nk_result result;
    int next_idx = output.idx + 1;

    if (features->hp_count > 0) {
        double gx = features->gravity_x / features->hp_count;
        double gy = features->gravity_y / features->hp_count;
        double gz = features->gravity_z / features->hp_count;
        double norm = sqrt(gx * gx + gy * gy + gz * gz);

        output.acc_hp.has_value = 1;
        output.acc_hp.value = features->hp_sum / features->hp_count;
        if (norm > 1.0e-9) {
            output.gravity.has_value = 1;
            output.gravity.x = gx / norm;
            output.gravity.y = gy / norm;
            output.gravity.z = gz / norm;
        }
    }
    result = features->callback(features->user_data, &output);
    if (result != NK_OK) {
        features->failure = result;
        return result;
    }
    features->second = (nk_sensing_second){0};
    features->second.idx = next_idx;
    features->hp_count = 0;
    features->hp_sum = 0.0;
    features->gravity_x = 0.0;
    features->gravity_y = 0.0;
    features->gravity_z = 0.0;
    return NK_OK;
}

static int64_t second_start(const nk_accel_features *features) {
    return features->start_ns + (int64_t)features->second.idx * NK_ACC_SECOND_NS;
}

nk_result nk_accel_features_create(nk_accel_features **out_features, int64_t start_ns,
                                   nk_accel_second_callback callback, void *user_data) {
    nk_accel_features *features;
    if (out_features == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_features = NULL;
    if (start_ns < 0 || start_ns > INT64_MAX - NK_ACC_MAX_SECONDS * NK_ACC_SECOND_NS ||
        callback == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    features = calloc(1, sizeof(*features));
    if (features == NULL) {
        return NK_ERROR_MEMORY;
    }
    features->start_ns = start_ns;
    features->last_ns = -1;
    features->warm_start_ns = -1;
    features->callback = callback;
    features->user_data = user_data;
    *out_features = features;
    return NK_OK;
}

nk_result nk_accel_features_advance(nk_accel_features *features, int64_t watermark_ns) {
    if (features == NULL || watermark_ns < features->start_ns ||
        watermark_ns > features->start_ns + NK_ACC_MAX_SECONDS * NK_ACC_SECOND_NS) {
        return NK_ERROR_ARGUMENT;
    }
    if (features->failure != NK_OK) {
        return features->failure;
    }
    if (features->finished) {
        return NK_ERROR_NOT_RUNNING;
    }
    while (features->second.idx < NK_ACC_MAX_SECONDS &&
           second_start(features) + NK_ACC_SECOND_NS <= watermark_ns) {
        nk_result result = emit_second(features);
        if (result != NK_OK) {
            return result;
        }
    }
    return NK_OK;
}

nk_result nk_accel_features_push(nk_accel_features *features, int64_t timestamp_ns, float x,
                                 float y, float z) {
    nk_accel_point *point;
    nk_result result;
    double gx;
    double gy;
    double gz;
    double dx;
    double dy;
    double dz;

    if (features == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    if (features->failure != NK_OK) {
        return features->failure;
    }
    if (features->finished) {
        return NK_ERROR_NOT_RUNNING;
    }
    if (timestamp_ns <= features->last_ns || timestamp_ns < second_start(features) ||
        timestamp_ns >= features->start_ns + NK_ACC_MAX_SECONDS * NK_ACC_SECOND_NS ||
        !isfinite(x) || !isfinite(y) || !isfinite(z)) {
        return NK_ERROR_ARGUMENT;
    }
    result = nk_accel_features_advance(features, timestamp_ns);
    if (result != NK_OK) {
        return result;
    }
    while (features->window_count > 0) {
        point = &features->window[features->head];
        if (timestamp_ns - point->timestamp_ns < NK_ACC_GRAVITY_WINDOW_NS) {
            break;
        }
        features->window_x -= point->x;
        features->window_y -= point->y;
        features->window_z -= point->z;
        features->head = (features->head + 1) % NK_ACC_WINDOW_CAPACITY;
        --features->window_count;
    }
    if (features->window_count == NK_ACC_WINDOW_CAPACITY ||
        features->second.acc_sample_count == INT_MAX) {
        features->failure = NK_ERROR_FORMAT;
        return features->failure;
    }
    if (features->window_count == 0) {
        features->warm_start_ns = timestamp_ns;
        features->window_x = features->window_y = features->window_z = 0.0;
    }
    point = &features->window[(features->head + features->window_count) % NK_ACC_WINDOW_CAPACITY];
    *point = (nk_accel_point){timestamp_ns, x, y, z};
    ++features->window_count;
    features->window_x += x;
    features->window_y += y;
    features->window_z += z;
    features->last_ns = timestamp_ns;
    ++features->second.acc_sample_count;

    /* Do not invent a full gravity window at startup or after a long gap. */
    if (timestamp_ns - features->warm_start_ns < NK_ACC_GRAVITY_WINDOW_NS) {
        return NK_OK;
    }
    gx = features->window_x / features->window_count;
    gy = features->window_y / features->window_count;
    gz = features->window_z / features->window_count;
    dx = (double)x - gx;
    dy = (double)y - gy;
    dz = (double)z - gz;
    features->hp_sum += sqrt(dx * dx + dy * dy + dz * dz) * 1000.0 / NK_ACC_STANDARD_GRAVITY;
    ++features->hp_count;
    features->gravity_x += gx;
    features->gravity_y += gy;
    features->gravity_z += gz;
    return NK_OK;
}

nk_result nk_accel_features_finish(nk_accel_features *features, int64_t end_ns) {
    nk_result result;
    if (features == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    if (features->failure != NK_OK) {
        return features->failure;
    }
    if (features->finished) {
        return NK_OK;
    }
    if (end_ns <= features->last_ns || end_ns < second_start(features)) {
        return NK_ERROR_ARGUMENT;
    }
    result = nk_accel_features_advance(features, end_ns);
    if (result != NK_OK) {
        return result;
    }
    if (second_start(features) < end_ns) {
        result = emit_second(features);
        if (result != NK_OK) {
            return result;
        }
    }
    features->finished = 1;
    return NK_OK;
}

void nk_accel_features_destroy(nk_accel_features *features) {
    free(features);
}

/* END NK_ACCEL_FEATURES_C */
