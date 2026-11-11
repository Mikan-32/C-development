#pragma once

#include <stdbool.h>
#include <stdint.h>

void data_consent_init(void);

// 同意済みかつ、指定バージョン以上で同意されているか確認
bool data_consent_is_agreed(void);
uint32_t data_consent_get_agreed_version(void);

// 同意状態とバージョンを設定
bool data_consent_set_agreed(bool agreed, uint32_t version);

void data_consent_reset(void);
