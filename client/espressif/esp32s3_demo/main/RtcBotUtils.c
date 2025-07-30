// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "RtcBotUtils.h"
#include "RtcHttpUtils.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "RTC_BOT_UTILS";

static void *impl_malloc_fn(size_t size) {
    uint32_t allocate_caps = 0;
#if CONFIG_PSRAM
    allocate_caps = MALLOC_CAP_SPIRAM;
#else
    allocate_caps = MALLOC_CAP_INTERNAL;
#endif
    return heap_caps_malloc(size, allocate_caps);
}

static void impl_free_fn(void *ptr) {
    heap_caps_free(ptr);
}

const char* common_headers[] = {
    "Content-Type", "application/json",
    "Authorization", "af78e30" CONFIG_RTC_APPID,
    NULL
};

int start_voice_bot(rtc_room_info_t* room_info) {
    char post_data[1024];
    cJSON *post_jobj = cJSON_CreateObject();
#ifdef CONFIG_AUDIO_CODEC_TYPE_OPUS
    cJSON_AddStringToObject(post_jobj, "audio_codec", "OPUS");
#elif defined(CONFIG_AUDIO_CODEC_TYPE_PCM) || defined(CONFIG_AUDIO_CODEC_TYPE_G711A)
    cJSON_AddStringToObject(post_jobj, "audio_codec", "G711A");
#elif defined(CONFIG_AUDIO_CODEC_TYPE_G722)
    cJSON_AddStringToObject(post_jobj, "audio_codec", "G722");
#elif defined(CONFIG_AUDIO_CODEC_TYPE_AAC)
    cJSON_AddStringToObject(post_jobj, "audio_codec", "AAC");
#endif
    // burst 功能
    cJSON_AddBoolToObject(post_jobj, "enable_burst", 0); // 默认关闭
    cJSON_AddNumberToObject(post_jobj, "burst_buffer_size", 500); // 500 ms
    cJSON_AddNumberToObject(post_jobj, "burst_interval", 20);

    const char* json_str = cJSON_Print(post_jobj);
    strcpy(post_data, json_str);
    cJSON_Delete(post_jobj);

    rtc_post_config_t post_config = {
        .uri = "http://" CONFIG_AIGENT_SERVER_HOST "/startvoicechat",
        .headers = common_headers,
        .post_data = post_data  // 根据需要传入智能体id和音色id
    };
    rtc_req_result_t post_result = rtc_http_post(&post_config);
    if (post_result.code == 200 && post_result.response != NULL) {
        // parse json
        cJSON* root = cJSON_Parse(post_result.response);
        rtc_request_free(&post_result);
        if (root == NULL) {
            ESP_LOGE(TAG, "Error parsing JSON");
            return -1;
        }
        
        cJSON* data = cJSON_GetObjectItem(root, "data");

        if (data == NULL) {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "Not found data object.");
            return -1;
        }
        
        cJSON* app_id_item = cJSON_GetObjectItem(data, "app_id");
        const char* app_id = cJSON_GetStringValue(app_id_item);
        strcpy(room_info->app_id, app_id);

        cJSON* uid_item = cJSON_GetObjectItem(data, "uid");
        const char* uid = cJSON_GetStringValue(uid_item);
        strcpy(room_info->uid, uid);
        
        cJSON* room_id_item = cJSON_GetObjectItem(data, "room_id");
        const char* room_id = cJSON_GetStringValue(room_id_item);
        strcpy(room_info->room_id, room_id);

        cJSON* task_id_item = cJSON_GetObjectItem(data, "task_id");
        const char* task_id = cJSON_GetStringValue(task_id_item);
        strcpy(room_info->task_id, task_id);

        cJSON* bot_uid_item = cJSON_GetObjectItem(data, "bot_uid");
        const char* bot_uid = cJSON_GetStringValue(bot_uid_item);
        strcpy(room_info->bot_uid, bot_uid);

        cJSON* token_item = cJSON_GetObjectItem(data, "token");
        const char* token = cJSON_GetStringValue(token_item);
        strcpy(room_info->token, token);
        
        cJSON_Delete(root);

        return 200;
    } else {
        cJSON* root = cJSON_Parse(post_result.response);
        if (root != NULL) {
            cJSON* message_item = cJSON_GetObjectItem(root, "message");
            const char* message = cJSON_GetStringValue(message_item);
            ESP_LOGE(TAG, "Error: %s", message);
            cJSON_Delete(root);
        }
        return post_result.code;
    }
}

int stop_voice_bot(const rtc_room_info_t* room_info) {

    char post_data[512];
    cJSON *post_jobj = cJSON_CreateObject();
    cJSON_AddStringToObject(post_jobj, "app_id", room_info->app_id);
    cJSON_AddStringToObject(post_jobj, "room_id", room_info->room_id);
    cJSON_AddStringToObject(post_jobj, "task_id", room_info->task_id);
    
    const char* json_str = cJSON_Print(post_jobj);
    strcpy(post_data, json_str);
    cJSON_Delete(post_jobj);
    
    rtc_post_config_t post_config = {
        .uri = "http://" CONFIG_AIGENT_SERVER_HOST "/stopvoicechat",
        .headers = common_headers,
        .post_data = post_data
    };
    rtc_req_result_t post_result = rtc_http_post(&post_config);
    if (post_result.code == 200 && post_result.response != NULL) {
        // parse json
        cJSON* root = cJSON_Parse(post_result.response);
        rtc_request_free(&post_result);
        if (root == NULL) {
            ESP_LOGE(TAG, "Error parsing JSON");
            return -1;
        }
        
        cJSON* data = cJSON_GetObjectItem(root, "data");

        if (data == NULL) {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "Not found data object.");
            return -1;
        }
        
        
        cJSON_Delete(root);

        return 200;
    } else {
        cJSON* root = cJSON_Parse(post_result.response);
        if (root != NULL) {
            cJSON* message_item = cJSON_GetObjectItem(root, "message");
            const char* message = cJSON_GetStringValue(message_item);
            ESP_LOGE(TAG, "Error: %s", message);
            cJSON_Delete(root);
        }
        return post_result.code;
    }
}

int update_voice_bot(const rtc_room_info_t* room_info, const char* command, const char* message) {
    char post_data[1024];
    cJSON *post_jobj = cJSON_CreateObject();
    cJSON_AddStringToObject(post_jobj, "app_id", room_info->app_id);
    cJSON_AddStringToObject(post_jobj, "room_id", room_info->room_id);
    cJSON_AddStringToObject(post_jobj, "task_id", room_info->task_id);
    cJSON_AddStringToObject(post_jobj, "command", command);
    if (message) {
        cJSON_AddStringToObject(post_jobj, "message", message);
    }
    
    const char* json_str = cJSON_Print(post_jobj);
    strcpy(post_data, json_str);
    cJSON_Delete(post_jobj);

    
    rtc_post_config_t post_config = {
        .uri = "http://" CONFIG_AIGENT_SERVER_HOST "/updatevoicechat",
        .headers = common_headers,
        .post_data = post_data
    };
    rtc_req_result_t post_result = rtc_http_post(&post_config);
    if (post_result.code == 200 && post_result.response != NULL) {
        // parse json
        cJSON* root = cJSON_Parse(post_result.response);
        rtc_request_free(&post_result);
        if (root == NULL) {
            ESP_LOGE(TAG, "Error parsing JSON");
            return -1;
        }
        
        cJSON* data = cJSON_GetObjectItem(root, "data");

        if (data == NULL) {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "Not found data object.");
            return -1;
        }
        
        cJSON_Delete(root);
        return 200;
    } else {
        cJSON* root = cJSON_Parse(post_result.response);
        if (root != NULL) {
            cJSON* message_item = cJSON_GetObjectItem(root, "message");
            const char* message = cJSON_GetStringValue(message_item);
            ESP_LOGE(TAG, "Error: %s", message);
            cJSON_Delete(root);
        }
        return post_result.code;
    }

}

int interrupt_voice_bot(const rtc_room_info_t* room_info) {
    return update_voice_bot(room_info, "interrupt", NULL);
}

int voice_bot_function_calling(const rtc_room_info_t* room_info, const char* message) {
    return update_voice_bot(room_info, "function", message);
}
