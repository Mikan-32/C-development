#include "unity.h"
#include "wav_reader.h"

#include <stdint.h>
#include <stdio.h>

static const char *fixture_path = "nk_wav_reader_fixture.wav";
static int fixture_created;

typedef struct nk_wav_test_output {
    uint64_t sample_count;
    int unexpected_sample;
    int reject_input;
} nk_wav_test_output;

static nk_wav_test_output output;

void setUp(void);
void tearDown(void);

static void write_u16_le(unsigned char *p, uint16_t value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void write_u32_le(unsigned char *p, uint32_t value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

/* One second of 44.1 kHz stereo PCM: left=1000, right=3000. */
static nk_result create_fixture(void) {
    unsigned char header[44] = {'R', 'I', 'F', 'F', 0,   0,   0,   0, 'W', 'A', 'V',
                                'E', 'f', 'm', 't', ' ', 0,   0,   0, 0,   0,   0,
                                0,   0,   0,   0,   0,   0,   0,   0, 0,   0,   0,
                                0,   0,   0,   'd', 'a', 't', 'a', 0, 0,   0,   0};
    const unsigned char frame[4] = {0xE8, 0x03, 0xB8, 0x0B};
    FILE *file = NULL;
    nk_result result = NK_OK;
    int i;

#if defined(_WIN32)
    if (fopen_s(&file, fixture_path, "wb") != 0) {
        return NK_ERROR_IO;
    }
#else
    file = fopen(fixture_path, "wb");
    if (file == NULL) {
        return NK_ERROR_IO;
    }
#endif
    fixture_created = 1;

    write_u32_le(header + 4, 36U + 44100U * 4U);
    write_u32_le(header + 16, 16);
    write_u16_le(header + 20, 1);
    write_u16_le(header + 22, 2);
    write_u32_le(header + 24, 44100);
    write_u32_le(header + 28, 44100U * 4U);
    write_u16_le(header + 32, 4);
    write_u16_le(header + 34, 16);
    write_u32_le(header + 40, 44100U * 4U);

    if (fwrite(header, 1, sizeof(header), file) != sizeof(header)) {
        result = NK_ERROR_IO;
    }

    for (i = 0; i < 44100 && result == NK_OK; ++i) {
        if (fwrite(frame, 1, sizeof(frame), file) != sizeof(frame)) {
            result = NK_ERROR_IO;
        }
    }

    if (fclose(file) != 0) {
        result = NK_ERROR_IO;
    }

    return result;
}

static nk_result receive_pcm(void *user_data, const int16_t *samples, int sample_count) {
    nk_wav_test_output *out = user_data;
    int i;

    if (out->reject_input) {
        return NK_ERROR_DATABASE;
    }

    if (samples == NULL || sample_count <= 0) {
        return NK_ERROR_ARGUMENT;
    }

    out->sample_count += (uint64_t)sample_count;

    for (i = 0; i < sample_count; ++i) {
        if (samples[i] != 2000) {
            out->unexpected_sample = 1;
        }
    }

    return NK_OK;
}

void setUp(void) {
    output = (nk_wav_test_output){0};
    fixture_created = 0;

    TEST_ASSERT_EQUAL_INT(NK_OK, create_fixture());
}

void tearDown(void) {
    if (fixture_created) {
        TEST_ASSERT_EQUAL_INT(0, remove(fixture_path));
        fixture_created = 0;
    }
}

static void test_stereo_wav_is_converted_to_mono_16khz(void) {
    nk_wav_info info = {0};
    nk_result result;

    result = nk_wav_replay_file(fixture_path, receive_pcm, &output, &info);

    TEST_ASSERT_EQUAL_INT(NK_OK, result);

    TEST_ASSERT_EQUAL_INT(44100, info.sample_rate);
    TEST_ASSERT_EQUAL_INT(2, info.channels);
    TEST_ASSERT_EQUAL_INT(16, info.bits_per_sample);

    TEST_ASSERT_TRUE(info.frame_count == UINT64_C(44100));
    TEST_ASSERT_TRUE(output.sample_count == UINT64_C(16000));
    TEST_ASSERT_EQUAL_INT(0, output.unexpected_sample);

    printf("Synthetic WAV: 44100 Hz stereo -> 16000 Hz mono, samples=%llu\n",
           (unsigned long long)output.sample_count);
}

static void test_callback_failure_is_returned(void) {
    nk_wav_info info = {0};
    nk_result result;

    output.reject_input = 1;

    result = nk_wav_replay_file(fixture_path, receive_pcm, &output, &info);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_DATABASE, result);
    TEST_ASSERT_TRUE(output.sample_count == 0);
    TEST_ASSERT_TRUE(info.frame_count == 0);
}

static void test_missing_wav_is_rejected(void) {
    nk_wav_info info = {0};
    nk_result result;

    TEST_ASSERT_EQUAL_INT(0, remove(fixture_path));
    fixture_created = 0;

    result = nk_wav_replay_file(fixture_path, receive_pcm, &output, &info);

    TEST_ASSERT_EQUAL_INT(NK_ERROR_IO, result);
    TEST_ASSERT_TRUE(output.sample_count == 0);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_stereo_wav_is_converted_to_mono_16khz);
    RUN_TEST(test_callback_failure_is_returned);
    RUN_TEST(test_missing_wav_is_rejected);

    return UNITY_END();
}
