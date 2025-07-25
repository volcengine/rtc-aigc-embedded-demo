// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#ifndef __RTC_BOT_UTILS_H__
#define __RTC_BOT_UTILS_H__

#include "common.h"
#if defined(CONFIG_VOLC_RTC_MODE)

int start_voice_bot(rtc_room_info_t* room_info);
int stop_voice_bot(const rtc_room_info_t* room_info);
int update_voice_bot(const rtc_room_info_t* room_info, const char* command, const char* message);
int interrupt_voice_bot(const rtc_room_info_t* room_info);
int voice_bot_function_calling(const rtc_room_info_t* room_info, const char* message);

#endif

#endif // __RTC_BOT_UTILS_H__