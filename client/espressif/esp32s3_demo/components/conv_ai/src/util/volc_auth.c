// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "util/volc_auth.h"

#include <stdlib.h>
#include <string.h>

#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md5.h>
#include <mbedtls/base64.h>

#include "platform/volc_platform.h"
#include "util/volc_base64.h"
#include "util/volc_list.h"
#include "util/volc_log.h"

typedef bool(predicate_func)(unsigned char c);

static bool _url_trim_pred(unsigned char ch) {
    if (ch < 32) {
        return true;
    }
    switch (ch) {
        case 0x20: /* ' ' - space */
        case 0x09: /* '\t' - horizontal tab */
        case 0x0A: /* '\n' - line feed */
        case 0x0B: /* '\v' - vertical tab */
        case 0x0C: /* '\f' - form feed */
        case 0x0D: /* '\r' - carriage return */
        case '\a': /* '\r' - carriage return */
            return true;
        default:
            return false;
    }
}
static int _right_trim_pred(unsigned char* str, predicate_func* pred) {
    int len = strlen((const char*)str);
    
    while(len > 0 && pred(str[len - 1])) {
        --len;
    }

    str[len] = '\0';
    return len;
}

static int __aes_cbc(bool encrypt, const unsigned char* key, unsigned char* iv, const unsigned char* input, uint32_t ilen, int key_bits, unsigned char* output) {
    int mode = encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT;
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, key, key_bits);
    mbedtls_aes_crypt_cbc(&aes_ctx, mode, ilen, iv, (const unsigned char *)input, output);
    mbedtls_aes_free(&aes_ctx);

    return 0;
}

#define AES_KEY_SIZE 16 // AES key size in bytes
char* volc_aes_decode(const char* secret, const char* input, const bool partial_secret) {
    size_t decoded_len = 0;
    int key_bits = 0;
    char* key = NULL;
    unsigned char iv[16] = {0};
    unsigned char* decoded_payload = NULL;
    if (input == NULL || strlen(input) == 0) {
        LOGE("Input is NULL or empty");
        return NULL;
    }

    if (partial_secret) {
        // If partial secret is used, we only use the first 8 bytes of the key and iv
        key = (char *)hal_calloc(AES_KEY_SIZE, 1);
        memcpy(key, secret, AES_KEY_SIZE);
        key_bits = 128; // AES-128
    } else {
        key = (char *)hal_calloc(strlen(secret) + 1, 1); // +1 for null terminator
        memcpy(key, secret, strlen(secret));
        key_bits = 192; // AES-192
    }

    decoded_len = volc_base64_decoded_length((const uint8_t*)input, strlen(input));
    decoded_payload = (unsigned char *)hal_calloc(decoded_len + 1, 1);
    if (!decoded_payload) {
        LOGE("Failed to allocate memory for decoded payload");
        return NULL;
    }
    volc_base64_decode((unsigned char *)decoded_payload, decoded_len, &decoded_len, (const unsigned char *)input, strlen(input));
    LOGV("decoded payload: %s, len: %zu", decoded_payload, decoded_len);

    uint8_t *output = hal_calloc(decoded_len + 16, 1); // +1 for null terminator

    memcpy(iv, secret, AES_KEY_SIZE); // Use the same secret for IV
    __aes_cbc(false, (const unsigned char *)key, (unsigned char *)iv, (const unsigned char *)decoded_payload, decoded_len, key_bits, output);

    _right_trim_pred(output, _url_trim_pred);

    LOGV("trimmed decrypted payload: %s, len: %d", output, (int)strlen((const char*)output));
    hal_free(decoded_payload);
    hal_free(key);
    return (char *)output;
}

void volc_sha256_hmac(const unsigned char* key, int keylen, const unsigned char* input, int ilen, unsigned char* output, int* plen) {
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, keylen, input, ilen, output);
    *(plen) = mbedtls_md_get_size(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256));
}