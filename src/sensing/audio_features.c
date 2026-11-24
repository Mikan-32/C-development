#include "audio_features.h"

#include <math.h>
#include <stdlib.h>

#define NK_AUDIO_FRAME_SAMPLES  1600
#define NK_AUDIO_HISTORY_FRAMES 80
#define NK_AUDIO_HOLD_SECONDS   6

#define NK_AUDIO_ALLOWED_FLAGS (NK_EPF_MIC_BUSY | NK_EPF_CHARGING | NK_EPF_SOUND_ON)

#define NK_AUDIO_PI 3.14159265358979323846

typedef struct nk_audio_filter {
    double b0, b1, b2;
    double a1, a2;
    double z1, z2;
} nk_audio_filter;

struct nk_audio_features {
    nk_audio_features_config config;
    nk_audio_second_callback callback;
    void *user_data;

    nk_audio_filter highpass;
    nk_audio_filter lowpass;

    double envelope[NK_AUDIO_HISTORY_FRAMES];
    double raw_power[NK_AUDIO_HISTORY_FRAMES];
    double band_power[NK_AUDIO_HISTORY_FRAMES];

    int history_next;
    int history_count;

    int frame_samples;
    int frame_blocked;
    double frame_raw;
    double frame_band;

    int second_samples;
    int available_samples;
    double second_power;
    unsigned int second_flags;
    int next_second;

    int hold_seconds;
    double hold_score;

    int finished;
    nk_result failure;
};

static nk_audio_filter make_filter(double frequency, int highpass) {
    nk_audio_filter filter = {0};

    double omega = 2.0 * NK_AUDIO_PI * frequency / NK_SENSING_MIC_HZ;

    double cosine = cos(omega);
    double alpha = sin(omega) / sqrt(2.0);
    double a0 = 1.0 + alpha;

    if (highpass) {
        filter.b0 = (1.0 + cosine) / (2.0 * a0);
        filter.b1 = -(1.0 + cosine) / a0;
    } else {
        filter.b0 = (1.0 - cosine) / (2.0 * a0);
        filter.b1 = (1.0 - cosine) / a0;
    }

    filter.b2 = filter.b0;
    filter.a1 = -2.0 * cosine / a0;
    filter.a2 = (1.0 - alpha) / a0;

    return filter;
}

static double filter_sample(nk_audio_filter *filter, double input) {
    double output = filter->b0 * input + filter->z1;

    filter->z1 = filter->b1 * input - filter->a1 * output + filter->z2;

    filter->z2 = filter->b2 * input - filter->a2 * output;

    return output;
}

static double power_db(double power) {
    /*
     * 無音でlog10(0)にならないようにする。
     */
    return power > 1e-12 ? 10.0 * log10(power) : -120.0;
}

static void finish_frame(nk_audio_features *features) {
    if (features->frame_blocked) {
        features->history_count = 0;
        features->history_next = 0;

        features->highpass.z1 = 0.0;
        features->highpass.z2 = 0.0;
        features->lowpass.z1 = 0.0;
        features->lowpass.z2 = 0.0;
    } else {
        int position = features->history_next;
        double count = features->frame_samples;

        features->raw_power[position] = features->frame_raw / count;

        features->band_power[position] = features->frame_band / count;

        features->envelope[position] = sqrt(features->raw_power[position]);

        features->history_next = (position + 1) % NK_AUDIO_HISTORY_FRAMES;

        if (features->history_count < NK_AUDIO_HISTORY_FRAMES) {
            features->history_count++;
        }
    }

    features->frame_samples = 0;
    features->frame_blocked = 0;
    features->frame_raw = 0.0;
    features->frame_band = 0.0;
}

/*
 * 100msごとの音量の並びから、
 * 2〜6秒周期の繰り返しを調べる。
 */
static double periodicity(const nk_audio_features *features) {
    double envelope[NK_AUDIO_HISTORY_FRAMES];
    double correlation[63] = {0};

    double mean = 0.0;
    double slope_sum = 0.0;
    double time_square_sum = 0.0;
    double original_energy = 0.0;
    double variation = 0.0;
    double best = 0.0;

    int i;
    int lag;

    for (i = 0; i < NK_AUDIO_HISTORY_FRAMES; ++i) {
        int index = (features->history_next + i) % NK_AUDIO_HISTORY_FRAMES;

        envelope[i] = features->envelope[index];
        mean += envelope[i];
        original_energy += envelope[i] * envelope[i];
    }

    mean /= NK_AUDIO_HISTORY_FRAMES;

    /*
     * 徐々に大きくなるだけの音を周期と誤認しないよう、
     * 平均値と直線的な変化を取り除く。
     */
    for (i = 0; i < NK_AUDIO_HISTORY_FRAMES; ++i) {
        double time = i - (NK_AUDIO_HISTORY_FRAMES - 1) / 2.0;

        slope_sum += time * (envelope[i] - mean);
        time_square_sum += time * time;
    }

    for (i = 0; i < NK_AUDIO_HISTORY_FRAMES; ++i) {
        double time = i - (NK_AUDIO_HISTORY_FRAMES - 1) / 2.0;

        envelope[i] -= mean + time * slope_sum / time_square_sum;

        variation += envelope[i] * envelope[i];
    }

    if (original_energy <= 1e-12 || variation <= original_energy * 1e-4) {
        return 0.0;
    }

    for (lag = 19; lag <= 61; ++lag) {
        double sx = 0.0;
        double sy = 0.0;
        double sxx = 0.0;
        double syy = 0.0;
        double sxy = 0.0;

        int count = NK_AUDIO_HISTORY_FRAMES - lag;

        for (i = lag; i < NK_AUDIO_HISTORY_FRAMES; ++i) {
            double x = envelope[i];
            double y = envelope[i - lag];

            sx += x;
            sy += y;
            sxx += x * x;
            syy += y * y;
            sxy += x * y;
        }

        {
            double vx = sxx - sx * sx / count;
            double vy = syy - sy * sy / count;

            if (vx > 1e-12 && vy > 1e-12) {
                correlation[lag] = (sxy - sx * sy / count) / sqrt(vx * vy);
            }
        }
    }

    /*
     * 2〜6秒の範囲に相関の山があることを確認する。
     */
    for (lag = 20; lag <= 60; ++lag) {
        if (correlation[lag] > correlation[lag - 1] && correlation[lag] >= correlation[lag + 1] &&
            correlation[lag] > best) {
            best = correlation[lag];
        }
    }

    return best > 1.0 ? 1.0 : best;
}

static double snore_score(const nk_audio_features *features) {
    double raw = 0.0;
    double band = 0.0;
    double ratio;
    double repeat;
    int i;

    for (i = 0; i < NK_AUDIO_HISTORY_FRAMES; ++i) {
        raw += features->raw_power[i];
        band += features->band_power[i];
    }

    if (raw <= 1e-12 || power_db(raw / NK_AUDIO_HISTORY_FRAMES) < features->config.min_sound_db) {
        return 0.0;
    }

    ratio = band / raw;

    if (ratio > 1.0) {
        ratio = 1.0;
    }

    if (ratio < features->config.min_band_ratio) {
        return 0.0;
    }

    repeat = periodicity(features);

    if (repeat <= 0.0 || repeat < features->config.min_periodicity) {
        return 0.0;
    }

    /*
     * 試作スコアであり、統計的な確率ではない。
     * 条件を満たした場合だけ0.6以上にする。
     */
    return 0.6 + 0.4 * repeat;
}

static nk_result emit_second(nk_audio_features *features) {
    nk_sensing_second second = {0};
    nk_result result;

    unsigned int blocked = NK_EPF_MIC_BUSY | NK_EPF_SOUND_ON;

    second.idx = features->next_second;
    second.mic_sample_count = features->available_samples;

    second.mic_busy = (features->second_flags & NK_EPF_MIC_BUSY) != 0U;

    second.charging = (features->second_flags & NK_EPF_CHARGING) != 0U;

    second.sound_on = (features->second_flags & NK_EPF_SOUND_ON) != 0U;

    if (features->available_samples > 0) {
        second.snd_db.has_value = 1;

        second.snd_db.value = power_db(features->second_power / features->available_samples);
    }

    if (features->history_count == NK_AUDIO_HISTORY_FRAMES &&
        (features->second_flags & blocked) == 0U) {

        double score = snore_score(features);

        second.snore_like.has_value = 1;

        if (score >= 0.6) {
            features->hold_score = score;
            features->hold_seconds = NK_AUDIO_HOLD_SECONDS;
            second.snore_like.value = score;
        } else if (features->hold_seconds > 0) {
            /*
             * 短い途切れで同じ区間を分断しないための
             * 試作ルール。最大6秒保持する。
             */
            second.snore_like.value = features->hold_score;
            features->hold_seconds--;
        }
    } else {
        features->hold_seconds = 0;
        features->hold_score = 0.0;
    }

    /*
     * WAVには加速度・照度がないので欠損のままにする。
     */
    result = features->callback(features->user_data, &second);

    if (result != NK_OK) {
        features->failure = result;
        return result;
    }

    features->next_second++;
    features->second_samples = 0;
    features->available_samples = 0;
    features->second_power = 0.0;
    features->second_flags = 0U;

    return NK_OK;
}

nk_result nk_audio_features_create(nk_audio_features **out_features,
                                   const nk_audio_features_config *config,
                                   nk_audio_second_callback callback, void *user_data) {
    nk_audio_features *features;

    if (out_features == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    *out_features = NULL;

    if (config == NULL || callback == NULL || !isfinite(config->min_sound_db) ||
        config->min_sound_db > 0.0 || !isfinite(config->min_band_ratio) ||
        config->min_band_ratio < 0.0 || config->min_band_ratio > 1.0 ||
        !isfinite(config->min_periodicity) || config->min_periodicity < 0.0 ||
        config->min_periodicity > 1.0) {
        return NK_ERROR_ARGUMENT;
    }

    features = calloc(1, sizeof(*features));

    if (features == NULL) {
        return NK_ERROR_MEMORY;
    }

    features->config = *config;
    features->callback = callback;
    features->user_data = user_data;

    features->highpass = make_filter(100.0, 1);
    features->lowpass = make_filter(1000.0, 0);

    features->failure = NK_OK;

    *out_features = features;

    return NK_OK;
}

nk_result nk_audio_features_push_pcm(nk_audio_features *features, const int16_t *samples,
                                     int sample_count, unsigned int flags) {
    uint64_t consumed;
    uint64_t maximum;
    int i;

    if (features == NULL || samples == NULL || sample_count <= 0 ||
        (flags & ~(unsigned int)NK_AUDIO_ALLOWED_FLAGS) != 0U) {
        return NK_ERROR_ARGUMENT;
    }

    if (features->failure != NK_OK) {
        return features->failure;
    }

    if (features->finished) {
        return NK_ERROR_NOT_RUNNING;
    }

    consumed =
        (uint64_t)features->next_second * NK_SENSING_MIC_HZ + (uint64_t)features->second_samples;

    maximum = (uint64_t)NK_SENSING_MAX_EPOCHS * NK_SENSING_EPOCH_SECONDS * NK_SENSING_MIC_HZ;

    if ((uint64_t)sample_count > maximum - consumed) {
        return NK_ERROR_ARGUMENT;
    }

    for (i = 0; i < sample_count; ++i) {
        double input = samples[i] / 32768.0;
        double filtered = 0.0;
        nk_result result;

        features->second_flags |= flags;

        if ((flags & NK_EPF_MIC_BUSY) == 0U) {
            features->available_samples++;
            features->second_power += input * input;
        }

        if ((flags & (NK_EPF_MIC_BUSY | NK_EPF_SOUND_ON)) != 0U) {
            features->frame_blocked = 1;
        } else {
            filtered = filter_sample(&features->highpass, input);

            filtered = filter_sample(&features->lowpass, filtered);

            features->frame_raw += input * input;
            features->frame_band += filtered * filtered;
        }

        features->frame_samples++;
        features->second_samples++;

        if (features->frame_samples == NK_AUDIO_FRAME_SAMPLES) {
            finish_frame(features);
        }

        if (features->second_samples == NK_SENSING_MIC_HZ) {
            result = emit_second(features);

            if (result != NK_OK) {
                return result;
            }
        }
    }

    return NK_OK;
}

nk_result nk_audio_features_finish(nk_audio_features *features) {
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

    if (features->frame_samples > 0) {
        finish_frame(features);
    }

    if (features->second_samples > 0) {
        result = emit_second(features);

        if (result != NK_OK) {
            return result;
        }
    }

    features->finished = 1;

    return NK_OK;
}

void nk_audio_features_destroy(nk_audio_features *features) {
    free(features);
}
