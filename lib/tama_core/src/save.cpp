#include "tama/save.h"

#include <cstring>

namespace tama {

void pet_to_save(const Pet& pet, const Settings& settings,
                 uint32_t achievements, uint16_t streak_days,
                 uint64_t streak_last_visit_unix_ms,
                 uint64_t last_save_real_unix_ms,
                 uint64_t total_pets,
                 uint64_t fetch_catches,
                 uint8_t  coat_pattern,
                 uint8_t  accessory_id,
                 uint8_t  personality_trait,
                 uint8_t  inherited_trait,
                 uint8_t  tricks_learned,
                 uint8_t  weather,
                 uint8_t  sickness,
                 uint8_t  scene_id,
                 const SaveData::MemorialEntry* memorial,
                 uint8_t  memorial_count,
                 uint8_t  memorial_head,
                 SaveData& out) {
  out = SaveData{};
  out.magic             = kSaveMagic;
  out.version           = kSaveVersion;
  out.hunger            = pet.stats.hunger;
  out.happiness         = pet.stats.happiness;
  out.cleanliness       = pet.stats.cleanliness;
  out.energy            = pet.stats.energy;
  out.life_stage        = (uint8_t)pet.stage;
  out.age_ms            = pet.age_ms;
  out.healthy_streak_ms = pet.healthy_streak_ms;
  out.neglect_streak_ms = pet.neglect_streak_ms;

  out.settings                  = settings;
  out.achievements              = achievements;
  out.streak_days               = streak_days;
  out.streak_last_visit_unix_ms = streak_last_visit_unix_ms;
  out.last_save_real_unix_ms    = last_save_real_unix_ms;
  out.total_pets                = total_pets;
  out.fetch_catches             = fetch_catches;
  out.coat_pattern              = coat_pattern;
  out.accessory_id              = accessory_id;
  out.personality_trait         = personality_trait;
  out.inherited_trait           = inherited_trait;
  out.tricks_learned            = tricks_learned;
  out.weather                   = weather;
  out.sickness                  = sickness;
  out.scene_id                  = scene_id;
  out.memorial_count            = memorial_count;
  out.memorial_head             = memorial_head;
  if (memorial) std::memcpy(out.memorial, memorial, sizeof(out.memorial));
}

bool save_to_pet(const SaveData& in, Pet& out,
                 Settings& settings_out,
                 uint32_t& achievements_out,
                 uint16_t& streak_days_out,
                 uint64_t& streak_last_visit_unix_ms_out,
                 uint64_t& last_save_real_unix_ms_out,
                 uint64_t& total_pets_out,
                 uint64_t& fetch_catches_out,
                 uint8_t&  coat_pattern_out,
                 uint8_t&  accessory_id_out,
                 uint8_t&  personality_trait_out,
                 uint8_t&  inherited_trait_out,
                 uint8_t&  tricks_learned_out,
                 uint8_t&  weather_out,
                 uint8_t&  sickness_out,
                 uint8_t&  scene_id_out,
                 SaveData::MemorialEntry* memorial_out,
                 uint8_t&  memorial_count_out,
                 uint8_t&  memorial_head_out) {
  if (in.magic != kSaveMagic) return false;
  if (in.version > kSaveVersion) return false;  // unknown future version

  // Common v1 fields
  out.stats.hunger      = in.hunger;
  out.stats.happiness   = in.happiness;
  out.stats.cleanliness = in.cleanliness;
  out.stats.energy      = in.energy;
  if (in.life_stage > (uint8_t)LifeStage::Gone) return false;
  out.stage             = (LifeStage)in.life_stage;
  out.age_ms            = in.age_ms;
  out.healthy_streak_ms = in.healthy_streak_ms;
  out.neglect_streak_ms = in.neglect_streak_ms;
  out.current_action    = Action::None;
  out.last_pet_ms       = 0;

  if (in.version >= 2) {
    settings_out                  = in.settings;
    achievements_out              = in.achievements;
    streak_days_out               = in.streak_days;
    streak_last_visit_unix_ms_out = in.streak_last_visit_unix_ms;
    last_save_real_unix_ms_out    = in.last_save_real_unix_ms;
    total_pets_out                = in.total_pets;
    fetch_catches_out             = in.fetch_catches;
    coat_pattern_out              = in.coat_pattern;
    accessory_id_out              = in.accessory_id;
    personality_trait_out         = in.personality_trait;
    inherited_trait_out           = in.inherited_trait;
    tricks_learned_out            = in.tricks_learned;
    weather_out                   = in.weather;
    sickness_out                  = in.sickness;
    scene_id_out                  = in.scene_id;
    memorial_count_out            = in.memorial_count;
    memorial_head_out             = in.memorial_head;
    if (memorial_out) std::memcpy(memorial_out, in.memorial, sizeof(in.memorial));
  } else {
    // v1 migration: zero everything new.
    settings_out                  = default_settings();
    achievements_out              = 0;
    streak_days_out               = 0;
    streak_last_visit_unix_ms_out = 0;
    last_save_real_unix_ms_out    = 0;
    total_pets_out                = 0;
    fetch_catches_out             = 0;
    coat_pattern_out              = 0;
    accessory_id_out              = 0;
    personality_trait_out         = 0;
    inherited_trait_out           = 0;
    tricks_learned_out            = 0;
    weather_out                   = 0;
    sickness_out                  = 0;
    scene_id_out                  = 0;
    memorial_count_out            = 0;
    memorial_head_out             = 0;
    if (memorial_out) std::memset(memorial_out, 0, sizeof(SaveData::memorial));
  }
  return true;
}

}  // namespace tama
