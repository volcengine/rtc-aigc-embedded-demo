// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "RtcHttpUtils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"
#include <string.h>

#define HTTP_FINSH_BIT 1

static const char *TAG = "RTC_HTTP_UTILS";
#if CONFIG_PSRAM
static const unsigned int mem_flags =  MALLOC_CAP_SPIRAM;
#else
static const unsigned int mem_flags =  MALLOC_CAP_INTERNAL;
#endif // CONFIG_PSRAM

typedef struct {
    EventGroupHandle_t http_finish_event;
    int output_len; 
    rtc_req_result_t result;
} rtc_http_post_context_t;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    rtc_http_post_context_t *context = (rtc_http_post_context_t *) evt->user_data;
   
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            memcpy(context->result.response + context->output_len, evt->data, evt->data_len);
            context->output_len += evt->data_len;
            context->result.response[context->output_len] = 0;
            break;
        case HTTP_EVENT_ON_FINISH:            
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            xEventGroupSetBits(context->http_finish_event, HTTP_FINSH_BIT);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}

rtc_req_result_t rtc_http_post(rtc_post_config_t* config) {
    if (!config || !config->uri || !config->post_data) {
        ESP_LOGE(TAG, "Invalid parameters: config");
    }
    
    rtc_http_post_context_t context = {0};
    context.http_finish_event = xEventGroupCreate();
    if (!context.http_finish_event) {
        ESP_LOGE(TAG, "http_finish_event create failed.");
        return context.result;
    }
    context.result.code = 0;
    context.result.response = heap_caps_malloc(2048, mem_flags);
    if (!context.result.response) {
        vEventGroupDelete(context.http_finish_event);
        ESP_LOGE(TAG, "http_finish_event create failed.");
        return context.result;
    }
    context.result.response[0] = 0;
    
    esp_http_client_config_t http_client_config = {
        .url = config->uri,
        .query = "",
        .event_handler = _http_event_handler,
        .user_data = &context,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_client_config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (config->headers) {
        int header_index = 0;
        while(config->headers[header_index]) {
            esp_http_client_set_header(client, config->headers[header_index], config->headers[header_index + 1]);
            header_index += 2;
        }
    }
    esp_http_client_set_post_field(client, config->post_data, strlen(config->post_data));
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "request failed: %s", esp_err_to_name(err));
    }

    EventBits_t ux_bits = xEventGroupWaitBits(context.http_finish_event, HTTP_FINSH_BIT , pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));  // wait 10s
    if ((ux_bits & HTTP_FINSH_BIT) == 0) {
        ESP_LOGE(TAG, "request failed: %s", esp_err_to_name(err));
    }
    
    context.result.code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "context.result.code: %d, context.result.response: %s", context.result.code, context.result.response);

    esp_http_client_cleanup(client);
    vEventGroupDelete(context.http_finish_event);
    return context.result;
}

void rtc_request_free(rtc_req_result_t *result) {
     if (result && result->response) {
        heap_caps_free(result->response);
        result->response = NULL;
    }
}