#include "tama/game.h"

#include <cstdio>
#include <cstring>

#include "tama/sprites.h"
#include "tama/ui.h"

namespace tama {

namespace {

// Stub default Speaker that drops everything.
class NullSpeaker final : public Speaker {
 public:
  void playClip(ClipId, uint8_t) override {}
  bool available() const override { return false; }
};
NullSpeaker g_null_speaker;

// Stub default Clock that returns 0 / never synced.
class NullClock final : public Clock {
 public:
  uint64_t now_unix_ms() override { return 0; }
  bool     is_synced()   override { return false; }
};
NullClock g_null_clock;

// Base32 alphabet excluding ambiguous chars (0/O/1/I/L removed).
constexpr const char kAlphabet[33] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

int decode_char(char c) {
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  for (int i = 0; i < 32; ++i) if (kAlphabet[i] == c) return i;
  return -1;
}

uint8_t crc8(const uint8_t* p, int n) {
  uint8_t c = 0xA5;
  for (int i = 0; i < n; ++i) {
    c ^= p[i];
    for (int b = 0; b < 8; ++b)
      c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
  }
  return c;
}

// Pack 8 input bytes into 13 base32 chars (104 bits → 8 chars at 5 bits + slack).
// Encode 8 bytes (64 bits) as 13 base32 chars (65 bits, top bit always 0).
void encode_base32(const uint8_t* in, int n, char* out) {
  uint64_t acc = 0;
  int bits = 0, oi = 0;
  for (int i = 0; i < n; ++i) {
    acc = (acc << 8) | in[i];
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      uint8_t idx = (uint8_t)((acc >> bits) & 0x1F);
      out[oi++] = kAlphabet[idx];
    }
  }
  if (bits > 0) {
    uint8_t idx = (uint8_t)((acc << (5 - bits)) & 0x1F);
    out[oi++] = kAlphabet[idx];
  }
  out[oi] = 0;
}

bool decode_base32(const char* in, uint8_t* out, int max_out, int& n_out) {
  uint64_t acc = 0;
  int bits = 0, oi = 0;
  while (*in) {
    if (*in == '-' || *in == ' ' || *in == '\t') { ++in; continue; }
    int v = decode_char(*in++);
    if (v < 0) return false;
    acc = (acc << 5) | (uint32_t)v;
    bits += 5;
    while (bits >= 8) {
      bits -= 8;
      if (oi >= max_out) return false;
      out[oi++] = (uint8_t)((acc >> bits) & 0xFF);
    }
  }
  n_out = oi;
  return true;
}

}  // namespace

const char* personality_name(Personality p) {
  switch (p) {
    case Personality::Playful: return "Playful";
    case Personality::Lazy:    return "Lazy";
    case Personality::Clever:  return "Clever";
    case Personality::Loyal:   return "Loyal";
    case Personality::Shy:     return "Shy";
    case Personality::None:    return "Easygoing";
  }
  return "Easygoing";
}

Game::Game() = default;

void Game::init(Storage& storage, uint32_t now_ms, Clock* clock, Speaker* speaker) {
  sprites_init();
  audio_init();
  clock_   = clock   ? clock   : &g_null_clock;
  speaker_ = speaker ? speaker : &g_null_speaker;

  SaveData s{};
  if (storage.load(s)) {
    if (save_to_pet(s, pet_,
                    settings_, achievements_,
                    streak_days_, streak_last_visit_unix_ms_,
                    last_save_real_unix_ms_, total_pets_, fetch_catches_,
                    coat_pattern_, accessory_id_, personality_trait_,
                    inherited_trait_, tricks_learned_, weather_, sickness_,
                    scene_id_, memorial_, memorial_count_, memorial_head_)) {
      // Loaded.
    }
  } else {
    // Fresh pet: roll a personality.
    uint32_t seed = now_ms ^ 0xBADBAD;
    personality_trait_ = (uint8_t)(1 + (seed % 5));  // Playful..Shy
  }

  // Offline decay catch-up if we have a synced clock and a previous timestamp.
  if (clock_->is_synced() && last_save_real_unix_ms_ != 0) {
    apply_offline_catchup(clock_->now_unix_ms());
  }

  last_tick_ms_ = now_ms;
  last_save_ms_ = now_ms;
}

void Game::apply_offline_catchup(uint64_t now_unix_ms) {
  if (now_unix_ms <= last_save_real_unix_ms_) return;
  uint64_t gap_ms = now_unix_ms - last_save_real_unix_ms_;
  if (gap_ms > kMaxOfflineCatchupMs) gap_ms = kMaxOfflineCatchupMs;
  // Apply decay in 60-second chunks to avoid float-precision issues.
  while (gap_ms > 0) {
    uint32_t step = gap_ms > 60000 ? 60000 : (uint32_t)gap_ms;
    apply_decay(step);
    update_evolution(step);
    gap_ms -= step;
  }
  dirty_ = true;
}

void Game::enqueue(Input in) {
  if (in == Input::None) return;
  uint8_t next = (uint8_t)((queued_tail_ + 1) % 16);
  if (next == queued_head_) return;
  queued_[queued_tail_] = in;
  queued_tail_ = next;
}

void Game::play_clip(ClipId clip) {
  if (speaker_) speaker_->playClip(clip, settings_.volume);
}

void Game::unlock_achievement(AchievementId id) {
  if (unlock(achievements_, id)) {
    play_clip(ClipId::Achieve);
    dirty_ = true;
  }
}

void Game::apply_input(Input in) {
  // While the menu is open, short button presses cycle tabs instead of
  // performing actions on Bailey -- single mental model: button is a
  // selector, long-press is escape.
  if (menu_open_ && (in == Input::Feed || in == Input::Play || in == Input::Clean)) {
    int t = (int)menu_tab_ + 1;
    if (t > (int)MenuTab::Sync) t = 0;
    menu_tab_ = (MenuTab)t;
    return;
  }

  if (pet_.stage == LifeStage::Gone) {
    if (in == Input::Restart || in == Input::MenuToggle) {
      // Record memorial before reset (Phase 3).
      SaveData::MemorialEntry m{};
      m.coat              = coat_pattern_;
      m.trait             = personality_trait_;
      m.peak_stage        = (uint8_t)LifeStage::Gone;  // best-effort
      m.age_minutes       = (uint32_t)(pet_.age_ms / 60000ULL);
      m.achievements_mask = achievements_;
      memorial_[memorial_head_] = m;
      memorial_head_ = (uint8_t)((memorial_head_ + 1) % 5);
      if (memorial_count_ < 5) memorial_count_++;

      // Reset pet but keep settings and achievements and inherit one trait.
      uint8_t parent_trait = personality_trait_;
      pet_   = Pet{};
      coat_pattern_ = 0;
      accessory_id_ = 0;
      sickness_     = 0;
      pet_.stats    = Stats{};
      // 50% chance to inherit parent's trait, else roll a new one.
      uint32_t r = (uint32_t)last_tick_ms_ * 1103515245u + 12345u;
      if (parent_trait != 0 && (r & 1)) {
        personality_trait_ = parent_trait;
        inherited_trait_   = parent_trait;
        unlock_achievement(AchievementId::HonoredAncestor);
      } else {
        personality_trait_ = (uint8_t)(1 + (r % 5));
        inherited_trait_   = 0;
      }
      dirty_ = true;
    }
    return;
  }

  switch (in) {
    case Input::Feed:
      pet_.stats.hunger = clamp_stat((int)pet_.stats.hunger + kActionEatBoost);
      pet_.current_action = Action::Eat;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Yip);
      unlock_achievement(AchievementId::FirstFeed);
      dirty_ = true;
      break;
    case Input::Play:
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kActionPlayBoost);
      pet_.stats.energy    = clamp_stat((int)pet_.stats.energy - kEnergyCostPlay);
      pet_.current_action = Action::Play;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Wuff);
      unlock_achievement(AchievementId::FirstPlay);
      dirty_ = true;
      break;
    case Input::Clean:
      pet_.stats.cleanliness = clamp_stat((int)pet_.stats.cleanliness + kActionCleanBoost);
      pet_.current_action = Action::Clean;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Splash);
      unlock_achievement(AchievementId::FirstClean);
      dirty_ = true;
      break;
    case Input::PetTap: {
      uint32_t since = last_tick_ms_ - pet_.last_pet_ms;
      if (since >= kPetCooldownMs || pet_.last_pet_ms == 0) {
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kActionPetBoost);
        pet_.current_action = Action::Pet;
        pet_.action_started_ms = last_tick_ms_;
        pet_.last_pet_ms = last_tick_ms_;
        total_pets_++;
        play_clip(ClipId::Heart);
        unlock_achievement(AchievementId::FirstPet);
        if (total_pets_ >= 100) unlock_achievement(AchievementId::Petted100);
        dirty_ = true;
      }
      break;
    }
    case Input::Stroke: {
      // Continuous touch-petting: small bonus, much shorter cooldown.
      uint32_t since = last_tick_ms_ - last_stroke_ms_;
      if (since >= kStrokeCooldownMs || last_stroke_ms_ == 0) {
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kActionStrokeBoost);
        last_stroke_ms_ = last_tick_ms_;
        total_pets_++;
        if (pet_.current_action == Action::None) {
          pet_.current_action = Action::Pet;
          pet_.action_started_ms = last_tick_ms_;
        }
        if (total_pets_ >= 100) unlock_achievement(AchievementId::Petted100);
        dirty_ = true;
      }
      break;
    }
    case Input::MenuToggle:
      menu_open_ = !menu_open_;
      break;
    case Input::MenuNext:
      if (menu_open_) {
        int t = (int)menu_tab_ + 1;
        if (t > (int)MenuTab::Sync) t = 0;
        menu_tab_ = (MenuTab)t;
      }
      break;
    case Input::Restart:
      // Handled above when stage == Gone
      break;
    case Input::None:
      break;
  }
}

void Game::apply_decay(uint32_t dt_ms) {
  if (pet_.stage == LifeStage::Gone) return;

  // Decay multiplier from settings (decay_mult / 10).
  uint32_t mult_num = settings_.decay_mult == 0 ? 10 : settings_.decay_mult;
  // Slower-decay multiplier means MORE ms per point.
  auto scaled = [&](uint32_t base) {
    // dt_ms_effective = dt_ms * (10 / mult); applied as accumulator divisor.
    // Implement by scaling the THRESHOLD inversely.
    return (uint32_t)((uint64_t)base * 10 / mult_num);
  };
  uint32_t h_thr = scaled(kBaseMsPerHungerPoint);
  uint32_t p_thr = scaled(kBaseMsPerHappinessPoint);
  uint32_t c_thr = scaled(kBaseMsPerCleanlinessPoint);
  uint32_t e_thr = scaled(kBaseMsPerEnergyRegen);

  // Personality tweaks
  switch ((Personality)personality_trait_) {
    case Personality::Playful: p_thr = (uint32_t)(p_thr * 8 / 10); break;
    case Personality::Lazy:    e_thr = (uint32_t)(e_thr * 8 / 10);
                               h_thr = (uint32_t)(h_thr * 12 / 10); break;
    default: break;
  }

  // Weather tweaks (Phase 2 data, applied here when set)
  if (weather_ == (uint8_t)Weather::Rain)
    c_thr = (uint32_t)(c_thr * 7 / 10);
  if (weather_ == (uint8_t)Weather::Snow)
    e_thr = (uint32_t)(e_thr * 13 / 10);

  // Sick state pauses positive progression but doesn't accelerate decay.
  hunger_accum_      += dt_ms;
  happiness_accum_   += dt_ms;
  cleanliness_accum_ += dt_ms;

  while (h_thr > 0 && hunger_accum_ >= h_thr && pet_.stats.hunger > 0) {
    pet_.stats.hunger--;
    hunger_accum_ -= h_thr;
    dirty_ = true;
  }
  if (pet_.stats.hunger == 0) hunger_accum_ = 0;

  while (p_thr > 0 && happiness_accum_ >= p_thr && pet_.stats.happiness > 0) {
    pet_.stats.happiness--;
    happiness_accum_ -= p_thr;
    dirty_ = true;
  }
  if (pet_.stats.happiness == 0) happiness_accum_ = 0;

  while (c_thr > 0 && cleanliness_accum_ >= c_thr && pet_.stats.cleanliness > 0) {
    pet_.stats.cleanliness--;
    cleanliness_accum_ -= c_thr;
    dirty_ = true;
  }
  if (pet_.stats.cleanliness == 0) cleanliness_accum_ = 0;

  if (pet_.current_action != Action::Play) {
    energy_accum_ += dt_ms;
    while (e_thr > 0 && energy_accum_ >= e_thr && pet_.stats.energy < 100) {
      pet_.stats.energy++;
      energy_accum_ -= e_thr;
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

  if (pet_.stats.all_above(30)) {
    pet_.healthy_streak_ms += dt_ms;
    pet_.neglect_streak_ms = 0;
  } else if (pet_.stats.all_zero()) {
    pet_.neglect_streak_ms += dt_ms;
    pet_.healthy_streak_ms = 0;
  } else {
    pet_.neglect_streak_ms = 0;
  }

  if (pet_.stage == LifeStage::Puppy &&
      pet_.healthy_streak_ms >= kHealthyForAdult) {
    pet_.stage = LifeStage::Adult;
    play_clip(ClipId::Fanfare);
    unlock_achievement(AchievementId::EvolvedToAdult);
    dirty_ = true;
  } else if (pet_.stage == LifeStage::Adult &&
             pet_.healthy_streak_ms >= kHealthyForSenior) {
    pet_.stage = LifeStage::Senior;
    play_clip(ClipId::Fanfare);
    unlock_achievement(AchievementId::EvolvedToSenior);
    dirty_ = true;
  }

  if (pet_.neglect_streak_ms >= kNeglectForDeath) {
    pet_.stage = LifeStage::Gone;
    pet_.mood  = Mood::Gone;
    pet_.healthy_streak_ms = 0;
    play_clip(ClipId::Sad);
    dirty_ = true;
  }
}

void Game::update_daylight(uint32_t now_ms) {
  if (clock_ && clock_->is_synced()) {
    uint64_t u = clock_->now_unix_ms();
    LocalTime lt = to_local(u, settings_.tz_offset_min);
    float f = day_fraction(lt);
    // Smooth day curve: dawn 5-7, dusk 19-21
    float d;
    if      (f < 5.0f/24)  d = 0.05f;
    else if (f < 7.0f/24)  d = (f - 5.0f/24) / (2.0f/24);   // ramp up
    else if (f < 19.0f/24) d = 1.0f;
    else if (f < 21.0f/24) d = 1.0f - (f - 19.0f/24) / (2.0f/24);
    else                   d = 0.05f;
    daylight_ = d;
    std::snprintf(clock_str_, sizeof(clock_str_), "%02d:%02d", lt.hour, lt.minute);

    // Auto-sleep at night (energy drains fast at night, encouraging rest)
    if (settings_.auto_sleep && (lt.hour >= 22 || lt.hour < 6) &&
        pet_.stats.energy > 30 && pet_.stage != LifeStage::Gone) {
      // Gentle nudge: don't suddenly knock them out, but let energy fall.
    }
  } else {
    // Fallback: 24-min synthetic cycle so day/night still happens visually.
    float t = (float)(now_ms % (24u * 60u * 1000u)) / (24.0f * 60.0f * 1000.0f);
    float d;
    if (t < 5.0f/24)       d = 0.05f;
    else if (t < 7.0f/24)  d = (t - 5.0f/24) / (2.0f/24);
    else if (t < 19.0f/24) d = 1.0f;
    else if (t < 21.0f/24) d = 1.0f - (t - 19.0f/24) / (2.0f/24);
    else                   d = 0.05f;
    daylight_ = d;
    clock_str_[0] = 0;
  }
}

void Game::check_streak(uint64_t now_unix_ms) {
  if (now_unix_ms == 0) return;
  uint32_t today = local_day_index(now_unix_ms, settings_.tz_offset_min);
  if (streak_last_visit_unix_ms_ == 0) {
    streak_days_ = 1;
    streak_last_visit_unix_ms_ = now_unix_ms;
    dirty_ = true;
    return;
  }
  uint32_t prev_day = local_day_index(streak_last_visit_unix_ms_, settings_.tz_offset_min);
  if (today == prev_day) return;             // already counted today
  if (today == prev_day + 1) {
    streak_days_++;
    // Daily bonus
    pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kStreakBonus);
    if (streak_days_ >= 3) unlock_achievement(AchievementId::Streak3Days);
    if (streak_days_ >= 7) unlock_achievement(AchievementId::Streak7Days);
  } else if (today > prev_day + 1) {
    streak_days_ = 1;
  }
  streak_last_visit_unix_ms_ = now_unix_ms;
  dirty_ = true;
}

void Game::check_achievements() {
  if (pet_.stats.hunger == 100 && pet_.stats.happiness == 100 &&
      pet_.stats.cleanliness == 100 && pet_.stats.energy == 100) {
    unlock_achievement(AchievementId::FullStats);
  }
}

void Game::tick(uint32_t now_ms) {
  uint32_t dt = now_ms - last_tick_ms_;
  if (dt > 60 * 60 * 1000u) dt = 1000u;
  last_tick_ms_ = now_ms;

  while (queued_head_ != queued_tail_) {
    Input in = queued_[queued_head_];
    queued_head_ = (uint8_t)((queued_head_ + 1) % 16);
    apply_input(in);
  }

  if (pet_.stage != LifeStage::Gone) pet_.age_ms += dt;

  apply_decay(dt);
  update_evolution(dt);
  update_mood();
  update_daylight(now_ms);
  check_achievements();

  // Streak check whenever we have a synced clock.
  if (clock_ && clock_->is_synced()) check_streak(clock_->now_unix_ms());

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
  draw_scene(r, *this, last_tick_ms_);
  if (menu_open_) draw_menu_overlay(r, *this);
}

void Game::maybe_save(Storage& storage) {
  if (!dirty_) return;
  if (last_tick_ms_ - last_save_ms_ < kSaveIntervalMs) return;
  force_save(storage);
}

void Game::force_save(Storage& storage) {
  SaveData s;
  uint64_t real_now = (clock_ && clock_->is_synced()) ? clock_->now_unix_ms() : 0;
  if (real_now) last_save_real_unix_ms_ = real_now;
  pet_to_save(pet_, settings_, achievements_, streak_days_,
              streak_last_visit_unix_ms_, last_save_real_unix_ms_,
              total_pets_, fetch_catches_, coat_pattern_, accessory_id_,
              personality_trait_, inherited_trait_, tricks_learned_,
              weather_, sickness_, scene_id_,
              memorial_, memorial_count_, memorial_head_, s);
  storage.save(s);
  dirty_ = false;
  last_save_ms_ = last_tick_ms_;
}

const char* Game::generate_sync_code() {
  // 8 bytes: hunger, happiness, cleanliness, energy, stage|coat<<4,
  // accessory, personality|inherited<<4, crc
  uint8_t buf[8];
  buf[0] = pet_.stats.hunger;
  buf[1] = pet_.stats.happiness;
  buf[2] = pet_.stats.cleanliness;
  buf[3] = pet_.stats.energy;
  buf[4] = (uint8_t)(((uint8_t)pet_.stage & 0x0F) | ((coat_pattern_ & 0x0F) << 4));
  buf[5] = accessory_id_;
  buf[6] = (uint8_t)((personality_trait_ & 0x0F) | ((inherited_trait_ & 0x0F) << 4));
  buf[7] = crc8(buf, 7);
  char raw[16];
  encode_base32(buf, 8, raw);
  // Format as 4-4-5 with dashes
  std::snprintf(sync_code_buf_, sizeof(sync_code_buf_), "%.4s-%.4s-%s",
                raw, raw + 4, raw + 8);
  return sync_code_buf_;
}

bool Game::apply_sync_code(const char* code) {
  if (!code) return false;
  uint8_t buf[8];
  int n = 0;
  if (!decode_base32(code, buf, 8, n)) return false;
  if (n < 8) return false;
  if (crc8(buf, 7) != buf[7]) return false;
  uint8_t stage = (uint8_t)(buf[4] & 0x0F);
  if (stage > (uint8_t)LifeStage::Gone) return false;
  pet_.stats.hunger      = buf[0] > 100 ? 100 : buf[0];
  pet_.stats.happiness   = buf[1] > 100 ? 100 : buf[1];
  pet_.stats.cleanliness = buf[2] > 100 ? 100 : buf[2];
  pet_.stats.energy      = buf[3] > 100 ? 100 : buf[3];
  pet_.stage             = (LifeStage)stage;
  coat_pattern_          = (uint8_t)((buf[4] >> 4) & 0x0F);
  accessory_id_          = buf[5];
  personality_trait_     = (uint8_t)(buf[6] & 0x0F);
  inherited_trait_       = (uint8_t)((buf[6] >> 4) & 0x0F);
  dirty_ = true;
  return true;
}

}  // namespace tama
