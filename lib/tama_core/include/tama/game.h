#pragma once

#include <cstdint>

#include "tama/achievements.h"
#include "tama/audio.h"
#include "tama/clock.h"
#include "tama/input.h"
#include "tama/pet.h"
#include "tama/renderer.h"
#include "tama/save.h"
#include "tama/settings.h"
#include "tama/storage.h"

namespace tama {

#ifndef BAILEY_FAST_DECAY
#define BAILEY_FAST_DECAY 0
#endif

// Base decay/regen tunables (multiplier defaults to settings.decay_mult/10).
#if BAILEY_FAST_DECAY
constexpr uint32_t kBaseMsPerHungerPoint      =   7200;
constexpr uint32_t kBaseMsPerHappinessPoint   =  10800;
constexpr uint32_t kBaseMsPerCleanlinessPoint =  14400;
constexpr uint32_t kBaseMsPerEnergyRegen      =   3000;
constexpr uint64_t kHealthyForAdult           = (uint64_t)24 * 60 * 1000;
constexpr uint64_t kHealthyForSenior          = (uint64_t)96 * 60 * 1000;
constexpr uint64_t kNeglectForDeath           = (uint64_t)60 * 1000;
#else
constexpr uint32_t kBaseMsPerHungerPoint      = 432000;
constexpr uint32_t kBaseMsPerHappinessPoint   = 648000;
constexpr uint32_t kBaseMsPerCleanlinessPoint = 864000;
constexpr uint32_t kBaseMsPerEnergyRegen      = 180000;
constexpr uint64_t kHealthyForAdult           = (uint64_t)24 * 3600 * 1000;
constexpr uint64_t kHealthyForSenior          = (uint64_t)96 * 3600 * 1000;
constexpr uint64_t kNeglectForDeath           = (uint64_t)60 * 60 * 1000;
#endif

constexpr uint32_t kEnergyCostPlay   = 10;
constexpr uint32_t kActionEatBoost   = 30;
constexpr uint32_t kActionPlayBoost  = 30;
constexpr uint32_t kActionCleanBoost = 60;
constexpr uint32_t kActionPetBoost   =  5;
constexpr uint32_t kActionStrokeBoost = 1;  // continuous stroke
constexpr uint32_t kStreakBonus      = 10;  // daily streak happiness bump

constexpr uint32_t kActionEatDurationMs   = 1000;
constexpr uint32_t kActionPlayDurationMs  = 1500;
constexpr uint32_t kActionCleanDurationMs = 1000;
constexpr uint32_t kActionPetDurationMs   =  600;

constexpr uint32_t kPetCooldownMs   = 12000;
constexpr uint32_t kStrokeCooldownMs = 250;
constexpr uint32_t kSaveIntervalMs  = 30000;
constexpr uint32_t kIdleFrameMs     = 700;

// Cap on offline catch-up: capped to 7 days so a long absence doesn't
// auto-kill Bailey faster than they could actually be neglected.
constexpr uint64_t kMaxOfflineCatchupMs = (uint64_t)7 * 24 * 3600 * 1000;

enum class GameMode : uint8_t {
  Idle = 0,
  // Phase 2 modes -- implemented in later phases.
  FetchAiming   = 1,
  FetchCatching = 2,
  Training      = 3,
  PhotoMode     = 4,
};

enum class Personality : uint8_t {
  None    = 0,
  Playful = 1,
  Lazy    = 2,
  Clever  = 3,
  Loyal   = 4,
  Shy     = 5,
};
const char* personality_name(Personality p);

enum class Weather : uint8_t {
  Sunny  = 0,
  Cloudy = 1,
  Rain   = 2,
  Snow   = 3,
};

class Game {
 public:
  Game();

  // The Clock and Speaker pointers are owned by the caller and must
  // outlive the Game. Pass nullptr for either to use a no-op default.
  void init(Storage& storage, uint32_t now_ms,
            Clock* clock = nullptr,
            Speaker* speaker = nullptr);

  void enqueue(Input in);
  void tick(uint32_t now_ms);
  void draw(Renderer& r) const;

  void maybe_save(Storage& storage);
  void force_save(Storage& storage);

  // --- Inspection ---
  const Pet& pet() const { return pet_; }
  const Settings& settings() const { return settings_; }
  Settings& mut_settings() { dirty_ = true; return settings_; }
  uint32_t  achievements() const { return achievements_; }
  uint16_t  streak_days()  const { return streak_days_; }
  Weather   weather()      const { return (Weather)weather_; }
  Personality personality() const { return (Personality)personality_trait_; }
  bool      menu_open()    const { return menu_open_; }
  GameMode  mode()         const { return mode_; }
  uint64_t  total_pets()   const { return total_pets_; }
  bool      is_sick()      const { return sickness_ != 0; }
  uint8_t   coat_pattern() const { return coat_pattern_; }
  uint8_t   accessory_id() const { return accessory_id_; }

  // Background tint from 0.0 (deep night) to 1.0 (full day), based on
  // local clock if synced, else millis-based 24-min cycle.
  float     daylight() const { return daylight_; }
  // Local-time HH:MM string for the status bar; empty if clock not synced.
  const char* clock_string() const { return clock_str_; }

  // --- Settings menus interact via these ---
  enum class MenuTab : uint8_t { Stats = 0, Achievements = 1, Settings = 2, Sync = 3 };
  MenuTab menu_tab() const { return menu_tab_; }
  void    set_menu_tab(MenuTab t) { menu_tab_ = t; }

  // --- Sync code (Phase 1) ---
  // Generates a 12-char alpha-numeric code (3 groups of 4) encoding
  // gameplay-essential state. Returns pointer to a static buffer.
  const char* generate_sync_code();
  // Parse a code typed by the user; returns true on success and applies.
  bool apply_sync_code(const char* code);

 private:
  void apply_input(Input in);
  void apply_decay(uint32_t dt_ms);
  void update_mood();
  void update_evolution(uint32_t dt_ms);
  void update_daylight(uint32_t now_ms);
  void check_streak(uint64_t now_unix_ms);
  void check_achievements();
  void apply_offline_catchup(uint64_t now_unix_ms);
  void play_clip(ClipId clip);
  void unlock_achievement(AchievementId id);

  Pet      pet_;
  Settings settings_;
  uint32_t achievements_  = 0;
  uint16_t streak_days_   = 0;
  uint64_t streak_last_visit_unix_ms_ = 0;
  uint64_t last_save_real_unix_ms_    = 0;
  uint64_t total_pets_                = 0;
  uint64_t fetch_catches_             = 0;
  uint8_t  coat_pattern_              = 0;
  uint8_t  accessory_id_              = 0;
  uint8_t  personality_trait_         = 0;
  uint8_t  inherited_trait_           = 0;
  uint8_t  tricks_learned_            = 0;
  uint8_t  weather_                   = 0;
  uint8_t  sickness_                  = 0;
  uint8_t  scene_id_                  = 0;
  SaveData::MemorialEntry memorial_[5] = {};
  uint8_t  memorial_count_            = 0;
  uint8_t  memorial_head_             = 0;

  uint32_t last_tick_ms_     = 0;
  uint32_t hunger_accum_     = 0;
  uint32_t happiness_accum_  = 0;
  uint32_t cleanliness_accum_= 0;
  uint32_t energy_accum_     = 0;
  uint32_t last_save_ms_     = 0;
  uint32_t last_stroke_ms_   = 0;
  bool     dirty_            = false;
  bool     menu_open_        = false;
  MenuTab  menu_tab_         = MenuTab::Stats;
  GameMode mode_             = GameMode::Idle;

  float    daylight_         = 1.0f;
  char     clock_str_[16]    = {0};
  Clock*   clock_            = nullptr;
  Speaker* speaker_          = nullptr;

  Input    queued_[16]       = {};
  uint8_t  queued_head_      = 0;
  uint8_t  queued_tail_      = 0;

  char     sync_code_buf_[16] = {0};
};

}  // namespace tama
