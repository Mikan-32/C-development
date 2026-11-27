#include <sqlcipher/sqlite3.h>
#include <stdio.h>

int main(void) {
    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    const unsigned char *version;
    int result;

    result = sqlite3_open(":memory:", &database);
    if (result != SQLITE_OK) {
        if (database != NULL) {
            fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(database));
            sqlite3_close(database);
        } else {
            fprintf(stderr, "Failed to create database handle\n");
        }

        return 1;
    }

    result = sqlite3_prepare_v2(database, "PRAGMA cipher_version;", -1, &statement, NULL);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare cipher_version query: %s\n", sqlite3_errmsg(database));
        sqlite3_close(database);
        return 1;
    }

    result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        fprintf(stderr, "SQLCipher version was not returned\n");
        sqlite3_finalize(statement);
        sqlite3_close(database);
        return 1;
    }

    version = sqlite3_column_text(statement, 0);
    if (version == NULL || version[0] == '\0') {
        fprintf(stderr, "SQLCipher version is empty\n");
        sqlite3_finalize(statement);
        sqlite3_close(database);
        return 1;
    }

    printf("SQLCipher version: %s\n", (const char *)version);

    sqlite3_finalize(statement);
    sqlite3_close(database);

    return 0;
}
