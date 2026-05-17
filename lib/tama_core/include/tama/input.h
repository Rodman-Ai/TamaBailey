#pragma once

#include <cstdint>

namespace tama {

enum class Input : uint8_t {
  None             = 0,
  Feed             = 1,
  Play             = 2,
  Clean            = 3,
  MenuToggle       = 4,
  PetTap           = 5,
  Restart          = 6,
  Stroke           = 7,
  MenuNext         = 8,
  // Phase 2
  CycleScene       = 9,
  CycleCoat        = 10,
  CycleAccessory   = 11,
  TakePhoto        = 12,  // Phase 3
};

}  // namespace tama
