#ifndef NK_REPLAY_H
#define NK_REPLAY_H

#include "sensing.h"

/*
 * NKS1形式の1秒特徴量ログを再生する。
 * 生音声PCMの再生ではない。
 *
 * callbacksとuser_dataの所有権は移動しない。
 */
nk_result nk_replay_file(const char *path, const nk_sensing_config *config,
                         const nk_sensing_callbacks *callbacks, void *user_data);

#endif
