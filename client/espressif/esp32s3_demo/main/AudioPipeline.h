// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#ifndef __AUDIO_PIPELINE_H__
#define __AUDIO_PIPELINE_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "audio_pipeline.h"
#include "Config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEFAULT_AUDIO_CODEC_TYPE_OPUS
#define RTC_DEMO_AUDIO_PIPELINE_CODEC_OPUS 1
#elif defined(DEFAULT_AUDIO_CODEC_TYPE_PCM)
#define RTC_DEMO_AUDIO_PIPELINE_CODEC_PCM 1
#elif defined(DEFAULT_AUDIO_CODEC_TYPE_G711A)
#define RTC_DEMO_AUDIO_PIPELINE_CODEC_G711A 1
#elif defined(DEFAULT_AUDIO_CODEC_TYPE_G722)
#define RTC_DEMO_AUDIO_PIPELINE_CODEC_G722 1
#elif defined(DEFAULT_AUDIO_CODEC_TYPE_AAC)
#define RTC_DEMO_AUDIO_PIPELINE_CODEC_AAC 1
#endif

struct recorder_pipeline_t;
typedef struct recorder_pipeline_t recorder_pipeline_t,*recorder_pipeline_handle_t;
recorder_pipeline_handle_t recorder_pipeline_open();
void recorder_pipeline_run(recorder_pipeline_handle_t);
void recorder_pipeline_close(recorder_pipeline_handle_t);
int recorder_pipeline_get_default_read_size(recorder_pipeline_handle_t);
int recorder_pipeline_read(recorder_pipeline_handle_t,char *buffer, int buf_size);

struct  player_pipeline_t;
typedef struct player_pipeline_t player_pipeline_t,*player_pipeline_handle_t;
player_pipeline_handle_t player_pipeline_open();
void player_pipeline_run(player_pipeline_handle_t);
void player_pipeline_close(player_pipeline_handle_t);
int player_pipeline_get_default_read_size(player_pipeline_handle_t);
int player_pipeline_write(player_pipeline_handle_t,char *buffer, int buf_size);
void player_pipeline_write_play_buffer_flag(player_pipeline_handle_t player_pipeline);

#ifdef __cplusplus
}
#endif
#endif