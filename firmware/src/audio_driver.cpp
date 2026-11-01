#include "audio_driver.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <freertos/stream_buffer.h>
#include <es7210.h>
extern "C" {
#include <es8311.h>
}

static constexpr i2s_port_t kPort = I2S_NUM_0;
static constexpr int kBlock = 160;  // 10 ms
static StreamBufferHandle_t micStream;
static cc::App* gApp;
static volatile uint32_t lastPlayMs = 0;

static bool initCodecs() {
  es8311_handle_t es = es8311_create(0, ES8311_ADDRRES_0);
  if (!es) return false;
  const es8311_clock_config_t clk = {false, false, true, cc::kSampleRate * 256, cc::kSampleRate};
  if (es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
  if (es8311_sample_frequency_config(es, clk.mclk_frequency, clk.sample_frequency) != ESP_OK) return false;
  // Codec stays at a fixed level: volume is applied in the mixer so fades and per-sound gains stay consistent.
  if (es8311_voice_volume_set(es, 85, nullptr) != ESP_OK) return false;

  audio_hal_codec_config_t cfg = {};
  cfg.adc_input = AUDIO_HAL_ADC_INPUT_ALL;
  cfg.codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE;
  cfg.i2s_iface.mode = AUDIO_HAL_MODE_SLAVE;
  cfg.i2s_iface.fmt = AUDIO_HAL_I2S_NORMAL;
  cfg.i2s_iface.samples = AUDIO_HAL_16K_SAMPLES;
  cfg.i2s_iface.bits = AUDIO_HAL_BIT_LENGTH_16BITS;
  esp_err_t e = es7210_adc_init(&Wire, &cfg);
  e |= es7210_adc_config_i2s(cfg.codec_mode, &cfg.i2s_iface);
  // The official example uses 0 dB here; 24 dB is a starting point, tune it on hardware (see HARDWARE.md).
  e |= es7210_adc_set_gain((es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2), GAIN_24DB);
  e |= es7210_adc_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);
  return e == ESP_OK;
}

void AudioDriver::setMicEnabled(bool on) {
  if (on == micOn_) return;
  micOn_ = on;
  // Powers the ADC down: off means the microphones are really not sampled.
  es7210_adc_ctrl_state(AUDIO_HAL_CODEC_MODE_ENCODE, on ? AUDIO_HAL_CTRL_START : AUDIO_HAL_CTRL_STOP);
}

void AudioDriver::task(void*) {
  static int16_t mono[kBlock], stereo[kBlock * 2], in[kBlock * 2];
  bool paOn = false;
  for (;;) {
    gApp->mix(mono, kBlock);
    bool any = false;
    for (int i = 0; i < kBlock; i++) {
      stereo[2 * i] = stereo[2 * i + 1] = mono[i];
      if (mono[i]) any = true;
    }
    uint32_t now = millis();
    if (any) {
      lastPlayMs = now;
      if (!paOn) { digitalWrite(PIN_PA_CTRL, HIGH); paOn = true; }
    } else if (paOn && now - lastPlayMs > 3000) {  // no idle hiss or wasted power
      digitalWrite(PIN_PA_CTRL, LOW);
      paOn = false;
    }
    size_t n;
    i2s_write(kPort, stereo, sizeof(stereo), &n, portMAX_DELAY);  // paces this task
    if (i2s_read(kPort, in, sizeof(in), &n, 0) == ESP_OK && n) {
      int frames = (int)(n / 4);
      for (int i = 0; i < frames; i++) in[i] = (int16_t)(((int32_t)in[2 * i] + in[2 * i + 1]) / 2);
      xStreamBufferSend(micStream, in, (size_t)frames * 2, 0);  // dropping beats blocking playback
    }
  }
}

bool AudioDriver::begin(cc::App& app) {
  gApp = &app;
  pinMode(PIN_PA_CTRL, OUTPUT);
  digitalWrite(PIN_PA_CTRL, LOW);
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = cc::kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = kBlock;
  cfg.tx_desc_auto_clear = true;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  cfg.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;
  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = PIN_I2S_DIN;
  if (i2s_set_pin(kPort, &pins) != ESP_OK) return false;
  i2s_zero_dma_buffer(kPort);
  bool ok = initCodecs();  // the codecs need MCLK running first
  app.hal.codecOk = ok;
  app.hal.micOk = ok;
  micStream = xStreamBufferCreate(8192, 2);
  if (!micStream) return false;
  xTaskCreatePinnedToCore(task, "audio", 4096, this, 6, nullptr, 0);
  return ok;
}

void AudioDriver::setRecordSink(int16_t* buf, size_t cap) {
  record_ = buf;
  recordCap_ = cap;
  recorded_ = 0;
}

void AudioDriver::pollMic(cc::App& app) {
  static int16_t buf[320];
  for (;;) {
    size_t n = xStreamBufferReceive(micStream, buf, sizeof(buf), 0) / 2;
    if (!n) return;
    if (record_ && recorded_ < recordCap_) {
      size_t take = n < recordCap_ - recorded_ ? n : recordCap_ - recorded_;
      memcpy(record_ + recorded_, buf, take * 2);
      recorded_ += take;
    }
    if (micOn_ && !app.audio.isPlaying() && millis() - lastPlayMs > 250) app.micInput(buf, (int)n);
  }
}
