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


#if defined (CONFIG_CHOICE_OPUS_ENCODER)
#include "opus_encoder.h"
#include "opus_decoder.h"
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
#include "aac_encoder.h"
#include "aac_decoder.h"
#elif defined (CONFIG_CHOICE_G711A_ENCODER)
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

#if (CONFIG_CHOICE_OPUS_ENCODER)
#define CODEC_NAME          "opus"
#define CODEC_SAMPLE_RATE   16000
#define BIT_RATE            64000
#define COMPLEXITY          10
#define FRAME_TIME_MS       20 

#define DEC_SAMPLE_RATE     48000
#define DEC_BIT_RATE        64000
#elif (CONFIG_CHOICE_AAC_ENCODER)
#define CODEC_NAME          "aac"
#define CODEC_SAMPLE_RATE   16000
#define BIT_RATE            80000
#elif (CONFIG_CHOICE_G711A_ENCODER)
#define CODEC_NAME          "g711a"
#define CODEC_SAMPLE_RATE   8000
#elif (CONFIG_CHOICE_G711A_INTERNAL)
#define CODEC_NAME          "g711a"
#define CODEC_SAMPLE_RATE   8000
#endif

struct  recorder_pipeline_t {
    audio_pipeline_handle_t audio_pipeline;
    audio_element_handle_t i2s_stream_reader;
    audio_element_handle_t audio_encoder;
    audio_element_handle_t raw_reader;
    audio_element_handle_t rsp;
    audio_element_handle_t algo_aec;
};


struct  player_pipeline_t {
    audio_pipeline_handle_t audio_pipeline;
    audio_element_handle_t raw_writer;
    audio_element_handle_t audio_decoder;
    audio_element_handle_t rsp;
    audio_element_handle_t i2s_stream_writer;
};

static audio_element_handle_t create_resample_stream(int src_rate, int src_ch, int dest_rate, int dest_ch)
{
    ESP_LOGI(TAG, "[3.1] Create resample stream");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = src_rate;
    rsp_cfg.src_ch = src_ch;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_ch;
    rsp_cfg.complexity = 5;
    return rsp_filter_init(&rsp_cfg);
}

static audio_element_handle_t create_algo_stream(void)
{
    ESP_LOGI(TAG, "[3.1] Create algorithm stream for aec");
    algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
    // algo_config.swap_ch = true;
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
    recorder_pipeline_handle_t pipeline = heap_caps_malloc(sizeof(recorder_pipeline_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
    // memset(&pipeline,0,sizeof(recorder_pipeline_t));
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

#if CONFIG_ESP32_S3_KORVO2_V3_BOARD
    es7210_adc_set_gain(ES7210_INPUT_MIC3, GAIN_30DB);
#elif CONFIG_M5STACK_ATOMS3R_BOARD
    es8311_set_mic_gain(ES8311_MIC_GAIN_36DB);
#endif

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline->audio_pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline->audio_pipeline);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, AUDIO_STREAM_READER);
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_set_channel_type(&i2s_cfg, CHANNEL_FORMAT);
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = I2S_SAMPLE_RATE;
    pipeline->i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    ESP_LOGI(TAG, "[3.3] Create audio encoder to handle data");

    pipeline->rsp = create_resample_stream(I2S_SAMPLE_RATE, 1, CODEC_SAMPLE_RATE, 1);
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->rsp, "rsp");

#if  defined (CONFIG_CHOICE_OPUS_ENCODER)
    raw_opus_enc_config_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
    opus_cfg.sample_rate        = CODEC_SAMPLE_RATE;
    opus_cfg.channel            = CHANNEL;
    opus_cfg.bitrate            = BIT_RATE;
    opus_cfg.complexity         = COMPLEXITY;
    pipeline->audio_encoder = raw_opus_decoder_init(&opus_cfg);
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->audio_encoder, CODEC_NAME);
    const char *link_tag[] = {"i2s", "opus", "raw"};
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
    aac_encoder_cfg_t aac_cfg = DEFAULT_AAC_ENCODER_CONFIG();
    aac_cfg.sample_rate        = CODEC_SAMPLE_RATE;
    aac_cfg.channel            = CHANNEL;
    aac_cfg.bitrate            = BIT_RATE;
    pipeline->audio_encoder = aac_encoder_init(&aac_cfg);
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->audio_encoder, CODEC_NAME);
    const char *link_tag[] = {"i2s", "aac", "raw"};
#elif defined (CONFIG_CHOICE_G711A_ENCODER)
    g711_encoder_cfg_t g711_cfg = DEFAULT_G711_ENCODER_CONFIG();
    pipeline->audio_encoder = g711_encoder_init(&g711_cfg);
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->audio_encoder, CODEC_NAME);
    const char *link_tag[] = {"i2s", "algo", "rsp", "g711a", "raw"};
#elif defined (CONFIG_CHOICE_G711A_INTERNAL)
    const char *link_tag[] = {"i2s", "algo", "rsp", "raw"};
#endif

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->i2s_stream_reader, "i2s");

    pipeline->algo_aec = create_algo_stream();
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->algo_aec, "algo");

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_cfg.out_rb_size = 2 * 1024;
    pipeline->raw_reader = raw_stream_init(&raw_cfg);
    audio_element_set_output_timeout(pipeline->raw_reader, portMAX_DELAY);
    audio_pipeline_register(pipeline->audio_pipeline, pipeline->raw_reader, "raw");
    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->audio_encoder-->raw");

    audio_pipeline_link(pipeline->audio_pipeline, &link_tag[0], sizeof(link_tag) / sizeof(link_tag[0]));
    return pipeline;
}

void recorder_pipeline_close(recorder_pipeline_handle_t pipeline)  {
    audio_pipeline_stop(pipeline->audio_pipeline);
    audio_pipeline_wait_for_stop(pipeline->audio_pipeline);
    audio_pipeline_terminate(pipeline->audio_pipeline);

    audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->algo_aec);
    audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->audio_encoder);
    audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->i2s_stream_reader);
    audio_pipeline_unregister(pipeline->audio_pipeline, pipeline->raw_reader);

    /* Release all resources */
    audio_pipeline_deinit(pipeline->audio_pipeline);
    audio_element_deinit(pipeline->algo_aec);
    audio_element_deinit(pipeline->raw_reader);
    audio_element_deinit(pipeline->i2s_stream_reader);
    audio_element_deinit(pipeline->audio_encoder);

    heap_caps_free(pipeline);
};

void recorder_pipeline_run(recorder_pipeline_handle_t pipeline){
    audio_pipeline_run(pipeline->audio_pipeline);
};

int recorder_pipeline_get_default_read_size(recorder_pipeline_handle_t pipeline){
    #if defined (CONFIG_CHOICE_OPUS_ENCODER)
        return BIT_RATE * FRAME_TIME_MS / 1000;//
    #elif defined (CONFIG_CHOICE_AAC_ENCODER)
        return -1;//
    #elif defined (CONFIG_CHOICE_G711A_ENCODER)
        return 160;
    #elif defined (CONFIG_CHOICE_G711A_INTERNAL)
        return 320;
    #endif
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

player_pipeline_handle_t player_pipeline_open(void) {
    player_pipeline_handle_t player_pipeline = heap_caps_malloc(sizeof(player_pipeline_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "[ 2 ] Start codec chip");

    assert(player_pipeline != 0);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    player_pipeline->audio_pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_cfg.out_rb_size = 8 * 1024;
    player_pipeline->raw_writer = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, AUDIO_STREAM_WRITER);
    i2s_cfg.type = AUDIO_STREAM_WRITER;
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
    i2s_cfg.need_expand = (16 != 32);
#endif
    i2s_cfg.out_rb_size = 8 * 1024;
    i2s_cfg.buffer_len = 1416;//708
    i2s_stream_set_channel_type(&i2s_cfg, CHANNEL_FORMAT);
    player_pipeline->i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(player_pipeline->i2s_stream_writer, I2S_SAMPLE_RATE, ALGORITHM_STREAM_SAMPLE_BIT, CHANNEL_NUM);

    player_pipeline->rsp = create_resample_stream(CODEC_SAMPLE_RATE, 1, I2S_SAMPLE_RATE, CHANNEL_NUM);
    audio_element_set_output_timeout(player_pipeline->rsp, portMAX_DELAY);
    audio_pipeline_register(player_pipeline->audio_pipeline, player_pipeline->rsp, "rsp");

#ifdef CONFIG_AUDIO_SUPPORT_OPUS_DECODER
    ESP_LOGI(TAG, "[3.3] Create opus decoder");
    raw_opus_dec_cfg_t opus_dec_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    opus_dec_cfg.samp_rate = DEC_SAMPLE_RATE;
    opus_dec_cfg.dec_frame_size = DEC_BIT_RATE * FRAME_TIME_MS / 1000;
    player_pipeline->audio_decoder = raw_opus_decoder_init(&opus_dec_cfg);
#elif CONFIG_AUDIO_SUPPORT_AAC_DECODER
    ESP_LOGI(TAG, "[3.3] Create aac decoder");
    aac_decoder_cfg_t  aac_dec_cfg  = DEFAULT_AAC_DECODER_CONFIG();
    player_pipeline->audio_decoder = aac_decoder_init(&aac_dec_cfg);
#elif CONFIG_AUDIO_SUPPORT_G711A_DECODER
    g711_decoder_cfg_t g711_dec_cfg = DEFAULT_G711_DECODER_CONFIG();
    g711_dec_cfg.out_rb_size = 8 * 1024;
    player_pipeline->audio_decoder = g711_decoder_init(&g711_dec_cfg);
#endif

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(player_pipeline->audio_pipeline, player_pipeline->raw_writer, "raw");
#ifndef CONFIG_CHOICE_G711A_INTERNAL
    audio_pipeline_register(player_pipeline->audio_pipeline, player_pipeline->audio_decoder, "dec");
#endif
    audio_pipeline_register(player_pipeline->audio_pipeline, player_pipeline->i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[3.5] Link it together raw-->audio_decoder-->i2s_stream-->[codec_chip]");
#if defined (CONFIG_CHOICE_G711A_INTERNAL)
    const char *link_tag[] = {"raw", "rsp", "i2s"};
#else
    const char *link_tag[] = {"raw", "dec", "rsp", "i2s"};
#endif
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

    audio_pipeline_unregister(player_pipeline->audio_pipeline, player_pipeline->raw_writer);
    audio_pipeline_unregister(player_pipeline->audio_pipeline, player_pipeline->i2s_stream_writer);
#ifndef CONFIG_CHOICE_G711A_INTERNAL
    audio_pipeline_unregister(player_pipeline->audio_pipeline, player_pipeline->audio_decoder);
#endif

    /* Release all resources */
    audio_pipeline_deinit(player_pipeline->audio_pipeline);
    audio_element_deinit(player_pipeline->raw_writer);
    audio_element_deinit(player_pipeline->i2s_stream_writer);
#ifndef CONFIG_CHOICE_G711A_INTERNAL
    audio_element_deinit(player_pipeline->audio_decoder);
#endif
    heap_caps_free(player_pipeline);
};

int player_pipeline_write(player_pipeline_handle_t player_pipeline, char *buffer, int buf_size){
    raw_stream_write(player_pipeline->raw_writer, buffer, buf_size);
    return 0;
};