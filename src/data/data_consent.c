#include "data_consent.h"

#include "nk_db.h"

#include <time.h>

static bool s_is_agreed = false;
static uint32_t s_agreed_version = 0;

void nk_data_consent_init(void) {
    s_is_agreed = false;
    s_agreed_version = 0;

    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return;
    }

    const char *sql = "SELECT agreed_version FROM user_consent WHERE id = 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            uint32_t ver = (uint32_t)sqlite3_column_int(stmt, 0);
            if (ver > 0) {
                s_is_agreed = true;
                s_agreed_version = ver;
            }
        }
        sqlite3_finalize(stmt);
    }
}

bool nk_data_consent_is_agreed(void) {
    return s_is_agreed;
}

uint32_t nk_data_consent_get_agreed_version(void) {
    return s_agreed_version;
}

bool nk_data_consent_needs_reagreement(uint32_t current_version) {
    if (!s_is_agreed) {
        return true;
    }
    return s_agreed_version < current_version;
}

bool nk_data_consent_set_agreed(bool agreed, uint32_t version) {
    uint32_t target_ver = agreed ? version : 0U;
    s_is_agreed = agreed && (target_ver > 0U);
    s_agreed_version = target_ver;

    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return true;
    }

    if (!s_is_agreed) {
        char *err_msg = NULL;
        int rc = sqlite3_exec(db, "DELETE FROM user_consent WHERE id = 1;", NULL, NULL, &err_msg);
        if (err_msg != NULL) {
            sqlite3_free(err_msg);
        }
        return rc == SQLITE_OK;
    }

    const char *sql = "INSERT INTO user_consent (id, agreed_version, agreed_at) "
                      "VALUES (1, ?1, ?2) "
                      "ON CONFLICT(id) DO UPDATE SET "
                      "agreed_version = excluded.agreed_version, "
                      "agreed_at = excluded.agreed_at;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, (int)target_ver);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

void nk_data_consent_reset(void) {
    s_is_agreed = false;
    s_agreed_version = 0;

    sqlite3 *db = nk_db_get_handle();
    if (db != NULL) {
        sqlite3_exec(db, "DELETE FROM user_consent WHERE id = 1;", NULL, NULL, NULL);
    }
}
