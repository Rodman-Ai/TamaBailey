#pragma once

#include <cstdint>

namespace tama {

enum class Input : uint8_t {
  None       = 0,
  Feed       = 1,  // BTN A short press
  Play       = 2,  // BTN B short press
  Clean      = 3,  // BTN C short press
  MenuToggle = 4,  // any button long press
  PetTap     = 5,  // optional touch: tap the pet sprite
  Restart    = 6,  // long-press while in Gone state -> new puppy
};

}  // namespace tama
