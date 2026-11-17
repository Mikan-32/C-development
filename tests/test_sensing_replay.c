// clang-format off
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "nk_sensing.h"
// clang-format on

// tests/data/README.md の命名規則に合わせたパス（必要に応じて "tests/data/いびき1.wav" 等に変更可）
#define TEST_WAV_PATH        "tests/data/20261116_snore1_web.wav"
#define EXPECTED_SNORE_COUNT 1

typedef struct {
    int snore_count;
    int noise_count;
    int turn_count;
    int epoch_count;
    int total_snore_seconds;
} replay_result_t;

static void on_test_epoch(const nk_epoch_t *epoch, void *user_data) {
    replay_result_t *res = (replay_result_t *)user_data;
    res->epoch_count++;
    res->total_snore_seconds += epoch->snore_s;
}

static void on_test_event(const nk_event_t *event, void *user_data) {
    replay_result_t *res = (replay_result_t *)user_data;
    if (event->type == NK_EVENT_SNORE) {
        res->snore_count++;
    } else if (event->type == NK_EVENT_NOISE) {
        res->noise_count++;
    } else if (event->type == NK_EVENT_TURN) {
        res->turn_count++;
    }
}

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// WAVファイルを読み込み、16kHz / モノラル / 16bit に正規化して nk_sensing に流し込む
static bool replay_wav_file(const char *filepath, replay_result_t *out_res) {
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        printf("[Replay Error] WAVファイルが開けません: %s\n", filepath);
        return false;
    }

    uint8_t riff_hdr[12];
    if (fread(riff_hdr, 1, 12, fp) != 12 || memcmp(riff_hdr, "RIFF", 4) != 0 ||
        memcmp(riff_hdr + 8, "WAVE", 4) != 0) {
        printf("[Replay Error] 不正なWAVヘッダーです\n");
        fclose(fp);
        return false;
    }

    uint16_t audio_format = 0;
    uint16_t num_channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    uint8_t *raw_pcm = NULL;
    uint32_t raw_pcm_bytes = 0;

    // チャンクを走査して fmt と data を取得
    uint8_t chunk_hdr[8];
    while (fread(chunk_hdr, 1, 8, fp) == 8) {
        uint32_t chunk_size = read_u32_le(chunk_hdr + 4);
        if (memcmp(chunk_hdr, "fmt ", 4) == 0 && chunk_size >= 16) {
            uint8_t fmt_buf[64];
            uint32_t to_read =
                (chunk_size < sizeof(fmt_buf)) ? chunk_size : (uint32_t)sizeof(fmt_buf);
            if (fread(fmt_buf, 1, to_read, fp) != to_read) {
                break;
            }
            audio_format = read_u16_le(fmt_buf + 0);
            num_channels = read_u16_le(fmt_buf + 2);
            sample_rate = read_u32_le(fmt_buf + 4);
            bits_per_sample = read_u16_le(fmt_buf + 14);
            if (chunk_size > to_read) {
                fseek(fp, (long)(chunk_size - to_read), SEEK_CUR);
            }
        } else if (memcmp(chunk_hdr, "data", 4) == 0) {
            raw_pcm = (uint8_t *)malloc(chunk_size);
            if (raw_pcm == NULL) {
                break;
            }
            raw_pcm_bytes = (uint32_t)fread(raw_pcm, 1, chunk_size, fp);
            break;
        } else {
            // 不要なチャンク（LIST, JUNK等）をスキップ（パディング考慮）
            long skip = (long)chunk_size + (chunk_size & 1U);
            fseek(fp, skip, SEEK_CUR);
        }
    }
    fclose(fp);

    if (raw_pcm == NULL || audio_format != 1 || bits_per_sample != 16 || num_channels == 0 ||
        sample_rate == 0) {
        printf("[Replay Error] 16-bit PCM WAV の解析に失敗しました (fmt=%u, ch=%u, rate=%u, "
               "bits=%u)\n",
               audio_format, num_channels, sample_rate, bits_per_sample);
        free(raw_pcm);
        return false;
    }

    uint32_t bytes_per_frame = (uint32_t)num_channels * 2U;
    uint32_t total_src_frames = raw_pcm_bytes / bytes_per_frame;

    // 16kHz モノラルへ線形補間リサンプリング
    const uint32_t target_rate = 16000U;
    uint32_t total_dst_frames =
        (uint32_t)(((uint64_t)total_src_frames * target_rate) / sample_rate);
    int16_t *mono_16k = (int16_t *)malloc(total_dst_frames * sizeof(int16_t));
    if (mono_16k == NULL) {
        free(raw_pcm);
        return false;
    }

    const int16_t *src_samples = (const int16_t *)raw_pcm;
    for (uint32_t i = 0; i < total_dst_frames; i++) {
        uint32_t src_idx = (uint32_t)(((uint64_t)i * sample_rate) / target_rate);
        if (src_idx >= total_src_frames) {
            src_idx = total_src_frames - 1;
        }
        // ステレオ等の場合は全チャンネルの平均をとってモノラル化
        int32_t sum = 0;
        for (uint16_t ch = 0; ch < num_channels; ch++) {
            sum += src_samples[src_idx * num_channels + ch];
        }
        mono_16k[i] = (int16_t)(sum / num_channels);
    }
    free(raw_pcm);

    // 1秒（16,000サンプル）単位で nk_sensing に流し込む
    memset(out_res, 0, sizeof(*out_res));
    nk_sensing_init(on_test_epoch, on_test_event, out_res);

    int64_t sim_time_ms = 1000;
    uint32_t offset = 0;
    while (offset + target_rate <= total_dst_frames) {
        nk_sensing_push_audio(sim_time_ms, mono_16k + offset, target_rate);
        offset += target_rate;
        sim_time_ms += 1000;
    }

    // 終端で連続いびき区間を確定させるため、無音5秒をフラッシュとして投入
    int16_t silence[16000];
    memset(silence, 0, sizeof(silence));
    for (int i = 0; i < 5; i++) {
        nk_sensing_push_audio(sim_time_ms, silence, target_rate);
        sim_time_ms += 1000;
    }

    free(mono_16k);
    printf("[Replay Result] %s (%.1f秒) -> SNOREイベント=%d回, いびき秒数=%d秒, NOISE=%d回\n",
           filepath, (double)total_dst_frames / target_rate, out_res->snore_count,
           out_res->total_snore_seconds, out_res->noise_count);
    return true;
}

void setUp(void) {
}

void tearDown(void) {
}

void test_sensing_replay_snore1_matches_index_md(void) {
    replay_result_t result;
    bool ok = replay_wav_file(TEST_WAV_PATH, &result);
    TEST_ASSERT_TRUE_MESSAGE(ok, "tests/data/ に WAV ファイルが配置されているか確認してください");
    TEST_ASSERT_EQUAL_INT(EXPECTED_SNORE_COUNT, result.snore_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sensing_replay_snore1_matches_index_md);
    return UNITY_END();
}
