#include "data_session.h"

#include "nk_db.h"

int nk_data_session_count(void) {
    sqlite3 *db = nk_db_get_handle();
    sqlite3_stmt *statement = NULL;
    if (db == NULL ||
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM session WHERE end_ms IS NOT NULL;", -1,
                           &statement, NULL) != SQLITE_OK) {
        return -1;
    }
    int count = sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1;
    return sqlite3_finalize(statement) == SQLITE_OK ? count : -1;
}

bool nk_data_session_exists(int64_t session_id) {
    sqlite3 *db = nk_db_get_handle();
    sqlite3_stmt *statement = NULL;
    if (db == NULL || session_id <= 0 ||
        sqlite3_prepare_v2(db, "SELECT 1 FROM session WHERE id=?1 AND end_ms IS NOT NULL;", -1,
                           &statement, NULL) != SQLITE_OK) {
        return false;
    }
    bool found = sqlite3_bind_int64(statement, 1, session_id) == SQLITE_OK &&
                 sqlite3_step(statement) == SQLITE_ROW;
    return sqlite3_finalize(statement) == SQLITE_OK && found;
}

bool nk_data_session_delete_by_id(int64_t session_id) {
    return nk_data_delete_session(nk_db_get_data(), session_id) == NK_OK;
}

bool nk_data_session_delete_all(void) {
    return nk_data_delete_all_sessions(nk_db_get_data()) == NK_OK;
}
