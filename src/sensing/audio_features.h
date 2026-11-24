#ifndef NK_AUDIO_FEATURES_H
#define NK_AUDIO_FEATURES_H

#include "result.h"
#include "sensing.h"

#include <stdint.h>

typedef struct nk_audio_features nk_audio_features;

/*
 * 音声判定の試作パラメータ。
 *
 * AI-1には具体的な値が未定のため、
 * 呼び出し側から明示的に指定する。
 */
typedef struct nk_audio_features_config {
    /*
     * 判定に使用する最低音量。単位はdBFS。
     */
    double min_sound_db;

    /*
     * 100〜1000 Hz帯域のエネルギー比率の下限。
     * 範囲は0〜1。
     */
    double min_band_ratio;

    /*
     * 2〜6秒周期の繰り返しの強さの下限。
     * 範囲は0〜1。
     */
    double min_periodicity;
} nk_audio_features_config;

/*
 * 1秒分の特徴量を受け取る関数。
 *
 * secondはコールバック実行中だけ有効。
 * 後から使用する場合は受け取り側でコピーする。
 *
 * NK_OK以外を返すと処理を停止する。
 */
typedef nk_result (*nk_audio_second_callback)(void *user_data, const nk_sensing_second *second);

/*
 * 音声特徴量の処理オブジェクトを作成する。
 *
 * configの内容は内部へコピーする。
 * user_dataの所有権は移動しない。
 *
 * 成功時に返される*out_featuresは、
 * 呼び出し側がnk_audio_features_destroy()で解放する。
 */
nk_result nk_audio_features_create(nk_audio_features **out_features,
                                   const nk_audio_features_config *config,
                                   nk_audio_second_callback callback, void *user_data);

/*
 * 16 kHz・モノラル・符号付き16bit PCMを渡す。
 *
 * sample_countは今回渡すサンプル数。
 * samplesは呼び出し中だけ借用し、保持・解放しない。
 * 1秒分そろうたびにcallbackを呼び出す。
 *
 * flagsには以下のみ指定できる。
 * NK_EPF_MIC_BUSY
 * NK_EPF_CHARGING
 * NK_EPF_SOUND_ON
 *
 * 通常のWAV再生ではflagsへ0を渡す。
 */
nk_result nk_audio_features_push_pcm(nk_audio_features *features, const int16_t *samples,
                                     int sample_count, unsigned int flags);

/*
 * 入力終了時に呼び出す。
 *
 * 最後の1秒未満の音声が残っている場合も、
 * 実際のサンプル数を持つ特徴量として出力する。
 *
 * 成功後の再呼び出しはNK_OK。
 * 成功後はpush_pcmを呼べない。
 */
nk_result nk_audio_features_finish(nk_audio_features *features);

/*
 * 処理オブジェクトを解放する。
 * user_dataは解放しない。
 * 残りの音声を自動出力する処理は行わない。
 */
void nk_audio_features_destroy(nk_audio_features *features);

#endif
