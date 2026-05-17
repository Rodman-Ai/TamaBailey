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
  MicTrigger       = 13,  // Phase 3: a loud sound was heard
  // Round 2 inputs
  Walk             = 14,  // start a walk (or step during walking)
  TreatGive        = 15,  // give one treat (cycles through tiers based on stock)
  Brush            = 16,  // brush Bailey -- small cleanliness bump
  CycleToy         = 17,  // cycle active toy
  CycleAge         = 18,  // demo: cycle Puppy/Adult/Senior on demand
  ImuFlick         = 19,  // IMU forward flick (throw / advance fetch)
  // Voice / menu trick commands
  VoiceSit         = 20,
  VoiceCome        = 21,
  VoiceHighFive    = 22,
  VoiceRollOver    = 23,
  VoiceJump        = 24,
  Bedtime          = 25,  // manual tuck-in (sets well_tucked_in_today)
  MenuCursorNext   = 26,  // move action-tab cursor down
};

}  // namespace tama
