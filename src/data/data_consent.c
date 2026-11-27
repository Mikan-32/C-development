#include "data_consent.h"

#include "nk_db.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int64_t consent_time_ms(void) {
    time_t now = time(NULL);
    if (now < 0 || (uint64_t)now > INT64_MAX / 1000) {
        return -1;
    }
    return (int64_t)now * 1000;
}

void nk_data_consent_init(void) {
    /* No process-local cache: another connection may revoke consent. */
}

uint32_t nk_data_consent_get_agreed_version(void) {
    sqlite3 *db = nk_db_get_handle();
    sqlite3_stmt *statement = NULL;
    uint32_t version = 0;
    if (db == NULL || sqlite3_prepare_v2(db,
                                         "SELECT version FROM consent WHERE revoked_ms IS NULL "
                                         "ORDER BY consented_ms DESC,id DESC LIMIT 1;",
                                         -1, &statement, NULL) != SQLITE_OK) {
        return 0;
    }
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(statement, 0);
        char *end = NULL;
        errno = 0;
        unsigned long long value = text != NULL ? strtoull(text, &end, 10) : 0;
        if (text != NULL && text[0] >= '0' && text[0] <= '9' && end != text && *end == '\0' &&
            errno == 0 && value > 0 && value <= UINT32_MAX) {
            char canonical[16];
            (void)snprintf(canonical, sizeof(canonical), "%u", (unsigned int)value);
            if (strcmp(text, canonical) == 0) {
                version = (uint32_t)value;
            }
        }
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        return 0;
    }
    return version;
}

bool nk_data_consent_is_agreed(void) {
    int agreed = 0;
    return nk_data_consent_get_agreed_version() != 0 &&
           nk_data_has_measurement_consent(nk_db_get_data(), &agreed) == NK_OK && agreed != 0;
}

bool nk_data_consent_needs_reagreement(uint32_t current_version) {
    return !nk_data_consent_is_agreed() || current_version == 0 ||
           nk_data_consent_get_agreed_version() != current_version;
}

bool nk_data_consent_set_agreed(bool agreed, uint32_t version) {
    nk_data *data = nk_db_get_data();
    int64_t now_ms = consent_time_ms();
    if (data == NULL || now_ms < 0 || (agreed && version == 0)) {
        return false;
    }
    if (!agreed) {
        return nk_data_revoke_consent(data, now_ms) == NK_OK;
    }
    char version_text[16];
    (void)snprintf(version_text, sizeof(version_text), "%u", (unsigned int)version);
    nk_consent_record consent = {version_text, 1, 0, now_ms};
    return nk_data_consent_set(data, &consent) == NK_OK;
}

void nk_data_consent_reset(void) {
    /* Test helper keeps the audit trail; it never removes consent history. */
    (void)nk_data_consent_set_agreed(false, 0);
}
