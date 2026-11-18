#ifndef NK_WAV_READER_H
#define NK_WAV_READER_H

#include "result.h"

#include <stdint.h>

/*
 * sensingへ渡す音声の形式。
 * 16 kHz・モノラル・符号付き16bit PCM。
 */
#define NK_WAV_OUTPUT_SAMPLE_RATE 16000

/*
 * 元のWAVファイルの情報。
 * frame_countはチャンネル数を含まない時間方向のフレーム数。
 */
typedef struct nk_wav_info {
    int sample_rate;
    int channels;
    int bits_per_sample;
    uint64_t frame_count;
} nk_wav_info;

/*
 * 読み取った音声を受け取る関数。
 *
 * samplesは16 kHz・モノラルの音声。
 * sample_countは今回渡すサンプル数。
 *
 * samplesはコールバック実行中だけ有効。
 * 後から使用する場合は受け取り側でコピーする。
 *
 * NK_OK以外を返すと読み取りを停止する。
 */
typedef nk_result (*nk_wav_pcm_callback)(void *user_data, const int16_t *samples, int sample_count);

/*
 * WAVを読み取り、音声サンプルをコールバックへ渡す。
 *
 * 対応入力：
 * PCM形式・16bit・モノラルまたはステレオ。
 *
 * 入力の周波数が異なる場合は16 kHzへ変換し、
 * ステレオの場合はモノラルへ変換する。
 *
 * 成功時に元ファイルの情報をout_infoへ格納する。
 * user_dataの所有権は移動しない。
 */
nk_result nk_wav_replay_file(const char *path, nk_wav_pcm_callback callback, void *user_data,
                             nk_wav_info *out_info);

#endif
