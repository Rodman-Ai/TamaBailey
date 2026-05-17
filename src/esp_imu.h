#pragma once

#include <cstdint>

#include "tama/game.h"

namespace bailey {

// QMI8658 6-axis IMU adapter. Polls accel at ~50 Hz, detects shake /
// forward-flick / step gestures and enqueues matching tama::Input
// events on the Game.
//
// Hardware: shares the I2C bus with the audio codec + touch panel
// (SDA=42, SCL=41). IRQ on GPIO 6 is currently unused; we just poll.
class EspImu {
 public:
  // Returns true if the chip was detected and configured.
  bool begin();
  bool ok() const { return ok_; }
  // Call from the main loop; gated internally to ~50 Hz.
  void poll(uint32_t now_ms, tama::Game& game);

 private:
  bool     ok_           = false;
  uint32_t last_poll_ms_ = 0;
  // Recent X-axis samples for shake detection (sign-flip counter).
  int8_t   recent_sign_[8]   = {0};
  uint8_t  recent_idx_       = 0;
  uint32_t last_shake_ms_    = 0;
  uint32_t last_flick_ms_    = 0;
  uint32_t last_step_ms_     = 0;
  bool     step_high_        = false;   // tiny state for step pulse edge
};

}  // namespace bailey
