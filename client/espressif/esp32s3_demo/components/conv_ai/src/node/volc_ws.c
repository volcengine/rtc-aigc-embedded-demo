// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "volc_ws.h"

#include "platform/volc_platform.h"
#include "base/volc_base.h"
#include "util/volc_list.h"
#include "util/volc_log.h"
#include "util/volc_json.h"
#include "util/volc_base64.h"
#include "websocket.h"

const char* ws_interrupt_str = "{\"type\": \"response.cancel\"}";

typedef struct {
    uint8_t opcode;
    uint8_t* buffer;
    int size;
    int capacity;
    int in_progress;
} ws_assembler_t;

typedef struct {
    bool b_pipeline_started;
    bool b_connected;
    char* p_aigw_path;
    char* p_data_buf;
    char* p_bot_id;
    char headers[1024];
    char uri[256];
    size_t data_buf_size;
    void* context;
    volc_msg_cb message_callback;
    volc_data_cb data_callback;
    char hardware_id[32];
    ws_assembler_t assembler;
    volc_ws_client_t* client;
} ws_impl_t;

static int __ws_init(ws_impl_t* ws, cJSON* p_config)
{
    int ret = 0;

    if (!ws || !p_config) {
        LOGE("ws or config is NULL");
        return -1;
    }

    ret = volc_json_read_string(p_config, "aigw_path", &ws->p_aigw_path);
    if (ret != 0) {
        LOGE("Failed to read aigw_path");
        return -1;
    }

    return 0;
}

static int __build_ws_message(const char* msg, uint8_t** out_buf, size_t* out_len) {
    size_t msg_len = strlen(msg) + 1;
    *out_len = msg_len - 1;
    *out_buf = (uint8_t*)hal_malloc(msg_len);
    if (!*out_buf) {
        LOGE("hal_malloc failed");
        return -1;
    }
    memcpy(*out_buf, msg, msg_len);
    (*out_buf)[msg_len - 1] = 0;
    return 0;
}

static void __send_message_2_user(ws_impl_t* ws, volc_msg_t* msg)
{
    if (ws->message_callback) {
        ws->message_callback(ws->context, msg);
    }
}

static void __send_data_2_user(ws_impl_t* ws, const char* data, int data_len, volc_data_info_t* info) {
    if (ws->data_callback) {
        ws->data_callback(ws->context, (const void*)data, data_len, info);
    }
}

static void __ws_recv_data(ws_impl_t* ws, const char* data, int data_len)
{
    cJSON* p_json = NULL;
    char* p_type = NULL;
    char* p_delta = NULL;
    char* p_status = NULL;
    void* p_data = NULL;
    size_t len = 0;
    volc_data_info_t info = { 0 };
    volc_msg_t msg = { 0 };

    if (NULL == data || 0 == data_len) {
        return;
    }

    p_json = cJSON_ParseWithLength(data, data_len);
    if (!p_json) {
        LOGE("Failed to parse json, data_len: %d, data: %s", data_len, data);
        return;
    }
    // LOGI("json: %s", data);
    volc_json_read_string(p_json, "type", &p_type);
    volc_json_read_string(p_json, "delta", &p_delta);
    volc_json_read_string(p_json, "response.status", &p_status);
    if (strcmp(p_type, "response.audio.delta") == 0 && p_delta) {
        if (strlen(p_delta) == 0) {
            LOGE("delta is empty, data: %s", data);
            goto err_out_label;
        }
        if (NULL == ws || !ws->b_pipeline_started) {
            LOGD("pipeline not started");
            goto err_out_label;
        }
        len = volc_base64_decoded_length((const uint8_t*)p_delta, strlen(p_delta));
        p_data = hal_malloc(len + 1);
        if (NULL == p_data) {
            LOGE("Failed to alloc memory");
            goto err_out_label;
        }
        volc_base64_decode((unsigned char *)p_data, len, &len, (const unsigned char *)p_delta, strlen(p_delta));
        info.type = VOLC_DATA_TYPE_AUDIO;
        info.info.audio.data_type = VOLC_AUDIO_DATA_TYPE_PCM;
        // info.info.audio.sent_ts = volc_get_time(); // TODO
        __send_data_2_user(ws, p_data, len, &info);
        hal_free(p_data);
    } else if (strcmp(p_type, "input_audio_buffer.speech_started") == 0) {
        msg.code = VOLC_MSG_CONV_STATUS;
        msg.data.conv_status = VOLC_CONV_STATUS_LISTENING;
        __send_message_2_user(ws, &msg);
    } else if (strcmp(p_type, "input_audio_buffer.speech_stopped") == 0) {
        msg.code = VOLC_MSG_CONV_STATUS;
        msg.data.conv_status = VOLC_CONV_STATUS_THINKING;
        __send_message_2_user(ws, &msg);
    } else if (strcmp(p_type, "response.done") == 0 && p_status) {
        msg.code = VOLC_MSG_CONV_STATUS;
        if (strcmp(p_status, "completed") == 0) {
            msg.data.conv_status = VOLC_CONV_STATUS_ANSWER_FINISH;
        } else if (strcmp(p_status, "cancelled") == 0) {
            msg.data.conv_status = VOLC_CONV_STATUS_INTERRUPTED;
        }
        LOGI("data: %s", data);
        __send_message_2_user(ws, &msg);
    } else {
        info.type = VOLC_DATA_TYPE_MESSAGE;
        info.info.message.is_binary = false;
        __send_data_2_user(ws, data, data_len, &info);
    }
err_out_label:
    if (p_type) {
        hal_free(p_type);
    }
    if (p_delta) {
        hal_free(p_delta);
    }
    if (p_status) {
        hal_free(p_status);
    }
    cJSON_Delete(p_json);
}

static void __ws_assembler_free(ws_assembler_t* a) {
    if (a->buffer) {
        hal_free(a->buffer);
    }
    a->buffer = NULL;
    a->size = 0;
    a->capacity = 0;
    a->in_progress = 0;
}

static void __ws_append_data(ws_impl_t* ws, volc_ws_event_data_t* data) {
    int new_capacity = 0;
    uint8_t* new_buffer = NULL;
    if (data->op_code == VOLC_WS_OPCODES_TEXT || data->op_code == VOLC_WS_OPCODES_BINARY) {
        ws->assembler.in_progress = 1;
        ws->assembler.opcode = data->op_code;
    } else if (data->op_code == VOLC_WS_OPCODES_CONT) {
        if (!ws->assembler.in_progress) {
            LOGW("append data, but assembler not in progress");
            return;
        }
    } else {
        LOGW("append data, skip opcode: %d, data_len: %d", data->op_code, data->data_len);
        return;
    }

    if (ws->assembler.in_progress) {
        new_capacity = ws->assembler.size + data->data_len + 1;
        if (new_capacity > ws->assembler.capacity) {
            LOGI("append data, new_capacity: %d", new_capacity);
            new_capacity = new_capacity < 1024 ? 1024 : new_capacity * 2;
            new_buffer = hal_realloc(ws->assembler.buffer, new_capacity);
            if (new_buffer == NULL) {
                LOGE("Failed to alloc memory");
                __ws_assembler_free(&ws->assembler);
                return;
            }
            ws->assembler.buffer = new_buffer;
            ws->assembler.capacity = new_capacity;
        }
        memcpy(ws->assembler.buffer + ws->assembler.size, data->data_ptr, data->data_len);
        ws->assembler.size += data->data_len;

        if (data->fin && (data->payload_len == data->payload_offset + data->data_len)) {
            LOGD("append data, fin, len: %d", ws->assembler.size);
            __ws_recv_data(ws, (const char*)ws->assembler.buffer, ws->assembler.size);
            memset(ws->assembler.buffer, 0, ws->assembler.capacity);
            ws->assembler.size = 0;
            ws->assembler.in_progress = 0;
        }
    }
}

static void __ws_stop(ws_impl_t* ws);
static void  __ws_event_handler(void* context, int32_t event_id, void* event_data) {
    ws_impl_t* ws = (ws_impl_t*)context;
    volc_msg_t msg = { 0 };
    volc_ws_event_data_t *data = (volc_ws_event_data_t *)event_data;
    if (NULL == ws) {
        LOGW("ws is NULL");
        return;
    }
    switch (event_id) {
        case VOLC_WS_EVENT_CONNECTED:
            ws->b_connected = true;
            msg.code = VOLC_MSG_CONNECTED;
            __send_message_2_user(ws, &msg);
            break;
        case VOLC_WS_EVENT_DISCONNECTED:
            ws->b_connected = false;
            msg.code = VOLC_MSG_DISCONNECTED;
            __send_message_2_user(ws, &msg);
            break;
        case VOLC_WS_EVENT_DATA:
            __ws_append_data(ws, data);
            break;
        case VOLC_WS_EVENT_CLOSED:
            LOGW("receive close event");
            msg.code = VOLC_MSG_DISCONNECTED;
            __send_message_2_user(ws, &msg);
            __ws_stop(ws);
            break;
        default:
            break;
    }
}

static int __ws_start(ws_impl_t* ws, volc_iot_info_t* iot_info)
{
    uint64_t current_time = hal_get_time_ms();
    int random_num = (int)current_time;//volc_get_random_num();
    char time_str[32] = { 0 };
    char num_str[32] = { 0 };
    char platform_info[16] = { 0 };
    char user_agent[64] = { 0 };
    if (!ws || !iot_info) {
        LOGE("ws or option is NULL");
        return -1;
    }

    hal_get_platform_info(platform_info, sizeof(platform_info));
    snprintf(user_agent, sizeof(user_agent), "%s(%s)", volc_get_version(), platform_info);
    snprintf(time_str, sizeof(time_str), "%llu", current_time);
    snprintf(num_str, sizeof(num_str), "%d", random_num);
    char* signature = volc_generate_signature(iot_info->device_secret, iot_info->product_key, iot_info->device_name, random_num, current_time, 1);
    volc_ws_config_t ws_cfg = { 0 };
    snprintf(ws->uri, sizeof(ws->uri), "%s%s?bot=%s", iot_info->ws_url, ws->p_aigw_path, ws->p_bot_id);
	ws_cfg.uri = ws->uri;
    ws_cfg.user_agent = user_agent;
	snprintf(ws->headers, sizeof(ws->headers), "authorization: Bearer %s\r\n"
                        "X-Signature: %s\r\n"
						"X-Auth-Type: 1\r\n"
						"X-Device-Name: %s\r\n"
						"X-Product-Key: %s\r\n"
						"X-Random-Num: %s\r\n"
						"X-Timestamp: %s\r\n"
						"X-Hardware-Id: %s\r\n", iot_info->api_key, signature, iot_info->device_name, iot_info->product_key, num_str, time_str, ws->hardware_id);
    LOGI("device secret: %s, product key: %s, device name: %s, api key: %s, random num: %s, timestamp: %s, hardware id: %s", iot_info->device_secret, iot_info->product_key, iot_info->device_name, iot_info->api_key, num_str, time_str, ws->hardware_id);
    ws_cfg.headers = ws->headers;
    LOGI("WS URL: %s", ws_cfg.uri);
    ws_cfg.user_context = ws;
    ws_cfg.buffer_size = 1024 * 5;
    ws_cfg.ws_event_handler = __ws_event_handler;
	ws->client = volc_ws_client_init(&ws_cfg);
	if(volc_ws_client_start(ws->client)) {
        LOGE("Failed to start websocket client");
		return -1;
	}
    ws->b_pipeline_started = true;
    return 0;
}

static void __ws_stop(ws_impl_t* ws)
{
    if (!ws) {
        LOGE("ws instance is NULL");
        return;
    }
    volc_ws_client_destroy(ws->client);
    ws->client = NULL;
    ws->b_pipeline_started = false;
}

static int __ws_input_audio_buffer_append(ws_impl_t* ws, const char* data_ptr, size_t data_len) {
    int ret = 0;
    cJSON* root = NULL;
    if (!ws || !data_ptr || data_len == 0) {
        LOGE("ws or data or data_len is NULL");
        return -1;
    }

    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "input_audio_buffer.append");
    cJSON_AddStringToObject(root, "audio", data_ptr);
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ret = volc_ws_client_send_text(ws->client, json, strlen(json), 1000);
    if (ret >= 0) {
        ret = 0;
    } else {
        LOGW("failed to send audio buffer");
    }
    if (json) {
        hal_free(json);
    }
    return ret;
}

static int __ws_input_audio_buffer_commit(ws_impl_t* ws) {
    int ret = 0;
    cJSON* root = NULL;
    if (!ws) {
        LOGE("ws instance is NULL");
        return -1;
    }
    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "input_audio_buffer.commit");
    char* json = cJSON_PrintUnformatted(root);
    LOGD("json: %s", json);
    cJSON_Delete(root);
    ret = volc_ws_client_send_text(ws->client, json, strlen(json), 1000);
    if (ret >= 0) {
        ret = 0;
    }
    if (json) {
        hal_free(json);
    }
    return ret;
}

static int __ws_response_create(ws_impl_t* ws) {
    int ret = 0;
    cJSON* root = NULL;
    if (!ws) {
        LOGE("ws instance is NULL");
        return -1;
    }
    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "response.create");
    // 创建 "response" 对象
    cJSON* response_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "response", response_obj);
    // 添加 “modalities” 字段
    cJSON* modalities_array = cJSON_CreateArray();
    cJSON_AddItemToArray(modalities_array, cJSON_CreateString("text"));
    cJSON_AddItemToArray(modalities_array, cJSON_CreateString("audio"));
    cJSON_AddItemToObject(response_obj, "modalities", modalities_array);
    // 生成 JSON 字符串
    const char* json = cJSON_PrintUnformatted(root);
    LOGD("json: %s", json);
    // 释放内存
    cJSON_Delete(root);
    ret = volc_ws_client_send_text(ws->client, json, strlen(json), 1000);
    if (json) {
        hal_free((void*)json);
    }
    return ret;
}

static int __ws_send_audio(ws_impl_t* ws, const void* data_ptr, size_t data_len, bool commit) {
    int ret = 0;
    size_t len = 0;
    if (!ws || !data_ptr || !data_len) {
        LOGE("ws or data or info is NULL");
        return -1;
    }
    len = volc_base64_encoded_length(data_len);
    if (ws->data_buf_size < len) {
        if (ws->p_data_buf) {
            hal_free(ws->p_data_buf);
        }
        ws->p_data_buf = (char*)hal_malloc(len);
        if (!ws->p_data_buf) {
            LOGE("failed to malloc data buf");
            ws->data_buf_size = 0;
            return -1;
        }
        ws->data_buf_size = len;
    }
    volc_base64_encode((unsigned char*) ws->p_data_buf, len, &len, (const unsigned char*) data_ptr, data_len);
    if (ret != 0) {
        LOGE("failed to encode audio");
        return -1;
    }
    ret = __ws_input_audio_buffer_append(ws, ws->p_data_buf, len);
    if (ret != 0) {
        LOGE("failed to append audio buffer");
        return -1;
    }
    if (commit) {
        ret = __ws_input_audio_buffer_commit(ws);
        if (ret != 0) {
            LOGE("failed to commit audio buffer");
            return -1;
        }
        ret = __ws_response_create(ws);
    }
    return ret;
}

static int __ws_send_message(ws_impl_t* ws, const void* data_ptr, size_t data_len) {
    if (!ws || !data_ptr || !data_len) {
        LOGE("ws or data or info is NULL");
        return -1;
    }
    return volc_ws_client_send_text(ws->client, (const char*)data_ptr, data_len, 1000);
}

volc_ws_t volc_ws_create(void* context, cJSON* p_config, volc_msg_cb message_callback, volc_data_cb data_callback) {
    ws_impl_t* ws = (ws_impl_t*)hal_calloc(1, sizeof(ws_impl_t));
    if (!ws) {
        LOGE("volc_ws_create: malloc ws failed");
        return NULL;
    }
    ws->message_callback = message_callback;
    ws->data_callback = data_callback;
    ws->context = context;

    hal_get_uuid(ws->hardware_id, sizeof(ws->hardware_id));

    if (__ws_init(ws, p_config) != 0) {
        hal_free(ws);
        LOGE("volc_ws_create: ws init failed");
        return NULL;
    }

    LOGI("ws create success, hardware id: %s", ws->hardware_id);
    return (volc_ws_t) ws;
}

void volc_ws_destroy(volc_ws_t ws) {
    ws_impl_t* ws_impl = (ws_impl_t*) ws;
    if (!ws_impl) {
        LOGE("ws instance is NULL");
        return;
    }

    __ws_stop(ws_impl);
    if (ws_impl->p_aigw_path) {
        hal_free(ws_impl->p_aigw_path);
    }
    if (ws_impl->p_data_buf) {
        hal_free(ws_impl->p_data_buf);
    }
    if (ws_impl) {
        hal_free(ws_impl);
    }
}

int volc_ws_start(volc_ws_t ws, const char* bot_id, volc_iot_info_t* iot_info) {
    ws_impl_t* ws_impl = (ws_impl_t*) ws;
    if (!ws_impl) {
        LOGE("ws instance is NULL");
        return -1;
    }
    ws_impl->p_bot_id = strdup(bot_id);
    return __ws_start(ws_impl, iot_info);
}

int volc_ws_send(volc_ws_t ws, const void* data, int size, volc_data_info_t* data_info) {
    ws_impl_t* ws_impl = (ws_impl_t*) ws;
    if (!ws_impl) {
        LOGE("ws instance is NULL");
        return -1;
    }

    if (!ws_impl->b_pipeline_started || !ws_impl->b_connected) {
        LOGD("pipeline started[%d], connected[%d], cannot process data", (int) ws_impl->b_pipeline_started, (int) ws_impl->b_connected);
        return -1;
    }

    switch (data_info->type) {
        case VOLC_DATA_TYPE_AUDIO: {
            return __ws_send_audio(ws_impl, data, size, data_info->info.audio.commit);
        }
        case VOLC_DATA_TYPE_VIDEO: {
            LOGW("video data not supported");
            break;
        }
        case VOLC_DATA_TYPE_MESSAGE: {
            return __ws_send_message(ws, data, size);
        }
        default:
            LOGW("unsupported data type: %d", data_info->type);
            return -1;
    }
    return 0;
}

int volc_ws_stop(volc_ws_t ws) {
    ws_impl_t* ws_impl = (ws_impl_t*) ws;
    if (!ws_impl) {
        LOGE("ws instance is NULL");
        return -1;
    }
    __ws_stop(ws_impl);
    return 0;
}

int volc_ws_interrupt(volc_ws_t ws) {
    int ret = 0;
    uint8_t* msg = NULL;
    size_t msg_len = 0;
    ws_impl_t* ws_impl = (ws_impl_t*) ws;
    if (!ws_impl) {
        LOGE("ws instance is NULL");
        return -1;
    }
    if (__build_ws_message(ws_interrupt_str, &msg, &msg_len) != 0) {
        LOGE("build control message failed");
        return -1;
    }
    if ((__ws_send_message(ws_impl, msg, msg_len)) <= 0) {
        LOGE("send control message failed");
        ret = -1;
    }
    if (msg) {
        hal_free(msg);
    }
    return ret;
}
