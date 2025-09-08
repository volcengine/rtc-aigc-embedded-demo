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
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_task_info.h"
#include "esp_random.h"
#include "esp_sntp.h"

#include "freertos/semphr.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_sys.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "AudioPipeline.h"
#include "cJSON.h"
#include "network.h"
#include "volc_conv_ai.h"

#define STATS_TASK_PRIO     5

static const char* TAG = "VolcRTCDemo";

#define CONV_AI_CONFIG_FORMAT "{\
  \"ver\": 1,\
  \"iot\": {\
    \"instance_id\": \"%s\",\
    \"product_key\": \"%s\",\
    \"product_secret\": \"%s\",\
    \"device_name\": \"%s\",\
    \"host\": \"%s\"\
  },\
  \"ws\": {\
    \"aigw_path\": \"%s\"\
  }\
}"

typedef struct {
    player_pipeline_handle_t player_pipeline;
    volc_event_handler_t volc_event_handler;
    volc_engine_t engine;
} engine_context_t;

static char config_buf[1024] = {0};
static engine_context_t engine_ctx = {0};
static bool is_ready = false;

static void _on_volc_event(volc_engine_t handle, volc_event_t* event, void* user_data)
{
    switch (event->code) {
        case VOLC_EV_CONNECTED:
            is_ready = true;
            printf("Volc Engine connected\n");
            break;
        case VOLC_EV_DISCONNECTED:
            is_ready = false;
            printf("Volc Engine disconnected\n");
            break;
        default:
            printf("Volc Engine event: %d\n", event->code);
            break;
    }
}

static void _on_volc_conversation_status(volc_engine_t handle, volc_conv_status_e status, void* user_data)
{
    printf("conversation status changed: %d\n", status);
}

static void _on_volc_audio_data(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_audio_frame_info_t* info_ptr, void* user_data)
{
	int error = 0;
	engine_context_t* demo = (engine_context_t*)user_data;
	if (demo == NULL) {
		printf("demo is NULL\n");
		return;
	}
    if (demo->player_pipeline == NULL) {
        printf("player pipeline is NULL\n");
        return;
    }
    player_pipeline_write(demo->player_pipeline, data_ptr, data_len);
}

static void _on_volc_video_data(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_video_frame_info_t* info_ptr, void* user_data)
{
}

static void _on_volc_message_data(volc_engine_t handle, const void* message, size_t size, volc_message_info_t* info_ptr, void* user_data)
{
    engine_context_t* demo = (engine_context_t*)user_data;
    static int cnt = 0;
    printf("--------------%d----------------\r\n", cnt++);
    printf("%.*s", (int)size, (const char *)message);
    printf("------------------------------\r\n");
}

void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    
    // 设置SNTP操作模式为轮询（客户端）
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // 设置NTP服务器（可以设置多个备用）
    esp_sntp_setservername(0, "pool.ntp.org");          // 主服务器
    esp_sntp_setservername(1, "cn.pool.ntp.org");       // 备用服务器：中国区
    esp_sntp_setservername(2, "ntp1.aliyun.com");       // 备用服务器：阿里云
    
    // 初始化SNTP服务
    esp_sntp_init();
    
    // 设置时区（例如北京时间CST-8）
    setenv("TZ", "CST-8", 1);
    tzset();
}

void print_current_time(void) {
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 格式化输出时间
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);
}

void wait_for_time_sync(void) {
    ESP_LOGI(TAG, "Waiting for time synchronization...");
    
    // 检查时间是否已同步（1970年之后）
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry_count = 15; // 最大重试次数
    
    while (timeinfo.tm_year < (2024 - 1900) && retry_count > 0) {
        ESP_LOGI(TAG, "Time not set yet, retrying... (%d)", retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
        retry_count--;
    }
    
    if (retry_count > 0) {
        print_current_time();
    } else {
        ESP_LOGE(TAG, "Failed to synchronize time within timeout period");
    }
}

static void sys_monitor_task(void *pvParameters) {
    static char run_info[1024] = { 0 };
    while (1) {
        vTaskGetRunTimeStats(run_info);
        printf("Task Runtime Stats:\n%s", run_info);
        printf("--------------------------------\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static uint64_t __get_time_ms(void) {
    struct timespec now_time;
    clock_gettime(CLOCK_REALTIME, &now_time);
    return now_time.tv_sec * 1000 + now_time.tv_nsec / 1000000;
}

static void conv_ai_task(void *pvParameters) {
    int error = 0;
    // step 1: start audio capture & play
    recorder_pipeline_handle_t pipeline = recorder_pipeline_open();
    player_pipeline_handle_t player_pipeline = player_pipeline_open();
    recorder_pipeline_run(pipeline);
    player_pipeline_run(player_pipeline);


    // step 2: create ai agent
    snprintf(config_buf, sizeof(config_buf), CONV_AI_CONFIG_FORMAT,
             CONFIG_VOLC_INSTANCE_ID,
             CONFIG_VOLC_PRODUCT_KEY,
             CONFIG_VOLC_PRODUCT_SECRET,
             CONFIG_VOLC_DEVICE_NAME,
             CONFIG_VOLC_HOST,
             CONFIG_VOLC_AIGW_PATH);
    ESP_LOGI(TAG, "conv ai config: %s", config_buf);
    volc_event_handler_t volc_event_handler = {
        .on_volc_event = _on_volc_event,
        .on_volc_conversation_status = _on_volc_conversation_status,
        .on_volc_audio_data = _on_volc_audio_data,
        .on_volc_video_data = _on_volc_video_data,
        .on_volc_message_data = _on_volc_message_data,
    };
    engine_ctx.volc_event_handler = volc_event_handler;
    engine_ctx.player_pipeline = player_pipeline;
    error = volc_create(&engine_ctx.engine, config_buf, &engine_ctx.volc_event_handler, &engine_ctx);
    if (error != 0) {
        ESP_LOGE(TAG, "Failed to create volc engine! error=%d", error);
        return;
    }

    // step 3: start ai agent
    volc_opt_t opt = {
        .mode = VOLC_MODE_WS,
        .bot_id = CONFIG_VOLC_BOT_ID
    };
    error = volc_start(engine_ctx.engine, &opt);
    if (error != 0) {
        ESP_LOGE(TAG, "Failed to start volc engine! error=%d", error);
        volc_destroy(engine_ctx.engine);
        return;
    }

    const int DEFAULT_READ_SIZE = 2560; // 80ms
    uint8_t *audio_buffer = heap_caps_malloc(DEFAULT_READ_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to alloc audio buffer!");
        return;
    }
    // step 4: start sending audio data
    volc_audio_frame_info_t info = { 0 };
    info.data_type = VOLC_AUDIO_DATA_TYPE_PCM;
    info.commit = false;
    while (true) {
        int ret =  recorder_pipeline_read(pipeline, (char*) audio_buffer, DEFAULT_READ_SIZE);
        if (ret == DEFAULT_READ_SIZE && is_ready) {
            // push_audio data
            volc_send_audio_data(engine_ctx.engine, audio_buffer, DEFAULT_READ_SIZE, &info);
        }
    }

    // step 6: stop and destroy engine
    volc_stop(engine_ctx.engine);
    usleep(1000 * 1000);
    volc_destroy(engine_ctx.engine);

    // step 7: stop audio capture & play
    recorder_pipeline_close(pipeline);
    player_pipeline_close(player_pipeline);
    ESP_LOGI(TAG, "............. finished\n");
}

void app_main(void)
{
    /* Initialize the default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize NVS flash for WiFi configuration */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

   bool connected = configure_network();
   if (connected == false) {
       ESP_LOGE(TAG, "Failed to connect to network");
       return;
   }

   // 初始化SNTP
    initialize_sntp();
    // 等待时间同步
    wait_for_time_sync();

    audio_board_handle_t board_handle = audio_board_init();   
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 50);
    ESP_LOGI(TAG, "Starting again!\n");

    // Allow other core to finish initialization
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Create and start stats task
    xTaskCreate(&conv_ai_task, "conv_ai_task", 8192, NULL, STATS_TASK_PRIO, NULL);
    xTaskCreate(&sys_monitor_task, "sys_monitor_task", 4096, NULL, STATS_TASK_PRIO, NULL);
}
