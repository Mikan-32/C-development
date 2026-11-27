#ifndef NK_DATA_INTERNAL_H
#define NK_DATA_INTERNAL_H

/* Private to src/data. UI/platform code uses the public data.h operations. */
#include "data.h"

#include <sqlcipher/sqlite3.h>

#define NK_DATA_ERROR_SIZE 256

struct nk_data {
    sqlite3 *database;
    char last_error[NK_DATA_ERROR_SIZE];
};

#endif
