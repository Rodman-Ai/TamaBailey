#pragma once

#include <cstdint>

#include "tama/input.h"
#include "tama/pet.h"
#include "tama/renderer.h"
#include "tama/storage.h"

namespace tama {

#ifndef BAILEY_FAST_DECAY
#define BAILEY_FAST_DECAY 0
#endif

// Tunables (all values in milliseconds per stat point).
#if BAILEY_FAST_DECAY
constexpr uint32_t kMsPerHungerPoint      =   7200;   // 12 min full->empty
constexpr uint32_t kMsPerHappinessPoint   =  10800;   // 18 min
constexpr uint32_t kMsPerCleanlinessPoint =  14400;   // 24 min
constexpr uint32_t kMsPerEnergyRegen      =   3000;   // 100 in ~5 min
constexpr uint64_t kHealthyForAdult       = (uint64_t)24 * 60 * 1000;   // 24 min
constexpr uint64_t kHealthyForSenior      = (uint64_t)96 * 60 * 1000;
constexpr uint64_t kNeglectForDeath       = (uint64_t)60 * 1000;        // 60 s
#else
constexpr uint32_t kMsPerHungerPoint      = 432000;   // 100 pts / 12 h
constexpr uint32_t kMsPerHappinessPoint   = 648000;   // 18 h
constexpr uint32_t kMsPerCleanlinessPoint = 864000;   // 24 h
constexpr uint32_t kMsPerEnergyRegen      = 180000;   // +20 / h
constexpr uint64_t kHealthyForAdult       = (uint64_t)24 * 3600 * 1000;
constexpr uint64_t kHealthyForSenior      = (uint64_t)96 * 3600 * 1000;
constexpr uint64_t kNeglectForDeath       = (uint64_t)60 * 60 * 1000;   // 60 min
#endif

constexpr uint32_t kEnergyCostPlay = 10;
constexpr uint32_t kActionEatBoost   = 30;
constexpr uint32_t kActionPlayBoost  = 30;
constexpr uint32_t kActionCleanBoost = 60;
constexpr uint32_t kActionPetBoost   =  5;

constexpr uint32_t kActionEatDurationMs   = 1000;
constexpr uint32_t kActionPlayDurationMs  = 1500;
constexpr uint32_t kActionCleanDurationMs = 1000;
constexpr uint32_t kActionPetDurationMs   =  600;

constexpr uint32_t kPetCooldownMs   = 12000;
constexpr uint32_t kSaveIntervalMs  = 30000;
constexpr uint32_t kIdleFrameMs     = 700;

class Game {
 public:
  Game();

  // One-time setup. Loads from storage if present; otherwise starts fresh.
  void init(Storage& storage, uint32_t now_ms);

  // Push a single discrete input. Multiple inputs in the same tick are
  // applied in the order received.
  void enqueue(Input in);

  // Advance the simulation. Call once per frame.
  void tick(uint32_t now_ms);

  // Render current state to `r`. Caller is responsible for calling r.present().
  void draw(Renderer& r) const;

  // Persist state if changed and the save interval has elapsed.
  void maybe_save(Storage& storage);

  // Force an immediate save.
  void force_save(Storage& storage);

  const Pet& pet() const { return pet_; }
  bool menu_open() const { return menu_open_; }

 private:
  void apply_input(Input in);
  void apply_decay(uint32_t dt_ms);
  void update_mood();
  void update_evolution(uint32_t dt_ms);

  Pet      pet_;
  uint32_t last_tick_ms_     = 0;
  uint32_t hunger_accum_     = 0;
  uint32_t happiness_accum_  = 0;
  uint32_t cleanliness_accum_= 0;
  uint32_t energy_accum_     = 0;
  uint32_t last_save_ms_     = 0;
  bool     dirty_            = false;
  bool     menu_open_        = false;

  Input    queued_[8]        = {};
  uint8_t  queued_head_      = 0;
  uint8_t  queued_tail_      = 0;
};

}  // namespace tama
