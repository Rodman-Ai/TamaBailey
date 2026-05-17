#pragma once

#include <cstdint>

namespace tama {

struct Stats {
  uint8_t hunger      = 80;
  uint8_t happiness   = 80;
  uint8_t cleanliness = 80;
  uint8_t energy      = 80;

  bool any_zero() const {
    return hunger == 0 || happiness == 0 || cleanliness == 0 || energy == 0;
  }
  bool all_zero() const {
    return hunger == 0 && happiness == 0 && cleanliness == 0 && energy == 0;
  }
  bool all_above(uint8_t threshold) const {
    return hunger >= threshold && happiness >= threshold &&
           cleanliness >= threshold && energy >= threshold;
  }
};

inline uint8_t clamp_stat(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return static_cast<uint8_t>(v);
}

}  // namespace tama
