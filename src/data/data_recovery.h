#ifndef NK_DATA_RECOVERY_H
#define NK_DATA_RECOVERY_H

#include "data.h"

typedef struct nk_data_recovery_report {
    int recovered_sessions;
    int interrupted_sessions;
    int completed_user_stops;
    int q04_sessions;
    int added_epochs;

    /* The most recent persisted interruption, also returned on later opens. */
    int has_interruption;
    int64_t interrupted_session_id;
    int64_t gap_start_ms;
    int64_t gap_end_ms;
} nk_data_recovery_report;

/* Startup only: the caller must ensure no measurement owns a live session. */
/* Run outside a transaction, on the thread owning data. This never starts */
/* sensors, grants consent, or changes already finished sessions. */
/* The update is atomic. On failure, report is zeroed and changes roll back. */
/* Downtime starts after the last saved valid epoch, or an earlier open Q04. */
/* Missing epochs keep optional values NULL. New Q04 pairs require >= 1 minute. */
/* A persisted USER/USER_STOP marker is completed as an intentional stop. */
nk_result nk_data_recover_unfinished(nk_data *data, int64_t recovered_ms,
                                     nk_data_recovery_report *report);

#endif
