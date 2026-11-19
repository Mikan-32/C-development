#include "nk_db.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static sqlite3 *s_db = NULL;

// キー文字列から簡易検証トークンを生成する内部関数（PCモック検証用）
static uint64_t compute_key_fingerprint(const char *key, int key_len) {
    uint64_t hash = 14695981039346656037ULL; // FNV-1a offset basis
    for (int i = 0; i < key_len; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// 必要なテーブルを初期生成する内部関数
static int create_tables(sqlite3 *db) {
    const char *sql_init = "CREATE TABLE IF NOT EXISTS user_consent ("
                           "    id INTEGER PRIMARY KEY CHECK (id = 1),"
                           "    agreed_version INTEGER NOT NULL,"
                           "    agreed_at INTEGER NOT NULL"
                           ");"
                           "CREATE TABLE IF NOT EXISTS sleep_sessions ("
                           "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "    started_at INTEGER NOT NULL,"
                           "    ended_at INTEGER NOT NULL,"
                           "    sleep_score INTEGER NOT NULL,"
                           "    note TEXT"
                           ");";

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql_init, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg != NULL) {
            sqlite3_free(err_msg);
        }
        return rc;
    }
    return SQLITE_OK;
}

// PC開発環境用：キーの整合性を検証する内部関数
static int verify_dev_key(sqlite3 *db, const char *key, int key_len) {
    const char *sql_meta = "CREATE TABLE IF NOT EXISTS _nk_key_check ("
                           "    id INTEGER PRIMARY KEY CHECK (id = 1),"
                           "    fingerprint TEXT NOT NULL"
                           ");";
    if (sqlite3_exec(db, sql_meta, NULL, NULL, NULL) != SQLITE_OK) {
        return SQLITE_AUTH;
    }

    char expected_fp[32];
    snprintf(expected_fp, sizeof(expected_fp), "%016llx",
             (unsigned long long)compute_key_fingerprint(key, key_len));

    sqlite3_stmt *stmt = NULL;
    const char *sql_select = "SELECT fingerprint FROM _nk_key_check WHERE id = 1;";
    if (sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL) != SQLITE_OK) {
        return SQLITE_AUTH;
    }

    int step_rc = sqlite3_step(stmt);
    if (step_rc == SQLITE_ROW) {
        // 既に登録済みのキー指紋がある場合は照合する
        const unsigned char *stored_fp = sqlite3_column_text(stmt, 0);
        bool match = (stored_fp != NULL && strcmp((const char *)stored_fp, expected_fp) == 0);
        sqlite3_finalize(stmt);
        return match ? SQLITE_OK : SQLITE_NOTADB; // キー不一致ならエラー
    }
    sqlite3_finalize(stmt);

    // 新規DBの場合は今回のキー指紋を登録する
    const char *sql_insert = "INSERT INTO _nk_key_check (id, fingerprint) VALUES (1, ?1);";
    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return SQLITE_AUTH;
    }
    sqlite3_bind_text(stmt, 1, expected_fp, -1, SQLITE_STATIC);
    step_rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (step_rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_AUTH;
}

int nk_db_open(const char *path, const char *key, int key_len) {
    if (s_db != NULL) {
        nk_db_close();
    }

    // 暗号化キーが未指定・空の場合は必ず拒否する
    if (path == NULL || key == NULL || key_len <= 0) {
        return SQLITE_AUTH;
    }

    int rc = sqlite3_open(path, &s_db);
    if (rc != SQLITE_OK) {
        if (s_db != NULL) {
            sqlite3_close(s_db);
            s_db = NULL;
        }
        return rc;
    }

#if defined(SQLITE_HAS_CODEC)
    // SQLCipher 暗号化キーの適用
    rc = sqlite3_key(s_db, key, key_len);
    if (rc != SQLITE_OK) {
        nk_db_close();
        return rc;
    }

    // 本番SQLCipher：実際にテーブル一覧が読めるか（復号成功か）を検証
    rc = sqlite3_exec(s_db, "SELECT count(*) FROM sqlite_master;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        nk_db_close();
        return rc;
    }
#else
    // PCモック環境（SQLCipher なし）：暗号化はせず、キーの整合性だけ検証する
    rc = verify_dev_key(s_db, key, key_len);
    if (rc != SQLITE_OK) {
        nk_db_close();
        return rc;
    }
#endif

    // 業務テーブル生成
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
