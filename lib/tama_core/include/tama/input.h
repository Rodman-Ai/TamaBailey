#pragma once

#include <cstdint>

namespace tama {

enum class Input : uint8_t {
  None       = 0,
  Feed       = 1,  // BTN A short press
  Play       = 2,  // BTN B short press
  Clean      = 3,  // BTN C short press
  MenuToggle = 4,  // any button long press
  PetTap     = 5,  // single touch / canvas click on pet
  Restart    = 6,  // long-press while Gone -> new puppy
  Stroke     = 7,  // continuous touch drag = soft pet
  MenuNext   = 8,  // double-tap or button press in menu -> next tab
};

}  // namespace tama
