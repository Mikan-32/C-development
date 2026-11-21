#ifndef NK_DATA_DB_H
#define NK_DATA_DB_H

#include "data.h"

#include <sqlcipher/sqlite3.h>
#include <stdbool.h>

/* Compatibility API for the PC onboarding UI. Always uses real SQLCipher. */
int nk_db_open(const char *path, const char *key, int key_len);
void nk_db_close(void);
bool nk_db_is_open(void);
sqlite3 *nk_db_get_handle(void);
nk_data *nk_db_get_data(void);

/* Borrow the application's UI connection; close only unbinds it, never owns it.
 * Unbind before its owner closes it. This adapter is UI-thread-only. */
nk_result nk_db_bind(nk_data *data);

#endif
