#pragma once

#include <cstddef>
#include <cstdint>

#include "tama/pet.h"
#include "tama/settings.h"

namespace tama {

constexpr uint32_t kSaveMagic    = 0x42414C59u;  // "BALY"
constexpr uint16_t kSaveVersion  = 2;

#pragma pack(push, 1)
struct SaveDataV1 {
  uint32_t magic;
  uint16_t version;
  uint8_t  hunger;
  uint8_t  happiness;
  uint8_t  cleanliness;
  uint8_t  energy;
  uint8_t  life_stage;
  uint8_t  _pad0;
  uint64_t age_ms;
  uint64_t healthy_streak_ms;
  uint64_t neglect_streak_ms;
};
static_assert(sizeof(SaveDataV1) == 36, "v1 size");

// v2 superset of v1 + Phase 1/2/3 fields. Single struct; whole record
// is forward-compatible (unknown trailing fields are tolerated).
struct SaveData {
  // ---- v1 base ----
  uint32_t magic;
  uint16_t version;
  uint8_t  hunger;
  uint8_t  happiness;
  uint8_t  cleanliness;
  uint8_t  energy;
  uint8_t  life_stage;
  uint8_t  _pad0;
  uint64_t age_ms;
  uint64_t healthy_streak_ms;
  uint64_t neglect_streak_ms;
  // ---- v2 additions ----
  Settings settings;
  uint32_t achievements;
  uint16_t streak_days;
  uint16_t _pad1;
  uint64_t streak_last_visit_unix_ms;
  uint64_t last_save_real_unix_ms;     // for offline decay catch-up
  uint64_t total_pets;                 // for petted-100 achievement
  uint64_t fetch_catches;              // Phase 2
  // Phase 2/3 fields
  uint8_t  coat_pattern;               // 0 = default
  uint8_t  accessory_id;               // 0 = none
  uint8_t  personality_trait;          // PersonalityTrait enum
  uint8_t  inherited_trait;            // PersonalityTrait enum
  uint8_t  tricks_learned;             // bitmask
  uint8_t  weather;                    // current weather enum
  uint8_t  sickness;                   // 0=none, 1=sick, 2=recovering
  uint8_t  scene_id;                   // mirrors settings.scene_id
  // Memorial ring buffer (Phase 3) -- 5 entries, very compact.
  struct MemorialEntry {
    uint8_t  coat;
    uint8_t  trait;
    uint8_t  peak_stage;
    uint8_t  _pad;
    uint32_t age_minutes;
    uint32_t achievements_mask;
  };
  MemorialEntry memorial[5];
  uint8_t       memorial_count;
  uint8_t       memorial_head;     // ring buffer write head
  uint8_t       _pad2[2];
};
#pragma pack(pop)

constexpr size_t kSaveBytes = sizeof(SaveData);

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
                 SaveData& out);

// Parse save bytes. Accepts v1 (migrates by zero-filling new fields) and
// v2. Returns false on unrecognized magic / unknown version.
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
                 uint8_t&  memorial_head_out);

}  // namespace tama
