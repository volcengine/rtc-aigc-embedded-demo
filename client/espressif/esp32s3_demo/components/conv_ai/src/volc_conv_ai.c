// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "volc_conv_ai.h"

#include <string.h>
#include "platform/volc_platform.h"
#include "util/volc_json.h"
#include "util/volc_log.h"
#include "node/volc_rtc.h"
#include "node/volc_ws.h"
#include "base/volc_device_manager.h"

#if defined(ENABLE_RTC_MODE)
#include "VolcEngineRTCLite.h"
#endif

#include "cJSON.h"

#define MAGIC_CONTROL "ctrl"
#define MAGIC_CONV    "conv"
#define MAGIC_LENGTH 4
#define MAGIC_OFFSET 8

typedef enum {
    VOLC_RT_STATE_NONE = 0,          // Initial state
    VOLC_RT_STATE_CREATED,            // engine created
    VOLC_RT_STATE_STARTED,            // engine started
    // VOLC_RT_STATE_STARTED,             // Joined a channel
    VOLC_RT_STATE_UNJOINED,           // Connection lost
    VOLC_RT_STATE_STOPPED,            // engine stopped
    VOLC_RT_STATE_DESTROYED,          // engine destroyed
    VOLC_RT_STATE_ERROR               // Error state
} volc_rt_state_e;

typedef struct {
    int audio_send_bytes;
    int audio_recv_bytes;
} media_stats_t;

typedef struct {
    volatile size_t status;
    bool b_key_frame_request;
    int target_kbps;
    void* user_data;

    volc_mode_e mode;
#if defined(ENABLE_WS_MODE)
    volc_ws_t ws;
#endif
#if defined(ENABLE_RTC_MODE)
    volc_rtc_t rtc;
#endif

    media_stats_t media_stats;
    volc_iot_info_t info;
    // volc_room_info_t room_info;
    volc_event_handler_t event_handler;
} volc_engine_impl_t;

static void _iot_info_free(volc_iot_info_t* info) {
    if (info) {
        info->device_name ? hal_free(info->device_name) : 0;
        info->device_secret ? hal_free(info->device_secret) : 0;
        info->host ? hal_free(info->host) : 0;
        info->instance_id ? hal_free(info->instance_id) : 0;
        info->product_key ? hal_free(info->product_key) : 0;
        info->product_secret ? hal_free(info->product_secret) : 0;
    }
}

static void __realtime_conv_status_to_user(volc_engine_impl_t* impl, volc_conv_status_e status) {
    if (impl->event_handler.on_volc_conversation_status) {
        impl->event_handler.on_volc_conversation_status(impl, status, impl->user_data);
    }
}

static void __realtime_event_2_user_event(void* context, volc_msg_t* msg) {
    volc_event_t event = { 0 };
    if (context == NULL || msg == NULL) {
        LOGE("context or message is NULL");
        return;
    }
    
    volc_engine_impl_t* impl = (volc_engine_impl_t*)context;
    switch (msg->code) {
        case VOLC_MSG_CONNECTED:
            event.code = VOLC_EV_CONNECTED;
            break;
        case VOLC_MSG_DISCONNECTED:
            event.code = VOLC_EV_DISCONNECTED;
            break;
        case VOLC_MSG_USER_JOINED:
        case VOLC_MSG_USER_OFFLINE:
            break;
        case VOLC_MSG_TOKEN_EXPIRED:
            break;
        case VOLC_MSG_KEY_FRAME_REQ:
            impl->b_key_frame_request = true;
            return;
        case VOLC_MSG_TARGET_BITRATE_CHANGED:
            impl->target_kbps = msg->data.target_bitrate / 1000;
            return;
        case VOLC_MSG_CONV_STATUS:
            __realtime_conv_status_to_user(impl, msg->data.conv_status);
            return;
        default:
            LOGW("Unknown message type: %d", msg->code);
            return; // Ignore unknown messages
    }

    if (impl->event_handler.on_volc_event) {
        impl->event_handler.on_volc_event(impl, &event, impl->user_data);
    }
}

static void __realtime_user_event_router(void* context, volc_msg_t* msg) {
    __realtime_event_2_user_event(context, msg);
}

static void __realtime_audio_router(volc_engine_impl_t* impl, const void* data, size_t len, volc_data_info_t* info) {
    if (impl->event_handler.on_volc_audio_data) {
        impl->event_handler.on_volc_audio_data(impl, data, len, &info->info.audio, impl->user_data);
    }
}

static void __realtime_video_router(volc_engine_impl_t* impl, const void* data, size_t len, volc_data_info_t* info) {
    if (impl->event_handler.on_volc_video_data) {
        impl->event_handler.on_volc_video_data(impl, data, len, &info->info.video, impl->user_data);
    }
}

static void __realtime_message_router(volc_engine_impl_t* impl, const void* data, size_t len, volc_data_info_t* info) {
    int ret = 0;
    if (impl->event_handler.on_volc_message_data) {
        impl->event_handler.on_volc_message_data(impl, data, len, &info->info.message, impl->user_data);
    }
}

static void __realtime_data_router(void* context, const void* data, size_t len, volc_data_info_t* info) {
    volc_engine_impl_t* impl = (volc_engine_impl_t*)context;
    if (info) { 
        switch(info->type) {
            case VOLC_DATA_TYPE_AUDIO:
                __realtime_audio_router(impl, data, len, info);
            break;
            case VOLC_DATA_TYPE_VIDEO:
                __realtime_video_router(impl, data, len, info);
            break;
            case VOLC_DATA_TYPE_MESSAGE:
                __realtime_message_router(impl, data, len, info);
            break;
            default:
                break;
        }
    }
}

static int __config_iot_parse(cJSON* iot_config, volc_engine_impl_t* engine) {
    int ret = 0;

    ret |= volc_json_read_string(iot_config, "instance_id", &engine->info.instance_id);
    ret |= volc_json_read_string(iot_config, "product_key", &engine->info.product_key);
    ret |= volc_json_read_string(iot_config, "product_secret", &engine->info.product_secret);
    ret |= volc_json_read_string(iot_config, "device_name", &engine->info.device_name);
    ret |= volc_json_read_string(iot_config, "host", &engine->info.host);

    return ret == 0 ? 0 : -1;
}

const char* volc_get_version(void) {
    return "1.0.0";
}

const char* volc_err_2_str(int err_code) {
    switch(err_code) {
        case VOLC_ERR_NO_ERROR:
            return "Success";
        case VOLC_ERR_FAILED:
            return "Failed";
        case VOLC_ERR_LICENSE_EXHAUSTED:
            return "License exhausted";
        case VOLC_ERR_LICENSE_EXPIRED:
            return "License expired";
        default:
            break;
    }
    return "Unknown error";
}

int volc_create(volc_engine_t* handle, const char* config_json, volc_event_handler_t* event_handler, void* user_data) {
    int ret = 0;
    volc_engine_impl_t* engine = NULL;
    cJSON* config = NULL;
    if (config_json == NULL || event_handler == NULL || handle == NULL) {
        LOGE("Configuration JSON is NULL");
        return VOLC_ERR_FAILED;
    }

    engine = (volc_engine_impl_t*)hal_malloc(sizeof(volc_engine_impl_t));
    if (engine == NULL) {
        LOGE("Failed to allocate memory for engine");
        return VOLC_ERR_FAILED;
    }
    memset(engine, 0, sizeof(volc_engine_impl_t));

    engine->event_handler = *event_handler;
    engine->user_data = user_data;
    engine->mode = VOLC_MODE_UNKNOWN;

    config = cJSON_Parse(config_json);

    cJSON* iot_config = cJSON_GetObjectItem(config, "iot");
    if (iot_config) {
       ret = __config_iot_parse(iot_config, engine);
       if (ret != 0) {
           LOGE("Failed to parse IoT configuration");
           goto err_out_label;
       }
        ret = volc_device_register(&engine->info, &engine->info.device_secret);
        if (ret != 0) {
            LOGE("Failed to register device with host %s, error code: %d", engine->info.host, ret);
            goto err_out_label;
        }
        LOGI("register successfully, device secret: %s", engine->info.device_secret);
        ret = volc_get_llm_config(&engine->info);
        if (ret != 0) {
            LOGE("Failed to get LLM config from host %s, error code: %d", engine->info.host, ret);
            goto err_out_label;
        }
    }

    cJSON* rtc_cfg = cJSON_GetObjectItem(config, "rtc");
    if (rtc_cfg) {
#if defined(ENABLE_RTC_MODE)
        engine->rtc = volc_rtc_create(engine->info.rtc_app_id, engine, rtc_cfg, __realtime_user_event_router, __realtime_data_router);
#else
        LOGW("RTC mode is not enabled");
#endif
    }

    cJSON* ws_cfg = cJSON_GetObjectItem(config, "ws");
    if (ws_cfg) {
#if defined(ENABLE_WS_MODE)
        engine->ws = volc_ws_create(engine, ws_cfg, __realtime_user_event_router, __realtime_data_router);
#else
        LOGW("WS mode is not enabled");
#endif
    }

    engine->status = VOLC_RT_STATE_CREATED;
    *handle = (volc_engine_t)engine;
    cJSON_Delete(config);
    LOGI("Engine created successfully");
    return 0;
err_out_label:
    _iot_info_free(&engine->info);
    engine ? hal_free(engine) : 0;
    cJSON_Delete(config);
    return ret;
}

void volc_destroy(volc_engine_t handle) {
    volc_engine_impl_t* engine = (volc_engine_impl_t *)handle;
    if (NULL == engine) {
        return;
    }
    _iot_info_free(&engine->info);
    hal_free(engine);
}

int volc_start(volc_engine_t handle, volc_opt_t* opt) {
    int ret = 0;
    volc_engine_impl_t* engine = (volc_engine_impl_t*)handle;
    if (engine == NULL || opt == NULL) {
        LOGE("engine handle(%p) or bot id(%p) is NULL", handle, opt);
        return -1;
    }

    if (NULL == opt->bot_id || strlen(opt->bot_id) <= 0) {
        LOGE("bot id is invalid");
        return -1;
    }
    
    if (engine->status != VOLC_RT_STATE_CREATED && engine->status != VOLC_RT_STATE_STOPPED) {
        LOGE("engine is not in CREATED state");
        return -1;
    }

    engine->mode = opt->mode;
    if (opt->mode == VOLC_MODE_WS) {
#if defined(ENABLE_WS_MODE)
        ret = volc_ws_start(engine->ws, opt->bot_id, &engine->info);
#else
        LOGE("WS mode is not enabled");
        ret = -1;
#endif
    } else if (opt->mode == VOLC_MODE_RTC) {
#if defined(ENABLE_RTC_MODE)
        ret = volc_rtc_start(engine->rtc, opt->bot_id, &engine->info);
#else
        LOGE("RTC mode is not enabled");
        ret = -1;
#endif
    }

    engine->status = VOLC_RT_STATE_STARTED;
    LOGI("engine started successfully");
    return ret;
}

int volc_stop(volc_engine_t handle) {
    int ret = 0;
    volc_engine_impl_t* engine = (volc_engine_impl_t*)handle;
    if (engine == NULL) {
        LOGE("engine handle is NULL");
        return -1;
    }
    if (engine->status != VOLC_RT_STATE_STARTED) {
        LOGW("engine is not in STARTED state");
        return -1;
    }

    switch (engine->mode)
    {
    case VOLC_MODE_WS:
#if defined(ENABLE_WS_MODE)
        ret = volc_ws_stop(engine->ws);
#else
        LOGE("WS mode is not enabled");
        ret = -1;
#endif
        break;
    case VOLC_MODE_RTC:
#if defined(ENABLE_RTC_MODE)
        ret = volc_rtc_stop(engine->rtc);
#else
        LOGE("RTC mode is not enabled");
        ret = -1;
#endif
        break;
    default:
        break;
    }
    engine->mode = VOLC_MODE_UNKNOWN;

    engine->status = VOLC_RT_STATE_STOPPED;
    LOGI("engine stopped successfully");
    return ret;
}

int volc_update(volc_engine_t handle, const void* data_ptr, size_t data_len) {
    int ret = 0;
    volc_message_info_t info = { 0 };
    volc_engine_impl_t* engine = (volc_engine_impl_t*)handle;
    if (engine == NULL) {
        LOGE("engine handle is NULL");
        return -1;
    }
    if (engine->status != VOLC_RT_STATE_STARTED) {
        LOGE("engine is not in STARTED state");
        return -1;
    }

    info.is_binary = true;
    return volc_send_message(handle, data_ptr, data_len, &info);
}

int volc_send_audio_data(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_audio_frame_info_t* info_ptr) {
    int ret = 0;
    volc_data_info_t info = { 0 };
    volc_engine_impl_t* engine = (volc_engine_impl_t*)handle;
    if (engine == NULL || data_ptr == NULL || data_len == 0) {
        LOGE("engine %p, data_ptr %p, or data_len %zu is NULL or zero", engine, data_ptr, data_len);
        return -1;
    }
    if (engine->status != VOLC_RT_STATE_STARTED) {
        LOGE("engine is not in STARTED state");
        return -1;
    }

    info.type = VOLC_DATA_TYPE_AUDIO;
    info.info.audio.data_type = info_ptr->data_type;
    info.info.audio.commit = info_ptr->commit;
    switch (engine->mode) {
        case VOLC_MODE_RTC:
#if defined(ENABLE_RTC_MODE)
            ret = volc_rtc_send(engine->rtc, data_ptr, data_len, &info);
#else
            LOGE("RTC mode is not enabled");
            ret = -1;
#endif
            break;
        case VOLC_MODE_WS:
#if defined(ENABLE_WS_MODE)
            ret = volc_ws_send(engine->ws, data_ptr, data_len, &info);
#else
            LOGE("WS mode is not enabled");
            ret = -1;
#endif
            break;
        default:
            break;
    }
    
    return ret;
}

int volc_send_video_data(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_video_frame_info_t* info_ptr) {
    int ret = 0;
    volc_data_info_t info = { 0 };
    volc_engine_impl_t* engine = (volc_engine_impl_t*)handle;
    if (engine == NULL || data_ptr == NULL || data_len == 0) {
        LOGE("engine %p, data_ptr %p, or data_len %zu is NULL or zero", engine, data_ptr, data_len);
        return -1;
    }
    if (engine->status != VOLC_RT_STATE_STARTED) {
        LOGE("engine is not in STARTED state");
        return -1;
    }

    info.type = VOLC_DATA_TYPE_VIDEO;
    info.info.video.data_type = info_ptr->data_type;
    switch (engine->mode) {
        case VOLC_MODE_RTC:
#if defined(ENABLE_RTC_MODE)
            ret = volc_rtc_send(engine->rtc, data_ptr, data_len, &info);
#else
            LOGE("RTC mode is not enabled");
            ret = -1;
#endif
            break;
        case VOLC_MODE_WS:
#if defined(ENABLE_WS_MODE)
            ret = volc_ws_send(engine->ws, data_ptr, data_len, &info);
#else
            LOGE("WS mode is not enabled");
            ret = -1;
#endif
            break;
        default:
            break;
    }
    return 0;
}

int volc_send_message(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_message_info_t* info_ptr) {
    int ret = 0;
    volc_data_info_t info = { 0 };
    if (handle == NULL || data_ptr == NULL || data_len == 0) {
        LOGE("engine handle %p, data_ptr %p, or data_len %zu is NULL or zero", handle, data_ptr, data_len);
        return -1;
    }
    
    volc_engine_impl_t* engine = (volc_engine_impl_t*)handle;
    if (engine->status != VOLC_RT_STATE_STARTED) {
        LOGE("engine is not in started state");
        return -1;
    }

    info.type = VOLC_DATA_TYPE_MESSAGE;
    info.info.message.is_binary = info_ptr ? info_ptr->is_binary : false;
    switch (engine->mode) {
        case VOLC_MODE_RTC:
#if defined(ENABLE_RTC_MODE)
            ret = volc_rtc_send(engine->rtc, data_ptr, data_len, &info);
#else
            LOGE("RTC mode is not enabled");
            ret = -1;
#endif
            break;
        case VOLC_MODE_WS:
#if defined(ENABLE_WS_MODE)
            ret = volc_ws_send(engine->ws, data_ptr, data_len, &info);
#else
            LOGE("WS mode is not enabled");
            ret = -1;
#endif
            break;
        default:
            break;
    }
    return ret;
}

int volc_interrupt(volc_engine_t handle) {
    int ret = 0;
    volc_engine_impl_t* engine = (volc_engine_impl_t*)handle;
    if (handle == NULL) {
        LOGE("engine handle is NULL");
        return -1;
    }
    LOGI("interrupt: %d", engine->mode);
    switch (engine->mode) {
        case VOLC_MODE_RTC:
#if defined(ENABLE_RTC_MODE)
            ret = volc_rtc_interrupt(engine->rtc);
#else
            LOGE("RTC mode is not enabled");
            ret = -1;
#endif
            break;
        case VOLC_MODE_WS:
#if defined(ENABLE_WS_MODE)
            ret = volc_ws_interrupt(engine->ws);
#else
            LOGE("WS mode is not enabled");
            ret = -1;
#endif
            break;
        default:
            break;
    }

    return ret;
}