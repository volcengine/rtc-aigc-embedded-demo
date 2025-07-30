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
    "Authorization", "Bearer " CONFIG_COZE_AUTH,
    NULL
};

int start_voice_bot(rtc_room_info_t* room_info) {
    char post_data[1024];
    cJSON *post_jobj = cJSON_CreateObject();
    cJSON_AddStringToObject(post_jobj, "bot_id", CONFIG_COZE_BOT_ID);

    cJSON *config = cJSON_CreateObject();
    cJSON_AddItemToObject(post_jobj, "config", config);

    cJSON *audio_config = cJSON_CreateObject();
    cJSON_AddItemToObject(config, "audio_config", audio_config);

#ifdef CONFIG_AUDIO_CODEC_TYPE_OPUS
    cJSON_AddStringToObject(audio_config, "codec", "OPUS");
#elif defined(CONFIG_AUDIO_CODEC_TYPE_PCM) || defined(CONFIG_AUDIO_CODEC_TYPE_G711A)
    cJSON_AddStringToObject(audio_config, "codec", "G711A");
#elif defined(CONFIG_AUDIO_CODEC_TYPE_G722)
    cJSON_AddStringToObject(audio_config, "codec", "G722");
#elif defined(CONFIG_AUDIO_CODEC_TYPE_AAC)
    cJSON_AddStringToObject(audio_config, "codec", "AACLC");
#endif


    const char* json_str = cJSON_Print(post_jobj);
    strcpy(post_data, json_str);
    cJSON_Delete(post_jobj);

    rtc_post_config_t post_config = {
        .uri = CONFIG_COZE_SERVER_HOST,
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
    return 0;
}

int update_voice_bot(const rtc_room_info_t* room_info, const char* command, const char* message) {
    return 0;
}
int interrupt_voice_bot(const rtc_room_info_t* room_info) {
    return 0;
}
int voice_bot_function_calling(const rtc_room_info_t* room_info, const char* message) {
    return 0;
}
