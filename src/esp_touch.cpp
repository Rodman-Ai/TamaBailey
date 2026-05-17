#include "esp_touch.h"

#include <Arduino.h>
#include <Wire.h>
#include <TouchDrvCSTXXX.hpp>

#include "pins.h"
#include "tama/ui.h"

namespace bailey {

namespace {
TouchDrvCSTXXX g_touch;
constexpr uint32_t kTapMaxMs       = 400;
constexpr int      kTapMaxMovePx   =   8;
constexpr int      kStrokeMovePx   =   4;
}

bool EspTouch::begin() {
  // Make sure the I2C bus is up. It's idempotent if EspImu or EspSpeaker
  // already brought it online.
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);

  // RST pulse: bring the CST816 up cleanly regardless of boot state.
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(10);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(50);

  g_touch.setPins(PIN_TOUCH_RST, PIN_TOUCH_IRQ);
  if (!g_touch.begin(Wire, CST816_SLAVE_ADDRESS,
                     PIN_TOUCH_SDA, PIN_TOUCH_SCL)) {
    Serial.println("[touch] CST816 not detected -- touch input disabled");
    ok_ = false;
    return false;
  }
  ok_ = true;
  Serial.printf("[touch] CST816 detected, chip id 0x%02X\n",
                g_touch.getChipID());
  return true;
}

void EspTouch::poll(uint32_t now_ms, tama::Game& game) {
  if (!ok_) return;
  if (now_ms - last_poll_ms_ < 16) return;  // ~60 Hz
  last_poll_ms_ = now_ms;

  int16_t xs[5], ys[5];
  uint8_t n = g_touch.getPoint(xs, ys, 1);
  bool touching = (n > 0);
  int  x = touching ? (int)xs[0] : last_x_;
  int  y = touching ? (int)ys[0] : last_y_;

  if (touching && !was_touching_) {
    // Touch DOWN -- record start.
    touch_start_ms_ = now_ms;
    touch_start_x_  = x;
    touch_start_y_  = y;
    last_x_ = x;
    last_y_ = y;
  } else if (!touching && was_touching_) {
    // Touch UP -- check for chrome-swipe first, then fall back to tap.
    int dx = last_x_ - touch_start_x_;
    int dy = last_y_ - touch_start_y_;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    constexpr int kSwipeMinPx  = 40;
    constexpr int kTopBandPx   = 60;        // upper trigger band
    constexpr int kBotBandPx   = 180;       // lower trigger band (y >= ...)
    bool consumed = false;
    if (ady >= kSwipeMinPx && ady > adx) {
      bool top_band    = touch_start_y_ <  kTopBandPx;
      bool bottom_band = touch_start_y_ >= kBotBandPx;
      if (top_band && dy < 0) {
        game.enqueue(tama::Input::HideChrome); consumed = true;
      } else if (top_band && dy > 0) {
        game.enqueue(tama::Input::ShowChrome); consumed = true;
      } else if (bottom_band && dy > 0) {
        game.enqueue(tama::Input::HideChrome); consumed = true;
      } else if (bottom_band && dy < 0) {
        game.enqueue(tama::Input::ShowChrome); consumed = true;
      }
    }
    if (!consumed) {
      uint32_t dt = now_ms - touch_start_ms_;
      int dist2 = dx * dx + dy * dy;
      if (dt <= kTapMaxMs && dist2 <= kTapMaxMovePx * kTapMaxMovePx) {
        // Tap. Stats-bar takes priority (it covers some pet area at the top).
        if (tama::point_on_stats_bar(last_x_, last_y_)) {
          game.enqueue(tama::Input::MenuToggle);
        } else if (tama::point_on_pet(last_x_, last_y_)) {
          game.enqueue(tama::Input::PetTap);
        }
      }
    }
  } else if (touching && was_touching_) {
    // Touch MOVE -- continuous stroke while on Bailey.
    int dx = x - last_x_;
    int dy = y - last_y_;
    if (dx * dx + dy * dy >= kStrokeMovePx * kStrokeMovePx &&
        tama::point_on_pet(x, y)) {
      game.enqueue(tama::Input::Stroke);
      last_x_ = x;
      last_y_ = y;
    }
  }

  was_touching_ = touching;
}

}  // namespace bailey
