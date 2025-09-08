// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "AudioPipeline.h"
#include <string.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_sys.h"
#include "board.h"
#include "algorithm_stream.h"
#include "filter_resample.h"
#include "i2s_stream.h"
#include "pthread.h"
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
#include "es7210.h"
#elif CONFIG_M5STACK_ATOMS3R_BOARD
#include "es8311.h"
#endif

#include "esp_timer.h"


#if defined (RTC_DEMO_AUDIO_PIPELINE_CODEC_OPUS)
#include "raw_opus_encoder.h"
#include "raw_opus_decoder.h"
#elif defined (RTC_DEMO_AUDIO_PIPELINE_CODEC_AAC)
#include "aac_encoder.h"
#include "aac_decoder.h"
#elif defined (RTC_DEMO_AUDIO_PIPELINE_CODEC_G711A)
#include "g711_encoder.h"
#include "g711_decoder.h"
#endif
#include "audio_idf_version.h"
#include "raw_stream.h"


#define CHANNEL                     1
static const char *TAG = "AUDIO_PIPELINE";
#define I2S_SAMPLE_RATE             16000
#define ALGO_SAMPLE_RATE            16000
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
#define ALGORITHM_STREAM_SAMPLE_BIT 32
#define CHANNEL_FORMAT              I2S_CHANNEL_TYPE_ONLY_LEFT
#define ALGORITHM_INPUT_FORMAT      "RM"
#define CHANNEL_NUM                 1
#elif CONFIG_M5STACK_ATOMS3R_BOARD
#define ALGORITHM_STREAM_SAMPLE_BIT 16
#define CHANNEL_FORMAT              I2S_CHANNEL_TYPE_RIGHT_LEFT
#define ALGORITHM_INPUT_FORMAT      "MR"
#define CHANNEL_NUM                 2
#endif


#define CODEC_SAMPLE_RATE   16000

struct  recorder_pipeline_t {
    audio_pipeline_handle_t audio_pipeline;
    audio_element_handle_t i2s_stream_reader;
    audio_element_handle_t raw_reader;
    audio_element_handle_t rsp;
    audio_element_handle_t algo_aec;
};


struct  player_pipeline_t {
    audio_pipeline_handle_t audio_pipeline;
    audio_element_handle_t raw_writer;
    audio_element_handle_t rsp;
    audio_element_handle_t i2s_stream_writer;
};

static audio_element_handle_t create_resample_stream(int src_rate, int src_ch, int dest_rate, int dest_ch)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = src_rate;
    rsp_cfg.src_ch = src_ch;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_ch;
    rsp_cfg.complexity = 5;
    audio_element_handle_t stream = rsp_filter_init(&rsp_cfg);
    return stream;
}

static audio_element_handle_t create_record_i2s_stream(void)
{
#if CONFIG_ESP32_S3_KORVO2_V3_BOARD
    es7210_adc_set_gain(ES7210_INPUT_MIC3, GAIN_30DB);
#elif CONFIG_M5STACK_ATOMS3R_BOARD
    es8311_set_mic_gain(ES8311_MIC_GAIN_36DB);
#endif
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, AUDIO_STREAM_READER); // 参数需要仔细检查
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_set_channel_type(&i2s_cfg, CHANNEL_FORMAT);
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = I2S_SAMPLE_RATE;
    return i2s_stream_init(&i2s_cfg);
}


static audio_element_handle_t create_record_raw_stream(void)
{
    audio_element_handle_t raw_stream = NULL;
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_cfg.out_rb_size = 2 * 1024;
    raw_stream = raw_stream_init(&raw_cfg);
    audio_element_set_output_timeout(raw_stream, portMAX_DELAY);
    return raw_stream;
}

static audio_element_handle_t create_record_algo_stream(void)
{
    ESP_LOGI(TAG, "[3.1] Create algorithm stream for aec");
    algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
    algo_config.sample_rate = ALGO_SAMPLE_RATE;
    algo_config.out_rb_size = 256;
    algo_config.algo_mask = ALGORITHM_STREAM_DEFAULT_MASK | ALGORITHM_STREAM_USE_AGC;
    algo_config.input_format = ALGORITHM_INPUT_FORMAT;
    audio_element_handle_t element_algo = algo_stream_init(&algo_config);
    audio_element_set_music_info(element_algo, ALGO_SAMPLE_RATE, 1, 16);
    audio_element_set_input_timeout(element_algo, portMAX_DELAY);
    return element_algo;
}

recorder_pipeline_handle_t recorder_pipeline_open()
{
    recorder_pipeline_handle_t pipeline = heap_caps_calloc(1, sizeof(recorder_pipeline_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // create and register streams
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline->audio_pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline->audio_pipeline);

    pipeline->i2s_stream_reader = create_record_i2s_stream();
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->i2s_stream_reader, "i2s");
    
    pipeline->algo_aec = create_record_algo_stream();
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->algo_aec, "algo");

    pipeline->rsp = create_resample_stream(I2S_SAMPLE_RATE, 1, CODEC_SAMPLE_RATE, 1);
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->rsp, "rsp");

    pipeline->raw_reader = create_record_raw_stream();
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->raw_reader, "raw");

    const char *link_tag[] = {"i2s", "algo", "raw"};
    audio_pipeline_link(pipeline->audio_pipeline, &link_tag[0], sizeof(link_tag) / sizeof(link_tag[0]));
    return pipeline;
}

void recorder_pipeline_close(recorder_pipeline_handle_t pipeline)  {
    audio_pipeline_stop(pipeline->audio_pipeline);
    audio_pipeline_wait_for_stop(pipeline->audio_pipeline);
    audio_pipeline_terminate(pipeline->audio_pipeline);

    if (pipeline->i2s_stream_reader) {
        audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->i2s_stream_reader);
        audio_element_deinit(pipeline->i2s_stream_reader);
    }
    if (pipeline->raw_reader) {
        audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->raw_reader);
        audio_element_deinit(pipeline->raw_reader);
    }
    if (pipeline->rsp) {
        audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->rsp);
        audio_element_deinit(pipeline->rsp);
    }
    if (pipeline->algo_aec) {
        audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->algo_aec);
        audio_element_deinit(pipeline->algo_aec);
    }

    audio_pipeline_deinit(pipeline->audio_pipeline);
    heap_caps_free(pipeline);
};

void recorder_pipeline_run(recorder_pipeline_handle_t pipeline){
    audio_pipeline_run(pipeline->audio_pipeline);
};

int recorder_pipeline_get_default_read_size(recorder_pipeline_handle_t pipeline){
        return 3200;
};

audio_element_handle_t recorder_pipeline_get_raw_reader(recorder_pipeline_handle_t pipeline){
    return pipeline->raw_reader;
};
audio_pipeline_handle_t recorder_pipeline_get_pipeline(recorder_pipeline_handle_t pipeline){
    return pipeline->audio_pipeline;
};

int recorder_pipeline_read(recorder_pipeline_handle_t pipeline,char *buffer, int buf_size) {
    return raw_stream_read(pipeline->raw_reader, buffer,buf_size);
}

static audio_element_handle_t create_player_raw_stream(void)
{
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_cfg.out_rb_size = 8 * 1024;
    return raw_stream_init(&raw_cfg);
}

static audio_element_handle_t create_player_i2s_stream(void)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, AUDIO_STREAM_WRITER);
    i2s_cfg.type = AUDIO_STREAM_WRITER;
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
    i2s_cfg.need_expand = (16 != 32);
#endif
    i2s_cfg.out_rb_size = 8 * 1024;
    i2s_cfg.buffer_len = 1416;//708
    i2s_stream_set_channel_type(&i2s_cfg, CHANNEL_FORMAT);
    audio_element_handle_t stream = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(stream, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, CHANNEL_NUM);
    return stream;
}

player_pipeline_handle_t player_pipeline_open(void) {
    player_pipeline_handle_t player_pipeline = heap_caps_calloc(1, sizeof(player_pipeline_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    assert(player_pipeline != 0);

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    player_pipeline->audio_pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    player_pipeline->raw_writer = create_player_raw_stream();
    audio_pipeline_register(player_pipeline->audio_pipeline, player_pipeline->raw_writer, "raw");

    player_pipeline->i2s_stream_writer = create_player_i2s_stream();
    audio_pipeline_register(player_pipeline->audio_pipeline, player_pipeline->i2s_stream_writer, "i2s");

    player_pipeline->rsp = create_resample_stream(CODEC_SAMPLE_RATE, 1, I2S_SAMPLE_RATE, CHANNEL_NUM);
    audio_element_set_output_timeout(player_pipeline->rsp, portMAX_DELAY);
    audio_pipeline_register(player_pipeline->audio_pipeline, player_pipeline->rsp, "rsp");

    const char *link_tag[] = {"raw", "rsp", "i2s"};
    audio_pipeline_link(player_pipeline->audio_pipeline, &link_tag[0], sizeof(link_tag) / sizeof(link_tag[0]));

    return player_pipeline;
}


void player_pipeline_run(player_pipeline_handle_t player_pipeline){
    audio_pipeline_run(player_pipeline->audio_pipeline);
};

void player_pipeline_close(player_pipeline_handle_t player_pipeline){
    audio_pipeline_stop(player_pipeline->audio_pipeline);
    audio_pipeline_wait_for_stop(player_pipeline->audio_pipeline);
    audio_pipeline_terminate(player_pipeline->audio_pipeline);

    if (player_pipeline->raw_writer) {
        audio_pipeline_unregister(player_pipeline->audio_pipeline, player_pipeline->raw_writer);
        audio_element_deinit(player_pipeline->raw_writer);
    }
    if (player_pipeline->rsp) {
        audio_pipeline_unregister(player_pipeline->audio_pipeline, player_pipeline->rsp);
        audio_element_deinit(player_pipeline->rsp); 
    }
    if (player_pipeline->i2s_stream_writer) {
        audio_pipeline_unregister(player_pipeline->audio_pipeline, player_pipeline->i2s_stream_writer);
        audio_element_deinit(player_pipeline->i2s_stream_writer); 
    }

    audio_pipeline_deinit(player_pipeline->audio_pipeline);
    heap_caps_free(player_pipeline);
};

int player_pipeline_write(player_pipeline_handle_t player_pipeline, char *buffer, int buf_size){
    raw_stream_write(player_pipeline->raw_writer, buffer, buf_size);
    return 0;
};
