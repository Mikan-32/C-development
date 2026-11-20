#ifndef NK_ANDROID_ACCEL_H
#define NK_ANDROID_ACCEL_H

#include "result.h"

#include <stdbool.h>

/* Call prepare/shutdown on the SDL main thread, after checking consent. */
/* prepare arms a worker; sampling waits for the foreground service to start. */
nk_result nk_android_accel_prepare(void);

/* Safe from JNI callbacks: requests a stop without waiting for the worker. */
void nk_android_accel_request_stop(void);
void nk_android_accel_request_user_stop(void);
bool nk_android_accel_is_active(void);
nk_result nk_android_accel_shutdown(void);
const char *nk_android_accel_status(void);

#endif
