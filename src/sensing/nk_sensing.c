#include "nk_sensing.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// AI-1 仕様書に基づく定数
#define ACC_HZ               50
#define ACC_EXPECTED_30S     1500.0f // 50Hz * 30s
#define MIC_HZ               16000
#define MIC_EXPECTED_30S     480000.0f // 16kHz * 30s
#define GRAVITY_WIN_SAMPLES  25        // 0.5秒 (50Hz * 0.5s)
#define STANDARD_GRAVITY     9.80665f
#define MOVE_THRESHOLD_MG    15.0f // 体動しきい値 15mg
#define OFFBED_THRESHOLD_MG  0.5f  // ベッド外しきい値 0.5mg
#define TURN_THRESHOLD_DEG   30.0f // 寝返り角度しきい値 30度
#define SNORE_LIKE_THRESHOLD 0.6f  // いびき判定しきい値
#define SNORE_MIN_DURATION_S 10    // いびきイベント最小継続秒数
#define NOISE_DIFF_DB        15.0f // 周囲より +15dB 以上で NOISE

// コールバック・外部状態
static nk_sensing_epoch_cb s_on_epoch = NULL;
static nk_sensing_event_cb s_on_event = NULL;
static void *s_user_data = NULL;
static bool s_mic_enabled = true;
static bool s_sound_playing = false;
static bool s_charging = false;

// 重力移動平均用リングバッファ（直近 0.5秒 = 25サンプル）
static float s_acc_ring_x[GRAVITY_WIN_SAMPLES];
static float s_acc_ring_y[GRAVITY_WIN_SAMPLES];
static float s_acc_ring_z[GRAVITY_WIN_SAMPLES];
static uint32_t s_acc_ring_count = 0;
static uint32_t s_acc_ring_idx = 0;

// 現在の1秒間バッファ（加速度）
static float s_sec_acc_hp_sum = 0.0f;
static uint32_t s_sec_acc_count = 0;
static float s_last_grav_x = 0.0f;
static float s_last_grav_y = 0.0f;
static float s_last_grav_z = 0.0f;

// 現在の1秒間バッファ（音声：100-1000Hz バンドパス用2次IIRフィルタ状態含む）
static double s_sec_audio_sq_sum = 0.0;
static double s_sec_band_sq_sum = 0.0;
static uint32_t s_sec_audio_count = 0;
static float s_lp_state = 0.0f; // 簡易ローパス (~1000Hz)
static float s_hp_state = 0.0f; // 簡易ハイパス (~100Hz)
static float s_prev_sample = 0.0f;

// 30秒エポック蓄積用
static uint32_t s_epoch_idx = 0;
static int s_sec_in_epoch = 0;
static uint32_t s_epoch_acc_samples = 0;
static uint32_t s_epoch_mic_samples = 0;

static float s_epoch_acc_hp[30];
static bool s_epoch_moving[30];
static float s_epoch_snd_db[30];
static float s_epoch_snore_like[30];

static float s_epoch_lux_buf[64];
static uint32_t s_epoch_lux_count = 0;

// エポック間の比較・イベント継続追跡用
static bool s_has_prev_epoch_grav = false;
static float s_prev_epoch_grav_x = 0.0f;
static float s_prev_epoch_grav_y = 0.0f;
static float s_prev_epoch_grav_z = 0.0f;

static int s_snore_streak_s = 0;
static int s_snore_silence_s = 0; // 息継ぎ無音カウンタを追加
static int64_t s_snore_start_ms = 0;
static float s_snore_db_sum = 0.0f;
static int s_snore_active_s = 0; // 実際にいびきが鳴っていた秒数
static float s_ambient_db = -60.0f;

// 浮動小数点配列のソート用比較関数
static int cmp_float(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static float calc_median(float *arr, uint32_t n) {
    if (n == 0) {
        return -1.0f;
    }
    qsort(arr, n, sizeof(float), cmp_float);
    if (n % 2 == 1) {
        return arr[n / 2];
    }
    return 0.5f * (arr[n / 2 - 1] + arr[n / 2]);
}

static void emit_event(int64_t t_ms, int64_t dur_ms, nk_event_type_t type, float value) {
    if (s_on_event != NULL) {
        nk_event_t ev;
        ev.t_ms = t_ms;
        ev.dur_ms = dur_ms;
        ev.type = type;
        ev.value = value;
        s_on_event(&ev, s_user_data);
    }
}

// 30秒エポックを確定してコールバック通知する
static void finalize_epoch(int64_t t_ms) {
    nk_epoch_t ep;
    memset(&ep, 0, sizeof(ep));

    ep.idx = s_epoch_idx++;
    ep.acc_cov = (float)s_epoch_acc_samples / ACC_EXPECTED_30S;
    if (ep.acc_cov > 1.0f) {
        ep.acc_cov = 1.0f;
    }

    ep.mic_cov = s_mic_enabled ? ((float)s_epoch_mic_samples / MIC_EXPECTED_30S) : 0.0f;
    if (ep.mic_cov > 1.0f) {
        ep.mic_cov = 1.0f;
    }

    // 体動・微小振動の集計
    int move_n = 0;
    float hp_sum = 0.0f;
    float still_hp[30];
    uint32_t still_n = 0;

    for (int i = 0; i < 30; i++) {
        hp_sum += s_epoch_acc_hp[i];
        if (s_epoch_moving[i]) {
            move_n++;
        } else {
            still_hp[still_n++] = s_epoch_acc_hp[i];
        }
    }
    ep.move_n = move_n;
    ep.move_mg = hp_sum / 30.0f;
    ep.micro_mg = (still_n > 0) ? calc_median(still_hp, still_n) : -1.0f;

    // 姿勢変化（前のエポックとの重力ベクトル角度差）
    ep.posture = 0;
    if (s_epoch_acc_samples > 0) {
        if (s_has_prev_epoch_grav) {
            float dot = s_prev_epoch_grav_x * s_last_grav_x + s_prev_epoch_grav_y * s_last_grav_y +
                        s_prev_epoch_grav_z * s_last_grav_z;
            float mag1 = sqrtf(s_prev_epoch_grav_x * s_prev_epoch_grav_x +
                               s_prev_epoch_grav_y * s_prev_epoch_grav_y +
                               s_prev_epoch_grav_z * s_prev_epoch_grav_z);
            float mag2 = sqrtf(s_last_grav_x * s_last_grav_x + s_last_grav_y * s_last_grav_y +
                               s_last_grav_z * s_last_grav_z);
            if (mag1 > 0.1f && mag2 > 0.1f) {
                float cos_theta = dot / (mag1 * mag2);
                if (cos_theta > 1.0f) {
                    cos_theta = 1.0f;
                }
                if (cos_theta < -1.0f) {
                    cos_theta = -1.0f;
                }
                float deg = acosf(cos_theta) * (float)(180.0 / M_PI);
                if (deg >= TURN_THRESHOLD_DEG) {
                    ep.posture = 1;
                    emit_event(t_ms, 0, NK_EVENT_TURN, deg);
                }
            }
        }
        s_prev_epoch_grav_x = s_last_grav_x;
        s_prev_epoch_grav_y = s_last_grav_y;
        s_prev_epoch_grav_z = s_last_grav_z;
        s_has_prev_epoch_grav = true;
    }

    // 音声・いびき集計
    if (s_mic_enabled && s_epoch_mic_samples > 0) {
        float db_sum = 0.0f;
        float db_sorted[30];
        int snore_s = 0;
        for (int i = 0; i < 30; i++) {
            db_sum += s_epoch_snd_db[i];
            db_sorted[i] = s_epoch_snd_db[i];
            if (!s_sound_playing && s_epoch_snore_like[i] >= SNORE_LIKE_THRESHOLD) {
                snore_s++;
            }
        }
        ep.snd_db_mean = db_sum / 30.0f;
        qsort(db_sorted, 30, sizeof(float), cmp_float);
        ep.snd_db_p95 = db_sorted[28]; // 30個の95%点 (index 28)
        ep.snore_s = snore_s;
    } else {
        ep.snd_db_mean = -120.0f;
        ep.snd_db_p95 = -120.0f;
        ep.snore_s = 0;
    }

    // 呼吸数（本段階では未推定値として負値設定）
    ep.breath_bpm = -1.0f;
    ep.breath_q = 0.0f;

    // 照度中央値
    ep.lux = (s_epoch_lux_count > 0) ? calc_median(s_epoch_lux_buf, s_epoch_lux_count) : -1.0f;

    // フラグ設定
    uint32_t flags = 0;
    if (ep.acc_cov < 0.8f) {
        flags |= NK_EPF_GAP_ACC;
    }
    if (s_mic_enabled && ep.mic_cov < 0.8f) {
        flags |= NK_EPF_GAP_MIC;
    }
    if (ep.micro_mg >= 0.0f && ep.micro_mg < OFFBED_THRESHOLD_MG) {
        flags |= NK_EPF_OFFBED;
    }
    if (s_charging) {
        flags |= NK_EPF_CHARGING;
    }
    if (s_sound_playing) {
        flags |= NK_EPF_SOUND_ON;
    }
    ep.flags = flags;

    if (s_on_epoch != NULL) {
        s_on_epoch(&ep, s_user_data);
    }

    // 次のエポックに向けてリセット
    s_sec_in_epoch = 0;
    s_epoch_acc_samples = 0;
    s_epoch_mic_samples = 0;
    s_epoch_lux_count = 0;
}

void nk_sensing_init(nk_sensing_epoch_cb on_epoch, nk_sensing_event_cb on_event, void *user_data) {
    s_on_epoch = on_epoch;
    s_on_event = on_event;
    s_user_data = user_data;

    s_mic_enabled = true;
    s_sound_playing = false;
    s_charging = false;

    memset(s_acc_ring_x, 0, sizeof(s_acc_ring_x));
    memset(s_acc_ring_y, 0, sizeof(s_acc_ring_y));
    memset(s_acc_ring_z, 0, sizeof(s_acc_ring_z));
    s_acc_ring_count = 0;
    s_acc_ring_idx = 0;

    s_sec_acc_hp_sum = 0.0f;
    s_sec_acc_count = 0;
    s_last_grav_x = 0.0f;
    s_last_grav_y = 0.0f;
    s_last_grav_z = 0.0f;

    s_sec_audio_sq_sum = 0.0;
    s_sec_band_sq_sum = 0.0;
    s_sec_audio_count = 0;
    s_lp_state = 0.0f;
    s_hp_state = 0.0f;
    s_prev_sample = 0.0f;

    s_epoch_idx = 0;
    s_sec_in_epoch = 0;
    s_epoch_acc_samples = 0;
    s_epoch_mic_samples = 0;
    s_epoch_lux_count = 0;

    s_has_prev_epoch_grav = false;
    s_snore_streak_s = 0;
    s_snore_start_ms = 0;
    s_snore_db_sum = 0.0f;
    s_ambient_db = -60.0f;
    s_snore_silence_s = 0;
    s_snore_active_s = 0;
}

void nk_sensing_set_mic_enabled(bool enabled) {
    s_mic_enabled = enabled;
}

void nk_sensing_set_sound_playing(bool playing) {
    s_sound_playing = playing;
}

void nk_sensing_set_charging(bool charging) {
    s_charging = charging;
}

void nk_sensing_flush_second(int64_t t_ms) {
    // 1. 加速度の1秒集計
    float acc_hp_mean = (s_sec_acc_count > 0) ? (s_sec_acc_hp_sum / (float)s_sec_acc_count) : 0.0f;
    bool moving = (acc_hp_mean > MOVE_THRESHOLD_MG);

    s_epoch_acc_hp[s_sec_in_epoch] = acc_hp_mean;
    s_epoch_moving[s_sec_in_epoch] = moving;
    s_epoch_acc_samples += s_sec_acc_count;

    s_sec_acc_hp_sum = 0.0f;
    s_sec_acc_count = 0;

    // 2. 音声の1秒集計
    float snd_db = -120.0f;
    float snore_like = 0.0f;

    if (s_mic_enabled && s_sec_audio_count > 0) {
        double rms = sqrt(s_sec_audio_sq_sum / (double)s_sec_audio_count);
        if (rms < 1.0) {
            rms = 1.0;
        }
        snd_db = (float)(20.0 * log10(rms / 32768.0));

        // 100-1000Hz 帯域エネルギー比率 + 一定以上の音量（-50dBFS超）でいびきらしさ算出
        double band_ratio =
            (s_sec_audio_sq_sum > 0.0) ? (s_sec_band_sq_sum / s_sec_audio_sq_sum) : 0.0;
        if (snd_db > -50.0f && band_ratio > 0.20) {
            snore_like = (float)(band_ratio * 2.5);
            if (snore_like > 1.0f) {
                snore_like = 1.0f;
            }
        }

        // NOISE イベント判定（周囲音より +15dB 以上、かついびきではない突発音）
        if (snore_like < SNORE_LIKE_THRESHOLD && snd_db > s_ambient_db + NOISE_DIFF_DB &&
            snd_db > -35.0f) {
            emit_event(t_ms, 1000, NK_EVENT_NOISE, snd_db);
        }
        // 環境音のゆっくり追従（無音・静寂時をベースにする）
        if (snd_db < s_ambient_db + 10.0f) {
            s_ambient_db = 0.9f * s_ambient_db + 0.1f * snd_db;
        }

        // いびき継続区間の追跡（息継ぎの無音 4秒までは同一いびき区間として扱う）
        if (!s_sound_playing && snore_like >= SNORE_LIKE_THRESHOLD) {
            if (s_snore_streak_s == 0) {
                s_snore_start_ms = t_ms - 1000;
                s_snore_db_sum = 0.0f;
                s_snore_active_s = 0;
            }
            s_snore_streak_s += (1 + s_snore_silence_s);
            s_snore_silence_s = 0;
            s_snore_active_s++;
            s_snore_db_sum += snd_db;
        } else if (s_snore_streak_s > 0 && s_snore_silence_s < 4) {
            // 息継ぎの無音（4秒未満）は許容して継続待ち
            s_snore_silence_s++;
        } else {
            if (s_snore_streak_s >= SNORE_MIN_DURATION_S && s_snore_active_s > 0) {
                float avg_db = s_snore_db_sum / (float)s_snore_active_s;
                emit_event(s_snore_start_ms, (int64_t)s_snore_streak_s * 1000LL, NK_EVENT_SNORE,
                           avg_db);
            }
            s_snore_streak_s = 0;
            s_snore_silence_s = 0;
            s_snore_active_s = 0;
            s_snore_db_sum = 0.0f;
        }
    }

    s_epoch_snd_db[s_sec_in_epoch] = snd_db;
    s_epoch_snore_like[s_sec_in_epoch] = snore_like;
    s_epoch_mic_samples += s_sec_audio_count;

    s_sec_audio_sq_sum = 0.0;
    s_sec_band_sq_sum = 0.0;
    s_sec_audio_count = 0;

    // 30秒経過でエポック確定
    s_sec_in_epoch++;
    if (s_sec_in_epoch >= 30) {
        finalize_epoch(t_ms);
    }
}

void nk_sensing_push_acc(int64_t t_ms, float x, float y, float z) {
    // 0.5秒移動平均リングバッファへ格納
    s_acc_ring_x[s_acc_ring_idx] = x;
    s_acc_ring_y[s_acc_ring_idx] = y;
    s_acc_ring_z[s_acc_ring_idx] = z;
    s_acc_ring_idx = (s_acc_ring_idx + 1) % GRAVITY_WIN_SAMPLES;
    if (s_acc_ring_count < GRAVITY_WIN_SAMPLES) {
        s_acc_ring_count++;
    }

    // 重力ベクトル（移動平均）計算
    float gx = 0.0f, gy = 0.0f, gz = 0.0f;
    for (uint32_t i = 0; i < s_acc_ring_count; i++) {
        gx += s_acc_ring_x[i];
        gy += s_acc_ring_y[i];
        gz += s_acc_ring_z[i];
    }
    gx /= (float)s_acc_ring_count;
    gy /= (float)s_acc_ring_count;
    gz /= (float)s_acc_ring_count;

    s_last_grav_x = gx;
    s_last_grav_y = gy;
    s_last_grav_z = gz;

    // 重力成分を引いた動的加速度 (m/s^2 -> mg)
    float dx = x - gx;
    float dy = y - gy;
    float dz = z - gz;
    float mag_ms2 = sqrtf(dx * dx + dy * dy + dz * dz);
    float acc_hp_mg = (mag_ms2 / STANDARD_GRAVITY) * 1000.0f;

    s_sec_acc_hp_sum += acc_hp_mg;
    s_sec_acc_count++;

    // 50サンプル（1秒）ごとに1秒値を確定
    if (s_sec_acc_count >= ACC_HZ) {
        nk_sensing_flush_second(t_ms);
    }
}

void nk_sensing_push_audio(int64_t t_ms, const int16_t *samples, uint32_t sample_count) {
    if (!s_mic_enabled || samples == NULL || sample_count == 0) {
        return;
    }

    // 16kHz 用の簡易1次IIRバンドパス係数（100Hz〜1000Hz）
    const float alpha_lp = 0.28f; // カットオフ ~1000Hz
    const float alpha_hp = 0.96f; // カットオフ ~100Hz

    for (uint32_t i = 0; i < sample_count; i++) {
        float s = (float)samples[i];
        s_sec_audio_sq_sum += (double)s * (double)s;

        // ハイパス -> ローパスで 100-1000Hz 成分抽出
        s_hp_state = alpha_hp * (s_hp_state + s - s_prev_sample);
        s_prev_sample = s;
        s_lp_state = s_lp_state + alpha_lp * (s_hp_state - s_lp_state);

        s_sec_band_sq_sum += (double)s_lp_state * (double)s_lp_state;
        s_sec_audio_count++;

        // マイク単独駆動時（加速度未入力時）は 16,000 サンプルごとに1秒確定
        if (s_sec_audio_count >= MIC_HZ && s_sec_acc_count == 0) {
            nk_sensing_flush_second(t_ms);
        }
    }
}

void nk_sensing_push_light(int64_t t_ms, float lux) {
    (void)t_ms;
    if (s_epoch_lux_count < 64) {
        s_epoch_lux_buf[s_epoch_lux_count++] = lux;
    }
}
