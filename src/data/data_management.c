#include "data_internal.h"

#include <stdio.h>
#include <string.h>

static nk_result database_error(nk_data *data) {
    (void)snprintf(data->last_error, sizeof(data->last_error), "%s",
                   sqlite3_errmsg(data->database));
    return NK_ERROR_DATABASE;
}

nk_result nk_data_has_versioned_consent(nk_data *data, const char *version, int *out_agreed) {
    if (out_agreed == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_agreed = 0;
    if (data == NULL || version == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    sqlite3_stmt *statement = NULL;
    const char *sql = "SELECT measurement,version FROM consent WHERE revoked_ms IS NULL "
                      "ORDER BY consented_ms DESC,id DESC LIMIT 1;";
    if (sqlite3_prepare_v2(data->database, sql, -1, &statement, NULL) != SQLITE_OK) {
        return database_error(data);
    }
    int step = sqlite3_step(statement);
    int agreed = 0;
    if (step == SQLITE_ROW) {
        const unsigned char *stored = sqlite3_column_text(statement, 1);
        agreed = sqlite3_column_int(statement, 0) != 0 && stored != NULL &&
                 strcmp((const char *)stored, version) == 0;
    }
    nk_result result = step == SQLITE_ROW || step == SQLITE_DONE ? NK_OK : database_error(data);
    if (sqlite3_finalize(statement) != SQLITE_OK && result == NK_OK) {
        result = database_error(data);
    }
    if (result == NK_OK) {
        *out_agreed = agreed;
    }
    return result;
}

nk_result nk_data_revoke_consent(nk_data *data, int64_t revoked_ms) {
    if (data == NULL || revoked_ms < 0) {
        return NK_ERROR_ARGUMENT;
    }
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(data->database,
                           "UPDATE consent SET revoked_ms=?1 WHERE revoked_ms IS NULL;", -1,
                           &statement, NULL) != SQLITE_OK) {
        return database_error(data);
    }
    nk_result result = NK_OK;
    if (sqlite3_bind_int64(statement, 1, revoked_ms) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) {
        result = database_error(data);
    }
    if (sqlite3_finalize(statement) != SQLITE_OK && result == NK_OK) {
        result = database_error(data);
    }
    return result;
}

static nk_result delete_records(nk_data *data, int64_t id) {
    static const char *const deletes[] = {"DELETE FROM event WHERE (?1=0 OR session_id=?1);",
                                          "DELETE FROM epoch WHERE (?1=0 OR session_id=?1);",
                                          "DELETE FROM session WHERE (?1=0 OR id=?1);"};
    if (data == NULL || id < 0) {
        return NK_ERROR_ARGUMENT;
    }
    if (!sqlite3_get_autocommit(data->database)) {
        (void)snprintf(data->last_error, sizeof(data->last_error),
                       "Deletion requires its own transaction");
        return NK_ERROR_DATABASE;
    }
    if (sqlite3_exec(data->database, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
        return database_error(data);
    }
    nk_result result = NK_OK;
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(data->database, "SELECT end_ms FROM session WHERE (?1=0 OR id=?1);", -1,
                           &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 1, id) != SQLITE_OK) {
        result = database_error(data);
        goto finish;
    }
    int step;
    int found = 0;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        found = 1;
        if (sqlite3_column_type(statement, 0) == SQLITE_NULL) {
            (void)snprintf(data->last_error, sizeof(data->last_error),
                           "Stop recording before deleting its data");
            result = NK_ERROR_ALREADY_RUNNING;
            goto finish;
        }
    }
    if (step != SQLITE_DONE) {
        result = database_error(data);
        goto finish;
    }
    if (id != 0 && !found) {
        result = NK_ERROR_ARGUMENT;
        goto finish;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        statement = NULL;
        result = database_error(data);
        goto finish;
    }
    statement = NULL;
    for (size_t i = 0; i < sizeof(deletes) / sizeof(deletes[0]); ++i) {
        if (sqlite3_prepare_v2(data->database, deletes[i], -1, &statement, NULL) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 1, id) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE) {
            result = database_error(data);
            goto finish;
        }
        if (sqlite3_finalize(statement) != SQLITE_OK) {
            statement = NULL;
            result = database_error(data);
            goto finish;
        }
        statement = NULL;
    }
finish:
    if (sqlite3_finalize(statement) != SQLITE_OK && result == NK_OK) {
        result = database_error(data);
    }
    if (result == NK_OK && sqlite3_exec(data->database, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
        result = database_error(data);
    }
    if (result != NK_OK) {
        char original[NK_DATA_ERROR_SIZE];
        memcpy(original, data->last_error, sizeof(original));
        if (sqlite3_exec(data->database, "ROLLBACK;", NULL, NULL, NULL) != SQLITE_OK) {
            (void)snprintf(data->last_error, sizeof(data->last_error),
                           "Rollback failed; original: %.200s", original);
        } else {
            memcpy(data->last_error, original, sizeof(original));
        }
    }
    return result;
}

nk_result nk_data_delete_session(nk_data *data, int64_t session_id) {
    return session_id > 0 ? delete_records(data, session_id) : NK_ERROR_ARGUMENT;
}

nk_result nk_data_delete_all_sessions(nk_data *data) {
    return delete_records(data, 0);
}
