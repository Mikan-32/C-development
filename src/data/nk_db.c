#include "nk_db.h"

#include <stddef.h>

// PC 開発用：SQLCipher がない環境では sqlite3_key をダミーとして定義
#if !defined(SQLITE_HAS_CODEC)
int sqlite3_key(sqlite3 *db, const void *pKey, int nKey) {
    (void)db;
    (void)pKey;
    (void)nKey;
    return SQLITE_OK; // 常に成功扱い
}
#endif

static sqlite3 *s_db = NULL;
// (以降のコードはそのまま)

// 必要なテーブルを初期生成する内部関数
static int create_tables(sqlite3 *db) {
    const char *sql_consent = "CREATE TABLE IF NOT EXISTS user_consent ("
                              "    id INTEGER PRIMARY KEY CHECK (id = 1)," // 常に1行のみ保持
                              "    agreed_version INTEGER NOT NULL,"
                              "    agreed_at INTEGER NOT NULL" // UNIXタイムスタンプ
                              ");";

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql_consent, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return rc;
    }

    return SQLITE_OK;
}

int nk_db_open(const char *path, const char *key, int key_len) {
    if (s_db != NULL) {
        nk_db_close();
    }

    int rc = sqlite3_open(path, &s_db);
    if (rc != SQLITE_OK) {
        if (s_db) {
            sqlite3_close(s_db);
            s_db = NULL;
        }
        return rc;
    }

    // SQLCipher 暗号化キーの適用
    if (key != NULL && key_len > 0) {
        rc = sqlite3_key(s_db, key, key_len);
        if (rc != SQLITE_OK) {
            nk_db_close();
            return rc;
        }
    }

    // テーブル生成
    rc = create_tables(s_db);
    if (rc != SQLITE_OK) {
        nk_db_close();
        return rc;
    }

    return SQLITE_OK;
}

void nk_db_close(void) {
    if (s_db != NULL) {
        sqlite3_close(s_db);
        s_db = NULL;
    }
}

bool nk_db_is_open(void) {
    return s_db != NULL;
}

sqlite3 *nk_db_get_handle(void) {
    return s_db;
}
