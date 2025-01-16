// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#ifndef __RTC_HTTP_UTILS_H__
#define __RTC_HTTP_UTILS_H__

typedef struct {
    int code;
    char* response;
} rtc_req_result_t;

typedef struct {
    const char* uri;
    const char** headers;  // key1,value1,key2,value2....keyn,valuen,NULL
    const char* post_data;
} rtc_post_config_t;

rtc_req_result_t rtc_http_post(rtc_post_config_t* config);
void rtc_request_free(rtc_req_result_t *result);

#endif // __RTC_HTTP_UTILS_H__