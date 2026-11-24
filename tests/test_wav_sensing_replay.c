#include "audio_features.h"
#include "sensing.h"
#include "unity.h"
#include "wav_reader.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NK_WAV_TEST_PI 3.14159265358979323846

/* UTF-8で「いびき1.wav」を指定する。 */
static const char *wav_filename = "\xE3\x81\x84\xE3\x81\xB3\xE3\x81\x8D"
                                  "1.wav";

static const char *wav_path = "tests/data/"
                              "\xE3\x81\x84\xE3\x81\xB3\xE3\x81\x8D"
                              "1.wav";

typedef struct nk_wav_replay_output {
    nk_audio_features *features;
    nk_sensing *sensing;
    uint64_t pcm_samples;
    uint64_t feature_samples;
    int second_count;
    int epoch_count;
    int snore_count;
    nk_event_record snores[8];
    nk_epoch_record last_epoch;
} nk_wav_replay_output;

static nk_wav_replay_output output;

void setUp(void);
void tearDown(void);

static nk_result receive_epoch(void *user_data, const nk_epoch_record *epoch) {
    nk_wav_replay_output *out = user_data;

    /*
     * WAVには加速度データがない。
     * 加速度の取得率が0で、欠損フラグが付くことも確認する。
     */
    if (epoch->idx != out->epoch_count || epoch->acc_cov != 0.0 ||
        (epoch->flags & NK_EPF_GAP_ACC) == 0U) {
        return NK_ERROR_ARGUMENT;
    }

    out->last_epoch = *epoch;
    out->epoch_count++;
    return NK_OK;
}

static nk_result receive_event(void *user_data, const nk_event_record *event) {
    nk_wav_replay_output *out = user_data;

    if (event->type == NK_EVENT_SNORE) {
        if (out->snore_count >= 8) {
            return NK_ERROR_ARGUMENT;
        }

        out->snores[out->snore_count++] = *event;
    }

    return NK_OK;
}

static nk_result receive_second(void *user_data, const nk_sensing_second *second) {
    nk_wav_replay_output *out = user_data;

    if (second->idx != out->second_count || second->mic_sample_count <= 0 ||
        second->mic_sample_count > NK_SENSING_MIC_HZ || second->acc_sample_count != 0 ||
        second->acc_hp.has_value) {
        return NK_ERROR_ARGUMENT;
    }

    out->feature_samples += (uint64_t)second->mic_sample_count;
    out->second_count++;

    return nk_sensing_push_second(out->sensing, second);
}

static nk_result receive_pcm(void *user_data, const int16_t *samples, int count) {
    nk_wav_replay_output *out = user_data;

    nk_result result = nk_audio_features_push_pcm(out->features, samples, count, 0);

    if (result == NK_OK) {
        out->pcm_samples += (uint64_t)count;
    }

    return result;
}

void setUp(void) {
    /*
     * 固定した試作設定。
     * index.mdに書かれた期待件数とは独立して設定する。
     */
    nk_audio_features_config audio_config = {-50.0, 0.5, 0.5};
    nk_sensing_config sensing_config = {0};
    nk_sensing_callbacks callbacks = {receive_epoch, receive_event};
    nk_result result;

    output = (nk_wav_replay_output){0};

    /* テスト用の時刻原点。録音日時ではない。 */
    sensing_config.start_ms = 0;
    sensing_config.mic_used = 1;
    sensing_config.light_used = 0;
    sensing_config.move_threshold_mg = 15.0;

    result = nk_sensing_create(&output.sensing, &sensing_config, &callbacks, &output);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    result = nk_audio_features_create(&output.features, &audio_config, receive_second, &output);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);
}

void tearDown(void) {
    nk_audio_features_destroy(output.features);
    nk_sensing_destroy(output.sensing);

    output.features = NULL;
    output.sensing = NULL;
}

/*
 * index.mdの対象行を読み、最後の列を期待件数として取得する。
 * 行なし・重複・読み取り失敗・不正な数値の場合は-1。
 */
static int read_expected_count(void) {
    FILE *file = fopen("tests/data/index.md", "rb");
    char line[2048];
    char marker[160];
    int expected = -1;
    int found = 0;
    int length;

    if (file == NULL) {
        return -1;
    }

    length = snprintf(marker, sizeof(marker), "| %c%s%c |", 96, wav_filename, 96);

    if (length < 0 || (size_t)length >= sizeof(marker)) {
        if (fclose(file) != 0) {
            fprintf(stderr, "Failed to close index.md\n");
        }
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, marker) != NULL) {
            char *last = strrchr(line, '|');
            char *end;
            long value;

            found++;

            if (last == NULL) {
                expected = -1;
                break;
            }

            *last = '\0';
            last = strrchr(line, '|');

            if (last == NULL) {
                expected = -1;
                break;
            }

            errno = 0;
            value = strtol(last + 1, &end, 10);

            if (end == last + 1 || errno == ERANGE || value < 0 || value > INT_MAX) {
                expected = -1;
                break;
            }

            while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
                end++;
            }

            if (*end != '\0') {
                expected = -1;
                break;
            }

            expected = (int)value;
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

static void finish_pipeline(void) {
    TEST_ASSERT_EQUAL_INT(NK_OK, nk_audio_features_finish(output.features));

    TEST_ASSERT_EQUAL_INT(NK_OK, nk_sensing_finish(output.sensing));
}

static void test_recording_matches_index(void) {
    nk_wav_info info = {0};
    nk_result result;
    int expected = read_expected_count();
    int i;

    TEST_ASSERT_TRUE_MESSAGE(expected >= 0,
                             "WAV entry in index.md is missing, duplicated or invalid");

    result = nk_wav_replay_file(wav_path, receive_pcm, &output, &info);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    finish_pipeline();

    printf("WAV replay: expected=%d actual=%d "
           "epochs=%d seconds=%d pcm_samples=%llu\n",
           expected, output.snore_count, output.epoch_count, output.second_count,
           (unsigned long long)output.pcm_samples);

    TEST_ASSERT_EQUAL_INT(expected, output.snore_count);

    /* 約27.69秒なので、最後の端数を含めて28回の秒データ。 */
    TEST_ASSERT_EQUAL_INT(1, output.epoch_count);
    TEST_ASSERT_EQUAL_INT(28, output.second_count);
    TEST_ASSERT_TRUE(output.pcm_samples == UINT64_C(443037));

    TEST_ASSERT_TRUE(output.feature_samples == output.pcm_samples);

    for (i = 0; i < output.snore_count; ++i) {
        nk_event_record *event = &output.snores[i];

        printf("SNORE[%d]: start_ms=%lld end_ms=%lld duration_ms=%lld\n", i, (long long)event->t_ms,
               (long long)(event->t_ms + event->dur_ms), (long long)event->dur_ms);

        TEST_ASSERT_TRUE(event->t_ms >= 0);
        TEST_ASSERT_TRUE(event->dur_ms >= INT64_C(10000));
        TEST_ASSERT_TRUE(event->value.has_value);
        TEST_ASSERT_TRUE(isfinite(event->value.value));
        TEST_ASSERT_NULL(event->reason);
    }
}

/* 音声処理へ直接無音を渡し、誤検出しないことを確認する。 */
static void test_silence_does_not_create_snore(void) {
    int16_t samples[256] = {0};
    int i;

    for (i = 0; i < 16000 * 30 / 256; ++i) {
        TEST_ASSERT_EQUAL_INT(NK_OK, receive_pcm(&output, samples, 256));
    }

    finish_pipeline();

    TEST_ASSERT_EQUAL_INT(0, output.snore_count);
    TEST_ASSERT_EQUAL_INT(1, output.epoch_count);
    TEST_ASSERT_TRUE(output.feature_samples == UINT64_C(480000));
    TEST_ASSERT_TRUE(output.last_epoch.snd_db_mean.has_value);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, -120.0f, (float)output.last_epoch.snd_db_mean.value);
}

/* 帯域内でも音量が一定の音を、いびきと誤検出しないことを確認する。 */
static void test_constant_tone_does_not_create_snore(void) {
    int16_t samples[256];
    int position;

    for (position = 0; position < 16000 * 30; position += 256) {
        int i;

        for (i = 0; i < 256; ++i) {
            samples[i] =
                (int16_t)(16000.0 * sin(2.0 * NK_WAV_TEST_PI * 400.0 * (position + i) / 16000.0));
        }

        TEST_ASSERT_EQUAL_INT(NK_OK, receive_pcm(&output, samples, 256));
    }

    finish_pipeline();

    TEST_ASSERT_EQUAL_INT(0, output.snore_count);
    TEST_ASSERT_TRUE(output.feature_samples == UINT64_C(480000));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_recording_matches_index);
    RUN_TEST(test_silence_does_not_create_snore);
    RUN_TEST(test_constant_tone_does_not_create_snore);

    return UNITY_END();
}
