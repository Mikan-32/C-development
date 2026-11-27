#ifndef NK_ANDROID_RECORDING_H
#define NK_ANDROID_RECORDING_H

#include "sensing.h"

typedef struct nk_android_recording nk_android_recording;

/*
 * 計測オブジェクトは専用のDB接続・session・sensingを所有する。
 * すべての操作はセンサー所有スレッドで行い、UIのDB接続は共有しない。
 * openの成功・失敗にかかわらず、*out_recordingが非NULLなら、
 * 呼び出し側がnk_android_recording_close(out_recording)で解放する。
 */
nk_result nk_android_recording_open(nk_android_recording **out_recording);
nk_result nk_android_recording_check_consent(nk_android_recording *recording);
nk_result nk_android_recording_start(nk_android_recording *recording, int64_t start_ms);
nk_result nk_android_recording_push_second(nk_android_recording *recording,
                                           const nk_sensing_second *second);
nk_result nk_android_recording_finish(nk_android_recording *recording, int64_t end_ms,
                                      nk_session_end_reason reason);

/*
 * 成功すると*recordingはNULLになる。
 * 失敗時は呼び出し側が所有権を維持し、再試行できるよう残す。
 */
nk_result nk_android_recording_close(nk_android_recording **recording);

#endif
