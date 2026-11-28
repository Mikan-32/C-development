#include "curtain_api.h"

#include "cJSON.h"

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOKEN_MAX  256
#define SECRET_MAX 256

struct nk_curtain_api {
    char base_url[NK_CURTAIN_API_URL_MAX];
    char token[TOKEN_MAX];
    char secret[SECRET_MAX];
    long timeout_ms;
    nk_curtain_api_error last_err;
    long last_http_code;
    int last_api_code;
};

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} response_buf;

static int clamp_pct(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > 100) {
        return 100;
    }
    return v;
}

static int64_t now_epoch_ms(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        return (int64_t)ts.tv_sec * 1000LL + (int64_t)(ts.tv_nsec / 1000000LL);
    }
    return (int64_t)time(NULL) * 1000LL;
}

static void generate_nonce(char out_nonce[33]) {
    unsigned char raw[16];
    if (RAND_bytes(raw, (int)sizeof(raw)) != 1) {
        int64_t t = now_epoch_ms();
        for (size_t i = 0; i < sizeof(raw); i++) {
            raw[i] = (unsigned char)((t >> ((i % 8) * 8)) ^ (i * 37U));
        }
    }
    for (size_t i = 0; i < sizeof(raw); i++) {
        (void)snprintf(&out_nonce[i * 2], 3, "%02x", (unsigned int)raw[i]);
    }
    out_nonce[32] = '\0';
}

static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    response_buf *rb = (response_buf *)userdata;
    if (rb == NULL || total == 0) {
        return 0;
    }
    if (rb->len + total + 1 > rb->cap) {
        size_t new_cap = (rb->cap == 0) ? 1024 : rb->cap * 2;
        while (new_cap < rb->len + total + 1) {
            new_cap *= 2;
        }
        char *next = (char *)realloc(rb->data, new_cap);
        if (next == NULL) {
            return 0;
        }
        rb->data = next;
        rb->cap = new_cap;
    }
    memcpy(rb->data + rb->len, ptr, total);
    rb->len += total;
    rb->data[rb->len] = '\0';
    return total;
}

static void copy_cstr(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    (void)snprintf(dst, dst_size, "%s", src);
}

static bool is_curtain_type(const char *type_str) {
    if (type_str == NULL) {
        return false;
    }
    return strcmp(type_str, "Curtain") == 0 || strcmp(type_str, "Curtain3") == 0;
}

static nk_result perform_request(nk_curtain_api *api, const char *method, const char *path,
                                 const char *json_body, cJSON **out_root) {
    if (api == NULL || method == NULL || path == NULL || out_root == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_root = NULL;
    api->last_err = NK_CURTAIN_API_ERR_NONE;
    api->last_http_code = 0;
    api->last_api_code = 0;

    if (api->token[0] == '\0' || api->secret[0] == '\0') {
        api->last_err = NK_CURTAIN_API_ERR_AUTH;
        return NK_ERROR_PERMISSION_REQUIRED;
    }

    char url[NK_CURTAIN_API_URL_MAX + 128];
    (void)snprintf(url, sizeof(url), "%s%s", api->base_url, path);

    int64_t ts_ms = now_epoch_ms();
    char ts_str[32];
    (void)snprintf(ts_str, sizeof(ts_str), "%lld", (long long)ts_ms);

    char nonce[33];
    generate_nonce(nonce);

    char sign_b64[128];
    nk_result sign_rc =
        nk_curtain_api_sign(api->token, api->secret, ts_ms, nonce, sign_b64, sizeof(sign_b64));
    if (sign_rc != NK_OK) {
        api->last_err = NK_CURTAIN_API_ERR_AUTH;
        return sign_rc;
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        api->last_err = NK_CURTAIN_API_ERR_NETWORK;
        return NK_ERROR_MEMORY;
    }

    char hdr_auth[TOKEN_MAX + 32];
    char hdr_sign[160];
    char hdr_t[48];
    char hdr_nonce[64];
    (void)snprintf(hdr_auth, sizeof(hdr_auth), "Authorization: %s", api->token);
    (void)snprintf(hdr_sign, sizeof(hdr_sign), "sign: %s", sign_b64);
    (void)snprintf(hdr_t, sizeof(hdr_t), "t: %s", ts_str);
    (void)snprintf(hdr_nonce, sizeof(hdr_nonce), "nonce: %s", nonce);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, hdr_auth);
    headers = curl_slist_append(headers, hdr_sign);
    headers = curl_slist_append(headers, hdr_t);
    headers = curl_slist_append(headers, hdr_nonce);
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf8");

    response_buf rb;
    memset(&rb, 0, sizeof(rb));

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, api->timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, api->timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body != NULL ? json_body : "{}");
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    api->last_http_code = http_code;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(rb.data);
        api->last_err = NK_CURTAIN_API_ERR_NETWORK;
        return NK_ERROR_IO;
    }

    if (http_code == 401 || http_code == 403) {
        free(rb.data);
        api->last_err = NK_CURTAIN_API_ERR_AUTH;
        return NK_ERROR_PERMISSION_REQUIRED;
    }

    if (http_code == 429) {
        free(rb.data);
        api->last_err = NK_CURTAIN_API_ERR_RATE_LIMIT;
        return NK_ERROR_IO;
    }

    if (http_code < 200 || http_code >= 300 || rb.data == NULL) {
        free(rb.data);
        api->last_err = NK_CURTAIN_API_ERR_DEVICE;
        return NK_ERROR_IO;
    }

    cJSON *root = cJSON_Parse(rb.data);
    free(rb.data);
    if (root == NULL) {
        api->last_err = NK_CURTAIN_API_ERR_PARSE;
        return NK_ERROR_FORMAT;
    }

    const cJSON *sc_item = cJSON_GetObjectItemCaseSensitive(root, "statusCode");
    if (!cJSON_IsNumber(sc_item)) {
        cJSON_Delete(root);
        api->last_err = NK_CURTAIN_API_ERR_PARSE;
        return NK_ERROR_FORMAT;
    }

    int status_code = sc_item->valueint;
    api->last_api_code = status_code;

    if (status_code != 100) {
        cJSON_Delete(root);
        if (status_code == 190 || status_code == 161 || status_code == 171) {
            api->last_err = NK_CURTAIN_API_ERR_DEVICE;
        } else if (status_code == 429) {
            api->last_err = NK_CURTAIN_API_ERR_RATE_LIMIT;
        } else {
            api->last_err = NK_CURTAIN_API_ERR_DEVICE;
        }
        return NK_ERROR_IO;
    }

    *out_root = root;
    return NK_OK;
}

nk_result nk_curtain_api_create(nk_curtain_api **out_api) {
    if (out_api == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_api = NULL;
    nk_curtain_api *api = (nk_curtain_api *)calloc(1, sizeof(nk_curtain_api));
    if (api == NULL) {
        return NK_ERROR_MEMORY;
    }
    copy_cstr(api->base_url, sizeof(api->base_url), NK_CURTAIN_API_DEFAULT_URL);
    api->timeout_ms = 10000L;
    *out_api = api;
    return NK_OK;
}

void nk_curtain_api_destroy(nk_curtain_api *api) {
    if (api == NULL) {
        return;
    }
    volatile unsigned char *p = (volatile unsigned char *)api;
    for (size_t i = 0; i < sizeof(nk_curtain_api); i++) {
        p[i] = 0;
    }
    free(api);
}

nk_result nk_curtain_api_set_base_url(nk_curtain_api *api, const char *base_url) {
    if (api == NULL || base_url == NULL || base_url[0] == '\0') {
        return NK_ERROR_ARGUMENT;
    }
    copy_cstr(api->base_url, sizeof(api->base_url), base_url);
    size_t len = strlen(api->base_url);
    while (len > 1 && api->base_url[len - 1] == '/') {
        api->base_url[--len] = '\0';
    }
    return NK_OK;
}

const char *nk_curtain_api_get_base_url(const nk_curtain_api *api) {
    if (api == NULL) {
        return NK_CURTAIN_API_DEFAULT_URL;
    }
    return api->base_url;
}

nk_result nk_curtain_api_set_timeout_ms(nk_curtain_api *api, long timeout_ms) {
    if (api == NULL || timeout_ms <= 0) {
        return NK_ERROR_ARGUMENT;
    }
    api->timeout_ms = timeout_ms;
    return NK_OK;
}

nk_result nk_curtain_api_set_credentials(nk_curtain_api *api, const char *token,
                                         const char *secret) {
    if (api == NULL || token == NULL || secret == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    copy_cstr(api->token, sizeof(api->token), token);
    copy_cstr(api->secret, sizeof(api->secret), secret);
    return NK_OK;
}

nk_result nk_curtain_api_sign(const char *token, const char *secret, int64_t timestamp_ms,
                              const char *nonce, char *out_sign, size_t out_sign_size) {
    if (token == NULL || secret == NULL || nonce == NULL || out_sign == NULL ||
        out_sign_size < 48) {
        return NK_ERROR_ARGUMENT;
    }

    char payload[TOKEN_MAX + 96];
    int n = snprintf(payload, sizeof(payload), "%s%lld%s", token, (long long)timestamp_ms, nonce);
    if (n <= 0 || (size_t)n >= sizeof(payload)) {
        return NK_ERROR_ARGUMENT;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    unsigned char *res = HMAC(EVP_sha256(), secret, (int)strlen(secret),
                              (const unsigned char *)payload, (size_t)n, digest, &digest_len);
    if (res == NULL || digest_len == 0) {
        return NK_ERROR_IO;
    }

    unsigned char encoded[96];
    int enc_len = EVP_EncodeBlock(encoded, digest, (int)digest_len);
    if (enc_len <= 0 || (size_t)enc_len >= out_sign_size) {
        return NK_ERROR_MEMORY;
    }
    memcpy(out_sign, encoded, (size_t)enc_len);
    out_sign[enc_len] = '\0';
    return NK_OK;
}

nk_result nk_curtain_api_list_devices(nk_curtain_api *api, nk_curtain_api_device *out_devices,
                                      size_t max_devices, size_t *out_count) {
    if (api == NULL || out_devices == NULL || max_devices == 0 || out_count == NULL) {
        return NK_ERROR_ARGUMENT;
    }
    *out_count = 0;

    cJSON *root = NULL;
    nk_result rc = perform_request(api, "GET", "/devices", NULL, &root);
    if (rc != NK_OK) {
        return rc;
    }

    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "body");
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(body, "deviceList");
    if (!cJSON_IsArray(list)) {
        cJSON_Delete(root);
        api->last_err = NK_CURTAIN_API_ERR_PARSE;
        return NK_ERROR_FORMAT;
    }

    size_t count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, list) {
        if (count >= max_devices) {
            break;
        }
        const cJSON *type_j = cJSON_GetObjectItemCaseSensitive(item, "deviceType");
        const cJSON *id_j = cJSON_GetObjectItemCaseSensitive(item, "deviceId");
        const cJSON *name_j = cJSON_GetObjectItemCaseSensitive(item, "deviceName");
        const cJSON *hub_j = cJSON_GetObjectItemCaseSensitive(item, "hubDeviceId");
        const cJSON *cloud_j = cJSON_GetObjectItemCaseSensitive(item, "enableCloudService");

        if (!cJSON_IsString(type_j) || !cJSON_IsString(id_j)) {
            continue;
        }
        if (!is_curtain_type(type_j->valuestring)) {
            continue;
        }

        nk_curtain_api_device *dev = &out_devices[count];
        memset(dev, 0, sizeof(*dev));
        copy_cstr(dev->device_id, sizeof(dev->device_id), id_j->valuestring);
        copy_cstr(dev->device_name, sizeof(dev->device_name),
                  cJSON_IsString(name_j) ? name_j->valuestring : "SwitchBot Curtain");
        copy_cstr(dev->device_type, sizeof(dev->device_type), type_j->valuestring);
        copy_cstr(dev->hub_device_id, sizeof(dev->hub_device_id),
                  cJSON_IsString(hub_j) ? hub_j->valuestring : "");
        dev->cloud_enabled = cJSON_IsBool(cloud_j) ? cJSON_IsTrue(cloud_j) : true;
        count++;
    }

    cJSON_Delete(root);
    *out_count = count;
    return NK_OK;
}

nk_result nk_curtain_api_get_status(nk_curtain_api *api, const char *device_id,
                                    nk_curtain_api_status *out_status) {
    if (api == NULL || device_id == NULL || device_id[0] == '\0' || out_status == NULL) {
        return NK_ERROR_ARGUMENT;
    }

    char path[NK_CURTAIN_API_ID_MAX + 32];
    (void)snprintf(path, sizeof(path), "/devices/%s/status", device_id);

    cJSON *root = NULL;
    nk_result rc = perform_request(api, "GET", path, NULL, &root);
    if (rc != NK_OK) {
        return rc;
    }

    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "body");
    const cJSON *pos_j = cJSON_GetObjectItemCaseSensitive(body, "slidePosition");
    const cJSON *mov_j = cJSON_GetObjectItemCaseSensitive(body, "moving");

    if (!cJSON_IsNumber(pos_j)) {
        cJSON_Delete(root);
        api->last_err = NK_CURTAIN_API_ERR_PARSE;
        return NK_ERROR_FORMAT;
    }

    /* SwitchBot slidePosition: 0=全開, 100=全閉 -> UI open_pct: 100=全開, 0=全閉 */
    int sb_pos = clamp_pct(pos_j->valueint);
    out_status->open_pct = 100 - sb_pos;
    out_status->moving = cJSON_IsBool(mov_j) ? cJSON_IsTrue(mov_j) : false;

    cJSON_Delete(root);
    return NK_OK;
}

nk_result nk_curtain_api_send_command(nk_curtain_api *api, const char *device_id,
                                      nk_curtain_api_command cmd, int open_pct) {
    if (api == NULL || device_id == NULL || device_id[0] == '\0') {
        return NK_ERROR_ARGUMENT;
    }

    char path[NK_CURTAIN_API_ID_MAX + 32];
    (void)snprintf(path, sizeof(path), "/devices/%s/commands", device_id);

    char body[160];
    if (cmd == NK_CURTAIN_API_CMD_OPEN) {
        (void)snprintf(
            body, sizeof(body),
            "{\"command\":\"turnOn\",\"parameter\":\"default\",\"commandType\":\"command\"}");
    } else if (cmd == NK_CURTAIN_API_CMD_CLOSE) {
        (void)snprintf(
            body, sizeof(body),
            "{\"command\":\"turnOff\",\"parameter\":\"default\",\"commandType\":\"command\"}");
    } else {
        /* 静音モード(1) + SwitchBot位置(0=全開, 100=全閉) */
        int sb_pos = 100 - clamp_pct(open_pct);
        (void)snprintf(
            body, sizeof(body),
            "{\"command\":\"setPosition\",\"parameter\":\"0,1,%d\",\"commandType\":\"command\"}",
            sb_pos);
    }

    cJSON *root = NULL;
    nk_result rc = perform_request(api, "POST", path, body, &root);
    if (rc == NK_OK && root != NULL) {
        cJSON_Delete(root);
    }
    return rc;
}

nk_curtain_api_error nk_curtain_api_last_error(const nk_curtain_api *api) {
    return api != NULL ? api->last_err : NK_CURTAIN_API_ERR_NONE;
}

long nk_curtain_api_last_http_status(const nk_curtain_api *api) {
    return api != NULL ? api->last_http_code : 0;
}

int nk_curtain_api_last_api_status(const nk_curtain_api *api) {
    return api != NULL ? api->last_api_code : 0;
}
