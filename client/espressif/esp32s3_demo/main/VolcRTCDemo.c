// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_task_info.h"
#include "esp_random.h"

#include <VolcEngineRTCLite.h>
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "Config.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_sys.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "AudioPipeline.h"
#include "RtcBotUtils.h"
#include "cJSON.h"

#define STATS_TASK_PRIO     5

static const char* TAG = "VolcRTCDemo";
static bool joined = false;

typedef struct {
    player_pipeline_handle_t player_pipeline;
    rtc_room_info_t* room_info;
    char remote_uid[128];
} engine_context_t;

// byte rtc lite callbacks
static void byte_rtc_on_join_room_success(byte_rtc_engine_t engine, const char* channel, int elapsed_ms) {
    ESP_LOGI(TAG, "join channel success %s elapsed %d ms now %d ms\n", channel, elapsed_ms, elapsed_ms);
    joined = true;
};

static void byte_rtc_on_rejoin_room_success(byte_rtc_engine_t engine, const char* channel, int elapsed_ms){
    // g_byte_rtc_data.channel_joined = TRUE;
    ESP_LOGI(TAG, "rejoin channel success %s\n", channel);
};

static void byte_rtc_on_user_joined(byte_rtc_engine_t engine, const char* channel, const char* user_name, int elapsed_ms){
    ESP_LOGI(TAG, "remote user joined  %s:%s\n", channel, user_name);
    engine_context_t* context = (engine_context_t *) byte_rtc_get_user_data(engine);
    strcpy(context->remote_uid, user_name);
};

static void byte_rtc_on_user_offline(byte_rtc_engine_t engine, const char* channel, const char* user_name, int reason){
    ESP_LOGI(TAG, "remote user offline  %s:%s\n", channel, user_name);
};

static void byte_rtc_on_user_mute_audio(byte_rtc_engine_t engine, const char* channel, const char* user_name, int muted){
    ESP_LOGI(TAG, "remote user mute audio  %s:%s %d\n", channel, user_name, muted);
};

static void byte_rtc_on_user_mute_video(byte_rtc_engine_t engine, const char* channel, const char* user_name, int muted){
    ESP_LOGI(TAG, "remote user mute video  %s:%s %d\n", channel, user_name, muted);
};

static void byte_rtc_on_connection_lost(byte_rtc_engine_t engine, const char* channel){
    ESP_LOGI(TAG, "connection Lost  %s\n", channel);
};

static void byte_rtc_on_room_error(byte_rtc_engine_t engine, const char* channel, int code, const char* msg){
    ESP_LOGE(TAG, "error occur %s %d %s\n", channel, code, msg?msg:"");
};

// remote audio
static void byte_rtc_on_audio_data(byte_rtc_engine_t engine, const char* channel, const char*  uid , uint16_t sent_ts,
                      audio_data_type_e codec, const void* data_ptr, size_t data_len){
    // ESP_LOGI(TAG, "byte_rtc_on_audio_data... len %d\n", data_len);
    engine_context_t* context = (engine_context_t *) byte_rtc_get_user_data(engine);
    player_pipeline_write(context->player_pipeline, data_ptr, data_len);
}

// remote video
static void byte_rtc_on_video_data(byte_rtc_engine_t engine, const char*  channel, const char* uid, uint16_t sent_ts,
                      video_data_type_e codec, int is_key_frame,
                      const void * data_ptr, size_t data_len){
    ESP_LOGI(TAG, "byte_rtc_on_video_data... len %d\n", data_len);
}

// remote message
// 字幕消息 参考https://www.volcengine.com/docs/6348/1337284
static void on_subtitle_message_received(byte_rtc_engine_t engine, const cJSON* root) {
    /*
        {
            "data" : 
            [
                {
                    "definite" : false,
                    "language" : "zh",
                    "mode" : 1,
                    "paragraph" : false,
                    "sequence" : 0,
                    "text" : "\\u4f60\\u597d",
                    "userId" : "voiceChat_xxxxx"
                }
            ],
            "type" : "subtitle"
        }
    */
    cJSON * type_obj = cJSON_GetObjectItem(root, "type");
    if (type_obj != NULL && strcmp("subtitle", cJSON_GetStringValue(type_obj)) == 0) {
        cJSON* data_obj_arr = cJSON_GetObjectItem(root, "data");
        cJSON* obji = NULL;
        cJSON_ArrayForEach(obji, data_obj_arr) {
            cJSON* user_id_obj = cJSON_GetObjectItem(obji, "userId");
            cJSON* text_obj = cJSON_GetObjectItem(obji, "text");
            if (user_id_obj && text_obj) {
                ESP_LOGE(TAG, "subtitle:%s:%s", cJSON_GetStringValue(user_id_obj), cJSON_GetStringValue(text_obj));
            }
        }
    }
}

// function calling 消息 参考 https://www.volcengine.com/docs/6348/1359441
static void on_function_calling_message_received(byte_rtc_engine_t engine, const cJSON* root, const char* json_str) {
    /*
        {
            "subscriber_user_id" : "",
            "tool_calls" : 
            [
                {
                    "function" : 
                    {
                        "arguments" : "{\\"location\\": \\"\\u5317\\u4eac\\u5e02\\"}",
                        "name" : "get_current_weather"
                    },
                    "id" : "call_py400kek0e3pczrqdxgnb3lo",
                    "type" : "function"
                }
            ]
        }
    */
    // 收到function calling 消息，需要根据具体情况要在服务端处理还是客户端处理

    engine_context_t* context = (engine_context_t *) byte_rtc_get_user_data(engine);
    
    // 服务端处理：
    // voice_bot_function_calling(context->room_info, json_str);

    // 在客户端处理,通过byte_rtc_rts_send_message接口通知智能体
    /*cJSON* tool_obj_arr = cJSON_GetObjectItem(root, "tool_calls");
    cJSON* obji = NULL;
    cJSON_ArrayForEach(obji, tool_obj_arr) {
        cJSON* id_obj = cJSON_GetObjectItem(obji, "id");
        cJSON* function_obj = cJSON_GetObjectItem(obji, "function");
        if (id_obj && function_obj) {
            cJSON* arguments_obj = cJSON_GetObjectItem(function_obj, "arguments");
            cJSON* name_obj = cJSON_GetObjectItem(function_obj, "name");
            cJSON* location_obj = cJSON_GetObjectItem(arguments_obj, "arguments");
            const char* func_name = cJSON_GetStringValue(name_obj);
            const char* loction = cJSON_GetStringValue(location_obj);
            const char* func_id = cJSON_GetStringValue(id_obj);

            if (strcmp(func_name, "get_current_weather") == 0) {
                cJSON *fc_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(fc_obj, "ToolCallID", func_id);
                cJSON_AddStringToObject(fc_obj, "Content", "今天白天风和日丽，天气晴朗，晚上阵风二级。");
                char *json_string = cJSON_Print(fc_obj);
                static char fc_message_buffer[256] = {'f', 'u', 'n', 'c'};
                int json_str_len = strlen(json_string);
                fc_message_buffer[4] = (json_str_len >> 24) & 0xff;
                fc_message_buffer[5] = (json_str_len >> 16) & 0xff;
                fc_message_buffer[6] = (json_str_len >> 8) & 0xff;
                fc_message_buffer[7] = (json_str_len >> 0) & 0xff;
                memcpy(fc_message_buffer + 8, json_string, json_str_len);
                ESP_LOGE(TAG, "send message: %s", json_string);
                cJSON_Delete(fc_obj);

                byte_rtc_rts_send_message(engine, context->room_info->room_id, context->remote_uid, fc_message_buffer, json_str_len + 8, 1, RTS_MESSAGE_RELIABLE);
            }
        }
    }*/
   
}

void on_message_received(byte_rtc_engine_t engine, const char*  room, const char* uid, const uint8_t* message, int size, bool binary) {
    // 字幕消息，参考https://www.volcengine.com/docs/6348/1337284
    // subv|length(4)|json str
    //
    // function calling 消息，参考https://www.volcengine.com/docs/6348/1359441
    // tool|length(4)|json str

    static char message_buffer[4096];
    if (size > 8) {
        memcpy(message_buffer, message, size);
        message_buffer[size] = 0;
        message_buffer[size + 1] = 0;
        cJSON *root = cJSON_Parse(message_buffer + 8);
        if (root != NULL) {
            if (message[0] == 's' && message[1] == 'u' && message[2] == 'b' && message[3] == 'v') {
                // 字幕消息
                on_subtitle_message_received(engine, root);
            } else if (message[0] == 't' && message[1] == 'o' && message[2] == 'o' && message[3] == 'l') {
                // function calling 消息
                on_function_calling_message_received(engine, root, message_buffer + 8);
            } else {
                ESP_LOGE(TAG, "unknown json message: %s", message_buffer + 8);
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "unknown message.");
        }
    } else {
        ESP_LOGE(TAG, "unknown message.");
    }
}

static void on_key_frame_gen_req(byte_rtc_engine_t engine, const char*  channel, const char*  uid) {}
// byte rtc lite callbacks end.


static void byte_rtc_task(void *pvParameters) {
    rtc_room_info_t* room_info = heap_caps_malloc(sizeof(rtc_room_info_t),  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // step 1: start ai agent & get room info
    int start_ret = start_voice_bot(room_info);
    if (start_ret != 200) {
        ESP_LOGE(TAG, "Bot start Failed, ret = %d", start_ret);
        return;
    }

    // step 2: start audio capture & play
    recorder_pipeline_handle_t pipeline = recorder_pipeline_open();
    player_pipeline_handle_t player_pipeline = player_pipeline_open();
    recorder_pipeline_run(pipeline);
    player_pipeline_run(player_pipeline);

    // step 3: start byte rtc engine
    byte_rtc_event_handler_t handler = {
        .on_join_room_success       =   byte_rtc_on_join_room_success,
        .on_room_error              =   byte_rtc_on_room_error,
        .on_user_joined             =   byte_rtc_on_user_joined,
        .on_user_offline            =   byte_rtc_on_user_offline,
        .on_user_mute_audio         =   byte_rtc_on_user_mute_audio,
        .on_user_mute_video         =   byte_rtc_on_user_mute_video,
        .on_audio_data              =   byte_rtc_on_audio_data,
        .on_video_data              =   byte_rtc_on_video_data,
        .on_key_frame_gen_req       =   on_key_frame_gen_req,
        .on_message_received        =   on_message_received,
    };

    byte_rtc_engine_t engine = byte_rtc_create(room_info->app_id, &handler);
    byte_rtc_set_log_level(engine, BYTE_RTC_LOG_LEVEL_ERROR);
    byte_rtc_set_params(engine, "{\"debug\":{\"log_to_console\":1}}");
#ifdef CONFIG_CHOICE_G711A_INTERNAL
    byte_rtc_set_params(engine,"{\"audio\":{\"codec\":{\"internal\":{\"enable\":1}}}}");
#endif

    byte_rtc_init(engine);
    byte_rtc_set_audio_codec(engine, AUDIO_CODEC_TYPE_G711A);
    byte_rtc_set_video_codec(engine, VIDEO_CODEC_TYPE_H264);

    engine_context_t engine_context = {
        .player_pipeline = player_pipeline,
        .room_info = room_info
    };
    byte_rtc_set_user_data(engine, &engine_context);

    // step 4: join room
    byte_rtc_room_options_t options;
    options.auto_subscribe_audio = 1; // 接收远端音频
    options.auto_subscribe_video = 0; // 不接收远端视频
    options.auto_publish_audio = 1;   // 发送音频
    options.auto_publish_video = 0;   // 发送视频
    byte_rtc_join_room(engine, room_info->room_id, room_info->uid, room_info->token, &options);

    const int DEFAULT_READ_SIZE = recorder_pipeline_get_default_read_size(pipeline);
    uint8_t *audio_buffer = heap_caps_malloc(DEFAULT_READ_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to alloc audio buffer!");
        return;
    }

    // step 5: start sending audio data
    while (true) {
        int ret =  recorder_pipeline_read(pipeline, (char*) audio_buffer, DEFAULT_READ_SIZE);
        if (ret == DEFAULT_READ_SIZE && joined) {
            // push_audio data
#ifdef CONFIG_CHOICE_G711A_INTERNAL
            audio_frame_info_t audio_frame_info = {.data_type = AUDIO_DATA_TYPE_PCM};
#else
            audio_frame_info_t audio_frame_info = {.data_type = AUDIO_DATA_TYPE_PCMA};
#endif
            byte_rtc_send_audio_data(engine, room_info->room_id, audio_buffer, DEFAULT_READ_SIZE, &audio_frame_info);
        }
    }

    // step 6: leave room and destroy engine
    byte_rtc_leave_room(engine, room_info->room_id);
    usleep(1000 * 1000);
    byte_rtc_fini(engine);
    usleep(1000 * 1000);
    byte_rtc_destory(engine);
    
    // step 7: stop ai agent or it will not stop until 3 minutes
    stop_voice_bot(room_info);
    heap_caps_free(room_info);

    // step 8: stop audio capture & play
    recorder_pipeline_close(pipeline);
    player_pipeline_close(player_pipeline);
    ESP_LOGI(TAG, "............. finished\n");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init() );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };


    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    audio_board_handle_t board_handle = audio_board_init();   
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 80);
    ESP_LOGI(TAG, "Starting again!\n");

    // Allow other core to finish initialization
    vTaskDelay(pdMS_TO_TICKS(100));

    // Create and start stats task
    xTaskCreate(&byte_rtc_task, "byte_rtc_task", 8192, NULL, STATS_TASK_PRIO, NULL);
}
