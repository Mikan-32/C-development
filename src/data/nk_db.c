#include "nk_db.h"

#include "data_internal.h"

/* Legacy UI adapter. Access only on the UI thread, never on the sensor worker. */
static nk_data *s_data;
static bool s_owned;

int nk_db_open(const char *path, const char *key, int key_len) {
    if (path == NULL || key == NULL || key_len <= 0) {
        return SQLITE_AUTH;
    }
    nk_db_close();
    if (s_data != NULL) {
        return SQLITE_BUSY;
    }
    nk_result result = nk_data_open(&s_data, path, key, (size_t)key_len);
    s_owned = s_data != NULL;
    if (result == NK_OK) {
        result = nk_data_create_schema(s_data);
    }
    if (result != NK_OK) {
        nk_db_close();
        return result == NK_ERROR_MEMORY ? SQLITE_NOMEM : SQLITE_NOTADB;
    }
    return SQLITE_OK;
}

nk_result nk_db_bind(nk_data *data) {
    if (data == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    if (data == s_data) {
        return NK_OK;
    }
    nk_db_close();
    if (s_data != NULL) {
        return NK_ERROR_DATABASE;
    }
    s_data = data;
    s_owned = false;
    return NK_OK;
}

void nk_db_close(void) {
    if (s_owned && nk_data_close(s_data) != NK_OK) {
        return; /* Retain the owned connection for a retry. */
    }
    s_data = NULL;
    s_owned = false;
}

bool nk_db_is_open(void) {
    return s_data != NULL;
}

sqlite3 *nk_db_get_handle(void) {
    return s_data != NULL ? s_data->database : NULL;
}

nk_data *nk_db_get_data(void) {
    return s_data;
}
