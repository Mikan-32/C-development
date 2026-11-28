#ifndef NK_ANDROID_BRIDGE_H
#define NK_ANDROID_BRIDGE_H

#include "result.h"

#include <stddef.h>
#include <stdint.h>

#define NK_ANDROID_DB_KEY_SIZE 32

#define NK_ANDROID_NIGHT_MIC      1U
#define NK_ANDROID_NIGHT_PLAYBACK 2U

typedef enum nk_android_night_state {
    NK_ANDROID_NIGHT_STOPPED = 0,
    NK_ANDROID_NIGHT_STARTED = 1,
    NK_ANDROID_NIGHT_FAILED = 2
} nk_android_night_state;

void nk_android_bridge_clear_key(void *key, size_t key_size);
nk_result nk_android_bridge_db_key(unsigned char *out_key, size_t key_size);
nk_result nk_android_bridge_night_start(unsigned int flags, const char *title, const char *body);
nk_result nk_android_bridge_night_stop(void);
nk_result nk_android_bridge_night_stop_now(void);
nk_android_night_state nk_android_bridge_night_state(void);
nk_result nk_android_bridge_notifications_allowed(int *out_allowed);

/* カーテン開閉予約の登録・取り消し（JNI 経由で NativeBridge.schedule / cancel を呼ぶ） */
nk_result nk_android_bridge_schedule_curtain(int schedule_id, int64_t at_ms, int kind,
                                             int timeout_ms, const char *title, const char *body);
nk_result nk_android_bridge_cancel_curtain(int schedule_id);

#endif /* NK_ANDROID_BRIDGE_H */
