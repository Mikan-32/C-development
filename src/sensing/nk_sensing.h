#pragma once

#include <stdbool.h>
#include <stdint.h>

// エポックフラグ（AI-1 仕様書 表4のビット定義）
#define NK_EPF_GAP_ACC  (1U << 0) // acc_cov < 0.8
#define NK_EPF_GAP_MIC  (1U << 1) // mic_cov < 0.8（マイク使用時）
#define NK_EPF_OFFBED   (1U << 2) // micro_mg < 0.5 mg（ベッド外疑い）
#define NK_EPF_MIC_BUSY (1U << 3) // 他アプリがマイク使用中
#define NK_EPF_CHARGING (1U << 4) // 充電中
#define NK_EPF_SOUND_ON (1U << 5) // 入眠サウンド再生中

// 30秒ごとのエポックデータ（AI-1 仕様書 表4準拠）
typedef struct {
    uint32_t idx;      // 計測開始からのエポック番号 (0〜)
    float acc_cov;     // 加速度受信率 (受信数 / 1500.0)
    float mic_cov;     // マイク受信率 (受信数 / 480000.0)
    int move_n;        // 体動があった秒数 (0〜30)
    float move_mg;     // 1秒ごとの acc_hp の平均 (mg)
    float micro_mg;    // 静止秒の acc_hp の中央値 (mg)。静止秒なし時は -1.0f
    int posture;       // 前エポックから重力方向が 30° 以上変化で 1、それ以外 0
    float snd_db_mean; // 音量の平均 (dBFS)。マイクなし時は -120.0f
    float snd_db_p95;  // 音量の95パーセンタイル (dBFS)。マイクなし時は -120.0f
    int snore_s;       // snore_like >= 0.6 だった秒数 (0〜30)
    float breath_bpm;  // 呼吸数推定値 (回/分)。未算出時は -1.0f
    float breath_q;    // 呼吸数の確からしさ (0〜1)
    float lux;         // 照度の中央値 (lx)。未受信時は -1.0f
    uint32_t flags;    // NK_EPF_* のビット論理和
} nk_epoch_t;

// イベント種別（AI-1 仕様書 表5準拠）
typedef enum {
    NK_EVENT_TURN = 0,  // 寝返り（姿勢変化 >= 30°）
    NK_EVENT_SNORE,     // いびき継続区間（10秒以上）
    NK_EVENT_NOISE,     // 大きな音（周囲平均より +15dB 以上）
    NK_EVENT_GAP_START, // 欠損開始
    NK_EVENT_GAP_END    // 欠損終了
} nk_event_type_t;

// イベントデータ（AI-1 仕様書 表5準拠）
typedef struct {
    int64_t t_ms;         // 発生時刻 (UTCミリ秒)
    int64_t dur_ms;       // 継続時間 (ミリ秒)
    nk_event_type_t type; // イベント種別
    float value;          // 角度(度) や 音量(dBFS)
} nk_event_t;

// session/ またはテストコードへ結果を通知するコールバック型
typedef void (*nk_sensing_epoch_cb)(const nk_epoch_t *epoch, void *user_data);
typedef void (*nk_sensing_event_cb)(const nk_event_t *event, void *user_data);

// 初期化（内部状態のリセットとコールバック登録）
void nk_sensing_init(nk_sensing_epoch_cb on_epoch, nk_sensing_event_cb on_event, void *user_data);

// 動作オプションの通知（外部状態フラグの反映）
void nk_sensing_set_mic_enabled(bool enabled);
void nk_sensing_set_sound_playing(bool playing);
void nk_sensing_set_charging(bool charging);

// 加速度データ 1 サンプル（50Hz, 単位: m/s^2）を投入する
void nk_sensing_push_acc(int64_t t_ms, float x, float y, float z);

// 音声 PCM データ（16kHz, モノラル, 16bit）を投入する
void nk_sensing_push_audio(int64_t t_ms, const int16_t *samples, uint32_t sample_count);

// 照度データ（lx）を投入する
void nk_sensing_push_light(int64_t t_ms, float lux);

// 1秒分の集計を強制確定する（テストやファイル終端処理用）
void nk_sensing_flush_second(int64_t t_ms);
