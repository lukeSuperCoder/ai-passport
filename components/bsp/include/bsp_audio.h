// components/bsp/include/bsp_audio.h
// ES8311 音频 codec:I2C 走控制口(复用 bsp_i2c 的共享总线),I2S 走全双工数据口。
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// 初始化 codec 与 I2S 全双工通道。硬件验证页需要录音时使用。
esp_err_t bsp_audio_init(void);

// 仅初始化播放通道。正式应用不需要麦克风时使用，可省去 RX DMA。
// 两种初始化接口都是幂等的；首次成功初始化选定本次启动的模式。
esp_err_t bsp_audio_init_playback(void);

// 设置采样格式。同格式重复调用是廉价的(直接复用已打开的 codec)。
//
// ⚠ 这里有个必须绕开的坑:esp_codec_dev_open() 在 codec【已打开】时会直接返回 OK 且
//   【不重新配置采样率】。若不先 close,16kHz 播完再播 8kHz 会以 16k 时钟送出 ——
//   音调和速度都快一倍。故本函数在格式变化时先 close 再 open。
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t ch);

// 播放 / 录音。bytes 为字节数(16bit 单声道时 = 采样数 x 2)。
// 仅播放模式下 bsp_audio_read() 返回 ESP_ERR_NOT_SUPPORTED。
esp_err_t bsp_audio_write(const void *pcm, size_t bytes);
esp_err_t bsp_audio_read(void *pcm, size_t bytes);

// 输出音量 0..100(%)。
void bsp_audio_set_volume(uint8_t percent);
