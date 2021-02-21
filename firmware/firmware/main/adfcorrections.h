/*
 * adfcorrections.h
 *
 *  Created on: 09.12.2020
 *      Author: Stefan Fuss
 */

#ifndef MAIN_ADFCORRECTIONS_H_
#define MAIN_ADFCORRECTIONS_H_

#include "input_key_service.h"


#define _INPUT_KEY_SERVICE_DEFAULT_CONFIG() {             \
    .based_cfg = {                                       \
        .task_stack = INPUT_KEY_SERVICE_TASK_STACK_SIZE, \
        .task_prio = INPUT_KEY_SERVICE_TASK_PRIORITY,    \
        .task_core = INPUT_KEY_SERVICE_TASK_ON_CORE,     \
		.task_func = NULL,								 \
        .extern_stack = false,                           \
		.service_start = NULL, \
		.service_stop = NULL, \
		.service_destroy = NULL, \
		.service_ioctl = NULL, \
		.service_name = NULL, \
		.user_data = NULL \
    }, \
	.handle = NULL \
}

#define _AUDIO_ELEMENT_INFO_DEFAULT()    { \
    .sample_rates = 44100,                \
    .channels = 2,                        \
    .bits = 16,                           \
    .bps = 0,                             \
    .byte_pos = 0,                        \
    .total_bytes = 0,                     \
    .duration = 0,                        \
    .uri = NULL,                          \
    .codec_fmt = ESP_CODEC_TYPE_UNKNOW,   \
	.reserve_data = { \
			.user_data_0 = 0, \
			.user_data_1 = 0, \
			.user_data_2 = 0, \
			.user_data_3 = 0, \
			.user_data_4 = 0 } \
}

#define _I2S_STREAM_CFG_DEFAULT() {                                              \
    .type = AUDIO_STREAM_WRITER,                                                \
    .i2s_config = {                                                             \
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),                    \
        .sample_rate = 44100,                                                   \
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,                           \
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           \
        .communication_format = I2S_COMM_FORMAT_I2S,                            \
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,          \
        .dma_buf_count = 3,                                                     \
        .dma_buf_len = 300,                                                     \
        .use_apll = true,                                                       \
        .tx_desc_auto_clear = true,                                             \
        .fixed_mclk = 0                                                         \
    },                                                                          \
    .i2s_port = I2S_NUM_0,                                                      \
    .use_alc = false,                                                           \
    .volume = 0,                                                                \
    .out_rb_size = I2S_STREAM_RINGBUFFER_SIZE,                                  \
    .task_stack = I2S_STREAM_TASK_STACK,                                        \
    .task_core = I2S_STREAM_TASK_CORE,                                          \
    .task_prio = I2S_STREAM_TASK_PRIO,                                          \
    .stack_in_ext = false,                                                      \
    .multi_out_num = 0,                                                         \
    .uninstall_drv = true,                                                      \
}

#endif /* MAIN_ADFCORRECTIONS_H_ */
