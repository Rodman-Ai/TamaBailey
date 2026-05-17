#include "esp_mic.h"

#include <Arduino.h>
#include "driver/i2s.h"

#include "pins.h"

namespace bailey {

namespace {

constexpr i2s_port_t kI2sRxPort  = I2S_NUM_1;
constexpr uint32_t   kSrTarget   = 16000;
constexpr int        kFrameSamples = 256;   // 16 ms at 16 kHz

bool i2s_rx_init() {
  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate          = kSrTarget;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = 0;
  cfg.dma_buf_count        = 4;
  cfg.dma_buf_len          = kFrameSamples;
  cfg.use_apll             = false;
  cfg.tx_desc_auto_clear   = false;
  cfg.fixed_mclk           = 0;
  if (i2s_driver_install(kI2sRxPort, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[mic] i2s_driver_install (RX) failed");
    return false;
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num   = I2S_PIN_NO_CHANGE;          // share TX clocks
  pins.bck_io_num   = PIN_AUDIO_I2S_BCLK;
  pins.ws_io_num    = PIN_AUDIO_I2S_LRC;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num  = PIN_AUDIO_I2S_DIN;
  if (i2s_set_pin(kI2sRxPort, &pins) != ESP_OK) {
    Serial.println("[mic] i2s_set_pin (RX) failed");
    return false;
  }
  return true;
}

// Compute simple short-time energy + zero-crossing rate over a frame.
void frame_features(const int16_t* x, int n,
                    uint16_t& energy_out, uint16_t& zcr_out) {
  uint32_t acc = 0;
  int      zcr = 0;
  int16_t  prev = 0;
  for (int i = 0; i < n; ++i) {
    int v = x[i];
    acc += (uint32_t)(v < 0 ? -v : v);
    if (i > 0 && ((prev < 0) != (v < 0))) ++zcr;
    prev = v;
  }
  uint32_t mag = acc / (uint32_t)n;
  if (mag > 0xFFFF) mag = 0xFFFF;
  energy_out = (uint16_t)mag;
  zcr_out    = (uint16_t)zcr;
}

}  // namespace

bool EspMic::begin() {
  if (!i2s_rx_init()) { ok_ = false; return false; }
  ok_ = true;
  Serial.println("[mic] I2S RX initialized");
  return true;
}

void EspMic::poll(uint32_t now_ms, tama::Game& game) {
  if (!ok_) return;
  if (now_ms - last_poll_ms_ < 16) return;
  last_poll_ms_ = now_ms;

  int16_t buf[kFrameSamples];
  size_t  bytes_read = 0;
  if (i2s_read(kI2sRxPort, buf, sizeof(buf), &bytes_read, 0) != ESP_OK) return;
  if (bytes_read != sizeof(buf)) return;   // no new full frame

  uint16_t energy, zcr;
  frame_features(buf, kFrameSamples, energy, zcr);

  // Adapt the noise floor slowly downward when quiet, upward only when
  // really sustained background noise rises.
  if (!voiced_ && energy < (noise_floor_q15_ * 2)) {
    noise_floor_q15_ = (noise_floor_q15_ * 31 + energy) / 32;
    if (noise_floor_q15_ < 32) noise_floor_q15_ = 32;
  }

  bool is_voice = (energy > (noise_floor_q15_ * 3));
  if (is_voice) {
    if (!voiced_) {
      voiced_ = true;
      frame_count_ = 0;
      utterance_started_ms_ = now_ms;
    }
    if (frame_count_ < kMaxFrames) {
      frame_energy_[frame_count_] = energy;
      frame_zcr_[frame_count_]    = zcr;
      ++frame_count_;
    }
    silence_streak_ = 0;
  } else if (voiced_) {
    if (frame_count_ < kMaxFrames) {
      frame_energy_[frame_count_] = energy;
      frame_zcr_[frame_count_]    = zcr;
      ++frame_count_;
    }
    if (++silence_streak_ >= 4) {
      voiced_ = false;
      silence_streak_ = 0;
      // Cooldown so a single utterance doesn't dispatch twice.
      if (now_ms - last_dispatch_ms_ > 900) {
        classify_and_dispatch(game);
        last_dispatch_ms_ = now_ms;
      }
      frame_count_ = 0;
    }
  }
}

void EspMic::classify_and_dispatch(tama::Game& game) {
  if (frame_count_ < 6) return;   // too short to be a phrase

  // Find peaks: local maxima above 1.5x noise floor with min 6-frame
  // spacing (~96 ms).
  uint32_t thr = noise_floor_q15_ * 2;
  int syllables = 0;
  int last_peak = -8;
  uint32_t total_energy = 0;
  for (int i = 1; i + 1 < frame_count_; ++i) {
    uint16_t e = frame_energy_[i];
    total_energy += e;
    if (e > thr && e >= frame_energy_[i - 1] && e >= frame_energy_[i + 1]) {
      if (i - last_peak >= 6) { ++syllables; last_peak = i; }
    }
  }
  if (syllables == 0) syllables = 1;

  int duration_ms = frame_count_ * 16;
  // End slope: change in energy over the last 100 ms (6 frames).
  int slope_end = 0;
  if (frame_count_ >= 8) {
    int a = (int)frame_energy_[frame_count_ - 7];
    int b = (int)frame_energy_[frame_count_ - 1];
    slope_end = b - a;
  }

  tama::Input chosen = tama::Input::VoiceSit;   // default fallback
  if (syllables >= 4 || duration_ms >= 800) {
    // 4-syllable bucket: High five / Roll over.
    // Fricative tail ('v') -> slope flat or slightly positive at end.
    chosen = (slope_end < -200) ? tama::Input::VoiceHighFive
                                : tama::Input::VoiceRollOver;
  } else {
    // 3-syllable bucket: Sit / Come / Jump.
    if (slope_end < -300)       chosen = tama::Input::VoiceJump;   // 'p' burst
    else if (total_energy >
             (uint32_t)frame_count_ * thr * 2u) {
                                chosen = tama::Input::VoiceCome;   // louder
    } else                      chosen = tama::Input::VoiceSit;
  }

  Serial.printf("[mic] utterance: %d syll, %d ms, slope %d -> %s\n",
                syllables, duration_ms, slope_end,
                chosen == tama::Input::VoiceSit       ? "Sit" :
                chosen == tama::Input::VoiceCome      ? "Come" :
                chosen == tama::Input::VoiceJump      ? "Jump" :
                chosen == tama::Input::VoiceHighFive  ? "HighFive" :
                                                        "RollOver");
  game.enqueue(chosen);
}

}  // namespace bailey
