#ifndef NK_ANDROID_BRIDGE_H
#define NK_ANDROID_BRIDGE_H
#include "result.h"

#include <stddef.h>
#define NK_ANDROID_DB_KEY_SIZE 32
typedef enum nk_android_night_flag {
    NK_ANDROID_NIGHT_MIC = 1,
    NK_ANDROID_NIGHT_PLAYBACK = 2
} nk_android_night_flag;
nk_result nk_android_bridge_db_key(unsigned char *out_key, size_t key_size);
void nk_android_bridge_clear_key(void *key, size_t key_size);
nk_result nk_android_bridge_night_start(unsigned int flags, const char *title, const char *body);
nk_result nk_android_bridge_night_stop(void);
/* Worker cleanup only: all pending DB writes must finish before this call. */
nk_result nk_android_bridge_night_stop_now(void);
typedef enum nk_android_night_state {
    NK_ANDROID_NIGHT_STARTED = 0,
    NK_ANDROID_NIGHT_STOPPED = 1,
    NK_ANDROID_NIGHT_FAILED = 2
} nk_android_night_state;
nk_android_night_state nk_android_bridge_night_state(void);
nk_result nk_android_bridge_notifications_allowed(int *out_allowed);
#endif
