#ifndef NK_CURTAIN_API_H
#define NK_CURTAIN_API_H

#include "result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NK_CURTAIN_API_DEFAULT_URL "https://api.switch-bot.com/v1.1"
#define NK_CURTAIN_API_URL_MAX     256
#define NK_CURTAIN_API_ID_MAX      64
#define NK_CURTAIN_API_NAME_MAX    128
#define NK_CURTAIN_API_TYPE_MAX    32
#define NK_CURTAIN_API_MAX_DEVICES 16

typedef enum nk_curtain_api_error {
    NK_CURTAIN_API_ERR_NONE = 0,
    NK_CURTAIN_API_ERR_NETWORK,    /* 通信不可・タイムアウト */
    NK_CURTAIN_API_ERR_AUTH,       /* HTTP 401 認証エラー */
    NK_CURTAIN_API_ERR_DEVICE,     /* statusCode != 100 / ハブ・機器未応答 */
    NK_CURTAIN_API_ERR_RATE_LIMIT, /* 1日のAPI上限 */
    NK_CURTAIN_API_ERR_PARSE       /* JSON形式不正 */
} nk_curtain_api_error;

typedef enum nk_curtain_api_command {
    NK_CURTAIN_API_CMD_OPEN = 0,
    NK_CURTAIN_API_CMD_CLOSE,
    NK_CURTAIN_API_CMD_SET_POSITION
} nk_curtain_api_command;

typedef struct nk_curtain_api_device {
    char device_id[NK_CURTAIN_API_ID_MAX];
    char device_name[NK_CURTAIN_API_NAME_MAX];
    char device_type[NK_CURTAIN_API_TYPE_MAX];
    char hub_device_id[NK_CURTAIN_API_ID_MAX];
    bool cloud_enabled;
} nk_curtain_api_device;

typedef struct nk_curtain_api_status {
    int open_pct; /* UI基準: 0=全閉, 100=全開 */
    bool moving;
} nk_curtain_api_status;

typedef struct nk_curtain_api nk_curtain_api;

/* クライアント生成・破棄（呼び出し側が nk_curtain_api_destroy で解放する） */
nk_result nk_curtain_api_create(nk_curtain_api **out_api);
void nk_curtain_api_destroy(nk_curtain_api *api);

/* 接続先ベース URL・タイムアウト（ミリ秒）・認証情報の設定 */
nk_result nk_curtain_api_set_base_url(nk_curtain_api *api, const char *base_url);
const char *nk_curtain_api_get_base_url(const nk_curtain_api *api);
nk_result nk_curtain_api_set_timeout_ms(nk_curtain_api *api, long timeout_ms);
nk_result nk_curtain_api_set_credentials(nk_curtain_api *api, const char *token,
                                         const char *secret);

/* SwitchBot v1.1 HMAC-SHA256 + Base64 署名生成（単体テスト検証用に公開） */
nk_result nk_curtain_api_sign(const char *token, const char *secret, int64_t timestamp_ms,
                              const char *nonce, char *out_sign, size_t out_sign_size);

/* 端末一覧取得（Curtain / Curtain3 のみを抽出して返す） */
nk_result nk_curtain_api_list_devices(nk_curtain_api *api, nk_curtain_api_device *out_devices,
                                      size_t max_devices, size_t *out_count);

/* 状態取得（slidePosition を UI 基準の open_pct に変換して返す） */
nk_result nk_curtain_api_get_status(nk_curtain_api *api, const char *device_id,
                                    nk_curtain_api_status *out_status);

/* 開閉命令送信（open_pct は CMD_SET_POSITION 時のみ使用: 0=全閉, 100=全開） */
nk_result nk_curtain_api_send_command(nk_curtain_api *api, const char *device_id,
                                      nk_curtain_api_command cmd, int open_pct);

/* 直近リクエストのエラー種別とステータスコード */
nk_curtain_api_error nk_curtain_api_last_error(const nk_curtain_api *api);
long nk_curtain_api_last_http_status(const nk_curtain_api *api);
int nk_curtain_api_last_api_status(const nk_curtain_api *api);

#endif /* NK_CURTAIN_API_H */
