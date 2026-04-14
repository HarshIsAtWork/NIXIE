#include "audio_processor.h"
#include "config.h"
#include <driver/i2s.h>

static const i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = (i2s_bits_per_sample_t)I2S_SAMPLE_BITS,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = 0,
  .dma_buf_count = 2,
  .dma_buf_len = 1024,
  .use_apll = false,
  .tx_desc_auto_clear = false,
  .fixed_mclk = 0
};

static const i2s_pin_config_t i2s_pins = {
  .bck_io_num = I2S_SCK_PIN,
  .ws_io_num = I2S_WS_PIN,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num = I2S_SD_PIN
};

static int16_t* audioBuffer = nullptr;
static int audioBufferIndex = 0;
static bool isBuffering = false;

bool audioInit() {
  esp_err_t err = i2s_driver_install((i2s_port_t)I2S_PORT, &i2s_config, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("i2s_driver_install fail %d\n", err);
    return false;
  }

  err = i2s_set_pin((i2s_port_t)I2S_PORT, &i2s_pins);
  if (err != ESP_OK) {
    Serial.printf("i2s_set_pin fail %d\n", err);
    return false;
  }

  i2s_zero_dma_buffer((i2s_port_t)I2S_PORT);

  audioBuffer = (int16_t*)malloc(MAX_AUDIO_SAMPLES * sizeof(int16_t));
  if (!audioBuffer) {
    Serial.println("Failed to allocate audio buffer");
    return false;
  }

  return true;
}

bool audioReadAndCompute(float &avgAbs, int &level, int &bars) {
  const int bufBytes = 1024;
  static uint8_t buf[bufBytes];
  size_t bytesRead = 0;
  esp_err_t err = i2s_read((i2s_port_t)I2S_PORT, buf, bufBytes, &bytesRead, portMAX_DELAY);
  if (err != ESP_OK || bytesRead == 0) {
    return false;
  }

  int sampleCount = bytesRead / 4;
  long long sumAbs = 0;
  int32_t *samples = (int32_t *)buf;

  for (int i = 0; i < sampleCount; i++) {
    int32_t sample32 = samples[i];
    int16_t sample16 = (int16_t)(sample32 >> 16);

    if (isBuffering && audioBufferIndex < MAX_AUDIO_SAMPLES) {
      audioBuffer[audioBufferIndex++] = sample16;
    }

    sumAbs += llabs(sample16);
  }

  avgAbs = sampleCount > 0 ? (float)sumAbs / sampleCount : 0;
  level = constrain(map((int)avgAbs, 0, 200000, 0, 8), 0, 8);
  bars = map((int)avgAbs, 0, 200000, 0, 50);
  return true;
}

int16_t* audioGetBuffer() {
  return audioBuffer;
}

int audioGetBufferSampleCount() {
  return audioBufferIndex;
}

void audioStartBuffering() {
  audioBufferIndex = 0;
  isBuffering = true;
  Serial.println("Audio buffering started");
}

void audioStopBuffering() {
  isBuffering = false;
  Serial.printf("Audio buffering stopped: %d samples captured\n", audioBufferIndex);
}
