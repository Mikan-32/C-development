#ifndef NK_ANDROID_RECORDING_H
#define NK_ANDROID_RECORDING_H

#include "sensing.h"

typedef struct nk_android_recording nk_android_recording;

/* One recording owns a separate DB connection, session and sensing instance. */
/* Use all functions on the sensor owner thread. No UI DB handle is shared. */
/* The caller must close any returned object, even if open returns an error. */
nk_result nk_android_recording_open(nk_android_recording **out_recording);
nk_result nk_android_recording_start(nk_android_recording *recording, int64_t start_ms);
nk_result nk_android_recording_push_second(nk_android_recording *recording,
                                           const nk_sensing_second *second);
nk_result nk_android_recording_finish(nk_android_recording *recording, int64_t end_ms,
                                      nk_session_end_reason reason);

/* On close failure the object remains owned by the caller for a retry. */
nk_result nk_android_recording_close(nk_android_recording **recording);

#endif
