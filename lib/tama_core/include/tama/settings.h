#pragma once

#include <cstdint>

namespace tama {

#pragma pack(push, 1)
struct Settings {
  uint8_t volume        = 70;   // 0..100
  uint8_t brightness    = 220;  // 0..255 (display PWM)
  uint8_t decay_mult    = 10;   // /10 multiplier: 5=0.5x, 10=1x, 30=3x ...
  int16_t tz_offset_min = 0;    // minutes east of UTC; e.g. -300 = EST
  uint8_t auto_sleep    = 1;    // 1 = auto-nap at night
  uint8_t mic_enabled   = 1;    // Phase 3 mic reactions
  uint8_t scene_id      = 0;    // 0=living room, 1=backyard, 2=dog park
  uint8_t _pad          = 0;
};
#pragma pack(pop)

constexpr Settings default_settings() { return Settings{}; }

}  // namespace tama
