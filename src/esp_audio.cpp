#include "esp_audio.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "driver/i2s.h"

#include "pins.h"

#ifndef BAILEY_AUDIO_PA_ACTIVE_LOW
#define BAILEY_AUDIO_PA_ACTIVE_LOW 0
#endif
#ifndef BAILEY_AUDIO_TEST_TONE
#define BAILEY_AUDIO_TEST_TONE 0
#endif

namespace bailey {

namespace {

// ---- ES8311 minimal init -----------------------------------------------
// Talks to the ES8311 codec at I2C address 0x18 (the Waveshare board's
// ES8311 is hardwired to that address). Writes the register sequence
// needed for 16 kHz / 16-bit stereo I2S playback through the speaker
// output. Transcribed from public ESP-IDF examples; not all registers
// strictly required but kept for clarity.

constexpr uint8_t kEs8311Addr = 0x18;

bool reg_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(kEs8311Addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// Read back a single register so we can verify writes actually stuck
// (handy when debugging "no sound" issues -- the audit prints a
// before/after register dump after init).
uint8_t reg_read(uint8_t reg) {
  Wire.beginTransmission(kEs8311Addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom((int)kEs8311Addr, 1) < 1) return 0xFF;
  return Wire.read();
}

bool es8311_init() {
  // Probe the codec first.
  Wire.beginTransmission(kEs8311Addr);
  if (Wire.endTransmission() != 0) {
    Serial.println("[audio] ES8311 not detected on I2C 0x18");
    return false;
  }

  // Reset.
  if (!reg_write(0x00, 0x1F)) return false;
  delay(20);
  reg_write(0x00, 0x00);

  // Clock manager.
  reg_write(0x01, 0x30);   // CLK_MANAGER1: enable digital
  reg_write(0x02, 0x10);   // CLK_MANAGER2: mclk divider = 1
  reg_write(0x03, 0x10);   // ADC OSR
  reg_write(0x04, 0x10);   // DAC OSR
  reg_write(0x05, 0x00);   // ADC/DAC clock divider
  reg_write(0x06, 0x03);   // BCLK + LRCK config
  reg_write(0x07, 0x00);
  reg_write(0x08, 0xFF);

  // Serial data port: I2S, 16-bit.
  reg_write(0x09, 0x0C);
  reg_write(0x0A, 0x0C);
  reg_write(0x0B, 0x00);
  reg_write(0x0C, 0x00);

  // System power.
  reg_write(0x0D, 0x01);
  reg_write(0x0E, 0x02);
  reg_write(0x0F, 0x44);
  reg_write(0x10, 0x1F);
  reg_write(0x11, 0xFC);
  reg_write(0x12, 0x00);

  // ADC settings (unused by us but configured to a sane default).
  reg_write(0x14, 0x1A);

  // Extra DAC enable / mixer / unmute sweep -- the Espressif reference
  // driver writes these even though some are no-ops on power-on; we
  // include them defensively because "init OK but no sound" usually
  // points to a missing unmute or mixer-route register.
  reg_write(0x42, 0x00);   // DAC mixer route (audio -> output)
  reg_write(0x37, 0x08);   // DAC unmute via alt mute bit
  reg_write(0x32, 0xBF);   // DAC volume ~= -8 dB (re-stamped post-unmute)
  reg_write(0x44, 0x18);   // DAC route enable + bit-7 power
  reg_write(0x45, 0x00);

  // Let the codec settle before the first I2S write races in.
  delay(50);

  // Read back a handful of key registers so the serial log shows
  // whether the writes actually landed. If any read returns 0xFF on
  // every register, the I2C bus is wedged.
  Serial.printf("[audio] ES8311 readback: 0x00=%02X 0x01=%02X "
                "0x06=%02X 0x09=%02X 0x0D=%02X 0x10=%02X 0x11=%02X "
                "0x32=%02X 0x37=%02X 0x44=%02X\n",
                reg_read(0x00), reg_read(0x01), reg_read(0x06),
                reg_read(0x09), reg_read(0x0D), reg_read(0x10),
                reg_read(0x11), reg_read(0x32), reg_read(0x37),
                reg_read(0x44));

  Serial.println("[audio] ES8311 init OK");
  return true;
}

// ---- I2S DMA -----------------------------------------------------------
// Uses the legacy driver/i2s.h ESP-IDF API so we don't depend on
// arduino-esp32 3.x's ESP_I2S.h wrapper (PlatformIO's default
// espressif32 platform still ships arduino-esp32 2.x where that header
// is absent). driver/i2s.h exists in both lineages.
constexpr i2s_port_t kI2sPort  = I2S_NUM_0;
constexpr uint32_t   kSrTarget = 16000;

bool i2s_init() {
  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate          = kSrTarget;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = 0;
  cfg.dma_buf_count        = 6;
  cfg.dma_buf_len          = 256;
  // APLL on ESP32-S3 can refuse to lock cleanly at 16 kHz / fixed_mclk
  // combinations; fall back to PLL_F160M which is solid at this rate
  // and inaudibly close to APLL for a small speaker.
  cfg.use_apll             = false;
  cfg.tx_desc_auto_clear   = true;
  cfg.fixed_mclk           = (int)kSrTarget * 256;  // 4.096 MHz on MCLK pin

  if (i2s_driver_install(kI2sPort, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[audio] i2s_driver_install failed");
    return false;
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num   = PIN_AUDIO_I2S_MCLK;
  pins.bck_io_num   = PIN_AUDIO_I2S_BCLK;
  pins.ws_io_num    = PIN_AUDIO_I2S_LRC;
  pins.data_out_num = PIN_AUDIO_I2S_DOUT;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(kI2sPort, &pins) != ESP_OK) {
    Serial.println("[audio] i2s_set_pin failed");
    return false;
  }
  // Pre-fill 100 ms of silence so the first real i2s_write doesn't
  // race against an empty DMA queue (some codecs latch garbage on the
  // first cycle and stay muted).
  constexpr int kSilenceFrames = kSrTarget / 10;  // 100 ms
  int16_t zero[256 * 2] = {};
  int written_total = 0;
  while (written_total < kSilenceFrames) {
    size_t w = 0;
    i2s_write(kI2sPort, zero, sizeof(zero), &w, portMAX_DELAY);
    written_total += (int)(w / (2 * sizeof(int16_t)));
  }
  return true;
}

#if BAILEY_AUDIO_TEST_TONE
// 440 Hz sine for 2 seconds. Useful smoke for the I2S + codec + PA
// chain when no game-side sound is heard.
void play_test_tone() {
  constexpr int kDurFrames = kSrTarget * 2;
  constexpr int kChunkFrames = 256;
  int16_t chunk[kChunkFrames * 2];
  for (int pos = 0; pos < kDurFrames; ) {
    int frames = kDurFrames - pos;
    if (frames > kChunkFrames) frames = kChunkFrames;
    for (int i = 0; i < frames; ++i) {
      float t = (float)(pos + i) / (float)kSrTarget;
      int16_t v = (int16_t)(sinf(2.0f * 3.14159265f * 440.0f * t) * 12000);
      chunk[i * 2 + 0] = v;
      chunk[i * 2 + 1] = v;
    }
    size_t w = 0;
    i2s_write(kI2sPort, chunk, frames * 2 * sizeof(int16_t),
              &w, portMAX_DELAY);
    pos += frames;
  }
}
#endif

// Linear-resample mono int16 22050 Hz -> stereo int16 16000 Hz with
// per-sample volume scaling.
void play_pcm(const int16_t* src, int n_src, uint8_t volume) {
  if (!src || n_src <= 0) return;
  const float ratio = (float)tama::kAudioSampleRate / (float)kSrTarget;
  const int n_dst = (int)(n_src / ratio);
  const int vol_num = (int)volume;
  // Stream in chunks to avoid a huge intermediate buffer.
  constexpr int kChunkFrames = 256;
  int16_t chunk[kChunkFrames * 2];   // stereo
  for (int dst_pos = 0; dst_pos < n_dst; ) {
    int frames = n_dst - dst_pos;
    if (frames > kChunkFrames) frames = kChunkFrames;
    for (int i = 0; i < frames; ++i) {
      int src_pos = (int)((dst_pos + i) * ratio);
      if (src_pos >= n_src) src_pos = n_src - 1;
      int32_t s = ((int32_t)src[src_pos] * vol_num) / 100;
      if (s >  32767) s =  32767;
      if (s < -32768) s = -32768;
      int16_t v = (int16_t)s;
      chunk[i * 2 + 0] = v;   // L
      chunk[i * 2 + 1] = v;   // R
    }
    size_t written = 0;
    i2s_write(kI2sPort, chunk, frames * 2 * sizeof(int16_t),
              &written, portMAX_DELAY);
    dst_pos += frames;
  }
}

const char* clip_name(tama::ClipId c) {
  switch (c) {
    case tama::ClipId::Yip:       return "Yip";
    case tama::ClipId::Wuff:      return "Wuff";
    case tama::ClipId::Splash:    return "Splash";
    case tama::ClipId::Heart:     return "Heart";
    case tama::ClipId::Snore:     return "Snore";
    case tama::ClipId::Whimper:   return "Whimper";
    case tama::ClipId::Sneeze:    return "Sneeze";
    case tama::ClipId::Fanfare:   return "Fanfare";
    case tama::ClipId::Achieve:   return "Achieve";
    case tama::ClipId::Sad:       return "Sad";
    case tama::ClipId::Birds:     return "Birds";
    case tama::ClipId::Waves:     return "Waves";
    case tama::ClipId::Wind:      return "Wind";
    case tama::ClipId::Thunder:   return "Thunder";
    case tama::ClipId::SnoreLoud: return "SnoreLoud";
    default:                      return "?";
  }
}

}  // namespace

bool EspSpeaker::begin() {
  pinMode(PIN_AUDIO_PA_CTRL, OUTPUT);
  // PA enable polarity is board-specific. Default is active-HIGH
  // (the most common); flip with -D BAILEY_AUDIO_PA_ACTIVE_LOW=1
  // if your board's amp enable is wired the other way.
#if BAILEY_AUDIO_PA_ACTIVE_LOW
  digitalWrite(PIN_AUDIO_PA_CTRL, LOW);
  Serial.println("[audio] PA polarity = active-LOW");
#else
  digitalWrite(PIN_AUDIO_PA_CTRL, HIGH);
  Serial.println("[audio] PA polarity = active-HIGH");
#endif
  Wire.begin(PIN_AUDIO_I2C_SDA, PIN_AUDIO_I2C_SCL, 400000);
  bool ok_codec = es8311_init();
  bool ok_i2s   = i2s_init();
  ok_ = ok_codec && ok_i2s;
  if (!ok_) {
    Serial.println("[audio] using log-only fallback");
    return ok_;
  }
#if BAILEY_AUDIO_TEST_TONE
  Serial.println("[audio] playing 2 s test tone (440 Hz)");
  play_test_tone();
  Serial.println("[audio] test tone done");
#endif
  return ok_;
}

void EspSpeaker::playClip(tama::ClipId clip, uint8_t volume) {
  if (!ok_) {
    Serial.printf("[audio] %s @ vol %u (stub)\n", clip_name(clip), volume);
    return;
  }
  int n = 0;
  const int16_t* pcm = tama::pcm_clip(clip, n);
  if (!pcm || n <= 0) return;
  play_pcm(pcm, n, volume);
}

}  // namespace bailey
