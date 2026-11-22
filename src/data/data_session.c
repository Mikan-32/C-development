#include "data_session.h"

#include "nk_db.h"

#include <stddef.h>

bool nk_data_session_insert(int64_t started_at, int64_t ended_at, int sleep_score,
                            int64_t *out_id) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return false;
    }

    const char *sql = "INSERT INTO sleep_sessions (started_at, ended_at, sleep_score) "
                      "VALUES (?1, ?2, ?3);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, started_at);
    sqlite3_bind_int64(stmt, 2, ended_at);
    sqlite3_bind_int(stmt, 3, sleep_score);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return false;
    }

    if (out_id != NULL) {
        *out_id = sqlite3_last_insert_rowid(db);
    }
    return true;
}

int nk_data_session_count(void) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return -1;
    }

    const char *sql = "SELECT COUNT(*) FROM sleep_sessions;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

bool nk_data_session_exists(int64_t session_id) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return false;
    }

    const char *sql = "SELECT 1 FROM sleep_sessions WHERE id = ?1 LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, session_id);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

bool nk_data_session_delete_by_id(int64_t session_id) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return false;
    }

    const char *sql = "DELETE FROM sleep_sessions WHERE id = ?1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, session_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // 実際に1件以上削除されたか確認
    return (rc == SQLITE_DONE) && (sqlite3_changes(db) > 0);
}

bool nk_data_session_delete_all(void) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL) {
        return false;
    }

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, "DELETE FROM sleep_sessions;", NULL, NULL, &err_msg);
    if (err_msg != NULL) {
        sqlite3_free(err_msg);
    }
    return rc == SQLITE_OK;
}

int nk_data_session_list(nk_sleep_session_t *out_list, int max_count) {
    sqlite3 *db = nk_db_get_handle();
    if (db == NULL || out_list == NULL || max_count <= 0) {
        return -1;
    }

    const char *sql = "SELECT id, started_at, ended_at, sleep_score "
                      "FROM sleep_sessions "
                      "ORDER BY started_at DESC, id DESC "
                      "LIMIT ?1;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, max_count);

    int count = 0;
    while (count < max_count && sqlite3_step(stmt) == SQLITE_ROW) {
        out_list[count].id = sqlite3_column_int64(stmt, 0);
        out_list[count].started_at = sqlite3_column_int64(stmt, 1);
        out_list[count].ended_at = sqlite3_column_int64(stmt, 2);
        out_list[count].sleep_score = sqlite3_column_int(stmt, 3);
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}
