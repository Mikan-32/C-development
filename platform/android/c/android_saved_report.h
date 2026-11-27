#ifndef NK_ANDROID_SAVED_REPORT_H
#define NK_ANDROID_SAVED_REPORT_H

#include "result.h"

#include <stddef.h>
#include <stdint.h>

/* A diagnostic snapshot of the latest stored session, before any recovery writes. */
typedef struct nk_android_saved_report {
    int found;
    int finished;
    int64_t session_id;
    int64_t start_ms;
    int64_t end_ms;
    int64_t expected_epochs;
    int64_t epochs;
    int64_t good_acc_epochs;
    int64_t charging_epochs;
    int battery_start; /* -1 means not stored. */
    int battery_end;
    char end_reason[32];
} nk_android_saved_report;

/* Opens an existing encrypted DB read-only; never creates, recovers, or finishes sessions. */
nk_result nk_android_saved_report_read(const char *path, const void *key, size_t key_size,
                                       nk_android_saved_report *out_report);

/* Human-readable text for both the on-device dialog and logcat. */
nk_result nk_android_saved_report_format(const nk_android_saved_report *report, char *text,
                                         size_t text_size);

#endif
