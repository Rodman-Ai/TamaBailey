#pragma once

#include "tama/audio.h"

namespace bailey {

// Phase 1 stub: logs each clip to Serial. The board's ES8311 codec
// integration lands in a follow-up; gameplay code already calls into
// this interface, so swapping in a real I2S/codec driver later is a
// drop-in replacement.
class EspSpeaker final : public tama::Speaker {
 public:
  void playClip(tama::ClipId clip, uint8_t volume = 100) override;
  bool available() const override { return false; }
};

}  // namespace bailey
