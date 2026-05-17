#pragma once

#include "tama/audio.h"

namespace bailey {

// Real ES8311 + I2S DAC backend for the Waveshare board. If begin() fails
// (codec missing or I2C error), playClip() falls back to a Serial log so
// the rest of the firmware keeps working.
class EspSpeaker final : public tama::Speaker {
 public:
  bool begin();
  void playClip(tama::ClipId clip, uint8_t volume = 100) override;
  bool available() const override { return ok_; }

 private:
  bool ok_ = false;
};

}  // namespace bailey
