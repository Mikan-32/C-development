#ifndef NK_ACCEL_FEATURES_H
#define NK_ACCEL_FEATURES_H

#include "sensing.h"

#include <stdint.h>

typedef struct nk_accel_features nk_accel_features;

/*
 * secondはコールバック実行中だけ有効。
 * 保持する場合は、受け取り側で内容をコピーする。
 */
typedef nk_result (*nk_accel_second_callback)(void *user_data, const nk_sensing_second *second);

/* All calls and callbacks run serially on the sensor owner thread. */
/* Timestamps use one monotonic clock, in nanoseconds; start is second idx=0. */
/*
 * 成功時の*out_featuresは、呼び出し側が
 * nk_accel_features_destroy()で解放する。
 * user_dataの所有権は移動せず、destroyでも解放しない。
 */
nk_result nk_accel_features_create(nk_accel_features **out_features, int64_t start_ns,
                                   nk_accel_second_callback callback, void *user_data);

/* Input is m/s^2. Timestamps must increase; samples before the current second */
/* (already emitted through advance) are rejected without changing the state. */
nk_result nk_accel_features_push(nk_accel_features *features, int64_t timestamp_ns, float x,
                                 float y, float z);

/* Emit completed seconds, including empty seconds. Only advance to a watermark */
/* older than the maximum expected delivery latency. Never repeats a second. */
nk_result nk_accel_features_advance(nk_accel_features *features, int64_t watermark_ns);

/* end_ns is exclusive and must be later than the last accepted sample. */
/* Emit a final partial second if needed. Successful finish is idempotent. */
nk_result nk_accel_features_finish(nk_accel_features *features, int64_t end_ns);

/*
 * featuresを解放する。user_dataは解放しない。
 * 未出力の特徴量を出力する場合は、解放前にfinishを呼ぶ。
 */
void nk_accel_features_destroy(nk_accel_features *features);

#endif
