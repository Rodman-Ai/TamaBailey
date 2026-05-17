#pragma once

#include <cstddef>
#include <cstdint>

#include "tama/pet.h"
#include "tama/settings.h"

namespace tama {

constexpr uint32_t kSaveMagic    = 0x42414C59u;  // "BALY"
constexpr uint16_t kSaveVersion  = 7;

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
  // ---- v3 additions (Round 2) ----
  uint32_t biscuits;                       // currency
  uint8_t  toy_owned;                      // bitmask of 5 toys
  uint8_t  active_toy;                     // 0..4 selected toy id
  uint8_t  treats[3];                      // small / medium / large counts
  uint8_t  wish;                           // active wish enum (0=none)
  uint64_t wish_started_ms;                // when current wish began
  uint32_t birthday_celebrated_unix_day;   // last day_index we celebrated
  uint8_t  well_tucked_in_today;           // bedtime routine flag
  uint8_t  vocab_learned;                  // bitmask of 5 words
  uint16_t trick_perf[5];                  // performance counter per trick
  uint64_t total_steps;                    // accumulated walking steps
  uint8_t  mood_history[7];                // last 7 days' average happiness
  uint8_t  mood_history_head;              // write index
  uint8_t  _pad3[3];
  // ---- v4 additions ----
  uint32_t friend_visits[8];               // Ollie/Mitchell/Enzo/Lincoln/Ruben/Francie/Bomi/Noshy
  // ---- v7 additions (Round 3 Phase 1) ----
  uint32_t bones_collected;                // bone collectibles found on walks
  uint16_t walk_today_steps;               // steps taken today (resets at midnight)
  uint16_t _pad7;
};
#pragma pack(pop)

constexpr size_t kSaveBytes = sizeof(SaveData);

// Validate a save record: magic + version + sane life_stage. Cleans v1/v2
// in place by zeroing newer fields (in.version is left as the caller-owned
// value; treat fields above the saved version as nominal defaults). Returns
// false on bad magic / future-version / out-of-range life stage.
bool save_validate_and_migrate(SaveData& in_out);

}  // namespace tama
