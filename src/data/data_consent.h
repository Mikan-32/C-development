#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NK_DATA_CONSENT_CURRENT_VERSION 1U

// 同意データ管理の初期化（DBが開いていれば保存済み状態をロード）
void nk_data_consent_init(void);

// 同意済みかどうかを取得
bool nk_data_consent_is_agreed(void);

// 同意済みの版番号を取得（未同意時は 0）
uint32_t nk_data_consent_get_agreed_version(void);

// 現在の最新規約版に対して、初回同意または再同意が必要か判定
bool nk_data_consent_needs_reagreement(uint32_t current_version);

// 同意状態と版番号を保存（DBが開いていればSQLiteにも永続化）
bool nk_data_consent_set_agreed(bool agreed, uint32_t version);

// テスト・初期化用：メモリおよびDBの同意状態をリセット
void nk_data_consent_reset(void);
