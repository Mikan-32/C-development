#include "sensing.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * 30秒分だけ保持する集計用バッファ。
 * 生の音声や加速度サンプルは保存しない。
 */
typedef struct nk_sensing_buffer {
    int64_t acc_samples;
    int64_t mic_samples;

    double acc_values[NK_SENSING_EPOCH_SECONDS];
    int acc_count;
    int move_count;

    double quiet_values[NK_SENSING_EPOCH_SECONDS];
    int quiet_count;

    double sound_values[NK_SENSING_EPOCH_SECONDS];
    int sound_count;

    int snore_valid_count;
    int snore_seconds;

    double lux_values[NK_SENSING_EPOCH_SECONDS];
    int lux_count;

    unsigned int flags;
} nk_sensing_buffer;

struct nk_sensing {
    nk_sensing_config config;
    nk_sensing_callbacks callbacks;
    void *user_data;

    int next_second;
    int next_epoch;

    int finished;

    int snore_start;
    int snore_length;
    double snore_db_sum;
    int snore_db_count;

    /*
     * コールバック失敗後は処理を継続しない。
     */
    nk_result failure;

    nk_sensing_buffer buffer;
};

static nk_result emit_snore(nk_sensing *sensing) {
    nk_event_record event = {0};
    nk_result result;

    /*
     * 10秒以上連続した区間だけをSNOREとして出力する。
     */
    if (sensing->snore_length >= 10) {
        event.type = NK_EVENT_SNORE;

        event.t_ms = sensing->config.start_ms + (int64_t)sensing->snore_start * INT64_C(1000);

        event.dur_ms = (int64_t)sensing->snore_length * INT64_C(1000);

        if (sensing->snore_db_count > 0) {
            event.value.has_value = 1;
            event.value.value = sensing->snore_db_sum / sensing->snore_db_count;
        }

        result = sensing->callbacks.on_event(sensing->user_data, &event);

        if (result != NK_OK) {
            sensing->failure = result;
            return result;
        }
    }

    sensing->snore_length = 0;
    sensing->snore_db_sum = 0.0;
    sensing->snore_db_count = 0;

    return NK_OK;
}

static nk_result update_snore(nk_sensing *sensing, const nk_sensing_second *second) {
    if (sensing->config.mic_used && second->snore_like.has_value &&
        second->snore_like.value >= 0.6 && !second->sound_on && !second->mic_busy) {

        if (sensing->snore_length == 0) {
            sensing->snore_start = second->idx;
        }

        sensing->snore_length++;

        if (second->snd_db.has_value) {
            sensing->snore_db_sum += second->snd_db.value;
            sensing->snore_db_count++;
        }

        return NK_OK;
    }

    /*
     * 非該当・欠損・サウンド再生・マイク使用中で区間を閉じる。
     * 途切れた区間をつなぎ合わせない。
     */
    return emit_snore(sensing);
}

static int valid_boolean(int value) {
    return value == 0 || value == 1;
}

static int valid_optional(nk_optional_double value) {
    if (!valid_boolean(value.has_value)) {
        return 0;
    }

    return !value.has_value || isfinite(value.value);
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;

    return (a > b) - (a < b);
}

static double mean_value(const double *values, int count) {
    double total = 0.0;
    int i;

    for (i = 0; i < count; ++i) {
        total += values[i];
    }

    return total / (double)count;
}

/*
 * 配列を並べ替えて中央値を返す。
 * countが1以上の場合だけ呼ぶ。
 */
static double median_value(double *values, int count) {
    qsort(values, (size_t)count, sizeof(values[0]), compare_double);

    if (count % 2 != 0) {
        return values[count / 2];
    }

    return values[count / 2 - 1] / 2.0 + values[count / 2] / 2.0;
}

static int valid_second(const nk_sensing *sensing, const nk_sensing_second *second) {
    if (second->acc_sample_count < 0 || second->mic_sample_count < 0) {
        return 0;
    }

    if (!valid_optional(second->acc_hp) || !valid_optional(second->snd_db) ||
        !valid_optional(second->snore_like) || !valid_optional(second->lux)) {
        return 0;
    }

    if (!valid_boolean(second->mic_busy) || !valid_boolean(second->charging) ||
        !valid_boolean(second->sound_on) || !valid_boolean(second->gravity.has_value)) {
        return 0;
    }

    if (second->acc_hp.has_value && (second->acc_sample_count == 0 || second->acc_hp.value < 0.0)) {
        return 0;
    }

    if (second->gravity.has_value) {
        if (second->acc_sample_count == 0 || !isfinite(second->gravity.x) ||
            !isfinite(second->gravity.y) || !isfinite(second->gravity.z)) {
            return 0;
        }
    }

    if (second->snd_db.has_value && second->mic_sample_count == 0) {
        return 0;
    }

    if (second->snore_like.has_value) {
        if (second->mic_sample_count == 0 || second->snore_like.value < 0.0 ||
            second->snore_like.value > 1.0) {
            return 0;
        }
    }

    /*
     * マイクを使わない設定では、
     * マイク由来の入力も受け取らない。
     */
    if (!sensing->config.mic_used && (second->mic_sample_count != 0 || second->snd_db.has_value ||
                                      second->snore_like.has_value || second->mic_busy)) {
        return 0;
    }

    if (second->lux.has_value && (!sensing->config.light_used || second->lux.value < 0.0)) {
        return 0;
    }

    return 1;
}

static nk_epoch_record make_epoch(nk_sensing *sensing) {
    nk_sensing_buffer *buffer = &sensing->buffer;
    nk_epoch_record epoch = {0};
    int rank;

    /*
     * session_idは受け取り側のsessionが設定する。
     */
    epoch.session_id = 0;
    epoch.idx = sensing->next_epoch;

    epoch.acc_cov = (double)buffer->acc_samples / (NK_SENSING_ACC_HZ * NK_SENSING_EPOCH_SECONDS);

    epoch.mic_cov = (double)buffer->mic_samples / (NK_SENSING_MIC_HZ * NK_SENSING_EPOCH_SECONDS);

    /*
     * サンプリング時刻の揺れなどで期待数を超えても、
     * 保存する取得率は最大1にする。
     */
    if (epoch.acc_cov > 1.0) {
        epoch.acc_cov = 1.0;
    }

    if (epoch.mic_cov > 1.0) {
        epoch.mic_cov = 1.0;
    }

    epoch.flags = buffer->flags;

    if (epoch.acc_cov < 0.8) {
        epoch.flags |= NK_EPF_GAP_ACC;
    }

    if (sensing->config.mic_used && epoch.mic_cov < 0.8) {
        epoch.flags |= NK_EPF_GAP_MIC;
    }

    if (buffer->acc_count > 0) {
        epoch.move_n.has_value = 1;
        epoch.move_n.value = buffer->move_count;

        epoch.move_mg.has_value = 1;
        epoch.move_mg.value = mean_value(buffer->acc_values, buffer->acc_count);
    }

    if (buffer->quiet_count > 0) {
        epoch.micro_mg.has_value = 1;
        epoch.micro_mg.value = median_value(buffer->quiet_values, buffer->quiet_count);

        if (epoch.micro_mg.value < 0.5) {
            epoch.flags |= NK_EPF_OFFBED;
        }
    }

    if (buffer->sound_count > 0) {
        epoch.snd_db_mean.has_value = 1;
        epoch.snd_db_mean.value = mean_value(buffer->sound_values, buffer->sound_count);

        qsort(buffer->sound_values, (size_t)buffer->sound_count, sizeof(buffer->sound_values[0]),
              compare_double);

        /*
         * nearest-rank方式の95パーセンタイル。
         */
        rank = (95 * buffer->sound_count + 99) / 100;

        epoch.snd_db_p95.has_value = 1;
        epoch.snd_db_p95.value = buffer->sound_values[rank - 1];
    }

    if (buffer->snore_valid_count > 0) {
        epoch.snore_s.has_value = 1;
        epoch.snore_s.value = buffer->snore_seconds;
    }

    if (buffer->lux_count > 0) {
        epoch.lux.has_value = 1;
        epoch.lux.value = median_value(buffer->lux_values, buffer->lux_count);
    }

    /*
     * posture・breath_bpm・breath_qは初版では未対応。
     * has_valueが0なので保存時はNULLになる。
     */

    return epoch;
}

nk_result nk_sensing_create(nk_sensing **out_sensing, const nk_sensing_config *config,
                            const nk_sensing_callbacks *callbacks, void *user_data) {
    nk_sensing *sensing;

    if (out_sensing == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    *out_sensing = NULL;

    if (config == NULL || callbacks == NULL || callbacks->on_epoch == NULL ||
        callbacks->on_event == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    /*
     * イベント時刻の計算がint64_tの範囲を超えないようにする。
     */
    if (config->start_ms >
        INT64_MAX - (int64_t)NK_SENSING_MAX_EPOCHS * NK_SENSING_EPOCH_SECONDS * INT64_C(1000)) {
        return NK_ERROR_ARGUMENT;
    }

    if (config->start_ms < 0 || !valid_boolean(config->mic_used) ||
        !valid_boolean(config->light_used) || !isfinite(config->move_threshold_mg) ||
        config->move_threshold_mg <= 0.0) {
        return NK_ERROR_ARGUMENT;
    }

    sensing = calloc(1, sizeof(*sensing));

    if (sensing == NULL) {
        return NK_ERROR_MEMORY;
    }

    sensing->config = *config;
    sensing->callbacks = *callbacks;
    sensing->user_data = user_data;
    sensing->failure = NK_OK;

    *out_sensing = sensing;

    return NK_OK;
}

nk_result nk_sensing_push_second(nk_sensing *sensing, const nk_sensing_second *second) {
    nk_sensing_buffer *buffer;
    nk_epoch_record epoch;
    nk_result result;

    if (sensing == NULL || second == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (sensing->failure != NK_OK) {
        return sensing->failure;
    }

    if (sensing->finished) {
        return NK_ERROR_NOT_RUNNING;
    }

    /*
     * 秒番号の抜け・重複・逆順を拒否する。
     * 欠損した秒も、取得数0として順番に渡す。
     */
    if (second->idx != sensing->next_second ||
        sensing->next_second >= NK_SENSING_MAX_EPOCHS * NK_SENSING_EPOCH_SECONDS) {
        return NK_ERROR_ARGUMENT;
    }

    if (!valid_second(sensing, second)) {
        return NK_ERROR_ARGUMENT;
    }

    result = update_snore(sensing, second);

    if (result != NK_OK) {
        return result;
    }

    buffer = &sensing->buffer;

    buffer->acc_samples += second->acc_sample_count;
    buffer->mic_samples += second->mic_sample_count;

    if (second->acc_hp.has_value) {
        buffer->acc_values[buffer->acc_count] = second->acc_hp.value;
        buffer->acc_count++;

        if (second->acc_hp.value > sensing->config.move_threshold_mg) {
            buffer->move_count++;
        } else {
            buffer->quiet_values[buffer->quiet_count] = second->acc_hp.value;
            buffer->quiet_count++;
        }
    }

    if (second->snd_db.has_value) {
        buffer->sound_values[buffer->sound_count] = second->snd_db.value;
        buffer->sound_count++;
    }

    /*
     * 入眠サウンド再生中の秒は、
     * いびきの秒数判定から除外する。
     */
    if (second->snore_like.has_value && !second->sound_on && !second->mic_busy) {
        buffer->snore_valid_count++;

        if (second->snore_like.value >= 0.6) {
            buffer->snore_seconds++;
        }
    }

    if (second->lux.has_value) {
        buffer->lux_values[buffer->lux_count] = second->lux.value;
        buffer->lux_count++;
    }

    if (second->mic_busy) {
        buffer->flags |= NK_EPF_MIC_BUSY;
    }

    if (second->charging) {
        buffer->flags |= NK_EPF_CHARGING;
    }

    if (second->sound_on) {
        buffer->flags |= NK_EPF_SOUND_ON;
    }

    sensing->next_second++;

    /*
     * 30秒そろうまでは出力しない。
     */
    if (sensing->next_second % NK_SENSING_EPOCH_SECONDS != 0) {
        return NK_OK;
    }

    epoch = make_epoch(sensing);

    result = sensing->callbacks.on_epoch(sensing->user_data, &epoch);

    if (result != NK_OK) {
        /*
         * 保存失敗などが起きた場合、
         * 同じ出力を重複送信せず処理を停止する。
         */
        sensing->failure = result;
        return result;
    }

    sensing->next_epoch++;

    memset(&sensing->buffer, 0, sizeof(sensing->buffer));

    return NK_OK;
}

nk_result nk_sensing_finish(nk_sensing *sensing) {
    nk_epoch_record epoch;
    nk_result result;

    if (sensing == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    if (sensing->failure != NK_OK) {
        return sensing->failure;
    }

    if (sensing->finished) {
        return NK_OK;
    }

    /*
     * 最後まで続いているSNOREも出力する。
     */
    result = emit_snore(sensing);

    if (result != NK_OK) {
        return result;
    }

    /*
     * 30秒未満の残りがある場合も出力する。
     * 取得率の分母は通常と同じ30秒。
     */
    if (sensing->next_second % NK_SENSING_EPOCH_SECONDS != 0) {

        epoch = make_epoch(sensing);

        result = sensing->callbacks.on_epoch(sensing->user_data, &epoch);

        if (result != NK_OK) {
            sensing->failure = result;
            return result;
        }

        sensing->next_epoch++;

        memset(&sensing->buffer, 0, sizeof(sensing->buffer));
    }

    sensing->finished = 1;

    return NK_OK;
}

void nk_sensing_destroy(nk_sensing *sensing) {
    free(sensing);
}
