#pragma once

#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

// データベースのオープン＆暗号化キー検証
// 戻り値: 成功時 0 (SQLITE_OK)、キー不一致やエラー時は負値または SQLite エラーコード
int nk_db_open(const char *path, const char *key, int key_len);

// データベースのクローズ
void nk_db_close(void);

// データベースが開かれているか確認
bool nk_db_is_open(void);

// 現在開いている sqlite3 ハンドルを取得
sqlite3 *nk_db_get_handle(void);
