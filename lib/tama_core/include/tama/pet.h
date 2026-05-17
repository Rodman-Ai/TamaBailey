#pragma once

#include <cstdint>

#include "tama/stats.h"

namespace tama {

enum class LifeStage : uint8_t { Puppy = 0, Adult = 1, Senior = 2, Gone = 3 };

enum class Mood : uint8_t {
  Neutral   = 0,
  Happy     = 1,
  Hungry    = 2,
  Sad       = 3,
  Dirty     = 4,
  Sleeping  = 5,
  Gone      = 6,  // legacy; never set in current builds (see MovingOut)
  MovingOut = 7,  // neglected -> bailey leaves for a new family
  Magic     = 8,  // old age -> magically turns back into a puppy
};

// Transient action played in response to player input.
enum class Action : uint8_t {
  None  = 0,
  Eat   = 1,
  Play  = 2,
  Clean = 3,
  Pet   = 4,
};

struct Pet {
  Stats     stats;
  LifeStage stage              = LifeStage::Puppy;
  Mood      mood               = Mood::Neutral;
  Action    current_action     = Action::None;
  uint32_t  action_started_ms  = 0;
  uint64_t  age_ms             = 0;
  uint64_t  healthy_streak_ms  = 0;
  uint64_t  neglect_streak_ms  = 0;
  uint32_t  last_pet_ms        = 0;
};

}  // namespace tama
