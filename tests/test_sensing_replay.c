#include "replay.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

typedef struct replay_output {
    int snore_count;
    int epoch_count;
    int snore_seconds;
    int reject_event;
    nk_event_record events[8];
} replay_output;

static replay_output output;
static nk_sensing *active = NULL;

void setUp(void);
void tearDown(void);

static nk_result on_epoch(void *user_data, const nk_epoch_record *epoch) {
    replay_output *out = user_data;

    if (epoch->idx != out->epoch_count) {
        return NK_ERROR_ARGUMENT;
    }

    out->epoch_count++;

    if (epoch->snore_s.has_value) {
        out->snore_seconds += epoch->snore_s.value;
    }

    return NK_OK;
}

static nk_result on_event(void *user_data, const nk_event_record *event) {
    replay_output *out = user_data;

    if (out->reject_event) {
        return NK_ERROR_DATABASE;
    }

    if (event->type == NK_EVENT_SNORE) {
        if (out->snore_count >= 8) {
            return NK_ERROR_ARGUMENT;
        }

        out->events[out->snore_count++] = *event;
    }

    return NK_OK;
}

static nk_sensing_config config_for_test(void) {
    nk_sensing_config config = {0};

    config.start_ms = INT64_C(1794326400000);
    config.mic_used = 1;
    config.move_threshold_mg = 15.0;

    return config;
}

static nk_sensing_callbacks callbacks_for_test(void) {
    nk_sensing_callbacks callbacks = {on_epoch, on_event};

    return callbacks;
}

void setUp(void) {
    output = (replay_output){0};
    active = NULL;
}

void tearDown(void) {
    nk_sensing_destroy(active);
    active = NULL;
}

/*
 * index.mdから期待値を読み取る。
 */
static int expected_count(const char *filename) {
    FILE *file = fopen("tests/data/index.md", "rb");
    char line[1024];
    char marker[160];
    int expected = -1;
    int found = 0;
    int length;

    if (file == NULL) {
        return -1;
    }

    length = snprintf(marker, sizeof(marker), "| %c%s%c |", 96, filename, 96);

    if (length < 0 || (size_t)length >= sizeof(marker)) {
        if (fclose(file) != 0) {
            fprintf(stderr, "Failed to close index.md\n");
        }
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, marker) != NULL) {
            char *last = strrchr(line, '|');
            char extra;

            found++;

            if (last == NULL) {
                expected = -1;
                break;
            }

            *last = '\0';
            last = strrchr(line, '|');

            if (last == NULL || sscanf(last + 1, "%d %c", &expected, &extra) != 1 || expected < 0) {
                expected = -1;
                break;
            }
        }
    }

    if (ferror(file)) {
        expected = -1;
    }

    if (fclose(file) != 0) {
        expected = -1;
    }

    return found == 1 ? expected : -1;
}

static void check_log(const char *filename, const char *path) {
    nk_sensing_config config = config_for_test();
    nk_sensing_callbacks callbacks = callbacks_for_test();
    int expected = expected_count(filename);
    nk_result result;

    TEST_ASSERT_TRUE_MESSAGE(expected >= 0, "index.md entry missing or invalid");

    result = nk_replay_file(path, &config, &callbacks, &output);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    printf("Replay %s: expected=%d actual=%d epochs=%d\n", filename, expected, output.snore_count,
           output.epoch_count);

    TEST_ASSERT_EQUAL_INT(expected, output.snore_count);
}

static void test_replay_quiet_night(void) {
    check_log("quiet_night.bin", "tests/data/quiet_night.bin");

    TEST_ASSERT_EQUAL_INT(2, output.epoch_count);
    TEST_ASSERT_EQUAL_INT(0, output.snore_seconds);
}

static void test_replay_snore_3events(void) {
    static const int starts[] = {2000, 25000, 75000};

    static const int durations[] = {10000, 12000, 15000};

    nk_sensing_config config = config_for_test();
    int i;

    check_log("snore_3events.bin", "tests/data/snore_3events.bin");

    TEST_ASSERT_EQUAL_INT(3, output.snore_count);
    TEST_ASSERT_EQUAL_INT(3, output.epoch_count);
    TEST_ASSERT_EQUAL_INT(37, output.snore_seconds);

    for (i = 0; i < 3; ++i) {
        TEST_ASSERT_TRUE(output.events[i].t_ms == config.start_ms + starts[i]);

        TEST_ASSERT_TRUE(output.events[i].dur_ms == durations[i]);

        TEST_ASSERT_TRUE(output.events[i].session_id == 0);

        TEST_ASSERT_NULL(output.events[i].reason);

        TEST_ASSERT_EQUAL_INT(1, output.events[i].value.has_value);

        TEST_ASSERT_FLOAT_WITHIN(0.001f, -25.0f, (float)output.events[i].value.value);
    }
}

static void start_direct(void) {
    nk_sensing_config config = config_for_test();
    nk_sensing_callbacks callbacks = callbacks_for_test();

    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_create(&active, &config, &callbacks, &output));
}

static void push_direct(int count, int sound_on, int mic_busy) {
    nk_sensing_second second = {0};
    int i;

    second.acc_sample_count = 50;
    second.mic_sample_count = 16000;
    second.acc_hp = (nk_optional_double){1, 1.0};
    second.snore_like = (nk_optional_double){1, 0.6};
    second.snd_db = (nk_optional_double){1, -25.0};
    second.sound_on = sound_on;
    second.mic_busy = mic_busy;

    for (i = 0; i < count; ++i) {
        second.idx = i;

        TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_push_second(active, &second));
    }
}

static void test_nine_seconds_are_not_an_event(void) {
    start_direct();
    push_direct(9, 0, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_finish(active));
    TEST_ASSERT_EQUAL_INT(0, output.snore_count);
    TEST_ASSERT_EQUAL_INT(1, output.epoch_count);
}

static void test_ten_seconds_at_eof_and_finish_is_idempotent(void) {
    nk_sensing_second second = {0};

    start_direct();
    push_direct(10, 0, 0);

    TEST_ASSERT_EQUAL_INT(0, output.snore_count);
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_finish(active));
    TEST_ASSERT_EQUAL_INT(1, output.snore_count);

    TEST_ASSERT_TRUE(output.events[0].dur_ms == INT64_C(10000));

    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_finish(active));
    TEST_ASSERT_EQUAL_INT(1, output.snore_count);
    TEST_ASSERT_EQUAL_INT(1, output.epoch_count);

    second.idx = 10;

    TEST_ASSERT_EQUAL_INT(NK_ERROR_NOT_RUNNING, nk_sensing_push_second(active, &second));
}

static void test_playback_is_excluded(void) {
    start_direct();
    push_direct(30, 1, 0);

    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_finish(active));
    TEST_ASSERT_EQUAL_INT(0, output.snore_count);
    TEST_ASSERT_EQUAL_INT(0, output.snore_seconds);
}

static void test_busy_microphone_is_excluded(void) {
    start_direct();
    push_direct(30, 0, 1);

    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_finish(active));
    TEST_ASSERT_EQUAL_INT(0, output.snore_count);
    TEST_ASSERT_EQUAL_INT(0, output.snore_seconds);
}

static void test_event_failure_stops_processing(void) {
    start_direct();
    push_direct(10, 0, 0);

    output.reject_event = 1;

    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE, nk_sensing_finish(active));

    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE, nk_sensing_finish(active));

    TEST_ASSERT_EQUAL_INT(0, output.snore_count);
}

static void test_missing_file_is_rejected(void) {
    nk_sensing_config config = config_for_test();
    nk_sensing_callbacks callbacks = callbacks_for_test();

    TEST_ASSERT_EQUAL_INT(
        NK_ERROR_IO, nk_replay_file("tests/data/not-present.bin", &config, &callbacks, &output));
}

static void test_truncated_file_is_rejected(void) {
    nk_sensing_config config = config_for_test();
    nk_sensing_callbacks callbacks = callbacks_for_test();

    TEST_ASSERT_EQUAL_INT(NK_ERROR_FORMAT,
                          nk_replay_file("tests/data/truncated.bin", &config, &callbacks, &output));
}

static void test_extra_bytes_are_rejected(void) {
    nk_sensing_config config = config_for_test();
    nk_sensing_callbacks callbacks = callbacks_for_test();

    TEST_ASSERT_EQUAL_INT(NK_ERROR_FORMAT,
                          nk_replay_file("tests/data/trailing.bin", &config, &callbacks, &output));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_replay_quiet_night);
    RUN_TEST(test_replay_snore_3events);
    RUN_TEST(test_nine_seconds_are_not_an_event);
    RUN_TEST(test_ten_seconds_at_eof_and_finish_is_idempotent);
    RUN_TEST(test_playback_is_excluded);
    RUN_TEST(test_busy_microphone_is_excluded);
    RUN_TEST(test_event_failure_stops_processing);
    RUN_TEST(test_missing_file_is_rejected);
    RUN_TEST(test_truncated_file_is_rejected);
    RUN_TEST(test_extra_bytes_are_rejected);

    return UNITY_END();
}
