#include "tama/game.h"

#include <cstring>

#include "tama/save.h"
#include "tama/sprites.h"
#include "tama/ui.h"

namespace tama {

Game::Game() = default;

void Game::init(Storage& storage, uint32_t now_ms) {
  sprites_init();
  SaveData s{};
  if (storage.load(s)) {
    if (save_to_pet(s, pet_)) {
      // Loaded successfully.
    }
  }
  last_tick_ms_ = now_ms;
  last_save_ms_ = now_ms;
}

void Game::enqueue(Input in) {
  if (in == Input::None) return;
  uint8_t next = (uint8_t)((queued_tail_ + 1) % 8);
  if (next == queued_head_) return;  // queue full -- drop
  queued_[queued_tail_] = in;
  queued_tail_ = next;
}

void Game::apply_input(Input in) {
  // In Gone state, only Restart does anything.
  if (pet_.stage == LifeStage::Gone) {
    if (in == Input::Restart || in == Input::MenuToggle) {
      pet_ = Pet{};  // reset to default puppy
      pet_.stats = Stats{};
      dirty_ = true;
    }
    return;
  }

  switch (in) {
    case Input::Feed:
      pet_.stats.hunger = clamp_stat((int)pet_.stats.hunger + kActionEatBoost);
      pet_.current_action = Action::Eat;
      pet_.action_started_ms = last_tick_ms_;
      dirty_ = true;
      break;
    case Input::Play:
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kActionPlayBoost);
      pet_.stats.energy    = clamp_stat((int)pet_.stats.energy - kEnergyCostPlay);
      pet_.current_action = Action::Play;
      pet_.action_started_ms = last_tick_ms_;
      dirty_ = true;
      break;
    case Input::Clean:
      pet_.stats.cleanliness = clamp_stat((int)pet_.stats.cleanliness + kActionCleanBoost);
      pet_.current_action = Action::Clean;
      pet_.action_started_ms = last_tick_ms_;
      dirty_ = true;
      break;
    case Input::PetTap: {
      uint32_t since = last_tick_ms_ - pet_.last_pet_ms;
      if (since >= kPetCooldownMs || pet_.last_pet_ms == 0) {
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kActionPetBoost);
        pet_.current_action = Action::Pet;
        pet_.action_started_ms = last_tick_ms_;
        pet_.last_pet_ms = last_tick_ms_;
        dirty_ = true;
      }
      break;
    }
    case Input::MenuToggle:
      menu_open_ = !menu_open_;
      break;
    case Input::Restart:
      // Only matters in Gone state (handled above)
      break;
    case Input::None:
      break;
  }
}

void Game::apply_decay(uint32_t dt_ms) {
  if (pet_.stage == LifeStage::Gone) return;

  hunger_accum_      += dt_ms;
  happiness_accum_   += dt_ms;
  cleanliness_accum_ += dt_ms;

  while (hunger_accum_ >= kMsPerHungerPoint && pet_.stats.hunger > 0) {
    pet_.stats.hunger--;
    hunger_accum_ -= kMsPerHungerPoint;
    dirty_ = true;
  }
  if (pet_.stats.hunger == 0) hunger_accum_ = 0;

  while (happiness_accum_ >= kMsPerHappinessPoint && pet_.stats.happiness > 0) {
    pet_.stats.happiness--;
    happiness_accum_ -= kMsPerHappinessPoint;
    dirty_ = true;
  }
  if (pet_.stats.happiness == 0) happiness_accum_ = 0;

  while (cleanliness_accum_ >= kMsPerCleanlinessPoint && pet_.stats.cleanliness > 0) {
    pet_.stats.cleanliness--;
    cleanliness_accum_ -= kMsPerCleanlinessPoint;
    dirty_ = true;
  }
  if (pet_.stats.cleanliness == 0) cleanliness_accum_ = 0;

  // Energy regenerates while not actively playing.
  if (pet_.current_action != Action::Play) {
    energy_accum_ += dt_ms;
    while (energy_accum_ >= kMsPerEnergyRegen && pet_.stats.energy < 100) {
      pet_.stats.energy++;
      energy_accum_ -= kMsPerEnergyRegen;
      dirty_ = true;
    }
    if (pet_.stats.energy == 100) energy_accum_ = 0;
  }
}

void Game::update_mood() {
  if (pet_.stage == LifeStage::Gone) { pet_.mood = Mood::Gone; return; }
  if (pet_.stats.energy < 20)        { pet_.mood = Mood::Sleeping; return; }
  if (pet_.stats.any_zero())         { pet_.mood = Mood::Sad; return; }
  if (pet_.stats.cleanliness < 30)   { pet_.mood = Mood::Dirty; return; }
  if (pet_.stats.hunger < 30)        { pet_.mood = Mood::Hungry; return; }
  if (pet_.stats.all_above(50))      { pet_.mood = Mood::Happy; return; }
  pet_.mood = Mood::Neutral;
}

void Game::update_evolution(uint32_t dt_ms) {
  if (pet_.stage == LifeStage::Gone) {
    pet_.neglect_streak_ms += dt_ms;
    return;
  }

  // "Healthy" tick = all stats >= 30 right now.
  if (pet_.stats.all_above(30)) {
    pet_.healthy_streak_ms += dt_ms;
    pet_.neglect_streak_ms = 0;
  } else if (pet_.stats.all_zero()) {
    pet_.neglect_streak_ms += dt_ms;
    pet_.healthy_streak_ms = 0;
  } else {
    pet_.neglect_streak_ms = 0;
    // healthy streak holds steady on bad-but-not-dead days
  }

  if (pet_.stage == LifeStage::Puppy &&
      pet_.healthy_streak_ms >= kHealthyForAdult) {
    pet_.stage = LifeStage::Adult;
    dirty_ = true;
  } else if (pet_.stage == LifeStage::Adult &&
             pet_.healthy_streak_ms >= kHealthyForSenior) {
    pet_.stage = LifeStage::Senior;
    dirty_ = true;
  }

  if (pet_.neglect_streak_ms >= kNeglectForDeath) {
    pet_.stage = LifeStage::Gone;
    pet_.mood  = Mood::Gone;
    pet_.healthy_streak_ms = 0;
    dirty_ = true;
  }
}

void Game::tick(uint32_t now_ms) {
  uint32_t dt = now_ms - last_tick_ms_;
  // Guard against wraparound (~49 days) and unreasonable jumps.
  if (dt > 60 * 60 * 1000u) dt = 1000u;
  last_tick_ms_ = now_ms;

  while (queued_head_ != queued_tail_) {
    Input in = queued_[queued_head_];
    queued_head_ = (uint8_t)((queued_head_ + 1) % 8);
    apply_input(in);
  }

  if (pet_.stage != LifeStage::Gone) pet_.age_ms += dt;

  apply_decay(dt);
  update_evolution(dt);
  update_mood();

  // Clear short action animations
  if (pet_.current_action != Action::None) {
    uint32_t elapsed = now_ms - pet_.action_started_ms;
    uint32_t dur = 0;
    switch (pet_.current_action) {
      case Action::Eat:   dur = kActionEatDurationMs;   break;
      case Action::Play:  dur = kActionPlayDurationMs;  break;
      case Action::Clean: dur = kActionCleanDurationMs; break;
      case Action::Pet:   dur = kActionPetDurationMs;   break;
      default: break;
    }
    if (elapsed > dur) pet_.current_action = Action::None;
  }
}

void Game::draw(Renderer& r) const {
  draw_scene(r, pet_, last_tick_ms_);
  if (menu_open_) draw_menu_overlay(r, pet_);
}

void Game::maybe_save(Storage& storage) {
  if (!dirty_) return;
  if (last_tick_ms_ - last_save_ms_ < kSaveIntervalMs) return;
  force_save(storage);
}

void Game::force_save(Storage& storage) {
  SaveData s;
  pet_to_save(pet_, s);
  storage.save(s);
  dirty_ = false;
  last_save_ms_ = last_tick_ms_;
}

}  // namespace tama
