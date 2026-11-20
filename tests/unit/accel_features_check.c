#include "accel_features.h"
#include "unity.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define NS INT64_C(1000000000)

static nk_accel_features *features;
static nk_sensing *pipeline;
static nk_sensing_second seconds[64];
static int second_count;
static int reject_output;
static nk_epoch_record last_epoch;
static int epoch_count;

void setUp(void);
void tearDown(void);

static nk_result on_second(void *user_data, const nk_sensing_second *second) {
    (void)user_data;
    if (reject_output) {
        return NK_ERROR_DATABASE;
    }
    if (second_count >= 64) {
        return NK_ERROR_FORMAT;
    }
    seconds[second_count++] = *second;
    return pipeline != NULL ? nk_sensing_push_second(pipeline, second) : NK_OK;
}

static nk_result on_epoch(void *user_data, const nk_epoch_record *epoch) {
    (void)user_data;
    last_epoch = *epoch;
    ++epoch_count;
    return NK_OK;
}

static nk_result on_event(void *user_data, const nk_event_record *event) {
    (void)user_data;
    (void)event;
    return NK_OK;
}

void setUp(void) {
    features = NULL;
    pipeline = NULL;
    second_count = 0;
    reject_output = 0;
    epoch_count = 0;
    memset(seconds, 0, sizeof(seconds));
    last_epoch = (nk_epoch_record){0};
}

void tearDown(void) {
    nk_accel_features_destroy(features);
    nk_sensing_destroy(pipeline);
}

static void create_features(int64_t start_ns) {
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_create(&features, start_ns, on_second, NULL));
}

static void push_stationary_second(int second, int samples) {
    for (int i = 0; i < samples; ++i) {
        int64_t timestamp = (int64_t)second * NS + (int64_t)i * NS / samples;
        TEST_ASSERT_EQUAL_INT(NK_OK,
                              nk_accel_features_push(features, timestamp, 0.0f, 0.0f, 9.80665f));
    }
}

static void test_stationary_gravity_and_real_sample_counts(void) {
    create_features(0);
    push_stationary_second(0, 53);
    push_stationary_second(1, 52);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, 2 * NS));
    TEST_ASSERT_EQUAL_INT(2, second_count);
    TEST_ASSERT_EQUAL_INT(53, seconds[0].acc_sample_count);
    TEST_ASSERT_EQUAL_INT(52, seconds[1].acc_sample_count);
    TEST_ASSERT_EQUAL_INT(1, seconds[1].acc_hp.has_value);
    TEST_ASSERT_TRUE(fabs(seconds[1].acc_hp.value) < 1.0e-9);
    TEST_ASSERT_EQUAL_INT(1, seconds[1].gravity.has_value);
    TEST_ASSERT_TRUE(fabs(seconds[1].gravity.z - 1.0) < 1.0e-9);
    TEST_ASSERT_TRUE(fabs(seconds[1].gravity.x) < 1.0e-9);
    TEST_ASSERT_EQUAL_INT(0, seconds[1].mic_sample_count);
    TEST_ASSERT_EQUAL_INT(0, seconds[1].snd_db.has_value);
}

static void test_half_second_mean_is_removed_and_converted_to_mg(void) {
    create_features(0);
    for (int i = 0; i < 50; ++i) {
        TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_push(features, i * INT64_C(20000000),
                                                            i >= 25 ? 1.0f : 0.0f, 0.0f, 9.80665f));
    }
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, NS));
    /* In the second half, the 25-sample mean goes from 1/25 to 25/25. */
    TEST_ASSERT_TRUE(fabs(seconds[0].acc_hp.value - 0.48 * 1000.0 / 9.80665) < 1.0e-8);
    TEST_ASSERT_TRUE(seconds[0].acc_hp.value > 15.0);
    double norm = sqrt(seconds[0].gravity.x * seconds[0].gravity.x +
                       seconds[0].gravity.y * seconds[0].gravity.y +
                       seconds[0].gravity.z * seconds[0].gravity.z);
    TEST_ASSERT_TRUE(fabs(norm - 1.0) < 1.0e-9);
}

static void test_gap_seconds_are_missing_and_gravity_rewarms(void) {
    create_features(0);
    push_stationary_second(0, 50);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_push(features, 3 * NS, 0, 0, 9.80665f));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, 3 * NS + NS / 10));
    TEST_ASSERT_EQUAL_INT(4, second_count);
    for (int i = 1; i <= 2; ++i) {
        TEST_ASSERT_EQUAL_INT(i, seconds[i].idx);
        TEST_ASSERT_EQUAL_INT(0, seconds[i].acc_sample_count);
        TEST_ASSERT_EQUAL_INT(0, seconds[i].acc_hp.has_value);
        TEST_ASSERT_EQUAL_INT(0, seconds[i].gravity.has_value);
    }
    TEST_ASSERT_EQUAL_INT(1, seconds[3].acc_sample_count);
    TEST_ASSERT_EQUAL_INT(0, seconds[3].acc_hp.has_value);
}

static void test_partial_second_is_not_padded_and_finish_is_idempotent(void) {
    create_features(0);
    for (int i = 0; i < 15; ++i) {
        TEST_ASSERT_EQUAL_INT(
            NK_OK, nk_accel_features_push(features, i * INT64_C(20000000), 0, 0, 9.80665f));
    }
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, 3 * NS / 10));
    TEST_ASSERT_EQUAL_INT(1, second_count);
    TEST_ASSERT_EQUAL_INT(15, seconds[0].acc_sample_count);
    TEST_ASSERT_EQUAL_INT(0, seconds[0].acc_hp.has_value);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, 3 * NS / 10));
    TEST_ASSERT_EQUAL_INT(1, second_count);
    TEST_ASSERT_EQUAL_INT(NK_ERROR_NOT_RUNNING,
                          nk_accel_features_push(features, NS, 0, 0, 9.80665f));
}

static void test_watermark_emits_missing_seconds_and_rejects_late_samples(void) {
    create_features(0);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_advance(features, 2 * NS));
    TEST_ASSERT_EQUAL_INT(2, second_count);
    TEST_ASSERT_EQUAL_INT(0, seconds[0].acc_sample_count);
    TEST_ASSERT_EQUAL_INT(0, seconds[1].acc_hp.has_value);
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_accel_features_push(features, NS, 0, 0, 9.80665f));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_push(features, 2 * NS, 0, 0, 9.80665f));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, 3 * NS));
    TEST_ASSERT_EQUAL_INT(3, second_count);
    TEST_ASSERT_EQUAL_INT(1, seconds[2].acc_sample_count);
}

static void test_callback_failure_is_terminal(void) {
    create_features(0);
    reject_output = 1;
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE, nk_accel_features_advance(features, NS));
    reject_output = 0;
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE, nk_accel_features_finish(features, NS));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE,
                          nk_accel_features_push(features, 2 * NS, 0, 0, 9.80665f));
    TEST_ASSERT_EQUAL_INT(0, second_count);
}

static void test_invalid_input_does_not_count_as_a_sample(void) {
    create_features(NS);
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_accel_features_push(features, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_accel_features_push(features, NS, NAN, 0, 0));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_push(features, NS, 0, 0, 9.80665f));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_accel_features_push(features, NS, 0, 0, 9.80665f));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_accel_features_finish(features, NS));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, NS + NS / 2));
    TEST_ASSERT_EQUAL_INT(1, seconds[0].acc_sample_count);
}

static void test_zero_gravity_has_no_invented_direction(void) {
    create_features(0);
    for (int i = 0; i < 50; ++i) {
        TEST_ASSERT_EQUAL_INT(NK_OK,
                              nk_accel_features_push(features, i * INT64_C(20000000), 0, 0, 0));
    }
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, NS));
    TEST_ASSERT_EQUAL_INT(1, seconds[0].acc_hp.has_value);
    TEST_ASSERT_TRUE(fabs(seconds[0].acc_hp.value) < 1.0e-9);
    TEST_ASSERT_EQUAL_INT(0, seconds[0].gravity.has_value);
}

static void test_53_hz_input_connects_to_existing_epoch_aggregation(void) {
    nk_sensing_config config = {.start_ms = 1000, .move_threshold_mg = 15.0};
    nk_sensing_callbacks callbacks = {on_epoch, on_event};
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_create(&pipeline, &config, &callbacks, NULL));
    create_features(0);
    for (int i = 0; i < 30; ++i) {
        push_stationary_second(i, 53);
    }
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, 30 * NS));
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_finish(pipeline));
    TEST_ASSERT_EQUAL_INT(30, second_count);
    TEST_ASSERT_EQUAL_INT(1, epoch_count);
    TEST_ASSERT_TRUE(fabs(last_epoch.acc_cov - 1.0) < 1.0e-9);
    TEST_ASSERT_EQUAL_INT(1, last_epoch.move_n.has_value);
    TEST_ASSERT_EQUAL_INT(0, last_epoch.move_n.value);
    TEST_ASSERT_EQUAL_INT(0, last_epoch.flags & NK_EPF_GAP_ACC);
}

static void test_empty_session_and_limits(void) {
    create_features(0);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_accel_features_finish(features, 0));
    TEST_ASSERT_EQUAL_INT(0, second_count);
    nk_accel_features_destroy(features);
    features = NULL;
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT,
                          nk_accel_features_create(&features, INT64_MAX, on_second, NULL));
    create_features(0);
    int64_t max_ns = (int64_t)NK_SENSING_MAX_EPOCHS * NK_SENSING_EPOCH_SECONDS * NS;
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT,
                          nk_accel_features_push(features, max_ns, 0, 0, 9.80665f));
    TEST_ASSERT_EQUAL_INT(NK_ERROR_ARGUMENT, nk_accel_features_advance(features, max_ns + 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stationary_gravity_and_real_sample_counts);
    RUN_TEST(test_half_second_mean_is_removed_and_converted_to_mg);
    RUN_TEST(test_gap_seconds_are_missing_and_gravity_rewarms);
    RUN_TEST(test_partial_second_is_not_padded_and_finish_is_idempotent);
    RUN_TEST(test_watermark_emits_missing_seconds_and_rejects_late_samples);
    RUN_TEST(test_callback_failure_is_terminal);
    RUN_TEST(test_invalid_input_does_not_count_as_a_sample);
    RUN_TEST(test_zero_gravity_has_no_invented_direction);
    RUN_TEST(test_53_hz_input_connects_to_existing_epoch_aggregation);
    RUN_TEST(test_empty_session_and_limits);
    return UNITY_END();
}
