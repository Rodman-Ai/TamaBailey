#include "tama/save.h"

namespace tama {

void pet_to_save(const Pet& pet, SaveData& out) {
  out.magic             = kSaveMagic;
  out.version           = kSaveVersion;
  out.hunger            = pet.stats.hunger;
  out.happiness         = pet.stats.happiness;
  out.cleanliness       = pet.stats.cleanliness;
  out.energy            = pet.stats.energy;
  out.life_stage        = (uint8_t)pet.stage;
  out._pad0             = 0;
  out.age_ms            = pet.age_ms;
  out.healthy_streak_ms = pet.healthy_streak_ms;
  out.neglect_streak_ms = pet.neglect_streak_ms;
}

bool save_to_pet(const SaveData& in, Pet& out) {
  if (in.magic != kSaveMagic)   return false;
  if (in.version != kSaveVersion) return false;
  if (in.life_stage > (uint8_t)LifeStage::Gone) return false;

  out.stats.hunger      = in.hunger;
  out.stats.happiness   = in.happiness;
  out.stats.cleanliness = in.cleanliness;
  out.stats.energy      = in.energy;
  out.stage             = (LifeStage)in.life_stage;
  out.age_ms            = in.age_ms;
  out.healthy_streak_ms = in.healthy_streak_ms;
  out.neglect_streak_ms = in.neglect_streak_ms;
  out.current_action    = Action::None;
  out.last_pet_ms       = 0;
  return true;
}

}  // namespace tama
