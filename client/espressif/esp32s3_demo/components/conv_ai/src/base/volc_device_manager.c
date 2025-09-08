// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "volc_device_manager.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "platform/volc_platform.h"
#include "util/volc_http.h"
#include "util/volc_json.h"
#include "util/volc_base64.h"
#include "util/volc_auth.h"
#include "util/volc_list.h"
#include "util/volc_log.h"

#include "volc_conv_ai.h"

#define VOLC_DYNAMIC_REGISTER_PATH "/2021-12-14/DynamicRegister"

#define VOLC_API_VERSION "2021-12-14"
#define VOLC_API_VERSION_QUERY_PARAM "Version=2021-12-14"
#define VOLC_API_ACTION_DYNAMIC_REGISTER  "Action=DynamicRegister"

char* volc_generate_signature(const char* secret_key, const char* product_key, const char* device_name, int rnd, uint64_t timestamp, int auth_type)
{
    char input_str[256] = {0};
    uint8_t hmac_result[32] = {0};
    int hmac_result_len = sizeof(hmac_result);
    int base64_encoded_len = 0;
    size_t olen = 0;

    snprintf(input_str, sizeof(input_str), "auth_type=%d&device_name=%s&random_num=%d&product_key=%s&timestamp=%" PRIu64, auth_type, device_name, rnd, product_key, timestamp);
    volc_sha256_hmac((const unsigned char*)secret_key, strlen(secret_key), (const unsigned char*)input_str, strlen(input_str), hmac_result, &hmac_result_len);
    base64_encoded_len = volc_base64_encoded_length(hmac_result_len);
    unsigned char* base64_encoded = (unsigned char*)hal_malloc(base64_encoded_len);
    if (!base64_encoded) {
        LOGE("Failed to allocate memory for base64 encoded string");
        return NULL;
    }

    volc_base64_encode(base64_encoded, base64_encoded_len, &olen, (const unsigned char*)hmac_result, sizeof(hmac_result));
    return (char*)base64_encoded;
}

int volc_device_register(volc_iot_info_t* info, char** output)
{
    int ret = 0;
    uint64_t current_time = hal_get_time_ms();
    int32_t random_num = (int32_t)current_time;
    char url[256] = {0};
    char* signature = volc_generate_signature(info->product_secret, info->product_key, info->device_name, random_num, current_time, 1);
    cJSON* response_json = NULL;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "InstanceID", info->instance_id);
    cJSON_AddStringToObject(root, "product_key", info->product_key);
    cJSON_AddStringToObject(root, "device_name", info->device_name);
    cJSON_AddNumberToObject(root, "random_num", random_num);
    cJSON_AddNumberToObject(root, "timestamp", (double)current_time);
    cJSON_AddNumberToObject(root, "auth_type", 1);
    cJSON_AddStringToObject(root, "signature", signature);
    char* json_str = cJSON_PrintUnformatted(root);

    snprintf(url, sizeof(url), "%s%s?%s&%s", info->host, VOLC_DYNAMIC_REGISTER_PATH, VOLC_API_ACTION_DYNAMIC_REGISTER, VOLC_API_VERSION_QUERY_PARAM);
    LOGI("url: %s, body: %s", url, json_str);

    char* response = volc_http_post(url, json_str, strlen(json_str));
    if (response == NULL) {
        LOGE("Failed to get response from server");
        ret = -1;
        goto err_out_label;
    }

    response_json = cJSON_Parse(response);
    if (response_json == NULL) {
        LOGE("Failed to parse response JSON");
        ret = -1;
        goto err_out_label;
    }
    char *payload = NULL;
    int code = 0;
    ret = volc_json_read_int(response_json, "ResponseMetadata.Error.CodeN", &code);
    if (0 == ret) {
        ret = (code == ERROR_LICENSE_EXHAUSTED) ? VOLC_ERR_LICENSE_EXHAUSTED : VOLC_ERR_FAILED;
        LOGE("register device failed, code: %d", ret);
        goto err_out_label;
    }
    ret = volc_json_read_string(response_json, "Result.payload", &payload);
    if (ret != 0) {
        LOGE("Failed to read payload from response JSON");
        ret = -1;
        goto err_out_label;
    }

    // TODO: device secret, should be freed by caller
    *output = volc_aes_decode(info->product_secret, payload, true);

err_out_label:
    if (payload) {
        hal_free(payload);
    }
    if (response) {
        hal_free(response);
    }
    if (root) {
        cJSON_Delete(root);
    }
    if (response_json) {
        cJSON_Delete(response_json);
    }
    if (signature) {
        hal_free(signature);
    }
    if (json_str) {
        hal_free(json_str);
    }
    return ret;
}

#define VOLC_GET_LLM_CONFIG_PATH "/2021-12-14/GetLLMConfig"

// #define VOLC_API_VERSION_QUERY_PARAM "Version=2021-12-14"
#define VOLC_API_ACTION_GET_LLM_CONFIG  "Action=GetLLMConfig"

int volc_get_llm_config(volc_iot_info_t* info) {
    uint64_t current_time = hal_get_time_ms();
    int32_t random_num = (int32_t)current_time;
    char url[256] = {0};
    char* signature = volc_generate_signature(info->device_secret, info->product_key, info->device_name, random_num, current_time, 0);
    int ret = 0;
    cJSON* response_json = NULL;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "InstanceID", info->instance_id);
    cJSON_AddStringToObject(root, "product_key", info->product_key);
    cJSON_AddStringToObject(root, "device_name", info->device_name);
    cJSON_AddNumberToObject(root, "random_num", random_num);
    cJSON_AddNumberToObject(root, "timestamp", (double)current_time);
    // cJSON_AddNumberToObject(root, "auth_type", 0);
    cJSON_AddStringToObject(root, "signature", signature);
    char* json_str = cJSON_PrintUnformatted(root);

    snprintf(url, sizeof(url), "%s%s?%s&%s", info->host, VOLC_GET_LLM_CONFIG_PATH, VOLC_API_ACTION_GET_LLM_CONFIG, VOLC_API_VERSION_QUERY_PARAM);

    char* response = volc_http_post(url, json_str, strlen(json_str));
    if (response == NULL) {
        LOGE("Failed to get response from server");
        goto err_out_label;
    }
    response_json = cJSON_Parse(response);
    char* payload = NULL;
    int code = 0;
    ret = volc_json_read_int(response_json, "ResponseMetadata.Error.CodeN", &code);
    if (0 == ret) {
        LOGE("get llm config failed, code: %d", code);
        ret = (code == ERROR_LICENSE_EXPIRED) ? VOLC_ERR_LICENSE_EXPIRED : VOLC_ERR_FAILED;
        goto err_out_label;
    }

    ret = volc_json_read_string(response_json, "Result.APIKey", &payload);
    ret |= volc_json_read_string(response_json, "Result.URL", &info->ws_url);
    ret |= volc_json_read_string(response_json, "Result.RTCAppID", &info->rtc_app_id);
    if (payload == NULL || info->ws_url == NULL || info->rtc_app_id == NULL) {
        LOGE("Failed to get LLM config from server: %s", response);
        goto err_out_label;
    }

    info->api_key = volc_aes_decode((const char*)info->device_secret, (const char*)payload, false);
    LOGD("trimmed decrypted payload: %s, len: %d, url: %s", info->api_key, (int)strlen(info->api_key), info->ws_url);
err_out_label:
    if (root) {
        cJSON_Delete(root);
    }
    if (response_json) {
        cJSON_Delete(response_json);
    }
    if (payload) {
        hal_free(payload);
    }
    if (response) {
        hal_free(response);
    }
    if (json_str) {
        hal_free(json_str);
    }
    if (signature) {
        hal_free(signature);
    }
    return ret;
}

#define VOLC_GET_RTC_CONFIG_PATH "/2021-12-14/GetRTCConfig"
#define VOLC_API_ACTION_GET_RTC_CONFIG  "Action=GetRTCConfig"
int volc_get_rtc_config(volc_iot_info_t* info, int audio_codec, const char* bot_id, const char* task_id, volc_room_info_t* room_info) {
    int ret = 0;
    uint64_t current_time = hal_get_time_ms();
    int32_t random_num = (int32_t)current_time;
    char url[256] = {0};
    char* signature = volc_generate_signature(info->device_secret, info->product_key, info->device_name, random_num, current_time, 0);
    cJSON* response_json = NULL;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "InstanceID", info->instance_id);
    cJSON_AddStringToObject(root, "product_key", info->product_key);
    cJSON_AddStringToObject(root, "device_name", info->device_name);
    cJSON_AddNumberToObject(root, "random_num", random_num);
    cJSON_AddNumberToObject(root, "timestamp", (double)current_time);
    cJSON_AddStringToObject(root, "signature", signature);
    cJSON_AddStringToObject(root, "bot_id", bot_id);
    cJSON_AddNumberToObject(root, "audio_codec", audio_codec);
    cJSON_AddStringToObject(root, "task_id", task_id);
    char* json_str = cJSON_PrintUnformatted(root);
    snprintf(url, sizeof(url), "%s%s?%s&%s", info->host, VOLC_GET_RTC_CONFIG_PATH, VOLC_API_ACTION_GET_RTC_CONFIG, VOLC_API_VERSION_QUERY_PARAM);
    LOGI("url: %s, body: %s", url, json_str);
    char* response = volc_http_post(url, json_str, strlen(json_str));
    if (response == NULL) {
        LOGE("Failed to get response from server");
        ret = -1;
        goto err_out_label;
    }
    response_json = cJSON_Parse(response);
    volc_json_read_string(response_json, "Result.RoomID", &room_info->rtc_opt.p_channel_name);
    volc_json_read_string(response_json, "Result.UserID", &room_info->rtc_opt.p_uid);
    volc_json_read_string(response_json, "Result.Token", &room_info->rtc_opt.p_token);
    volc_json_read_string(response_json, "Result.TaskID", &room_info->task_id);
    if (room_info->rtc_opt.p_channel_name == NULL || room_info->rtc_opt.p_uid == NULL || room_info->rtc_opt.p_token == NULL || room_info->task_id == NULL) {
        LOGE("Failed to get RTC config from server: %s", response);
        hal_free(response);
        cJSON_Delete(response_json);
        ret = -1;
        goto err_out_label;
    }
err_out_label:
    if (root) {
        cJSON_Delete(root);
    }
    if (response_json) {
        cJSON_Delete(response_json);
    }
    if (signature) {
        hal_free(signature);
    }
    if (json_str) {
        hal_free(json_str);
    }
    if (response) {
        hal_free(response);
    }
    return ret;
}
