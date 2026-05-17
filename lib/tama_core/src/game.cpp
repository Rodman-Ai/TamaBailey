#include "tama/game.h"

#include <cmath>
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

const char* trick_name(Trick t) {
  switch (t) {
    case Trick::Sit:      return "Sit";
    case Trick::Shake:    return "Shake";
    case Trick::RollOver: return "Roll Over";
    case Trick::Speak:    return "Speak";
    case Trick::Spin:     return "Spin";
    default:              return "?";
  }
}

uint64_t trick_age_threshold(Trick t) {
  // Auto-learn schedule. Scale matches BAILEY_FAST_DECAY.
#if BAILEY_FAST_DECAY
  const uint64_t base[] = { 60000ULL, 180000ULL, 360000ULL, 600000ULL, 900000ULL };
#else
  // 1h, 4h, 12h, 24h, 48h
  const uint64_t base[] = {
    1ULL  * 3600 * 1000,
    4ULL  * 3600 * 1000,
    12ULL * 3600 * 1000,
    24ULL * 3600 * 1000,
    48ULL * 3600 * 1000,
  };
#endif
  int i = (int)t;
  if (i < 0 || i >= (int)Trick::COUNT) return UINT64_MAX;
  return base[i];
}

const char* toy_name(Toy t) {
  switch (t) {
    case Toy::Ball:        return "Ball";
    case Toy::Frisbee:     return "Frisbee";
    case Toy::Rope:        return "Rope";
    case Toy::SqueakyDuck: return "Squeaky";
    case Toy::Stick:       return "Stick";
    default:               return "?";
  }
}

const char* treat_name(TreatTier t) {
  switch (t) {
    case TreatTier::Biscuit: return "Biscuit";
    case TreatTier::Bacon:   return "Bacon";
    case TreatTier::Steak:   return "Steak";
    default:                 return "?";
  }
}

const char* wish_name(Wish w) {
  switch (w) {
    case Wish::Treat:  return "Wants a treat";
    case Wish::Walk:   return "Wants a walk";
    case Wish::Pet:    return "Wants petting";
    case Wish::Fetch:  return "Wants to play fetch";
    case Wish::Brush:  return "Wants a brush";
    case Wish::None:
    default:           return "";
  }
}

const char* word_name(Word w) {
  switch (w) {
    case Word::Name:    return "Bailey";
    case Word::Sit:     return "sit";
    case Word::Outside: return "outside";
    case Word::Treat:   return "treat";
    case Word::Bedtime: return "bedtime";
    default:            return "?";
  }
}

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
  if (storage.load(s) && save_validate_and_migrate(s)) {
    // Base fields
    pet_.stats.hunger      = s.hunger;
    pet_.stats.happiness   = s.happiness;
    pet_.stats.cleanliness = s.cleanliness;
    pet_.stats.energy      = s.energy;
    pet_.stage             = (LifeStage)s.life_stage;
    pet_.age_ms            = s.age_ms;
    pet_.healthy_streak_ms = s.healthy_streak_ms;
    pet_.neglect_streak_ms = s.neglect_streak_ms;
    pet_.current_action    = Action::None;
    pet_.last_pet_ms       = 0;
    // v2 fields
    settings_                  = s.settings;
    achievements_              = s.achievements;
    streak_days_               = s.streak_days;
    streak_last_visit_unix_ms_ = s.streak_last_visit_unix_ms;
    last_save_real_unix_ms_    = s.last_save_real_unix_ms;
    total_pets_                = s.total_pets;
    fetch_catches_             = s.fetch_catches;
    coat_pattern_              = s.coat_pattern;
    accessory_id_              = s.accessory_id;
    personality_trait_         = s.personality_trait;
    inherited_trait_           = s.inherited_trait;
    tricks_learned_            = s.tricks_learned;
    weather_                   = s.weather;
    sickness_                  = s.sickness;
    scene_id_                  = s.scene_id;
    for (int i = 0; i < 5; ++i) memorial_[i] = s.memorial[i];
    memorial_count_            = s.memorial_count;
    memorial_head_             = s.memorial_head;
    // v3 fields
    biscuits_                  = s.biscuits;
    toy_owned_                 = s.toy_owned ? s.toy_owned : 1;
    active_toy_                = s.active_toy < (int)Toy::COUNT ? s.active_toy : 0;
    for (int i = 0; i < 3; ++i) treats_[i] = s.treats[i];
    wish_                      = s.wish;
    wish_started_ms_           = s.wish_started_ms;
    birthday_celebrated_day_   = s.birthday_celebrated_unix_day;
    well_tucked_in_today_      = s.well_tucked_in_today;
    vocab_learned_             = s.vocab_learned;
    for (int i = 0; i < 5; ++i) trick_perf_[i] = s.trick_perf[i];
    total_steps_               = s.total_steps;
    for (int i = 0; i < 7; ++i) mood_history_[i] = s.mood_history[i];
    mood_history_head_         = s.mood_history_head;
    for (int i = 0; i < (int)Friend::COUNT; ++i) friend_visits_[i] = s.friend_visits[i];
  } else {
    // Fresh pet: roll a personality and START AS ADULT so demo features
    // (fetch, walks, tricks, accessories) are reachable immediately.
    // Use the in-game age cycler (Input::CycleAge) to switch back to
    // Puppy or jump to Senior.
    uint32_t seed = now_ms ^ 0xBADBAD;
    personality_trait_ = (uint8_t)(1 + (seed % 5));  // Playful..Shy
    pet_.stage         = LifeStage::Adult;
    pet_.age_ms        = kHealthyForAdult;        // pretend we earned it
    pet_.healthy_streak_ms = kHealthyForAdult;
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
    // Round 2: pay out 2 biscuits per achievement unlock.
    biscuits_ += 2;
    if (biscuits_ >= 50) {
      // Set the bit directly (avoid recursion through unlock_achievement).
      uint32_t b = achievement_bit(AchievementId::BiscuitTycoon);
      if (!(achievements_ & b)) achievements_ |= b;
    }
    dirty_ = true;
  }
}

void Game::apply_input(Input in) {
  // While the menu is open, short button presses cycle tabs instead of
  // performing actions on Bailey -- single mental model: button is a
  // selector, long-press is escape.
  // NPC visitor: any action-class input greets the first visitor (slot 0).
  // Grants a small bonus + clears slot 0. With two friends present, the
  // second action greets the now-promoted-to-slot-0 visitor.
  if ((npc_visit_kind_ != 0 || npc_visit_kind2_ != 0) &&
      (in == Input::Feed || in == Input::Play || in == Input::Clean ||
       in == Input::PetTap || in == Input::Walk || in == Input::Brush)) {
    pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 5);
    biscuits_ += 1;
    if (npc_visit_kind_ != 0) {
      // Promote slot 1 -> slot 0 (if present) so the next greet hits it.
      npc_visit_kind_ = npc_visit_kind2_;
      npc_visit_ms_   = npc_visit_ms2_;
      npc_visit_kind2_ = 0;
      npc_visit_ms2_   = 0;
    } else {
      npc_visit_kind2_ = 0;
      npc_visit_ms2_   = 0;
    }
    dirty_ = true;
    // Fall through so the actual action also runs.
  }

  if (menu_open_ && (in == Input::Feed || in == Input::Play || in == Input::Clean)) {
    // Tab-specific A/B handling. C always cycles to next tab.
    if (menu_tab_ == MenuTab::Shop) {
      if (in == Input::Feed) { buy_item(shop_cursor_); return; }
      if (in == Input::Play) {
        shop_cursor_ = (uint8_t)((shop_cursor_ + 1) % 15);
        return;
      }
    } else if (menu_tab_ == MenuTab::Actions) {
      // 12 rows; A executes, B moves cursor down. C falls through.
      if (in == Input::Feed) {
        static const Input kActions[12] = {
          Input::Walk, Input::Play, Input::TreatGive, Input::Brush,
          Input::CycleToy, Input::Bedtime,
          Input::VoiceSit, Input::VoiceCome, Input::VoiceHighFive,
          Input::VoiceRollOver, Input::VoiceJump,
          Input::PlayWithFriend,
        };
        Input chosen = kActions[actions_cursor_ % 12];
        // Close the menu before dispatching so the action plays
        // unobscured.
        menu_open_ = false;
        apply_input(chosen);
        return;
      }
      if (in == Input::Play) {
        actions_cursor_ = (uint8_t)((actions_cursor_ + 1) % 12);
        return;
      }
    }
    menu_tab_ = next_menu_tab(menu_tab_);
    return;
  }

  // While a MovingOut / Magic transition is playing, user inputs are
  // suppressed so the animation can finish unmolested. The transition
  // auto-completes from tick().
  if (in_transition()) {
    return;
  }

  // Manual Restart (debug only): instantly restart the pet without a
  // transition. Hidden from the UI but still accepted from the queue.
  if (in == Input::Restart) {
    restart_pet(false);
    return;
  }

  switch (in) {
    case Input::Feed: {
      uint32_t boost = kActionEatBoost;
      if (well_tucked_in_today_) boost *= 2;  // bedtime routine bonus
      pet_.stats.hunger = clamp_stat((int)pet_.stats.hunger + boost);
      pet_.current_action = Action::Eat;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Yip);
      unlock_achievement(AchievementId::FirstFeed);
      // Bailey's wish counted as treat-adjacent: if asked for treat, feeding
      // doesn't fulfill it (treat-specific Input::TreatGive does).
      dirty_ = true;
      break;
    }
    case Input::Play: {
      // While in a fetch flow, button is treated as the "catch" press.
      if (mode_ == GameMode::FetchCatching) {
        // Hit -- award full happiness + skill increment
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kActionPlayBoost);
        pet_.stats.energy    = clamp_stat((int)pet_.stats.energy - kEnergyCostPlay);
        fetch_catches_++;
        last_fetch_result_   = 1;
        mode_                = GameMode::FetchResult;
        mode_started_ms_     = last_tick_ms_;
        play_clip(ClipId::Wuff);
        unlock_achievement(AchievementId::FirstPlay);
        if (fetch_catches_ >= 10) unlock_achievement(AchievementId::FetchPro);
        fulfill_wish_if_matches(Wish::Fetch);
        dirty_ = true;
        break;
      }
      // Puppy or sick: skip the fetch flow, do classic Play.
      if (pet_.stage == LifeStage::Puppy || sickness_ != 0) {
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + kActionPlayBoost);
        pet_.stats.energy    = clamp_stat((int)pet_.stats.energy - kEnergyCostPlay);
        pet_.current_action  = Action::Play;
        pet_.action_started_ms = last_tick_ms_;
        play_clip(ClipId::Wuff);
        unlock_achievement(AchievementId::FirstPlay);
        dirty_ = true;
        break;
      }
      // Adult+: start a fetch flow.
      if (mode_ == GameMode::Idle) {
        mode_              = GameMode::FetchAiming;
        mode_started_ms_   = last_tick_ms_;
        pet_.current_action = Action::Play;
        pet_.action_started_ms = last_tick_ms_;
        play_clip(ClipId::Yip);
        dirty_ = true;
      }
      break;
    }
    case Input::Clean:
      pet_.stats.cleanliness = clamp_stat((int)pet_.stats.cleanliness + kActionCleanBoost);
      pet_.current_action = Action::Clean;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Splash);
      unlock_achievement(AchievementId::FirstClean);
      // A bath also doubles as the cure for sickness.
      if (sickness_ != 0) try_cure_sickness();
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
        fulfill_wish_if_matches(Wish::Pet);
        // If Bailey knows tricks, he performs one in delight.
        perform_random_trick();
        dirty_ = true;
      }
      break;
    }
    case Input::Walk: {
      if (mode_ == GameMode::Walking) {
        // Each Walk press during Walking = a step.
        walk_steps_++;
        total_steps_++;
        pet_.stats.energy = clamp_stat((int)pet_.stats.energy - 1);
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 1);
        // 1/8 chance to find an item per step.
        uint32_t r = (last_tick_ms_ + (uint32_t)total_steps_) * 2654435761u;
        if ((r & 7) == 0) {
          int kind = (r >> 3) % 3;
          if (kind == 0) {
            grant_biscuits(1);                          // bone
          } else if (kind == 1 && (toy_owned_ != kAllToysMask)) {
            // Unlock a random missing toy
            for (int i = 0; i < (int)Toy::COUNT; ++i) {
              uint8_t bit = 1u << ((i + (r >> 6)) % (int)Toy::COUNT);
              if (!(toy_owned_ & bit)) { toy_owned_ |= bit; break; }
            }
          } else {
            treats_[r & 3 ? 0 : 1]++;                   // biscuit treat usually
          }
        }
        fulfill_wish_if_matches(Wish::Walk);
        dirty_ = true;
        break;
      }
      // Otherwise: start a walk (Adult+ only, idle, has energy).
      if (pet_.stage != LifeStage::Puppy && mode_ == GameMode::Idle &&
          pet_.stats.energy >= 30) {
        mode_              = GameMode::Walking;
        mode_started_ms_   = last_tick_ms_;
        walk_steps_        = 0;
        walk_target_       = 20;
        pet_.current_action = Action::Play;
        pet_.action_started_ms = last_tick_ms_;
        dirty_ = true;
      }
      break;
    }
    case Input::TreatGive: {
      // Give the highest-tier treat available.
      int tier = -1;
      for (int i = (int)TreatTier::COUNT - 1; i >= 0; --i) {
        if (treats_[i] > 0) { tier = i; break; }
      }
      if (tier < 0) break;  // no treats in inventory
      treats_[tier]--;
      uint32_t boost = (uint32_t)(5 + tier * 7);    // 5/12/19
      uint32_t food  = (uint32_t)(3 + tier * 4);    // 3/7/11
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + boost);
      pet_.stats.hunger    = clamp_stat((int)pet_.stats.hunger    + food);
      pet_.current_action  = Action::Eat;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Yip);
      fulfill_wish_if_matches(Wish::Treat);
      dirty_ = true;
      break;
    }
    case Input::Brush: {
      pet_.stats.cleanliness = clamp_stat((int)pet_.stats.cleanliness + 15);
      pet_.stats.happiness   = clamp_stat((int)pet_.stats.happiness   +  3);
      pet_.current_action    = Action::Clean;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Heart);
      fulfill_wish_if_matches(Wish::Brush);
      dirty_ = true;
      break;
    }
    case Input::CycleToy: {
      for (int i = 1; i <= (int)Toy::COUNT; ++i) {
        int cand = (active_toy_ + i) % (int)Toy::COUNT;
        if (toy_owned_ & (1u << cand)) { active_toy_ = (uint8_t)cand; dirty_ = true; break; }
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
      if (menu_open_) menu_tab_ = next_menu_tab(menu_tab_);
      break;
    case Input::CycleScene: {
      uint8_t next = (uint8_t)((settings_.scene_id + 1) % 3);
      settings_.scene_id = next;
      scene_id_ = next;
      // Track scenic-tour achievement via a side bitmask in settings._pad.
      // (Cheap: reuse byte; bit 0 = visited #0, etc.)
      settings_._pad |= (uint8_t)(1u << next);
      if ((settings_._pad & 0x07) == 0x07)
        unlock_achievement(AchievementId::ScenicTour);
      dirty_ = true;
      break;
    }
    case Input::CycleCoat: {
      uint8_t next = (uint8_t)((coat_pattern_ + 1) % 5);
      choose_coat(next);
      break;
    }
    case Input::CycleAccessory: {
      // Cycle through unlocked ones only (0=bare always allowed).
      for (uint8_t try_n = 0; try_n < 4; ++try_n) {
        uint8_t cand = (uint8_t)((accessory_id_ + 1 + try_n) % 4);
        if (accessory_unlocked(cand)) {
          equip_accessory(cand);
          break;
        }
      }
      break;
    }
    case Input::TakePhoto:
      // Phase 3 photo mode handled on the web side via canvas.toDataURL.
      // Core just records the achievement.
      unlock_achievement(AchievementId::PhotoFan);
      dirty_ = true;
      break;
    case Input::CycleAge: {
      // Cycle Puppy -> Adult -> Senior -> Puppy. Skip Gone (no death).
      LifeStage next;
      switch (pet_.stage) {
        case LifeStage::Puppy:  next = LifeStage::Adult;  break;
        case LifeStage::Adult:  next = LifeStage::Senior; break;
        case LifeStage::Senior: next = LifeStage::Puppy;  break;
        default:                next = LifeStage::Adult;  break;
      }
      pet_.stage = next;
      // Set age_ms / healthy_streak_ms to the floor of the new stage so
      // evolution thresholds don't immediately fire.
      switch (next) {
        case LifeStage::Puppy:
          pet_.age_ms = 0;
          pet_.healthy_streak_ms = 0;
          break;
        case LifeStage::Adult:
          pet_.age_ms = kHealthyForAdult;
          pet_.healthy_streak_ms = kHealthyForAdult;
          break;
        case LifeStage::Senior:
          pet_.age_ms = kHealthyForSenior;
          pet_.healthy_streak_ms = kHealthyForSenior;
          break;
        default: break;
      }
      dirty_ = true;
      break;
    }
    case Input::ImuFlick: {
      // IMU forward flick: if Adult+ and aiming a fetch throw, jump
      // straight to the in-flight phase. Otherwise start a fetch (which
      // is what Play does on Adult+).
      if (mode_ == GameMode::FetchAiming) {
        mode_            = GameMode::FetchInFlight;
        mode_started_ms_ = last_tick_ms_;
      } else {
        apply_input(Input::Play);  // chain to normal Play handler
      }
      break;
    }
    case Input::VoiceSit:
    case Input::VoiceCome:
    case Input::VoiceHighFive:
    case Input::VoiceRollOver:
    case Input::VoiceJump: {
      uint8_t kind = (uint8_t)((int)in - (int)Input::VoiceSit + 1);  // 1..5
      voice_trick_kind_       = kind;
      voice_trick_started_ms_ = last_tick_ms_;
      // Bailey performs a trick: log into trick_perf so the favorite
      // tracker counts it (re-uses the existing Trick enum: Sit=0,
      // Shake=1, RollOver=2, Speak=3, Spin=4; we map HighFive->Shake,
      // Come->Speak, Jump->Spin).
      static const Trick kMap[6] = {
        Trick::Sit, Trick::Sit, Trick::Speak,
        Trick::Shake, Trick::RollOver, Trick::Spin,
      };
      Trick t = kMap[kind];
      trick_perf_[(int)t]++;
      pet_.stats.happiness   = clamp_stat((int)pet_.stats.happiness + 5);
      pet_.current_action    = Action::Pet;
      pet_.action_started_ms = last_tick_ms_;
      ClipId clip = (kind == 4 /*RollOver*/) ? ClipId::Yip : ClipId::Wuff;
      play_clip(clip);
      unlock_achievement(AchievementId::LearnedFirstTrick);
      dirty_ = true;
      break;
    }
    case Input::Bedtime: {
      // Manual tuck-in: force the bedtime bonus regardless of clock.
      well_tucked_in_today_ = 1;
      pet_.stats.happiness  = clamp_stat((int)pet_.stats.happiness + 3);
      play_clip(ClipId::Heart);
      dirty_ = true;
      break;
    }
    case Input::MenuCursorNext:
      if (menu_open_ && menu_tab_ == MenuTab::Actions) {
        actions_cursor_ = (uint8_t)((actions_cursor_ + 1) % 12);
      }
      break;
    case Input::PlayWithFriend:
    case Input::PlayWithFriendOllie:
    case Input::PlayWithFriendMitchell:
    case Input::PlayWithFriendEnzo:
    case Input::PlayWithFriendLincoln:
    case Input::PlayWithFriendRuben: {
      Friend f;
      if (in == Input::PlayWithFriend) {
        // Hash now -> pick one of the five friends.
        uint32_t r = (uint32_t)last_tick_ms_ * 2654435761u;
        f = (Friend)((r >> 8) % (int)Friend::COUNT);
      } else {
        f = (Friend)((int)in - (int)Input::PlayWithFriendOllie);
      }
      uint8_t want = (uint8_t)((int)f + 1);
      // Slot allocation policy:
      //   1. If friend already in a slot -> refresh that slot.
      //   2. Else if an empty slot exists -> fill it.
      //   3. Else (both filled) -> replace the OLDER slot.
      bool refreshed = false;
      if (npc_visit_kind_ == want) {
        npc_visit_ms_   = last_tick_ms_;
        refreshed = true;
      } else if (npc_visit_kind2_ == want) {
        npc_visit_ms2_  = last_tick_ms_;
        refreshed = true;
      }
      if (!refreshed) {
        if (npc_visit_kind_ == 0) {
          npc_visit_kind_ = want;
          npc_visit_ms_   = last_tick_ms_;
        } else if (npc_visit_kind2_ == 0) {
          npc_visit_kind2_ = want;
          npc_visit_ms2_   = last_tick_ms_;
        } else {
          // Replace the older slot.
          if (npc_visit_ms_ <= npc_visit_ms2_) {
            npc_visit_kind_ = want;
            npc_visit_ms_   = last_tick_ms_;
          } else {
            npc_visit_kind2_ = want;
            npc_visit_ms2_   = last_tick_ms_;
          }
        }
        // Only count + award on a fresh arrival, not a refresh.
        friend_visits_[(int)f]++;
        unlock_achievement(AchievementId::PlayDate);
        bool met_all = true;
        for (int i = 0; i < (int)Friend::COUNT; ++i)
          if (friend_visits_[i] == 0) { met_all = false; break; }
        if (met_all) unlock_achievement(AchievementId::Socialite);
      }
      // Pet animation pulse + clip
      pet_.current_action    = Action::Pet;
      pet_.action_started_ms = last_tick_ms_;
      pet_.stats.happiness   = clamp_stat((int)pet_.stats.happiness + 8);
      play_clip(ClipId::Wuff);
      dirty_ = true;
      break;
    }
    case Input::MicTrigger:
      // Loud sound: pet Bailey (with the usual cooldown) AND mark achievement.
      unlock_achievement(AchievementId::CalledByName);
      apply_input(Input::PetTap);  // chain to the pet logic
      break;
    case Input::Restart:
      // Handled above when stage == Gone
      break;
    case Input::None:
      break;
  }
}

void Game::apply_decay(uint32_t dt_ms) {
  if (in_transition()) return;

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
  if (in_transition()) return;  // keep the transition mood pinned
  if (pet_.stats.energy < 20)        { pet_.mood = Mood::Sleeping; return; }
  if (pet_.stats.any_zero())         { pet_.mood = Mood::Sad; return; }
  if (pet_.stats.cleanliness < 30)   { pet_.mood = Mood::Dirty; return; }
  if (pet_.stats.hunger < 30)        { pet_.mood = Mood::Hungry; return; }
  if (pet_.stats.all_above(50))      { pet_.mood = Mood::Happy; return; }
  pet_.mood = Mood::Neutral;
}

void Game::update_evolution(uint32_t dt_ms) {
  // No-op during transitions; tick() finishes them.
  if (in_transition()) return;

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
    if (coat_pattern_ == 0) {
      mode_ = GameMode::PickingCoat;
      mode_started_ms_ = last_tick_ms_;
    }
    dirty_ = true;
  } else if (pet_.stage == LifeStage::Adult &&
             pet_.healthy_streak_ms >= kHealthyForSenior) {
    pet_.stage = LifeStage::Senior;
    play_clip(ClipId::Fanfare);
    unlock_achievement(AchievementId::EvolvedToSenior);
    dirty_ = true;
  }

  // Death replaced with two narrative loops:
  //   - Sustained neglect: Bailey moves in with a different family.
  //   - Long healthy life as Senior: Bailey magically returns to puppyhood.
  if (pet_.neglect_streak_ms >= kNeglectForMoveOut) {
    play_clip(ClipId::Sad);
    enter_transition(Mood::MovingOut);
  } else if (pet_.stage == LifeStage::Senior &&
             pet_.healthy_streak_ms >= kHealthyForSenior + kSeniorLoopMs) {
    play_clip(ClipId::Fanfare);
    enter_transition(Mood::Magic);
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
        pet_.stats.energy > 30 && !in_transition()) {
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

  if (!in_transition()) pet_.age_ms += dt;

  apply_decay(dt);
  update_evolution(dt);
  update_mood();
  update_daylight(now_ms);
  check_achievements();
  update_fetch_mode(now_ms);
  update_walk(now_ms);
  update_wish(now_ms);
  update_sickness(dt);
  update_tricks();
  update_vocab();
  update_ambient(now_ms);

  // Streak check + weather roll + birthday/bedtime whenever we have a
  // synced clock.
  if (clock_ && clock_->is_synced()) {
    uint64_t u = clock_->now_unix_ms();
    check_streak(u);
    update_weather(u);
    update_birthday(u);
    update_bedtime(u);
    roll_over_day_if_needed(u);
  }

  // Sample today's happiness for the daily-average rollover.
  today_happiness_sum_ += pet_.stats.happiness;
  today_samples_       += 1;
  if (today_samples_ == 0) {  // overflow guard (every ~65k ticks: rescale)
    today_happiness_sum_ /= 2;
    today_samples_       = 32768;
  }

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
    if (elapsed > dur) {
      pet_.current_action = Action::None;
      voice_trick_kind_   = 0;   // clear any in-flight voice trick
    }
  }

  // Finish MovingOut / Magic transitions once the dwell time elapses.
  if (in_transition() && (now_ms - transition_started_ms_) >= kTransitionMs) {
    bool magic = (pet_.mood == Mood::Magic);
    restart_pet(magic);
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
  uint64_t real_now = (clock_ && clock_->is_synced()) ? clock_->now_unix_ms() : 0;
  if (real_now) last_save_real_unix_ms_ = real_now;

  SaveData s{};
  s.magic                   = kSaveMagic;
  s.version                 = kSaveVersion;
  s.hunger                  = pet_.stats.hunger;
  s.happiness               = pet_.stats.happiness;
  s.cleanliness             = pet_.stats.cleanliness;
  s.energy                  = pet_.stats.energy;
  s.life_stage              = (uint8_t)pet_.stage;
  s.age_ms                  = pet_.age_ms;
  s.healthy_streak_ms       = pet_.healthy_streak_ms;
  s.neglect_streak_ms       = pet_.neglect_streak_ms;
  s.settings                = settings_;
  s.achievements            = achievements_;
  s.streak_days             = streak_days_;
  s.streak_last_visit_unix_ms = streak_last_visit_unix_ms_;
  s.last_save_real_unix_ms  = last_save_real_unix_ms_;
  s.total_pets              = total_pets_;
  s.fetch_catches           = fetch_catches_;
  s.coat_pattern            = coat_pattern_;
  s.accessory_id            = accessory_id_;
  s.personality_trait       = personality_trait_;
  s.inherited_trait         = inherited_trait_;
  s.tricks_learned          = tricks_learned_;
  s.weather                 = weather_;
  s.sickness                = sickness_;
  s.scene_id                = scene_id_;
  for (int i = 0; i < 5; ++i) s.memorial[i] = memorial_[i];
  s.memorial_count          = memorial_count_;
  s.memorial_head           = memorial_head_;
  s.biscuits                = biscuits_;
  s.toy_owned               = toy_owned_;
  s.active_toy              = active_toy_;
  for (int i = 0; i < 3; ++i) s.treats[i] = treats_[i];
  s.wish                    = wish_;
  s.wish_started_ms         = wish_started_ms_;
  s.birthday_celebrated_unix_day = birthday_celebrated_day_;
  s.well_tucked_in_today    = well_tucked_in_today_;
  s.vocab_learned           = vocab_learned_;
  for (int i = 0; i < 5; ++i) s.trick_perf[i] = trick_perf_[i];
  s.total_steps             = total_steps_;
  for (int i = 0; i < 7; ++i) s.mood_history[i] = mood_history_[i];
  s.mood_history_head       = mood_history_head_;
  for (int i = 0; i < (int)Friend::COUNT; ++i) s.friend_visits[i] = friend_visits_[i];

  storage.save(s);
  dirty_ = false;
  last_save_ms_ = last_tick_ms_;
}

// ===== Phase 2 systems =====

void Game::update_fetch_mode(uint32_t now_ms) {
  if (mode_ == GameMode::Idle) return;
  uint32_t elapsed = now_ms - mode_started_ms_;
  constexpr uint32_t kAimMs       =  700;
  constexpr uint32_t kFlightMs    = 1000;
  constexpr uint32_t kCatchWinMs  =  500;
  constexpr uint32_t kResultMs    =  900;
  switch (mode_) {
    case GameMode::FetchAiming:
      if (elapsed >= kAimMs) {
        mode_ = GameMode::FetchInFlight;
        mode_started_ms_ = now_ms;
      }
      break;
    case GameMode::FetchInFlight:
      if (elapsed >= kFlightMs) {
        mode_ = GameMode::FetchCatching;
        mode_started_ms_ = now_ms;
      }
      break;
    case GameMode::FetchCatching:
      if (elapsed >= kCatchWinMs) {
        // Missed: small consolation happiness
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 8);
        last_fetch_result_   = 2;
        mode_                = GameMode::FetchResult;
        mode_started_ms_     = now_ms;
        dirty_ = true;
      }
      break;
    case GameMode::FetchResult:
      if (elapsed >= kResultMs) {
        mode_ = GameMode::Idle;
        pet_.current_action = Action::None;
        last_fetch_result_ = 0;
      }
      break;
    case GameMode::PickingCoat:
      // Stays open until player picks
      break;
    case GameMode::PhotoMode:
      // Phase 3
      break;
    case GameMode::Walking:
      // Walk progression is handled in update_walk(); nothing to do here.
      break;
    case GameMode::Idle:
      break;
  }
}

void Game::update_weather(uint64_t now_unix_ms) {
  uint32_t today = local_day_index(now_unix_ms, settings_.tz_offset_min);
  if (today == last_weather_roll_day_) return;
  last_weather_roll_day_ = today;
  // Bias toward sunny: 0..15 sunny, 16..23 cloudy, 24..28 rain, 29..31 snow
  uint32_t r = today * 2654435761u;
  uint8_t roll = (uint8_t)(r % 32);
  uint8_t w;
  if      (roll < 16) w = (uint8_t)Weather::Sunny;
  else if (roll < 24) w = (uint8_t)Weather::Cloudy;
  else if (roll < 29) w = (uint8_t)Weather::Rain;
  else                w = (uint8_t)Weather::Snow;
  weather_ = w;
  // Visiting through bad weather counts toward achievement
  if (w == (uint8_t)Weather::Rain || w == (uint8_t)Weather::Snow)
    unlock_achievement(AchievementId::WeatheredTheStorm);
  dirty_ = true;
}

void Game::update_sickness(uint32_t dt_ms) {
  if (in_transition()) { sickness_ = 0; return; }
  // Sick when both food and bath stay low for a sustained period.
  bool danger = pet_.stats.hunger < 20 && pet_.stats.cleanliness < 20;
  if (danger) {
    sick_started_ms_ += dt_ms;
#if BAILEY_FAST_DECAY
    constexpr uint32_t kSickThreshold = 5000;
#else
    constexpr uint32_t kSickThreshold = 5u * 60 * 1000;  // 5 min
#endif
    if (sickness_ == 0 && sick_started_ms_ >= kSickThreshold) {
      sickness_ = 1;
      play_clip(ClipId::Sneeze);
      dirty_ = true;
    }
  } else {
    sick_started_ms_ = 0;
  }
}

void Game::try_cure_sickness() {
  if (sickness_ == 0) return;
  sickness_ = 0;
  sick_started_ms_ = 0;
  pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 10);
  play_clip(ClipId::Heart);
  unlock_achievement(AchievementId::SurvivedSickness);
  dirty_ = true;
}

void Game::update_tricks() {
  if (in_transition()) return;
  uint64_t age = pet_.age_ms;
  uint8_t before = tricks_learned_;
  // Clever personality halves trick thresholds.
  uint64_t scale_num = ((Personality)personality_trait_ == Personality::Clever) ? 1 : 2;
  uint64_t scale_den = 2;
  for (int i = 0; i < (int)Trick::COUNT; ++i) {
    uint64_t thr = trick_age_threshold((Trick)i) * scale_num / scale_den;
    if (age >= thr) tricks_learned_ |= (1u << i);
  }
  if (tricks_learned_ != before) {
    unlock_achievement(AchievementId::LearnedFirstTrick);
    if (tricks_learned_ == kAllTricksMask)
      unlock_achievement(AchievementId::LearnedAllTricks);
    dirty_ = true;
  }
}

void Game::equip_accessory(uint8_t id) {
  if (id != 0 && !accessory_unlocked(id)) return;
  accessory_id_ = id;
  if (id != 0) unlock_achievement(AchievementId::Dapper);
  dirty_ = true;
}

void Game::choose_coat(uint8_t id) {
  if (id > 4) return;
  coat_pattern_ = id;
  if (mode_ == GameMode::PickingCoat) {
    mode_ = GameMode::Idle;
  }
  dirty_ = true;
}

// ===== Round 2 systems =====

void Game::grant_biscuits(uint32_t n) {
  uint32_t before = biscuits_;
  biscuits_ += n;
  if (before < 50 && biscuits_ >= 50) unlock_achievement(AchievementId::BiscuitTycoon);
  dirty_ = true;
}

void Game::fulfill_wish_if_matches(Wish what) {
  if (wish_ == 0 || (Wish)wish_ != what) return;
  pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 10);
  grant_biscuits(1);
  wish_ = 0;
  wish_started_ms_ = 0;
  // Track via streak_last_visit (reused) to count fulfilled wishes:
  static uint16_t fulfilled = 0;  // process-local; the achievement is a one-shot
  if (++fulfilled >= 5) unlock_achievement(AchievementId::WishGranter);
  dirty_ = true;
}

void Game::update_wish(uint32_t now_ms) {
  if (in_transition()) { wish_ = 0; return; }
#if BAILEY_FAST_DECAY
  constexpr uint64_t kWishIntervalMs = 60ULL * 1000;   // 1 min in fast mode
#else
  constexpr uint64_t kWishIntervalMs = 30ULL * 60 * 1000;  // 30 min
#endif
  // If no wish, periodically roll one.
  if (wish_ == 0) {
    if (last_tick_ms_ - (uint32_t)wish_started_ms_ > kWishIntervalMs) {
      uint32_t r = now_ms * 2654435761u + (uint32_t)pet_.age_ms;
      wish_ = (uint8_t)(1 + (r % 5));   // Treat..Brush
      wish_started_ms_ = last_tick_ms_;
      dirty_ = true;
    }
  } else {
    // Expire stale wishes.
    if (last_tick_ms_ - (uint32_t)wish_started_ms_ > kWishIntervalMs * 2) {
      wish_ = 0;
      wish_started_ms_ = last_tick_ms_;
    }
  }
}

void Game::update_walk(uint32_t now_ms) {
  // Per-slot auto-leave once a friend's visit timer expires. Independent
  // from the random ambient spawn below.
  if (npc_visit_kind_ != 0 && now_ms - npc_visit_ms_ > kFriendVisitMs) {
    // Promote slot 1 -> slot 0 if present so slot order stays compact.
    npc_visit_kind_  = npc_visit_kind2_;
    npc_visit_ms_    = npc_visit_ms2_;
    npc_visit_kind2_ = 0;
    npc_visit_ms2_   = 0;
    dirty_ = true;
  }
  if (npc_visit_kind2_ != 0 && now_ms - npc_visit_ms2_ > kFriendVisitMs) {
    npc_visit_kind2_ = 0;
    npc_visit_ms2_   = 0;
    dirty_ = true;
  }

  // Rare ambient visit: only spawns when slot 0 is free (we don't fire
  // two random visitors at once). Player-invited visits can still fill
  // slot 1.
  if (!in_transition() && mode_ == GameMode::Idle && npc_visit_kind_ == 0) {
    uint32_t r = (now_ms ^ 0x5A5A5A5A) * 2654435761u;
#if BAILEY_FAST_DECAY
    constexpr uint32_t kInvProb = 200;       // dense in fast mode
#else
    constexpr uint32_t kInvProb = 10000;     // ~once / 3 min at 60fps
#endif
    if ((r % kInvProb) == 0) {
      npc_visit_kind_ = (uint8_t)(1 + ((r >> 8) % (int)Friend::COUNT));
      npc_visit_ms_   = now_ms;
      dirty_ = true;
    }
  }

  if (mode_ != GameMode::Walking) return;
  if (pet_.stats.energy < 10 || walk_steps_ >= walk_target_) {
    if (total_steps_ >= 100)
      unlock_achievement(AchievementId::WalkOfALifetime);
    mode_ = GameMode::Idle;
    pet_.current_action = Action::None;
    walk_steps_ = 0;
    walk_target_ = 0;
    dirty_ = true;
  }
}

void Game::update_birthday(uint64_t now_unix_ms) {
  if (now_unix_ms == 0) return;
  LocalTime lt = to_local(now_unix_ms, settings_.tz_offset_min);
#ifndef BAILEY_BIRTHDAY_MONTH
#define BAILEY_BIRTHDAY_MONTH 1
#endif
#ifndef BAILEY_BIRTHDAY_DAY
#define BAILEY_BIRTHDAY_DAY 13
#endif
  bool birthday  = (lt.month == BAILEY_BIRTHDAY_MONTH && lt.day == BAILEY_BIRTHDAY_DAY);
  bool halloween = (lt.month == 10 && lt.day == 31);
  bool christmas = (lt.month == 12 && lt.day == 25);
  is_birthday_today_ = birthday;
  active_holiday_    = birthday ? 1 : (halloween ? 2 : (christmas ? 3 : 0));

  if (birthday) {
    uint32_t day = local_day_index(now_unix_ms, settings_.tz_offset_min);
    if (birthday_celebrated_day_ != day) {
      birthday_celebrated_day_ = day;
      if (accessory_id_ == 0 && accessory_unlocked(3)) accessory_id_ = 3;
      unlock_achievement(AchievementId::BirthdayBoy);
      play_clip(ClipId::Fanfare);
      dirty_ = true;
    }
  }
  if (halloween || christmas) {
    unlock_achievement(AchievementId::SeasonalGreetings);
    if (christmas) weather_ = (uint8_t)Weather::Snow;
  }
}

void Game::update_bedtime(uint64_t now_unix_ms) {
  if (now_unix_ms == 0) return;
  LocalTime lt = to_local(now_unix_ms, settings_.tz_offset_min);
  // Bedtime window: 20:00-21:00 local time. Mark tucked-in if Bailey
  // got attention during that hour.
  if (lt.hour == 20 && pet_.current_action != Action::None) {
    well_tucked_in_today_ = 1;
    dirty_ = true;
  }
  // Reset flag at noon so it applies once per night.
  if (lt.hour == 12 && lt.minute == 0) {
    well_tucked_in_today_ = 0;
  }
}

// Ambient idle behaviors --------------------------------------------------

void Game::update_ambient(uint32_t now_ms) {
  if (mode_ != GameMode::Idle) return;
  if (pet_.current_action != Action::None) return;
  if (in_transition()) return;
  if (pet_.mood == Mood::Sleeping || pet_.mood == Mood::Sad) {
    ambient_behavior_   = 0;   // pinned to stand for these moods
    ambient_x_offset_   = 0;
    return;
  }

#if BAILEY_FAST_DECAY
  constexpr uint32_t kIntervalMs = 2000;
#else
  constexpr uint32_t kIntervalMs = 10000;
#endif

  uint32_t elapsed = now_ms - ambient_started_ms_;

  // Drive walking behavior's x-offset every tick (interpolates smoothly).
  if (ambient_behavior_ == 1) {
    // 4-second slide; ramp x_offset from 0 -> 32 (or -32) then back.
    float t = (float)elapsed / 4000.0f;
    if (t > 1.0f) t = 1.0f;
    int max_off = 32;
    // Triangle wave: 0 -> max at t=0.5 -> 0 at t=1
    int off = (int)(max_off * (1.0f - fabsf(t * 2 - 1.0f)));
    ambient_x_offset_ = (int16_t)(off * ambient_walk_dir_);
    if (t >= 1.0f) {
      // Walk done -> back to stand for the rest of the interval.
      ambient_behavior_ = 0;
      ambient_x_offset_ = 0;
    }
  } else {
    ambient_x_offset_ = 0;
  }

  // Periodically roll a new behavior.
  if (elapsed >= kIntervalMs) {
    ambient_started_ms_ = now_ms;
    uint32_t r = (now_ms * 2654435761u) >> 11;
    uint8_t  pct = (uint8_t)(r % 100);
    uint8_t  next;
    if      (pct < 50) next = 0;  // stand
    else if (pct < 70) next = 1;  // walk
    else if (pct < 85) next = 2;  // sit
    else if (pct < 95) next = 3;  // pant
    else               next = 4;  // bark

    ambient_behavior_ = next;
    if (next == 1) {
      ambient_walk_dir_ = (r & 1) ? 1 : -1;
    } else {
      ambient_x_offset_ = 0;
    }
    if (next == 4) {
      // Bark plays the Wuff clip.
      play_clip(ClipId::Wuff);
    }
  }
}

// Death-removal helpers ----------------------------------------------------

void Game::enter_transition(Mood m) {
  pet_.mood              = m;
  transition_started_ms_ = last_tick_ms_;
  // Pick which family Bailey moves to (cosmetic only).
  uint32_t h = (uint32_t)(pet_.age_ms ^ ((uint64_t)last_tick_ms_ << 16));
  move_out_family_idx_   = (uint8_t)((h * 2654435761u) >> 29);  // 0..7
  // Reset the streak counters so the new puppy doesn't immediately
  // re-trigger another transition.
  pet_.neglect_streak_ms = 0;
  pet_.healthy_streak_ms = 0;
  dirty_ = true;
}

void Game::restart_pet(bool magic) {
#if BAILEY_MEMORIAL_WALL
  // Memorial wall (gated off by default) keeps a record of past Baileys.
  SaveData::MemorialEntry m{};
  m.coat              = coat_pattern_;
  m.trait             = personality_trait_;
  m.peak_stage        = (uint8_t)pet_.stage;
  m.age_minutes       = (uint32_t)(pet_.age_ms / 60000ULL);
  m.achievements_mask = achievements_;
  memorial_[memorial_head_] = m;
  memorial_head_ = (uint8_t)((memorial_head_ + 1) % 5);
  if (memorial_count_ < 5) memorial_count_++;
#endif

  uint8_t parent_trait = personality_trait_;
  pet_   = Pet{};
  pet_.stats = Stats{};
  // Settings, achievements, biscuits, toys, treats, vocab, etc. all
  // intentionally KEPT -- only the pet itself resets.
  sickness_ = 0;
  // Magic loop keeps the same coat/personality; move-out picks fresh.
  if (magic) {
    inherited_trait_ = personality_trait_;
  } else {
    coat_pattern_ = 0;
    accessory_id_ = 0;
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
  }
  transition_started_ms_ = 0;
  move_out_family_idx_   = 0;
  dirty_ = true;
}

Trick Game::favorite_trick() const {
  uint16_t best = 0;
  int best_i = 0;
  for (int i = 0; i < (int)Trick::COUNT; ++i) {
    if (trick_perf_[i] > best) { best = trick_perf_[i]; best_i = i; }
  }
  return (Trick)best_i;
}

uint8_t Game::mood_history(uint8_t day_back) const {
  if (day_back >= 7) return 0;
  // mood_history_head_ points to the SLOT we'd next write into; the most
  // recent completed entry is at (head - 1) mod 7.
  int idx = ((int)mood_history_head_ - 1 - (int)day_back + 14) % 7;
  return mood_history_[idx];
}

void Game::roll_over_day_if_needed(uint64_t now_unix_ms) {
  if (now_unix_ms == 0) return;
  uint32_t day = local_day_index(now_unix_ms, settings_.tz_offset_min);
  if (today_day_index_ == 0) {
    today_day_index_ = day;
    return;
  }
  if (day == today_day_index_) return;

  // Roll over: write yesterday's average happiness into the ring buffer.
  uint8_t avg = today_samples_ > 0
                ? (uint8_t)(today_happiness_sum_ / today_samples_)
                : 0;
  mood_history_[mood_history_head_] = avg;
  mood_history_head_ = (uint8_t)((mood_history_head_ + 1) % 7);

  // Reset bedtime flag once per day (was tied to a noon check before).
  well_tucked_in_today_ = 0;

  today_day_index_     = day;
  today_happiness_sum_ = 0;
  today_samples_       = 0;
  today_actions_       = 0;
  dirty_ = true;
}

void Game::perform_random_trick() {
  if (tricks_learned_ == 0) return;
  // Pick a learned trick deterministically from age + last_tick_ms
  uint32_t r = ((uint32_t)pet_.age_ms ^ last_tick_ms_) * 2654435761u;
  for (int try_n = 0; try_n < (int)Trick::COUNT; ++try_n) {
    int i = ((int)(r >> 4) + try_n) % (int)Trick::COUNT;
    if (tricks_learned_ & (1u << i)) {
      trick_perf_[i]++;
      dirty_ = true;
      return;
    }
  }
}

void Game::update_vocab() {
  if (in_transition()) return;
  // Vocab learned at same age thresholds as tricks, but offset by trick index.
  uint64_t age = pet_.age_ms;
  uint8_t before = vocab_learned_;
  for (int i = 0; i < (int)Word::COUNT; ++i) {
    if (age >= trick_age_threshold((Trick)i)) vocab_learned_ |= (1u << i);
  }
  if (vocab_learned_ != before) dirty_ = true;
}

Game::MenuTab Game::next_menu_tab(MenuTab cur) {
  int t = (int)cur + 1;
  constexpr int last = (int)MenuTab::Actions;
  if (t > last) t = 0;
#if !BAILEY_MEMORIAL_WALL
  if (t == (int)MenuTab::Memorial) ++t;
#endif
  if (t > last) t = 0;
  return (MenuTab)t;
}

// Shop catalog (index -> definition).
// 0..4: toy unlocks (skip if owned)
// 5..7: accessory unlocks (skip if unlocked via achievement)
// 8..10: treats (always buyable, +1 to count)
// 11..14: coat patterns (skip if already that)
uint32_t Game::shop_price(uint8_t i) const {
  if (i < 5) return 8;             // toy
  if (i < 8) return 15;            // accessory
  if (i < 11) {
    static const uint32_t treat_prices[3] = {3, 10, 25};
    return treat_prices[i - 8];
  }
  if (i < 15) return 20;           // coat
  return 0;
}

bool Game::buy_item(uint8_t i) {
  uint32_t price = shop_price(i);
  if (price == 0 || biscuits_ < price) return false;
  if (i < 5) {
    uint8_t bit = (uint8_t)(1u << i);
    if (toy_owned_ & bit) return false;
    toy_owned_ |= bit;
  } else if (i < 8) {
    uint8_t acc = (uint8_t)(i - 5 + 1);   // 1..3
    if (accessory_unlocked(acc)) return false;
    // Bypass achievement gate by stamping the corresponding bit.
    if (acc == 1) unlock_achievement(AchievementId::FirstPet);
    else if (acc == 2) unlock_achievement(AchievementId::Streak3Days);
    else if (acc == 3) unlock_achievement(AchievementId::EvolvedToAdult);
  } else if (i < 11) {
    treats_[i - 8]++;
  } else if (i < 15) {
    uint8_t coat = (uint8_t)(i - 11 + 1);
    if (coat_pattern_ == coat) return false;
    coat_pattern_ = coat;
  }
  biscuits_ -= price;
  dirty_ = true;
  return true;
}

bool Game::accessory_unlocked(uint8_t id) const {
  // Mapping: id 1 (bandana) -> First Pet; id 2 (collar) -> 3-day streak;
  // id 3 (party hat) -> EvolvedToAdult.
  switch (id) {
    case 0: return true;  // bare
    case 1: return is_unlocked(achievements_, AchievementId::FirstPet);
    case 2: return is_unlocked(achievements_, AchievementId::Streak3Days);
    case 3: return is_unlocked(achievements_, AchievementId::EvolvedToAdult);
    default: return false;
  }
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
