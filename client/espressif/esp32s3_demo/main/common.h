// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *key;
    const char *value;
} http_req_header_t;

typedef struct {
    char room_id[129];
    char uid[129];
    char app_id[25];
    char task_id[129];
    char bot_uid[129];
    char token[257];
} rtc_room_info_t;

#ifdef __cplusplus
}
#endif

#endif