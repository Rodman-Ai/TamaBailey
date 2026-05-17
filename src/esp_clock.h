#pragma once

#include "tama/clock.h"

namespace bailey {

// Wi-Fi + SNTP-backed clock. Wi-Fi credentials come from include/secrets.h
// if present (copy from include/secrets.h.example and edit). If creds are
// absent or connection fails, is_synced() stays false and the game uses
// its synthetic 24-minute day/night cycle instead.
class EspClock final : public tama::Clock {
 public:
  void begin();                  // call from setup() after Serial.begin
  void poll();                   // call periodically from loop()

  uint64_t now_unix_ms() override;
  bool     is_synced()   override;
};

}  // namespace bailey
