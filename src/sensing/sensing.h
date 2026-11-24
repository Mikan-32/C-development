#ifndef NK_SENSING_H
#define NK_SENSING_H

#include "data.h"
#include "result.h"

#include <stdint.h>

/*
 * AI-1の計測・保存単位。
 */
#define NK_SENSING_EPOCH_SECONDS 30
#define NK_SENSING_ACC_HZ        50
#define NK_SENSING_MIC_HZ        16000
#define NK_SENSING_MAX_EPOCHS    1680

typedef struct nk_sensing nk_sensing;

/*
 * 1秒分の重力方向。
 *
 * has_valueが0の場合は欠損。
 * 値がある場合、x・y・zは単位ベクトル。
 */
typedef struct nk_sensing_gravity {
    int has_value;
    double x;
    double y;
    double z;
} nk_sensing_gravity;

/*
 * メモリ上で扱う1秒分の特徴量。
 * この構造体自体はDBへ保存しない。
 *
 * Androidではセンサー処理が作成し、
 * PCテストでは合成データを渡す。
 */
typedef struct nk_sensing_second {
    /*
     * 計測開始からの秒番号。
     * 0、1、2……の順に渡す。
     */
    int idx;

    /*
     * 実際に取得できたサンプル数。
     *
     * 通常は加速度50、マイク16000。
     * 欠損した秒は0。
     */
    int acc_sample_count;
    int mic_sample_count;

    /*
     * 重力成分を除いた加速度の平均。単位はmg。
     */
    nk_optional_double acc_hp;

    nk_sensing_gravity gravity;

    /*
     * 1秒分の音量。単位はdBFS。
     */
    nk_optional_double snd_db;

    /*
     * いびきらしさ。範囲は0〜1。
     * 音声処理が未対応・取得失敗の場合は欠損にする。
     */
    nk_optional_double snore_like;

    /*
     * その秒に有効な照度。単位はlx。
     * Android側の取得処理が照度の有効性を管理する。
     */
    nk_optional_double lux;

    /*
     * その秒の状態。0または1。
     */
    int mic_busy;
    int charging;
    int sound_on;
} nk_sensing_second;

/*
 * 計測開始時の設定。
 */
typedef struct nk_sensing_config {
    int64_t start_ms;

    int mic_used;
    int light_used;

    /*
     * 体動しきい値。AI-1の既定値は15.0 mg。
     */
    double move_threshold_mg;
} nk_sensing_config;

/*
 * 集計結果を受け取る関数。
 *
 * recordはコールバック実行中だけ有効。
 * 後から使用する場合は受け取り側で内容をコピーする。
 *
 * NK_OK以外を返した場合、sensing側は処理を停止する。
 */
typedef nk_result (*nk_sensing_epoch_callback)(void *user_data, const nk_epoch_record *record);

typedef nk_result (*nk_sensing_event_callback)(void *user_data, const nk_event_record *record);

typedef struct nk_sensing_callbacks {
    nk_sensing_epoch_callback on_epoch;
    nk_sensing_event_callback on_event;
} nk_sensing_callbacks;

/*
 * 集計オブジェクトを作成する。
 *
 * configとcallbacksの内容は内部へコピーする。
 * user_dataの所有権は移動しない。
 *
 * 成功時に返される*out_sensingは、
 * 呼び出し側がnk_sensing_destroy()で解放する。
 */
nk_result nk_sensing_create(nk_sensing **out_sensing, const nk_sensing_config *config,
                            const nk_sensing_callbacks *callbacks, void *user_data);

/*
 * 1秒分の特徴量を渡す。
 *
 * 入力はidx=0から連続する順番で渡す。
 * 取得できなかった秒も、サンプル数0・特徴量欠損で渡す。
 *
 * 30秒分そろったらon_epochを呼び出す。
 * 出力のsession_idは0とし、session側で設定する。
 */
nk_result nk_sensing_push_second(nk_sensing *sensing, const nk_sensing_second *second);

/*
 * 継続中のSNOREを最後の入力秒の直後で確定する。
 * 30秒未満の端数も、取得率の分母を30秒としてepochを出力する。
 *
 * 成功後の再呼び出しはNK_OK。
 * 成功後はpush_secondを呼べない。
 *
 * push・finish・コールバックは同じスレッドで直列に実行する。
 */
nk_result nk_sensing_finish(nk_sensing *sensing);

/*
 * 集計オブジェクトを解放する。
 *
 * DBやsession、user_dataは解放しない。
 * 未出力のデータを自動保存する処理は行わない。
 */
void nk_sensing_destroy(nk_sensing *sensing);

#endif
