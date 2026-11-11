#pragma once

#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

// データベースのオープン＆暗号化キー適用
// path: DBファイルへのパス（":memory:" でインメモリDBも可）
// key: 暗号化パスフレーズ
// key_len: キーのバイト長
// 戻り値: 成功時 0、失敗時 SQLite エラーコード
int nk_db_open(const char *path, const char *key, int key_len);

// データベースのクローズ
void nk_db_close(void);

// データベースが開かれているか確認
bool nk_db_is_open(void);

// 現在開いている sqlite3 ハンドルを取得（他のモジュールからSQL実行用）
sqlite3 *nk_db_get_handle(void);
