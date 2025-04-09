// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#ifndef __AUDIO_PIPELINE_H__
#define __AUDIO_PIPELINE_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "audio_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define CONFIG_CHOICE_G711A_ENCODER 1
// #define CONFIG_CHOICE_OPUS_ENCODER 1
// #define CONFIG_CHOICE_AAC_ENCODER 1
// #define CONFIG_AUDIO_SUPPORT_G711A_DECODER 1
#define CONFIG_CHOICE_G711A_INTERNAL 1

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