#include "data_consent.h"

static bool s_is_agreed = false;
static uint32_t s_agreed_version = 0;
static bool s_is_initialized = false;

void data_consent_init(void) {
    s_is_initialized = true;
}

bool data_consent_is_agreed(void) {
    if (!s_is_initialized) {
        data_consent_init();
    }
    return s_is_agreed;
}

uint32_t data_consent_get_agreed_version(void) {
    if (!s_is_initialized) {
        data_consent_init();
    }
    return s_agreed_version;
}

bool data_consent_set_agreed(bool agreed, uint32_t version) {
    if (!s_is_initialized) {
        data_consent_init();
    }
    s_is_agreed = agreed;
    s_agreed_version = agreed ? version : 0;
    return true;
}

void data_consent_reset(void) {
    s_is_agreed = false;
    s_agreed_version = 0;
    s_is_initialized = true;
}
