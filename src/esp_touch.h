#pragma once

#include <cstdint>

#include "tama/game.h"

namespace bailey {

// CST816-family capacitive touch panel adapter. Polls the chip at ~60 Hz
// and translates tap / drag gestures into tama::Input events:
//   tap on stats bar -> Input::MenuToggle
//   tap on pet       -> Input::PetTap
//   drag on pet      -> Input::Stroke (rate-limited internally by Game)
//
// Hardware: shares the I2C bus with the IMU (QMI8658) and audio codec
// (ES8311). RST + IRQ on pins.h::PIN_TOUCH_RST/PIN_TOUCH_IRQ; we poll
// rather than use the interrupt.
class EspTouch {
 public:
  bool begin();
  bool ok() const { return ok_; }
  void poll(uint32_t now_ms, tama::Game& game);

 private:
  bool      ok_              = false;
  uint32_t  last_poll_ms_    = 0;
  bool      was_touching_    = false;
  uint32_t  touch_start_ms_  = 0;
  int       touch_start_x_   = 0;
  int       touch_start_y_   = 0;
  int       last_x_          = 0;
  int       last_y_          = 0;
};

}  // namespace bailey
