#pragma once

#include <cstddef>
#include <cstdint>

#include "tama/pet.h"
#include "tama/settings.h"

namespace tama {

constexpr uint32_t kSaveMagic    = 0x42414C59u;  // "BALY"
constexpr uint16_t kSaveVersion  = 36;

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
  // ---- v8 additions (Round 3 Phase 1C) ----
  uint32_t daily_quest_awarded_day;        // last day_index we awarded quest biscuits
  // ---- v9 additions (Round 3 Phase 3) ----
  uint32_t best_friend_hash;               // 0 = no bond; else hash of paired bailey
  // ---- v10 additions ----
  uint32_t last_gift_received_day;         // today_day_index of last redeemed gift; 1/day cap
  // ---- v11 additions ----
  uint16_t stories_heard;                  // bedtime stories told to Bailey lifetime
  uint16_t _pad11;
  // ---- v12 additions ----
  uint16_t dig_successes;                  // successful walk-dig timing-window hits
  uint16_t _pad12;
  // ---- v13 additions ----
  uint8_t  seasonal_unlocks;               // bitmap: bit0 pumpkin, bit1 santa, bit2 shamrock
  uint8_t  _pad13[3];
  // ---- v14 additions ----
  uint8_t  bath_toys_owned;                // bitmap: bit0 duck, bit1 boat, bit2 fish
  uint8_t  bath_toy_active;                // 0 none / 1 duck / 2 boat / 3 fish
  uint8_t  _pad14[2];
  // ---- v15 additions ----
  uint16_t hide_seek_wins;                 // Hide & Seek wins lifetime
  uint16_t _pad15;
  // ---- v16 additions ----
  // High 32 bits of the achievement bitmask. The low 32 bits stay at
  // the v2 offset (the existing `achievements` field above) so v1..v15
  // saves load with their stamped achievements intact and only the
  // upper word zero-init's.
  uint32_t achievements_hi;
  uint32_t _pad16;
  // ---- v17 additions (Round 5 Phase A1: personalization) ----
  char     pet_name[12];                   // null-terminated, default "Bailey"
  uint8_t  birthday_month;                 // 1..12, default 1
  uint8_t  birthday_day;                   // 1..31, default 13
  uint8_t  _pad17[2];
  // ---- v18 additions (Round 5 Phase A2: decor + stickers) ----
  uint8_t  stickers_unlocked;              // bitmap: paw / star / bone / heart / fire
  uint8_t  wall_poster;                    // 0..3 cycled in scene 4
  uint8_t  _pad18[2];
  // ---- v19 additions (Round 5 Phase C1: progression) ----
  uint32_t trainer_xp;                     // +1 per user action; level = sqrt(xp / 10)
  uint64_t time_played_ms;                 // sum of tick dt; excludes offline catchup
  // ---- v20 additions (Round 5 Phase C2: daily goal + active streak) ----
  uint16_t active_streak_days;             // consecutive days with >= 3 actions
  uint16_t _pad20;
  // ---- v21 additions (Round 5 Phase B subset: firefly catches) ----
  uint16_t fireflies_caught;               // lifetime firefly mini-game catches
  uint16_t _pad21;
  // ---- v22 additions (Round 5 Phase C remainder: login wheel) ----
  uint32_t last_login_wheel_day;           // day_index of the last spin (0 = never)
  uint8_t  last_wheel_reward;              // 0..4 reward id of the last spin
  uint8_t  _pad22[3];
  // ---- v23 additions (Round 5 Phase D remainder: postcards) ----
  uint8_t  last_postcard_msg_id;           // 0..15 message bank index
  uint8_t  postcards_received;             // lifetime count (caps at 255)
  uint16_t _pad23;
  // ---- v24 additions (Round 5 Phase A remainder: bed + bowl) ----
  uint8_t  bed_type;                       // 0 basket / 1 kennel pad / 2 blanket
  uint8_t  bowl_color;                     // 0 blue / 1 red / 2 silver
  uint16_t _pad24;
  // ---- v25 additions (Round 5 Phase B remainder: mini-game scores) ----
  uint16_t fish_caught;                    // lifetime fishing catches
  uint16_t tug_high_score;                 // best A/B alternation count in 5 s
  uint8_t  memory_iq;                      // highest memory-paws round survived
  uint8_t  vet_visits;                     // lifetime vet visit rituals
  uint16_t stick_chases;                   // lifetime stick-chase wins
  // ---- v26 additions (Round 6 Phase 6A: health + weight) ----
  uint8_t  health_stat;                    // 0..100; decays while sick, restored by cure
  uint8_t  pet_weight;                     // 0..100, neutral 50; tracks feed-vs-walk balance
  uint16_t _pad26;
  // ---- v27 additions (Round 6 Phase 6B: bonds + titles) ----
  uint8_t  friend_bond_levels[8];          // 0..5 heart bond per Friend slot
  uint8_t  earned_titles_mask;             // bit0 Bone Hunter / 1 Soul Bond / 2 Walker / 3 Showstopper
  uint8_t  chosen_title_id;                // 0=auto-by-XP, 1..4 = earned title
  uint16_t _pad27;
  // ---- v28 additions (Round 6 Phase 6C: diary + cherry-blossom day) ----
  uint8_t  diary_entries[7];               // ring buffer of last 7 days, message-bank ids 0..7 (0xFF = empty)
  uint8_t  diary_head;                     // write index 0..6
  uint32_t cherry_blossom_last_day;        // day_index of last +5 biscuit grant
  uint8_t  _pad28[3];
  // ---- v29 additions (Round 6 Phase 6D: vet history + auto-feeder) ----
  uint32_t vet_history_days[5];            // day_index of each of the last 5 cure rituals
  uint8_t  vet_history_head;               // ring buffer write head
  uint8_t  vet_history_count;              // 0..5; saturates
  uint8_t  auto_feeder_owned;              // 0 = not owned, 1 = owned (purchased from shop)
  uint8_t  _pad29;
  // ---- v30 additions (Round 6 Phase 6E: social depth) ----
  uint8_t  soul_bond_friend_id;            // 0..7 if any friend has 25+ visits (1st reached); 0xFF none
  uint8_t  friend_wishlist_mask;           // bitmask of friends queued for invites
  uint32_t friend_last_visit_day[8];       // day_index of last visit per Friend slot
  uint16_t _pad30;
  // ---- v31 additions (Round 6 Phase 6F: progression + events) ----
  uint8_t  quest_history[7];               // ring buffer of last 7 quest ids that were awarded
  uint8_t  quest_history_head;             // write index 0..6
  uint8_t  quest_history_count;            // 0..7; saturates
  uint32_t day_of_dogs_last_day;           // day_index of last Aug 26 event spawn
  uint8_t  birthday_cake_seen_day;         // last day_index we played the cake animation (lo 8 bits)
  uint8_t  _pad31[2];
  // ---- v32 additions (Round 6 Phase 6G: cosmetic depth) ----
  uint8_t  collar_badge_id;                // 0 none / 1 paw / 2 star / 3 bone / 4 heart
  uint8_t  accessory_size;                 // 0 small / 1 default / 2 large
  uint8_t  extra_coats_unlocked;           // bitmask: bit0 Cream(5) / bit1 Merle(6) / bit2 Husky(7)
  uint8_t  _pad32;
  // ---- v33 additions (Round 6 Phase 6H: gift log + weekly + perks) ----
  uint8_t  friend_last_gift[8];            // per-friend last gift item id (0..5)
  uint32_t weekly_steps_progress;          // steps walked since last week-rollover
  uint32_t weekly_last_awarded_week;       // week_index (day_index/7) of last reward
  uint8_t  trainer_perks_mask;             // bit0 Bigger Bites, bit1 Best Pals, bit2 Lucky Streak
  uint8_t  _pad33[3];
  // ---- v34 additions (Round 6 Phase 6I: events + daily seal) ----
  uint8_t  daily_seals_total;              // lifetime daily check-in seals (0..255)
  uint32_t daily_seals_last_day;           // last day_index a seal was granted
  uint8_t  halloween_costumes_unlocked;    // bit0 witch hat (id 9), bit1 ghost sheet (id 10)
  uint8_t  _pad34[2];
  // ---- v35 additions (Round 6 Phase 6J: trainer leaderboard) ----
  uint32_t leaderboard_hashes[8];          // ring buffer of partner sync-code hashes seen
  uint8_t  leaderboard_head;
  uint8_t  leaderboard_count;              // 0..8; saturates
  uint8_t  _pad35[2];
  // ---- v36 additions (Round 6 Phase 6K: mini-games + wallpaper) ----
  uint8_t  scene_wallpaper[8];             // per-scene wallpaper variant 0..3
  uint16_t pumpkin_tap_high_score;         // lifetime best taps in 5 s on Halloween
  uint8_t  trick_chain_runs;               // lifetime 5-trick chain completions (saturates 255)
  uint8_t  _pad36;
};
#pragma pack(pop)

constexpr size_t kSaveBytes = sizeof(SaveData);

// Validate a save record: magic + version + sane life_stage. Cleans v1/v2
// in place by zeroing newer fields (in.version is left as the caller-owned
// value; treat fields above the saved version as nominal defaults). Returns
// false on bad magic / future-version / out-of-range life stage.
bool save_validate_and_migrate(SaveData& in_out);

}  // namespace tama
