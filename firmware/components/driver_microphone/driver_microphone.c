#include <sdkconfig.h>

#include <stdint.h>
#include <string.h>

#include <esp_log.h>
#include <esp_err.h>

#include "include/driver_microphone.h"
#include "include/driver_microphone_internal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_sleep.h"

#include "opus.h"

#ifdef CONFIG_DRIVER_MICROPHONE_ENABLE

#define TAG "microphone"

#define READ_LEN (1024 * 64)

static struct {
  mic_sampling_rate rate;
  mic_encoding encoding;
  uint32_t frame_size;
  volatile int running;  // Set to 1 when initialized, set to 0 when the recording task should stop
  volatile int stopped;  // Set to 0 when initialized, set to 1 when the recording task has stopped
} mic_state = {MIC_SAMP_RATE_8_KHZ, MIC_ENCODING_PCM_8_BIT, 0, 0, 0};

uint32_t driver_microphone_get_sampling_rate() {
  switch (mic_state.rate) {
    case MIC_SAMP_RATE_8_KHZ:
      return 8000;
    case MIC_SAMP_RATE_12_KHZ:
      return 12000;
    case MIC_SAMP_RATE_16_KHZ:
      return 16000;
    case MIC_SAMP_RATE_24_KHZ:
      return 24000;
    case MIC_SAMP_RATE_48_KHZ:
      return 48000;
    default:
      return 0;
  }
}

static void ICS41350_record_task(void *arg) {
  uint8_t sample_size = 2;
  if (mic_state.encoding == MIC_ENCODING_PCM_8_BIT) {
    sample_size = 1;
  }

  uint32_t buffer_size = sample_size * mic_state.frame_size;

  void *buffer = malloc(buffer_size);
  if (!buffer) {
    ESP_LOGE(TAG, "MALLOC FAILED");
    return;
  }

  OpusEncoder *opus_encoder = NULL;
  void *opus_buffer         = NULL;

  if (mic_state.encoding == MIC_ENCODING_OPUS) {
    int err = 0;
    opus_encoder =
        opus_encoder_create(driver_microphone_get_sampling_rate(), 1, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK) {
      ESP_LOGE(TAG, "Failed to create opus encoder");
      free(buffer);
      return;
    }
    opus_buffer = malloc(mic_state.frame_size);
    if (!opus_buffer) {
      ESP_LOGE(TAG, "Failed to allocate opus buffer");
      free(buffer);
      opus_encoder_destroy(opus_encoder);
      return;
    }
  }

  while (mic_state.running) {
    size_t read = 0;
    i2s_read(CONFIG_DRIVER_MICROPHONE_I2S_NUM, (char *)buffer, buffer_size, &read, portMAX_DELAY);
    if (mic_state.encoding == MIC_ENCODING_OPUS) {
      for (size_t i = 0; i < read; i++) {
        ((uint16_t *)buffer)[i] ^= 0x8000;
      }
      int ret = opus_encode(opus_encoder, buffer, mic_state.frame_size, opus_buffer,
                            mic_state.frame_size);
      if (ret > 0) {
        driver_microphone_ring_buffer_put(mic_state.encoding, opus_buffer, ret);
      }
    } else {
      driver_microphone_ring_buffer_put(mic_state.encoding, buffer, read);
    }
  }

  if (mic_state.encoding == MIC_ENCODING_OPUS) {
    free(opus_buffer);
    opus_encoder_destroy(opus_encoder);
  }
  free(buffer);
  mic_state.stopped = 1;
}

esp_err_t driver_microphone_init(mic_sampling_rate rate, mic_encoding encoding, uint16_t frame_size,
                                 uint8_t frame_backlog) {
  if (mic_state.running || !mic_state.stopped) {
    return ESP_ERR_INVALID_STATE;
  }
  ESP_LOGD(TAG, "init called");

  mic_state.rate = rate;

  i2s_config_t i2s_config = {.mode                 = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
                             .sample_rate          = driver_microphone_get_sampling_rate(),
                             .bits_per_sample      = encoding == MIC_ENCODING_PCM_8_BIT ? 8 : 16,
                             .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
                             .communication_format = I2S_COMM_FORMAT_PCM,
                             .dma_buf_count        = 2,
                             .dma_buf_len          = 8,
                             .use_apll             = 0,
                             .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1};

  i2s_pin_config_t pin_config = {
      .ws_io_num   = 25,
      .data_in_num = 35,
  };

  // install and start i2s driver
  i2s_driver_install(CONFIG_DRIVER_MICROPHONE_I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(CONFIG_DRIVER_MICROPHONE_I2S_NUM, &pin_config);
  i2s_set_clk(CONFIG_DRIVER_MICROPHONE_I2S_NUM, driver_microphone_get_sampling_rate(), 16,
              I2S_CHANNEL_MONO);

  xTaskCreate(ICS41350_record_task, "ICS41350_record_task", 1024 * 2, NULL, 5, NULL);

  ESP_LOGD(TAG, "init done");
  return ESP_OK;
}

#endif
