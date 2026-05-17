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
    // Reassemble the 64-bit achievement bitmask from the v2 low word
    // and the v16 high word. v15-and-older saves load achievements_hi
    // as 0 via the migrator.
    achievements_              = (uint64_t)s.achievements |
                                 ((uint64_t)s.achievements_hi << 32);
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
    // v7 fields
    bones_collected_  = s.bones_collected;
    walk_today_steps_ = s.walk_today_steps;
    // v8 fields
    daily_quest_awarded_day_ = s.daily_quest_awarded_day;
    // v9 fields
    best_friend_hash_        = s.best_friend_hash;
    // v10 fields
    last_gift_received_day_  = s.last_gift_received_day;
    // v11 fields
    stories_heard_           = s.stories_heard;
    // v12 fields
    dig_successes_           = s.dig_successes;
    // v13 fields
    seasonal_unlocks_        = s.seasonal_unlocks;
    // v14 fields
    bath_toys_owned_         = s.bath_toys_owned;
    bath_toy_active_         = s.bath_toy_active;
    // v15 fields
    hide_seek_wins_          = s.hide_seek_wins;
    // v17 fields
    {
      // Copy the persisted pet name; defensively null-terminate.
      int n = 0;
      while (n < 11 && s.pet_name[n] != '\0') {
        pet_name_[n] = s.pet_name[n];
        ++n;
      }
      pet_name_[n] = '\0';
      if (pet_name_[0] == '\0')
        std::memcpy(pet_name_, "Bailey", 7);
    }
    birthday_month_ = (s.birthday_month >= 1 && s.birthday_month <= 12)
                        ? s.birthday_month : 1;
    birthday_day_   = (s.birthday_day >= 1 && s.birthday_day <= 31)
                        ? s.birthday_day : 13;
    // v18 fields
    stickers_unlocked_ = s.stickers_unlocked & 0x1F;   // 5 bits
    wall_poster_       = s.wall_poster & 0x3;
    // v19 fields
    trainer_xp_        = s.trainer_xp;
    time_played_ms_    = s.time_played_ms;
    // v20 fields
    active_streak_days_ = s.active_streak_days;
    // v21 fields
    fireflies_caught_   = s.fireflies_caught;
    // v22 fields
    last_login_wheel_day_ = s.last_login_wheel_day;
    last_wheel_reward_    = s.last_wheel_reward & 0x07;
    // v23 fields
    last_postcard_msg_id_ = s.last_postcard_msg_id;
    postcards_received_   = s.postcards_received;
    // v24 fields
    bed_type_   = s.bed_type   < 3 ? s.bed_type   : 0;
    bowl_color_ = s.bowl_color < 3 ? s.bowl_color : 0;
    // v25 fields
    fish_caught_     = s.fish_caught;
    tug_high_score_  = s.tug_high_score;
    memory_iq_       = s.memory_iq;
    vet_visits_      = s.vet_visits;
    stick_chases_    = s.stick_chases;
    // v26 fields
    health_stat_     = s.health_stat;
    pet_weight_      = s.pet_weight;
    // v27 fields
    for (int i = 0; i < (int)Friend::COUNT; ++i)
      friend_bond_levels_[i] = s.friend_bond_levels[i];
    earned_titles_mask_ = s.earned_titles_mask;
    chosen_title_id_    = s.chosen_title_id;
    // v28 fields
    for (int i = 0; i < 7; ++i) diary_entries_[i] = s.diary_entries[i];
    diary_head_                = s.diary_head;
    cherry_blossom_last_day_   = s.cherry_blossom_last_day;
    // v29 fields
    for (int i = 0; i < 5; ++i) vet_history_days_[i] = s.vet_history_days[i];
    vet_history_head_   = s.vet_history_head;
    vet_history_count_  = s.vet_history_count;
    auto_feeder_owned_  = s.auto_feeder_owned;
    // v30 fields
    soul_bond_friend_id_  = s.soul_bond_friend_id;
    friend_wishlist_mask_ = s.friend_wishlist_mask;
    for (int i = 0; i < (int)Friend::COUNT; ++i)
      friend_last_visit_day_[i] = s.friend_last_visit_day[i];
    // v31 fields
    for (int i = 0; i < 7; ++i) quest_history_[i] = s.quest_history[i];
    quest_history_head_      = s.quest_history_head;
    quest_history_count_     = s.quest_history_count;
    day_of_dogs_last_day_    = s.day_of_dogs_last_day;
    birthday_cake_seen_day_  = s.birthday_cake_seen_day;
    // v32 fields
    collar_badge_id_         = s.collar_badge_id;
    accessory_size_          = s.accessory_size;
    extra_coats_unlocked_    = s.extra_coats_unlocked;
    // v33 fields
    for (int i = 0; i < (int)Friend::COUNT; ++i)
      friend_last_gift_[i]   = s.friend_last_gift[i];
    weekly_steps_progress_    = s.weekly_steps_progress;
    weekly_last_awarded_week_ = s.weekly_last_awarded_week;
    trainer_perks_mask_       = s.trainer_perks_mask;
    // v34 fields
    daily_seals_total_          = s.daily_seals_total;
    daily_seals_last_day_       = s.daily_seals_last_day;
    halloween_costumes_unlocked_ = s.halloween_costumes_unlocked;
    // v35 fields
    for (int i = 0; i < 8; ++i) leaderboard_hashes_[i] = s.leaderboard_hashes[i];
    leaderboard_head_   = s.leaderboard_head;
    leaderboard_count_  = s.leaderboard_count;
    // v36 fields
    for (int i = 0; i < 8; ++i) scene_wallpaper_[i] = s.scene_wallpaper[i];
    pumpkin_tap_high_score_  = s.pumpkin_tap_high_score;
    trick_chain_runs_        = s.trick_chain_runs;
    // v37 fields
    snowball_hits_           = s.snowball_hits;
    petals_caught_           = s.petals_caught;
    grooming_score_          = s.grooming_score;
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

  // Seed the ambient-spawn RNG. Mix now_ms through Knuth's multiplicative
  // hash and the golden-ratio constant so two start-ups a few ms apart
  // diverge quickly. Guarantee non-zero (xorshift32 sticks at 0).
  rng_state_ = ((uint32_t)now_ms * 2654435761u) ^ 0x9E3779B9u;
  if (rng_state_ == 0) rng_state_ = 0x9E3779B9u;
}

uint32_t Game::rng_next() {
  uint32_t x = rng_state_;
  if (x == 0) x = 0x9E3779B9u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state_ = x;
  return x;
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
    // Round 6 Phase 6H: Lucky Streak perk (bit 2) bumps the payout by 1.
    biscuits_ += perk_unlocked(2) ? 3 : 2;
    if (biscuits_ >= 50) {
      // Set the bit directly (avoid recursion through unlock_achievement).
      uint64_t b = achievement_bit(AchievementId::BiscuitTycoon);
      if (!(achievements_ & b)) achievements_ |= b;
    }
    // Round 5 Phase A2: map specific achievements to sticker unlocks.
    switch (id) {
      case AchievementId::Petted100:    stickers_unlocked_ |= 0x01; break; // paw
      case AchievementId::Streak7Days:  stickers_unlocked_ |= 0x02; break; // star
      case AchievementId::FetchPro:     stickers_unlocked_ |= 0x04; break; // bone
      case AchievementId::Showstopper:  stickers_unlocked_ |= 0x08; break; // heart
      case AchievementId::MasterDigger: stickers_unlocked_ |= 0x10; break; // fire
      default: break;
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
        shop_cursor_ = (uint8_t)((shop_cursor_ + 1) % 20);
        return;
      }
    } else if (menu_tab_ == MenuTab::Actions) {
      // Main = 10 rows. Reordered so the most-used picks come first:
      //   0 Friends >  1 Tricks >  2 Change scene  3 Change hat
      //   4..9 the everyday-action rows
      // Tricks submenu = 6 rows (5 tricks + <Back).
      // Friends submenu = 10 rows (Random + 8 friends + <Back).
      if (actions_submenu_ == 0) {
        // Index 0 = "Play with a friend >", index 1 = "Tricks >".
        if (in == Input::Feed) {
          if (actions_cursor_ == 0) {
            actions_submenu_ = 2; actions_cursor_ = 0; return;
          }
          if (actions_cursor_ == 1) {
            actions_submenu_ = 1; actions_cursor_ = 0; return;
          }
          static const Input kMain[10] = {
            Input::None,            // Friends >  (handled above)
            Input::None,            // Tricks >   (handled above)
            Input::CycleScene,      // Change scene
            Input::CycleAccessory,  // Change hat
            Input::Walk, Input::Play, Input::TreatGive, Input::Brush,
            Input::CycleToy, Input::Bedtime,
          };
          Input chosen = kMain[actions_cursor_ % 10];
          if (chosen != Input::None) {
            menu_open_ = false;
            apply_input(chosen);
          }
          return;
        }
        if (in == Input::Play) {
          actions_cursor_ = (uint8_t)((actions_cursor_ + 1) % 10);
          return;
        }
      } else if (actions_submenu_ == 1) {
        // Tricks submenu: 5 tricks + <Back. <Back is index 5.
        if (in == Input::Feed) {
          if (actions_cursor_ == 5) {
            actions_submenu_ = 0;
            actions_cursor_  = 1;   // land on the Tricks row of main
            return;
          }
          static const Input kTricks[5] = {
            Input::VoiceSit, Input::VoiceCome, Input::VoiceHighFive,
            Input::VoiceRollOver, Input::VoiceJump,
          };
          Input chosen = kTricks[actions_cursor_ % 5];
          menu_open_       = false;
          actions_submenu_ = 0;
          apply_input(chosen);
          return;
        }
        if (in == Input::Play) {
          actions_cursor_ = (uint8_t)((actions_cursor_ + 1) % 6);
          return;
        }
      } else {
        // Friends submenu: Random + 8 named + <Back. <Back is index 9.
        if (in == Input::Feed) {
          if (actions_cursor_ == 9) {
            actions_submenu_ = 0;
            actions_cursor_  = 0;   // land on the Friends row of main
            return;
          }
          Input chosen;
          if (actions_cursor_ == 0) {
            chosen = Input::PlayWithFriend;       // random
          } else {
            // 1..8 -> Ollie..Noshy
            chosen = (Input)((int)Input::PlayWithFriendOllie +
                             (actions_cursor_ - 1));
          }
          menu_open_       = false;
          actions_submenu_ = 0;
          apply_input(chosen);
          return;
        }
        if (in == Input::Play) {
          actions_cursor_ = (uint8_t)((actions_cursor_ + 1) % 10);
          return;
        }
      }
    }
    menu_tab_ = next_menu_tab(menu_tab_);
    actions_submenu_ = 0;
    actions_cursor_  = 0;
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
      // Round 5 Phase B mini-games: A button has mini-game-specific
      // meaning during Fishing / MemoryPaws / TugOfWar modes.
      if (mode_ == GameMode::Fishing) {
        // Hit the nibble: A within the 1 s window after fishing_nibble_ms_.
        uint32_t since_nibble = last_tick_ms_ - fishing_nibble_ms_;
        if (last_tick_ms_ >= fishing_nibble_ms_ && since_nibble < 1000) {
          fish_caught_++;
          pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 10);
          play_clip(ClipId::Yip);
        }
        mode_ = GameMode::Idle;
        dirty_ = true;
        break;
      }
      if (mode_ == GameMode::TugOfWar) {
        if (tug_last_btn_ != 1) {
          tug_count_++;
          tug_last_btn_ = 1;
        }
        break;
      }
      if (mode_ == GameMode::MemoryPaws && memory_target_button_ == 1) {
        memory_round_index_++;
        if (memory_round_index_ > memory_iq_) memory_iq_ = memory_round_index_;
        memory_target_button_ = (uint8_t)(1 + (rng_next() % 4));
        memory_round_started_ms_ = last_tick_ms_;
        break;
      }
      // Walk dig: A button during an active dig prompt counts as a
      // successful dig (+3 bones, achievement at 10 lifetime).
      if (mode_ == GameMode::Walking && dig_prompt_active()) {
        dig_successes_++;
        bones_collected_       += 3;
        last_walk_find_kind_    = 1;
        last_walk_find_ms_      = last_tick_ms_;
        dig_prompt_until_ms_    = 0;
        if (dig_successes_ >= 10) unlock_achievement(AchievementId::MasterDigger);
        dirty_ = true;
        break;
      }
      // During a walk without an active prompt, A does nothing useful
      // (feeding mid-walk would be a weird input).
      if (mode_ == GameMode::Walking) break;
      uint32_t boost = kActionEatBoost;
      if (well_tucked_in_today_) boost *= 2;  // bedtime routine bonus
      if (horoscope_id() == 2) boost += 10;   // HUNGRY: bigger feed boost
      if (coat_pattern_ == 1) boost += 5;     // Tan coat: hearty appetite
      if (perk_unlocked(0))   boost += 2;     // Round 6 Phase 6H: Bigger Bites perk
      pet_.stats.hunger = clamp_stat((int)pet_.stats.hunger + boost);
      // Round 6 Phase 6A: each feed nudges weight up by 1.
      if (pet_weight_ < 100) pet_weight_++;
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
      // Mini-game B button maps:
      //  TugOfWar -- alternates with A; only counts on a switch.
      //  MemoryPaws (target=2) -- correct press.
      if (mode_ == GameMode::TugOfWar) {
        if (tug_last_btn_ != 2) {
          tug_count_++;
          tug_last_btn_ = 2;
        }
        return;
      }
      if (mode_ == GameMode::MemoryPaws && memory_target_button_ == 2) {
        memory_round_index_++;
        if (memory_round_index_ > memory_iq_) memory_iq_ = memory_round_index_;
        memory_target_button_ = (uint8_t)(1 + (rng_next() % 4));
        memory_round_started_ms_ = last_tick_ms_;
        return;
      }
      // During a walk, B advances a step (matches the HUD label).
      if (mode_ == GameMode::Walking) {
        apply_input(Input::Walk);
        break;
      }
      // While in a fetch flow, button is treated as the "catch" press.
      if (mode_ == GameMode::FetchCatching) {
        // Hit -- award full happiness + skill increment.
        uint32_t play_boost = kActionPlayBoost;
        if (horoscope_id() == 0) play_boost += 10;   // PLAYFUL: bigger boost
        if (gourmet_active())    play_boost = play_boost * 5 / 4;
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + play_boost);
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
        uint32_t play_boost = kActionPlayBoost;
        if (horoscope_id() == 0) play_boost += 10;
        if (gourmet_active())    play_boost = play_boost * 5 / 4;
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + play_boost);
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
      // MemoryPaws: C maps to target 3; otherwise fall through.
      if (mode_ == GameMode::MemoryPaws && memory_target_button_ == 3) {
        memory_round_index_++;
        if (memory_round_index_ > memory_iq_) memory_iq_ = memory_round_index_;
        memory_target_button_ = (uint8_t)(1 + (rng_next() % 4));
        memory_round_started_ms_ = last_tick_ms_;
        break;
      }
      // During a walk, C ends the walk early.
      if (mode_ == GameMode::Walking) {
        mode_ = GameMode::Idle;
        pet_.current_action = Action::None;
        walk_steps_ = 0;
        walk_target_ = 0;
        dirty_ = true;
        break;
      }
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
      // Round 5 Phase B: a firefly in flight is caught by ANY tap,
      // regardless of the pet-tap cooldown. +5 happiness, +1 bone
      // every 5 catches as a small extra reward.
      if (firefly_active()) {
        fireflies_caught_++;
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 5);
        if (fireflies_caught_ % 5 == 0) bones_collected_++;
        firefly_spawn_ms_ = 0;     // clear
        play_clip(ClipId::Yip);
        dirty_ = true;
        return;
      }
      uint32_t since = last_tick_ms_ - pet_.last_pet_ms;
      if (since >= kPetCooldownMs || pet_.last_pet_ms == 0) {
        uint32_t pet_boost = kActionPetBoost;
        if (coat_pattern_ == 3) pet_boost += 5;   // Tri-color: extra cuddly
        if (gourmet_active())   pet_boost = pet_boost * 5 / 4;
        if (trick_combo_active()) pet_boost = pet_boost * 6 / 5;   // +20 %
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + pet_boost);
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
        walk_today_steps_++;
        // Round 6 Phase 6H: weekly-challenge accumulator.
        if (weekly_steps_progress_ < 0xFFFF0000u) weekly_steps_progress_++;
        // Round 6 Phase 6A: every 20 steps trims 1 weight.
        if ((total_steps_ % 20) == 0 && pet_weight_ > 0) pet_weight_--;
        pet_.stats.energy = clamp_stat((int)pet_.stats.energy - 1);
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 1);
        // 1/8 chance to find an item per step (1/4 on CURIOUS horoscope days).
        uint32_t r = (last_tick_ms_ + (uint32_t)total_steps_) * 2654435761u;
        uint32_t find_mask = (horoscope_id() == 3) ? 3u : 7u;
        if ((r & find_mask) == 0) {
          int kind = (r >> 3) % 3;
          if (kind == 0) {
            bones_collected_++;                         // collectible bone
            if (coat_pattern_ == 4) grant_biscuits(1);  // Black coat: extra biscuit
            last_walk_find_kind_ = 1;
          } else if (kind == 1 && (toy_owned_ != kAllToysMask)) {
            // Unlock a random missing toy
            for (int i = 0; i < (int)Toy::COUNT; ++i) {
              uint8_t bit = 1u << ((i + (r >> 6)) % (int)Toy::COUNT);
              if (!(toy_owned_ & bit)) { toy_owned_ |= bit; break; }
            }
            last_walk_find_kind_ = 2;
          } else {
            treats_[r & 3 ? 0 : 1]++;                   // biscuit treat usually
            last_walk_find_kind_ = 3;
          }
          last_walk_find_ms_ = last_tick_ms_;
        }
        fulfill_wish_if_matches(Wish::Walk);
        // Walk-dig prompt: ~5 % per step. Don't restack an already
        // active prompt.
        if (!dig_prompt_active() && (rng_next() % 20) == 0) {
          dig_prompt_until_ms_ = last_tick_ms_ + 1500;   // 1.5 s window
        }
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
      if (gourmet_active()) boost = boost * 5 / 4;  // GOURMET buff: +25 %
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + boost);
      pet_.stats.hunger    = clamp_stat((int)pet_.stats.hunger    + food);
      // Round 6 Phase 6A: treats put on weight faster (+2).
      if (pet_weight_ <= 98) pet_weight_ += 2; else pet_weight_ = 100;
      pet_.current_action  = Action::Eat;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Yip);
      fulfill_wish_if_matches(Wish::Treat);
      // Recipe-combo tracking: feed biscuit + bacon + steak within 60 s
      // (in any order) to activate the GOURMET buff for 600 s.
      if (combo_mask_ == 0 ||
          last_tick_ms_ - combo_window_start_ms_ > 60000) {
        combo_window_start_ms_ = last_tick_ms_;
        combo_mask_ = 0;
      }
      combo_mask_ |= (uint8_t)(1u << tier);
      if (combo_mask_ == 0x07) {
        gourmet_until_ms_ = last_tick_ms_ + 600000;   // 10 min
        combo_mask_ = 0;
      }
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
      // Every menu OPEN starts on a clean Actions tab.
      if (menu_open_) {
        menu_tab_        = MenuTab::Actions;
        actions_cursor_  = 0;
        actions_submenu_ = 0;
      }
      break;
    case Input::MenuNext:
      if (menu_open_) menu_tab_ = next_menu_tab(menu_tab_);
      break;
    case Input::CycleScene: {
      uint8_t next = (uint8_t)((settings_.scene_id + 1) % 8);
      settings_.scene_id = next;
      scene_id_ = next;
      // Track scenic-tour achievement via a side bitmask in settings._pad.
      // 8 scenes => 8-bit mask (full uint8_t).
      settings_._pad |= (uint8_t)(1u << next);
      if (settings_._pad == 0xFF)
        unlock_achievement(AchievementId::ScenicTour);
      // Round 3: ambient sound cue per scene. Indoor scenes stay silent.
      switch (next) {
        case 1: case 2: case 6: play_clip(ClipId::Birds); break;  // outdoor
        case 3: play_clip(ClipId::Waves); break;                  // beach
        case 7: play_clip(ClipId::Wind);  break;                  // snow
        default: break;                                            // indoor
      }
      dirty_ = true;
      break;
    }
    case Input::CycleCoat: {
      // Round 6 Phase 6G: extend cycle to 0..7. Skip locked extras.
      uint8_t next = coat_pattern_;
      for (int step = 0; step < 8; ++step) {
        next = (uint8_t)((next + 1) % 8);
        if (next <= 4 || coat_unlocked(next)) { choose_coat(next); break; }
      }
      break;
    }
    case Input::CycleAccessory: {
      // Cycle through unlocked ones only (0=bare always allowed).
      // 9 = 4 base (none/bandana/collar/hat) + 5 seasonal
      // (pumpkin/santa/shamrock/egg-basket/heart-bandana).
      for (uint8_t try_n = 0; try_n < 9; ++try_n) {
        uint8_t cand = (uint8_t)((accessory_id_ + 1 + try_n) % 9);
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
      // Round 5 Phase D2: 1 s gold-frame flash on-screen so the user
      // gets immediate "snap!" feedback.
      photo_flash_until_ms_ = last_tick_ms_ + 1000;
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
      if (trick_perf_[(int)t] >= 10) unlock_achievement(AchievementId::Showstopper);
      // Round 5 Phase B: trick combo tracking. Reset the 30-s window
      // if it's stale; OR the bit for this trick; activate the bonus
      // when 3 distinct tricks have been performed.
      if (last_tick_ms_ - recent_tricks_first_ms_ > 30000) {
        recent_tricks_mask_     = 0;
        recent_tricks_first_ms_ = last_tick_ms_;
      }
      uint8_t bit = (uint8_t)(1u << (int)t);
      if ((recent_tricks_mask_ & bit) == 0) {
        recent_tricks_mask_ |= bit;
        if (__builtin_popcount(recent_tricks_mask_) >= 3) {
          trick_combo_until_ms_ = last_tick_ms_ + 60000;
          recent_tricks_mask_   = 0;   // reset so next 3 also count
        }
      }
      // Round 6 Phase 6K: trick CHAIN tracker -- 5 tricks (any kind)
      // performed within 15 s. Awards +5 biscuits and bumps the
      // chain counter on every completion.
      if (trick_chain_count_ == 0 ||
          last_tick_ms_ - trick_chain_first_ms_ > 15000) {
        trick_chain_first_ms_ = last_tick_ms_;
        trick_chain_count_    = 0;
      }
      if (trick_chain_count_ < 255) trick_chain_count_++;
      if (trick_chain_count_ >= 5) {
        if (trick_chain_runs_ < 255) trick_chain_runs_++;
        grant_biscuits(5);
        trick_chain_count_    = 0;
        trick_chain_first_ms_ = 0;
        play_clip(ClipId::Fanfare);
      }
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
      // Round 3: bedtime story bubble. Pick a deterministic-but-varied
      // story by stepping the index each tuck-in. 8s on-screen.
      stories_heard_++;
      bedtime_story_idx_      = (uint8_t)((bedtime_story_idx_ + 1) % 8);
      bedtime_story_until_ms_ = last_tick_ms_ + 8000;
      if (stories_heard_ >= 10) unlock_achievement(AchievementId::Goodnight);
      dirty_ = true;
      break;
    }
    case Input::MenuCursorNext:
      if (menu_open_ && menu_tab_ == MenuTab::Actions) {
        int n = (actions_submenu_ == 1) ? 6
              : (actions_submenu_ == 2) ? 10
              : 10;   // main grew 8 -> 10 with Change scene + Change hat
        actions_cursor_ = (uint8_t)((actions_cursor_ + 1) % n);
      }
      break;
    case Input::PlayWithFriend:
    case Input::PlayWithFriendOllie:
    case Input::PlayWithFriendMitchell:
    case Input::PlayWithFriendEnzo:
    case Input::PlayWithFriendLincoln:
    case Input::PlayWithFriendRuben:
    case Input::PlayWithFriendFrancie:
    case Input::PlayWithFriendBomi:
    case Input::PlayWithFriendNoshy: {
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
        // Round 6 Phase 6B: each fresh visit bumps the per-friend bond
        // level. Cap is normally 5 hearts; Best Pals perk (bit 1)
        // raises it to 6.
        uint8_t cap = perk_unlocked(1) ? 6 : 5;
        if (friend_bond_levels_[(int)f] < cap) friend_bond_levels_[(int)f]++;
        // Round 6 Phase 6E: stamp this friend's last-visit day (only
        // meaningful when we have a synced clock) and lock in the
        // soul-bonded friend the first time anyone reaches 25 visits.
        if (today_day_index_ != 0)
          friend_last_visit_day_[(int)f] = today_day_index_;
        if (soul_bond_friend_id_ == 0xFF && friend_visits_[(int)f] >= 25)
          soul_bond_friend_id_ = (uint8_t)f;
        // Round 6 Phase 6H: friend brings a small gift (rotates 1..5).
        // Deterministic from the per-friend visit counter so the
        // RNG state is unchanged (other tests rely on stable ambient
        // spawn probabilities).
        uint8_t gift = (uint8_t)(1 + ((friend_visits_[(int)f] - 1) % 5));
        friend_last_gift_[(int)f] = gift;
        switch (gift) {
          case 2: treats_[0]++;     break;   // biscuit treat
          case 3: bones_collected_++; break; // bone
          case 5: grant_biscuits(1); break;  // single biscuit
          default: break;                     // ball/sticker are visual
        }
        unlock_achievement(AchievementId::PlayDate);
        bool met_all = true;
        for (int i = 0; i < (int)Friend::COUNT; ++i)
          if (friend_visits_[i] == 0) { met_all = false; break; }
        if (met_all) unlock_achievement(AchievementId::Socialite);
        update_earned_titles();
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
    case Input::TradeBones:
      // 5 bones -> 1 biscuit. Same path as Shop row 15.
      buy_item(15);
      break;
    case Input::CycleBathToy: {
      // Cycle 0 (none) + owned bath toys. Always allow 0.
      for (uint8_t try_n = 0; try_n < 4; ++try_n) {
        uint8_t cand = (uint8_t)((bath_toy_active_ + 1 + try_n) % 4);
        if (cand == 0 || (bath_toys_owned_ & (1u << (cand - 1)))) {
          bath_toy_active_ = cand;
          dirty_ = true;
          break;
        }
      }
      break;
    }
    case Input::HideSeek: {
      // Quick "where's Bailey?" surprise. Pure-luck distribution:
      // 50 % win (+20 happiness, +1 lifetime win),
      // 25 % peek (+5 happiness consolation),
      // 25 % miss (-5 happiness).
      uint8_t roll = (uint8_t)(rng_next() & 0x3);
      if (roll < 2) {
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 20);
        hide_seek_wins_++;
        hide_seek_last_outcome_ = 1;
        if (hide_seek_wins_ >= 5)
          unlock_achievement(AchievementId::HideAndSeekChamp);
        play_clip(ClipId::Wuff);
      } else if (roll == 2) {
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 5);
        hide_seek_last_outcome_ = 2;
        play_clip(ClipId::Heart);
      } else {
        pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness - 5);
        hide_seek_last_outcome_ = 3;
        play_clip(ClipId::Whimper);
      }
      hide_seek_last_ms_ = last_tick_ms_;
      dirty_ = true;
      break;
    }
    // Round 5 Phase B: mini-games. Each enters its own GameMode and
    // is driven by update_minigames(now_ms) each tick.
    case Input::Fish: {
      if (mode_ != GameMode::Idle) break;
      if (settings_.scene_id != 3) break;     // Beach only
      mode_                = GameMode::Fishing;
      fishing_started_ms_  = last_tick_ms_;
      // Nibble window opens 1-3 s after cast.
      fishing_nibble_ms_   = last_tick_ms_ + 1000 + (rng_next() % 2000);
      dirty_ = true;
      break;
    }
    case Input::MemoryPaws: {
      if (mode_ != GameMode::Idle) break;
      mode_                       = GameMode::MemoryPaws;
      memory_round_index_         = 0;
      memory_round_started_ms_    = last_tick_ms_;
      memory_target_button_       = (uint8_t)(1 + (rng_next() % 4));
      dirty_ = true;
      break;
    }
    case Input::TugOfWar: {
      if (mode_ != GameMode::Idle) break;
      mode_              = GameMode::TugOfWar;
      tug_started_ms_    = last_tick_ms_;
      tug_count_         = 0;
      tug_last_btn_      = 0;
      dirty_ = true;
      break;
    }
    case Input::ChaseStick: {
      // Fetch-flow variant in the Forest scene. Counts towards
      // stick_chases instead of fetch_catches when caught. Forward
      // to the regular fetch trigger to reuse the state machine.
      if (settings_.scene_id != 6) break;     // Forest only
      stick_chases_++;
      apply_input(Input::Play);   // existing fetch starter
      dirty_ = true;
      break;
    }
    case Input::VetVisit: {
      // Cure + a small animation pulse. Only meaningful while sick.
      if (sickness_ == 0) break;
      try_cure_sickness();
      vet_visits_++;
      pet_.current_action    = Action::Clean;
      pet_.action_started_ms = last_tick_ms_;
      play_clip(ClipId::Heart);
      dirty_ = true;
      break;
    }
    case Input::PumpkinTap: {
      // Round 6 Phase 6K: Halloween rhythm-tap mini-game. Only active
      // on Oct 31 (active_holiday_ == 2). First tap starts the 5 s
      // window; subsequent taps inside the window count toward the
      // session score; expiry updates the high-score record.
      if (active_holiday_ != 2) break;
      if (pumpkin_tap_started_ms_ == 0 ||
          last_tick_ms_ - pumpkin_tap_started_ms_ >= 5000) {
        pumpkin_tap_started_ms_ = last_tick_ms_;
        pumpkin_tap_count_      = 0;
      }
      pumpkin_tap_count_++;
      if (pumpkin_tap_count_ > pumpkin_tap_high_score_)
        pumpkin_tap_high_score_ = pumpkin_tap_count_;
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 1);
      dirty_ = true;
      break;
    }
    case Input::CycleWallpaper: {
      // Round 6 Phase 6K: cycle wallpaper variant for the current scene.
      uint8_t s = settings_.scene_id;
      if (s < 8) {
        scene_wallpaper_[s] = (uint8_t)((scene_wallpaper_[s] + 1) % 4);
        dirty_ = true;
      }
      break;
    }
    case Input::SnowballThrow: {
      // Round 6 Phase 6L: Snowball Fight -- Snow Park scene (id 4) OR
      // current weather Snow.  Each tap counts a snowball.
      if (settings_.scene_id != 4 && weather_ != (uint8_t)Weather::Snow) break;
      if (snowball_hits_ < 0xFFFF) snowball_hits_++;
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 1);
      dirty_ = true;
      break;
    }
    case Input::PetalCatch: {
      // Round 6 Phase 6L: Petal Catch -- only on Cherry Blossom Day
      // (active_holiday_ == 8). Each tap catches a drifting petal.
      if (active_holiday_ != 8) break;
      if (petals_caught_ < 0xFFFF) petals_caught_++;
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 1);
      dirty_ = true;
      break;
    }
    case Input::GroomBrush: {
      // Round 6 Phase 6L: Grooming rhythm tap -- always available.
      // Boosts cleanliness like Brush (+2) and banks a lifetime score.
      if (in_transition()) break;
      pet_.stats.cleanliness = clamp_stat((int)pet_.stats.cleanliness + 2);
      if (grooming_score_ < 0xFFFF) grooming_score_++;
      pet_.current_action = Action::Clean;
      pet_.action_started_ms = last_tick_ms_;
      dirty_ = true;
      break;
    }
    case Input::ImuShake:
      // Physical shake of the device. From Idle, if Bailey is eligible
      // to walk, kick off a walk -- gives motion-control a clear hook.
      // Otherwise fall back to a pet (same as a loud mic trigger).
      if (mode_ == GameMode::Idle &&
          pet_.stage != LifeStage::Puppy &&
          pet_.stats.energy >= 30 &&
          !in_transition()) {
        apply_input(Input::Walk);   // start walk
      } else {
        apply_input(Input::PetTap);
      }
      break;
    case Input::Restart:
      // Handled above when stage == Gone
      break;
    case Input::None:
      break;
  }

  // Round 5 Phase C1: award trainer XP for "real" user actions.
  // Heavier actions (treats, walking, tricks) get more weight than
  // simple pets / button mashes.
  uint32_t xp = 0;
  switch (in) {
    case Input::Feed: case Input::PetTap: case Input::Stroke:
    case Input::Brush: case Input::MicTrigger:
      xp = 1; break;
    case Input::Play: case Input::Clean: case Input::CycleToy:
    case Input::CycleAccessory: case Input::CycleScene:
    case Input::CycleCoat: case Input::TakePhoto: case Input::CycleAge:
      xp = 2; break;
    case Input::Walk: case Input::TreatGive: case Input::Bedtime:
    case Input::ImuFlick: case Input::ImuShake: case Input::HideSeek:
      xp = 3; break;
    case Input::VoiceSit: case Input::VoiceCome: case Input::VoiceHighFive:
    case Input::VoiceRollOver: case Input::VoiceJump:
      xp = 5; break;
    case Input::PlayWithFriend:
    case Input::PlayWithFriendOllie: case Input::PlayWithFriendMitchell:
    case Input::PlayWithFriendEnzo:  case Input::PlayWithFriendLincoln:
    case Input::PlayWithFriendRuben: case Input::PlayWithFriendFrancie:
    case Input::PlayWithFriendBomi:  case Input::PlayWithFriendNoshy:
      xp = 4; break;
    default: break;
  }
  if (xp > 0) {
    award_xp(xp);
    // Round 5 Phase C2: count this as a "user action" toward today's
    // daily goal (cap to avoid wrap on long sessions).
    if (today_actions_ < 999) today_actions_++;
  }
}

void Game::apply_decay(uint32_t dt_ms) {
  if (in_transition()) return;

  // Sleep schedule: halve effective decay during sleep hours (when
  // auto_sleep is on). Doubling thresholds = halving the rate.
  bool sleeping = is_sleep_hour() && settings_.auto_sleep;

  // Decay multiplier from settings (decay_mult / 10).
  uint32_t mult_num = settings_.decay_mult == 0 ? 10 : settings_.decay_mult;
  if (sleeping) mult_num = (mult_num + 1) / 2;        // half-speed decay
  // Horoscope SLEEPY: 25 % slower decay on top.
  if (horoscope_id() == 1) mult_num = (mult_num * 3 + 3) / 4;
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
  // Coat passive: Brindle (id 2) -- snappier energy regen.
  if (coat_pattern_ == 2) e_thr = (uint32_t)(e_thr * 8 / 10);

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

  // Round 6 Phase 6D: auto-feeder slowly tops up hunger to 60 while
  // owned + idle. One point per "auto-feeder step" (5 min normal /
  // 2 s in BAILEY_FAST_DECAY). Only fires when hunger is below 60 so
  // it can't compete with treats for the obesity loop.
  if (auto_feeder_owned_ && pet_.stats.hunger < 60) {
    auto_feeder_acc_ms_ += dt_ms;
#if BAILEY_FAST_DECAY
    constexpr uint32_t kAutoFeedStep = 2000;
#else
    constexpr uint32_t kAutoFeedStep = 5u * 60 * 1000;
#endif
    while (auto_feeder_acc_ms_ >= kAutoFeedStep && pet_.stats.hunger < 60) {
      auto_feeder_acc_ms_ -= kAutoFeedStep;
      pet_.stats.hunger++;
      dirty_ = true;
    }
  } else {
    auto_feeder_acc_ms_ = 0;
  }
}

void Game::update_mood() {
  if (in_transition()) return;  // keep the transition mood pinned
  // Urgent states always win, even at night.
  if (pet_.stats.any_zero())         { pet_.mood = Mood::Sad; return; }
  if (pet_.stats.cleanliness < 20)   { pet_.mood = Mood::Dirty; return; }
  if (pet_.stats.hunger < 20)        { pet_.mood = Mood::Hungry; return; }
  // Sleep schedule: during the night window, Bailey defaults to Sleeping.
  if (is_sleep_hour() && settings_.auto_sleep) {
    pet_.mood = Mood::Sleeping;
    return;
  }
  if (pet_.stats.energy < 20)        { pet_.mood = Mood::Sleeping; return; }
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

bool Game::is_sleep_hour() const {
  if (!have_local_hour_) return false;
  // Window crosses midnight if bedtime > wake.
  if (kBedtimeHour > kWakeHour) {
    return current_hour_ >= kBedtimeHour || current_hour_ < kWakeHour;
  }
  return current_hour_ >= kBedtimeHour && current_hour_ < kWakeHour;
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
    current_hour_     = (uint8_t)lt.hour;
    have_local_hour_  = true;
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
  // Round 6 Phase 6B: keep the earned-titles mask in sync with the
  // underlying counters (bones / steps / Showstopper achievement).
  update_earned_titles();
  // Round 6 Phase 6G: extra-coat milestone unlocks.
  // Cream  (id 5): Bailey reaches Senior stage.
  // Merle  (id 6): trainer level >= 5.
  // Husky  (id 7): >= 1000 total walking steps.
  if (pet_.stage == LifeStage::Senior) unlock_coat(5);
  if (trainer_level() >= 5)            unlock_coat(6);
  if (total_steps_ >= 1000)            unlock_coat(7);
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
  // Round 5 Phase C1: accumulate lifetime play-time. `dt` is already
  // clamped to 1 s when the gap exceeds 1 hour (see line above), so
  // multi-day offline jumps count as just one second here.
  time_played_ms_ += dt;

  // update_daylight caches current_hour_, which apply_decay and
  // update_mood both consult for sleep-schedule behavior; run it first.
  update_daylight(now_ms);
  apply_decay(dt);
  update_evolution(dt);
  update_mood();
  check_achievements();
  update_fetch_mode(now_ms);
  update_walk(now_ms);
  update_wish(now_ms);
  update_sickness(dt);
  update_tricks();
  update_vocab();
  update_ambient(now_ms);
  maybe_trigger_lightning(now_ms);
  maybe_trigger_snore(now_ms);
  maybe_spawn_firefly(now_ms);
  update_minigames(now_ms);

  // Streak check + weather roll + birthday/bedtime whenever we have a
  // synced clock.
  if (clock_ && clock_->is_synced()) {
    uint64_t u = clock_->now_unix_ms();
    check_streak(u);
    update_weather(u);
    update_birthday(u);
    update_bedtime(u);
    update_daily_quest(u);
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
  // Split the 64-bit bitmask back into low (v2) + high (v16) words.
  s.achievements            = (uint32_t)(achievements_ & 0xFFFFFFFFu);
  s.achievements_hi         = (uint32_t)((achievements_ >> 32) & 0xFFFFFFFFu);
  s._pad16                  = 0;
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
  // v7 additions
  s.bones_collected  = bones_collected_;
  s.walk_today_steps = walk_today_steps_;
  s._pad7            = 0;
  // v8 additions
  s.daily_quest_awarded_day = daily_quest_awarded_day_;
  // v9 additions
  s.best_friend_hash        = best_friend_hash_;
  // v10 additions
  s.last_gift_received_day  = last_gift_received_day_;
  // v11 additions
  s.stories_heard           = stories_heard_;
  s._pad11                  = 0;
  // v12 additions
  s.dig_successes           = dig_successes_;
  s._pad12                  = 0;
  // v13 additions
  s.seasonal_unlocks        = seasonal_unlocks_;
  s._pad13[0] = s._pad13[1] = s._pad13[2] = 0;
  // v14 additions
  s.bath_toys_owned         = bath_toys_owned_;
  s.bath_toy_active         = bath_toy_active_;
  s._pad14[0] = s._pad14[1] = 0;
  // v15 additions
  s.hide_seek_wins          = hide_seek_wins_;
  s._pad15                  = 0;
  // v17 additions (v16 high-word stamped above near `s.achievements_hi`).
  {
    int n = 0;
    while (n < 11 && pet_name_[n] != '\0') {
      s.pet_name[n] = pet_name_[n];
      ++n;
    }
    s.pet_name[n] = '\0';
  }
  s.birthday_month          = birthday_month_;
  s.birthday_day            = birthday_day_;
  s._pad17[0] = s._pad17[1] = 0;
  // v18 additions
  s.stickers_unlocked       = stickers_unlocked_;
  s.wall_poster             = wall_poster_;
  s._pad18[0] = s._pad18[1] = 0;
  // v19 additions
  s.trainer_xp              = trainer_xp_;
  s.time_played_ms          = time_played_ms_;
  // v20 additions
  s.active_streak_days      = active_streak_days_;
  s._pad20                  = 0;
  // v21 additions
  s.fireflies_caught        = fireflies_caught_;
  s._pad21                  = 0;
  // v22 additions
  s.last_login_wheel_day    = last_login_wheel_day_;
  s.last_wheel_reward       = last_wheel_reward_;
  s._pad22[0] = s._pad22[1] = s._pad22[2] = 0;
  // v23 additions
  s.last_postcard_msg_id    = last_postcard_msg_id_;
  s.postcards_received      = postcards_received_;
  s._pad23                  = 0;
  // v24 additions
  s.bed_type                = bed_type_;
  s.bowl_color              = bowl_color_;
  s._pad24                  = 0;
  // v25 additions
  s.fish_caught             = fish_caught_;
  s.tug_high_score          = tug_high_score_;
  s.memory_iq               = memory_iq_;
  s.vet_visits              = vet_visits_;
  s.stick_chases            = stick_chases_;
  // v26 additions
  s.health_stat             = health_stat_;
  s.pet_weight              = pet_weight_;
  s._pad26                  = 0;
  // v27 additions
  for (int i = 0; i < (int)Friend::COUNT; ++i)
    s.friend_bond_levels[i] = friend_bond_levels_[i];
  s.earned_titles_mask      = earned_titles_mask_;
  s.chosen_title_id         = chosen_title_id_;
  s._pad27                  = 0;
  // v28 additions
  for (int i = 0; i < 7; ++i) s.diary_entries[i] = diary_entries_[i];
  s.diary_head              = diary_head_;
  s.cherry_blossom_last_day = cherry_blossom_last_day_;
  s._pad28[0] = s._pad28[1] = s._pad28[2] = 0;
  // v29 additions
  for (int i = 0; i < 5; ++i) s.vet_history_days[i] = vet_history_days_[i];
  s.vet_history_head        = vet_history_head_;
  s.vet_history_count       = vet_history_count_;
  s.auto_feeder_owned       = auto_feeder_owned_;
  s._pad29                  = 0;
  // v30 additions
  s.soul_bond_friend_id     = soul_bond_friend_id_;
  s.friend_wishlist_mask    = friend_wishlist_mask_;
  for (int i = 0; i < (int)Friend::COUNT; ++i)
    s.friend_last_visit_day[i] = friend_last_visit_day_[i];
  s._pad30                  = 0;
  // v31 additions
  for (int i = 0; i < 7; ++i) s.quest_history[i] = quest_history_[i];
  s.quest_history_head      = quest_history_head_;
  s.quest_history_count     = quest_history_count_;
  s.day_of_dogs_last_day    = day_of_dogs_last_day_;
  s.birthday_cake_seen_day  = birthday_cake_seen_day_;
  s._pad31[0] = s._pad31[1] = 0;
  // v32 additions
  s.collar_badge_id         = collar_badge_id_;
  s.accessory_size          = accessory_size_;
  s.extra_coats_unlocked    = extra_coats_unlocked_;
  s._pad32                  = 0;
  // v33 additions
  for (int i = 0; i < (int)Friend::COUNT; ++i)
    s.friend_last_gift[i]   = friend_last_gift_[i];
  s.weekly_steps_progress    = weekly_steps_progress_;
  s.weekly_last_awarded_week = weekly_last_awarded_week_;
  s.trainer_perks_mask       = trainer_perks_mask_;
  s._pad33[0] = s._pad33[1] = s._pad33[2] = 0;
  // v34 additions
  s.daily_seals_total           = daily_seals_total_;
  s.daily_seals_last_day        = daily_seals_last_day_;
  s.halloween_costumes_unlocked = halloween_costumes_unlocked_;
  s._pad34[0] = s._pad34[1]     = 0;
  // v35 additions
  for (int i = 0; i < 8; ++i) s.leaderboard_hashes[i] = leaderboard_hashes_[i];
  s.leaderboard_head            = leaderboard_head_;
  s.leaderboard_count           = leaderboard_count_;
  s._pad35[0] = s._pad35[1]     = 0;
  // v36 additions
  for (int i = 0; i < 8; ++i) s.scene_wallpaper[i] = scene_wallpaper_[i];
  s.pumpkin_tap_high_score      = pumpkin_tap_high_score_;
  s.trick_chain_runs            = trick_chain_runs_;
  s._pad36                      = 0;
  // v37 additions
  s.snowball_hits               = snowball_hits_;
  s.petals_caught               = petals_caught_;
  s.grooming_score              = grooming_score_;
  s._pad37                      = 0;

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
    case GameMode::Fishing:
    case GameMode::MemoryPaws:
    case GameMode::TugOfWar:
      // Mini-game progression handled in update_minigames().
      break;
    case GameMode::Idle:
      break;
  }
}

void Game::update_weather(uint64_t now_unix_ms) {
  uint32_t today = local_day_index(now_unix_ms, settings_.tz_offset_min);
  if (today == last_weather_roll_day_) return;
  last_weather_roll_day_ = today;
  // Distribution (32 buckets): 14 Sunny / 6 Cloudy / 5 Rain / 4 Snow / 3 Fog.
  uint32_t r = today * 2654435761u;
  uint8_t roll = (uint8_t)(r % 32);
  uint8_t w;
  if      (roll < 14) w = (uint8_t)Weather::Sunny;
  else if (roll < 20) w = (uint8_t)Weather::Cloudy;
  else if (roll < 25) w = (uint8_t)Weather::Rain;
  else if (roll < 29) w = (uint8_t)Weather::Snow;
  else                w = (uint8_t)Weather::Fog;
  if (w != weather_) {
    prev_weather_           = weather_;
    last_weather_change_ms_ = last_tick_ms_;
  }
  weather_ = w;
  // Visiting through bad weather counts toward achievement
  if (w == (uint8_t)Weather::Rain || w == (uint8_t)Weather::Snow)
    unlock_achievement(AchievementId::WeatheredTheStorm);
  dirty_ = true;
}

void Game::maybe_trigger_lightning(uint32_t now_ms) {
  // Round 4: random lightning strike during Rain weather. ~once per
  // 3-8 s.  Renderer reads last_lightning_ms_ to paint the 80 ms flash.
  if (weather_ != (uint8_t)Weather::Rain) return;
  if (now_ms - last_lightning_ms_ < 3000) return;
  uint32_t roll = rng_next();
  // ~1 % per tick rolled past the 3 s floor; ceiling is ~8 s
  // because by then the cumulative probability is high.
  if ((roll % 100) == 0 || now_ms - last_lightning_ms_ > 8000) {
    last_lightning_ms_ = now_ms;
    play_clip(ClipId::Thunder);
  }
}

Weather Game::tomorrow_weather() const {
  // Same 32-bucket distribution as update_weather, but rolled for
  // today_day_index_ + 1.
  if (today_day_index_ == 0) return Weather::Sunny;
  uint32_t r = (today_day_index_ + 1) * 2654435761u;
  uint8_t roll = (uint8_t)(r % 32);
  if      (roll < 14) return Weather::Sunny;
  else if (roll < 20) return Weather::Cloudy;
  else if (roll < 25) return Weather::Rain;
  else if (roll < 29) return Weather::Snow;
  else                return Weather::Fog;
}

uint32_t Game::trainer_level() const {
  // level = sqrt(xp / 10), integer, capped at 30. Each level needs
  // (level^2) * 10 XP. So lvl 1 = 10 XP, lvl 2 = 40, lvl 5 = 250,
  // lvl 10 = 1000, lvl 30 = 9000.
  uint32_t x = trainer_xp_ / 10;
  uint32_t lvl = 0;
  while ((lvl + 1) * (lvl + 1) <= x && lvl < 30) ++lvl;
  return lvl;
}

void Game::award_xp(uint32_t n) {
  // Round 6 Phase 6F: scale by the active-streak XP bonus (100..200 %).
  uint32_t bonus = xp_bonus_pct();
  uint64_t scaled = (uint64_t)n * bonus / 100;
  uint64_t next = (uint64_t)trainer_xp_ + scaled;
  if (next > 9999) next = 9999;
  trainer_xp_ = (uint32_t)next;
  dirty_ = true;
}

uint8_t Game::skill_intelligence() const {
  // Sum of trick performance counters * 4 + learned-tricks bit count * 5.
  uint32_t sum = 0;
  for (int i = 0; i < (int)Trick::COUNT; ++i) sum += trick_perf_[i];
  uint32_t learned = (uint32_t)__builtin_popcount(tricks_learned_);
  uint32_t v = sum * 4 + learned * 5;
  if (v > 100) v = 100;
  return (uint8_t)v;
}

uint8_t Game::skill_stamina() const {
  // 100 lifetime steps = 50 points; 1000 steps = max.
  uint64_t v = total_steps_ / 10;
  if (v > 100) v = 100;
  return (uint8_t)v;
}

uint8_t Game::skill_charm() const {
  // 50 lifetime pets = 50; 100 = max.
  uint64_t v = total_pets_;
  if (v > 100) v = 100;
  return (uint8_t)v;
}

bool Game::wheel_available() const {
  if (today_day_index_ == 0) return false;          // no synced clock yet
  return last_login_wheel_day_ != today_day_index_;
}

uint8_t Game::spin_wheel() {
  if (!wheel_available()) return 255;
  // Deterministic-ish pick from a fresh rng_next draw; reroll if we
  // land on the sticker reward but all 5 stickers are already unlocked.
  uint8_t reward = (uint8_t)(rng_next() % 5);
  for (int try_n = 0; try_n < 5 && reward == 4 && stickers_unlocked_ == 0x1F;
       ++try_n) {
    reward = (uint8_t)(rng_next() % 4);   // re-roll among the other 4
  }
  switch (reward) {
    case 0:  grant_biscuits(5);            break;
    case 1:  bones_collected_ += 3;        break;
    case 2:  treats_[0]++;                 break;   // Biscuit treat
    case 3:  treats_[1]++;                 break;   // Bacon treat
    case 4: {
      // Pick the first locked sticker bit.
      for (int i = 0; i < 5; ++i) {
        if (!((stickers_unlocked_ >> i) & 1)) {
          stickers_unlocked_ |= (uint8_t)(1u << i);
          break;
        }
      }
      break;
    }
    default: break;
  }
  last_login_wheel_day_ = today_day_index_;
  last_wheel_reward_    = reward;
  play_clip(ClipId::Achieve);
  dirty_ = true;
  return reward;
}

void Game::set_pet_name(const char* name) {
  if (!name) return;
  int n = 0;
  while (n < 11 && name[n] != '\0') {
    char c = name[n];
    // Defensively strip control bytes / nulls; allow printable ASCII
    // including space.
    if (c < 0x20 || c > 0x7E) break;
    pet_name_[n] = c;
    ++n;
  }
  if (n == 0) {
    std::memcpy(pet_name_, "Bailey", 7);
  } else {
    pet_name_[n] = '\0';
  }
  dirty_ = true;
}

void Game::set_birthday(uint8_t month, uint8_t day) {
  if (month < 1 || month > 12 || day < 1 || day > 31) return;
  birthday_month_ = month;
  birthday_day_   = day;
  dirty_          = true;
}

uint32_t Game::fishing_phase_ms_remaining() const {
  if (mode_ != GameMode::Fishing) return 0;
  // 5 s overall window before timeout.
  uint32_t elapsed = last_tick_ms_ - fishing_started_ms_;
  if (elapsed >= 5000) return 0;
  return 5000 - elapsed;
}

uint32_t Game::tug_ms_remaining() const {
  if (mode_ != GameMode::TugOfWar) return 0;
  uint32_t elapsed = last_tick_ms_ - tug_started_ms_;
  if (elapsed >= 5000) return 0;
  return 5000 - elapsed;
}

void Game::update_minigames(uint32_t now_ms) {
  // Fishing: auto-end after the 5 s window if the player never tapped
  // during the nibble.
  if (mode_ == GameMode::Fishing) {
    if (now_ms - fishing_started_ms_ >= 5000) {
      mode_ = GameMode::Idle;
      dirty_ = true;
    }
    return;
  }
  // TugOfWar: end after 5 s; record high score; +happiness scaled.
  if (mode_ == GameMode::TugOfWar) {
    if (now_ms - tug_started_ms_ >= 5000) {
      if (tug_count_ > tug_high_score_) tug_high_score_ = tug_count_;
      // 1 happiness per 4 alternations, capped.
      uint32_t boost = tug_count_ / 4;
      if (boost > 30) boost = 30;
      pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + boost);
      mode_ = GameMode::Idle;
      dirty_ = true;
    }
    return;
  }
  // MemoryPaws: a 3 s reaction window per round. Missing it ends the
  // game.
  if (mode_ == GameMode::MemoryPaws) {
    if (now_ms - memory_round_started_ms_ >= 3000) {
      mode_ = GameMode::Idle;
      dirty_ = true;
    }
    return;
  }
}

void Game::maybe_spawn_firefly(uint32_t now_ms) {
  // Round 5 Phase B: spawn a firefly during low-daylight (evening /
  // night) at a low rate. Cleared either by player PetTap (catch) or
  // by the 3 s firefly-active timer expiring naturally.
  if (firefly_active()) return;
  if (mode_ != GameMode::Idle) return;
  if (daylight_ > 0.4f) return;
#if BAILEY_FAST_DECAY
  constexpr uint32_t kInvProb = 200;
#else
  constexpr uint32_t kInvProb = 4000;
#endif
  if ((rng_next() % kInvProb) != 0) return;
  // Pick a spot in the play area (upper third so it looks like it's
  // hovering above the floor line).
  firefly_x_ = (int16_t)(40 + (rng_next() % 160));
  firefly_y_ = (int16_t)(60 + (rng_next() % 60));
  firefly_spawn_ms_ = now_ms;
}

void Game::maybe_trigger_snore(uint32_t now_ms) {
  // Round 4: periodic SnoreLoud clip while pet is Sleeping. Every 6 s.
  if (pet_.mood != Mood::Sleeping) return;
  if (now_ms - last_snore_ms_ < 6000) return;
  last_snore_ms_ = now_ms;
  play_clip(ClipId::SnoreLoud);
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
  // Round 6 Phase 6A: while sick, health drains ~10 points per simulated
  // game-hour (or per 6 s in BAILEY_FAST_DECAY mode).
  if (sickness_ != 0 && health_stat_ > 0) {
    health_decay_acc_ms_ += dt_ms;
#if BAILEY_FAST_DECAY
    constexpr uint32_t kHealthStepMs = 600;          // 1 pt every 0.6 s
#else
    constexpr uint32_t kHealthStepMs = 6u * 60 * 1000; // 1 pt every 6 min
#endif
    while (health_decay_acc_ms_ >= kHealthStepMs && health_stat_ > 0) {
      health_decay_acc_ms_ -= kHealthStepMs;
      health_stat_--;
      dirty_ = true;
    }
  } else {
    health_decay_acc_ms_ = 0;
  }
}

void Game::try_cure_sickness() {
  if (sickness_ == 0) return;
  sickness_ = 0;
  sick_started_ms_ = 0;
  pet_.stats.happiness = clamp_stat((int)pet_.stats.happiness + 10);
  // Round 6 Phase 6A: cure restores 50 health (clamped at 100).
  uint32_t h = (uint32_t)health_stat_ + 50;
  health_stat_ = (uint8_t)(h > 100 ? 100 : h);
  // Round 6 Phase 6D: push this cure into the vet-history ring buffer
  // (only when we have a synced clock; today_day_index_ == 0 means
  // we don't yet know what day it is).
  if (today_day_index_ != 0) {
    vet_history_days_[vet_history_head_] = today_day_index_;
    vet_history_head_ = (uint8_t)((vet_history_head_ + 1) % 5);
    if (vet_history_count_ < 5) vet_history_count_++;
  }
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
  // Round 6 Phase 6G: ids 5..7 are the extra Cream / Merle / Husky
  // coats; allow them only when their unlock bit is set.
  if (id > 7) return;
  if (id > 4 && !coat_unlocked(id)) return;
  coat_pattern_ = id;
  if (mode_ == GameMode::PickingCoat) {
    mode_ = GameMode::Idle;
  }
  dirty_ = true;
}

// ===== Round 2 systems =====

void Game::grant_biscuits(uint32_t n) {
  // Horoscope LUCKY: +50 % biscuit grants (round up so a +1 stays +1 too).
  if (horoscope_id() == 4) n = (n * 3 + 1) / 2;
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

  // Rare ambient visit: spawns when slot 0 is free. ~50% of those
  // spawns ALSO bring a different second friend (if slot 1 is free).
  // Player-invited visits still fill slot 1 the same way as before.
  if (!in_transition() && mode_ == GameMode::Idle && npc_visit_kind_ == 0) {
#if BAILEY_FAST_DECAY
    constexpr uint32_t kInvProb = 60;        // very dense in fast mode
#else
    constexpr uint32_t kInvProb = 3000;      // ~once / ~50 s at 60fps
#endif
    if ((rng_next() % kInvProb) == 0) {
      // Round 5 Phase D2: 1 % chance of a mystery dog instead of a
      // named friend. Mystery visitor is solo (no slot 1 friend).
      if ((rng_next() % 100) == 0) {
        npc_visit_kind_ = kMysteryVisitorKind;   // 255 sentinel
        npc_visit_ms_   = now_ms;
        unlock_achievement(AchievementId::MysteryMet);
      } else {
        // Round 6 Phase 6E: bias ambient spawns toward wish-listed
        // friends. Half the time, if the wishlist is non-empty, pick
        // uniformly from the wishlisted set; otherwise fall back to
        // the uniform-over-all pick.
        int friend_a;
        uint8_t wl = friend_wishlist_mask_;
        if (wl != 0 && (rng_next() & 1u)) {
          int count = 0;
          int picks[(int)Friend::COUNT];
          for (int i = 0; i < (int)Friend::COUNT; ++i)
            if (wl & (1u << i)) picks[count++] = i;
          friend_a = picks[rng_next() % count];
        } else {
          friend_a = (int)(rng_next() % (int)Friend::COUNT);
        }
        npc_visit_kind_ = (uint8_t)(friend_a + 1);
        npc_visit_ms_   = now_ms;
        // Half the time, bring a second different friend along, but
        // only if slot 1 isn't already taken by a player-invite.
        if (npc_visit_kind2_ == 0 && (rng_next() & 1u)) {
          int friend_b = (int)(rng_next() % (uint32_t)((int)Friend::COUNT - 1));
          if (friend_b >= friend_a) friend_b++;   // uniform over the other 7
          npc_visit_kind2_ = (uint8_t)(friend_b + 1);
          npc_visit_ms2_   = now_ms;
        }
      }
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
  // Round 5: user-configurable birthday (was BAILEY_BIRTHDAY_MONTH/DAY
  // compile-time macros; now stored in save as birthday_month_/_day_).
  bool birthday   = (lt.month == birthday_month_ && lt.day == birthday_day_);
  bool halloween  = (lt.month == 10 && lt.day == 31);
  bool christmas  = (lt.month == 12 && lt.day == 25);
  bool stpatrick  = (lt.month == 3  && lt.day == 17);
  // Round 5 Phase D1: Easter (fixed Apr 12 for simplicity -- a movable
  // feast pinned to a single date), Valentine's, New Year.
  bool easter     = (lt.month == 4  && lt.day == 12);
  bool valentines = (lt.month == 2  && lt.day == 14);
  bool newyear    = (lt.month == 1  && lt.day == 1);
  // Round 6 Phase 6C: Cherry Blossom Day (Mar 27).
  bool cherry     = (lt.month == 3  && lt.day == 27);
  // Round 6 Phase 6F: Day of Dogs (Aug 26).
  bool dog_day    = (lt.month == 8  && lt.day == 26);
  is_birthday_today_ = birthday;
  // Holiday IDs: 0 none, 1 birthday, 2 halloween, 3 christmas, 4 st-patrick,
  // 5 easter, 6 valentines, 7 newyear, 8 cherry blossom, 9 day-of-dogs.
  active_holiday_    = birthday   ? 1 :
                       halloween  ? 2 :
                       christmas  ? 3 :
                       stpatrick  ? 4 :
                       easter     ? 5 :
                       valentines ? 6 :
                       newyear    ? 7 :
                       cherry     ? 8 :
                       dog_day    ? 9 : 0;
  // Round 6 Phase 6C: once per Cherry Blossom Day, grant +5 biscuits.
  if (cherry) {
    uint32_t day = local_day_index(now_unix_ms, settings_.tz_offset_min);
    if (cherry_blossom_last_day_ != day) {
      cherry_blossom_last_day_ = day;
      grant_biscuits(5);
      dirty_ = true;
    }
  }
  // Round 6 Phase 6F: Day of Dogs (Aug 26) -- bump every friend's
  // visit count by 1 and grant +10 biscuits, once per year.
  if (dog_day) {
    uint32_t day = local_day_index(now_unix_ms, settings_.tz_offset_min);
    if (day_of_dogs_last_day_ != day) {
      day_of_dogs_last_day_ = day;
      for (int i = 0; i < (int)Friend::COUNT; ++i) {
        friend_visits_[i]++;
        if (friend_bond_levels_[i] < 5) friend_bond_levels_[i]++;
        friend_last_visit_day_[i] = day;
      }
      grant_biscuits(10);
      // Spawn a friend in slot 0 to make the event feel like a party.
      if (npc_visit_kind_ == 0) {
        int pick = (int)(rng_next() % (int)Friend::COUNT);
        npc_visit_kind_ = (uint8_t)(pick + 1);
        npc_visit_ms_   = last_tick_ms_;
      }
      unlock_achievement(AchievementId::Socialite);
      update_earned_titles();
      dirty_ = true;
    }
  }
  // Auto-unlock the seasonal accessory the first time we see the day.
  if (halloween)  seasonal_unlocks_ |= 0x01;   // pumpkin       (id 4)
  if (christmas)  seasonal_unlocks_ |= 0x02;   // santa hat     (id 5)
  if (stpatrick)  seasonal_unlocks_ |= 0x04;   // shamrock      (id 6)
  if (easter)     seasonal_unlocks_ |= 0x08;   // egg basket    (id 7)
  if (valentines) seasonal_unlocks_ |= 0x10;   // heart bandana (id 8)
  // Round 6 Phase 6I: Halloween also unlocks the full witch hat
  // (id 9) + ghost sheet (id 10) costumes.
  if (halloween)  halloween_costumes_unlocked_ |= 0x03;

  if (birthday) {
    uint32_t day = local_day_index(now_unix_ms, settings_.tz_offset_min);
    if (birthday_celebrated_day_ != day) {
      birthday_celebrated_day_ = day;
      if (accessory_id_ == 0 && accessory_unlocked(3)) accessory_id_ = 3;
      unlock_achievement(AchievementId::BirthdayBoy);
      play_clip(ClipId::Fanfare);
      dirty_ = true;
    }
    // Round 6 Phase 6F: arm the cake-pending flag (cleared automatically
    // 6 seconds after first detection so the 3-stage animation plays
    // through twice and self-dismisses).
    uint32_t bday_day = local_day_index(now_unix_ms, settings_.tz_offset_min);
    if ((uint8_t)bday_day != birthday_cake_seen_day_) {
      if (birthday_cake_started_ms_ == 0) {
        birthday_cake_started_ms_ = last_tick_ms_;
      } else if (last_tick_ms_ - birthday_cake_started_ms_ > 6000) {
        birthday_cake_seen_day_ = (uint8_t)bday_day;
        birthday_cake_started_ms_ = 0;
        dirty_ = true;
      }
    }
  } else {
    birthday_cake_started_ms_ = 0;
  }
  if (halloween || christmas || stpatrick || easter || valentines || newyear) {
    unlock_achievement(AchievementId::SeasonalGreetings);
    if (christmas) weather_ = (uint8_t)Weather::Snow;
  }
  // Round 5 Phase D1: stamp New Year fireworks start-time on the
  // first tick of Jan 1. ui.cpp checks this to render a 5 s burst.
  if (newyear) {
    uint32_t day = local_day_index(now_unix_ms, settings_.tz_offset_min);
    if (last_new_year_day_ != day) {
      last_new_year_day_       = day;
      new_year_fireworks_until_ms_ = last_tick_ms_ + 5000;
      play_clip(ClipId::Fanfare);
      dirty_ = true;
    }
  }
  // Round 4: on Christmas, auto-switch to Snow Park scene once per
  // day. Player can CycleScene afterward and we won't undo their pick.
  if (christmas) {
    uint32_t day = local_day_index(now_unix_ms, settings_.tz_offset_min);
    if (last_xmas_auto_scene_day_ != day) {
      last_xmas_auto_scene_day_ = day;
      settings_.scene_id = 7;
      scene_id_          = 7;
      dirty_             = true;
    }
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

  // Walk (slow) and Run (fast) both drive an x-offset that traces a
  // triangle wave 0 -> max -> 0 over a window.
  if (ambient_behavior_ == 1 || ambient_behavior_ == 5) {
    bool is_run = (ambient_behavior_ == 5);
    float window = is_run ? 2000.0f : 4000.0f;
    int   max_off = is_run ? 48 : 32;
    float t = (float)elapsed / window;
    if (t > 1.0f) t = 1.0f;
    int off = (int)(max_off * (1.0f - fabsf(t * 2 - 1.0f)));
    ambient_x_offset_ = (int16_t)(off * ambient_walk_dir_);
    if (t >= 1.0f) {
      ambient_behavior_ = 0;
      ambient_x_offset_ = 0;
    }
  } else if (ambient_behavior_ == 7) {
    // Scenery interact: walk to +50, settle there for 4 s, walk back.
    constexpr int   target = 50;
    constexpr float walk_ms = 1500.0f;
    constexpr float settle_ms = 4000.0f;
    constexpr float full = walk_ms * 2 + settle_ms;
    float et = (float)elapsed;
    if (et < walk_ms) {
      ambient_x_offset_ = (int16_t)(target * (et / walk_ms));
    } else if (et < walk_ms + settle_ms) {
      ambient_x_offset_ = (int16_t)target;
    } else if (et < full) {
      float back = (et - walk_ms - settle_ms) / walk_ms;
      ambient_x_offset_ = (int16_t)(target * (1.0f - back));
    } else {
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
    if      (pct < 25) next = 0;  // stand    25%
    else if (pct < 45) next = 1;  // walk     20%
    else if (pct < 60) next = 5;  // run      15%
    else if (pct < 70) next = 2;  // sit      10%
    else if (pct < 78) next = 6;  // lie down  8%
    else if (pct < 85) next = 3;  // pant      7%
    else if (pct < 90) next = 4;  // bark      5%
    else               next = 7;  // scenery interact 10%

    ambient_behavior_ = next;
    if (next == 1 || next == 5) {
      ambient_walk_dir_ = (r & 1) ? 1 : -1;
    } else {
      ambient_x_offset_ = 0;
    }
    if (next == 4) {
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
    // Round 6 Phase 6I: first synced tick also grants a daily seal.
    if (daily_seals_last_day_ != day) {
      daily_seals_last_day_ = day;
      if (daily_seals_total_ < 255) daily_seals_total_++;
      dirty_ = true;
    }
    return;
  }
  if (day == today_day_index_) return;

  // Roll over: write yesterday's average happiness into the ring buffer.
  uint8_t avg = today_samples_ > 0
                ? (uint8_t)(today_happiness_sum_ / today_samples_)
                : 0;
  mood_history_[mood_history_head_] = avg;
  mood_history_head_ = (uint8_t)((mood_history_head_ + 1) % 7);

  // Round 6 Phase 6C: write yesterday's diary entry. Message-bank id
  // is templated from today_actions_ + walk_today_steps_ + avg:
  //   0 great day (avg>=80), 1 good day (avg>=60), 2 ok (avg>=40),
  //   3 bored (avg>=20), 4 lonely (avg<20),
  //   5 active (>=10 actions today),
  //   6 wanderer (walk_today_steps_ >= 50),
  //   7 quiet (no actions + no steps).
  uint8_t diary_id;
  if (today_actions_ == 0 && walk_today_steps_ == 0)  diary_id = 7;
  else if (walk_today_steps_ >= 50)                   diary_id = 6;
  else if (today_actions_ >= 10)                      diary_id = 5;
  else if (avg >= 80)                                 diary_id = 0;
  else if (avg >= 60)                                 diary_id = 1;
  else if (avg >= 40)                                 diary_id = 2;
  else if (avg >= 20)                                 diary_id = 3;
  else                                                diary_id = 4;
  diary_entries_[diary_head_] = diary_id;
  diary_head_ = (uint8_t)((diary_head_ + 1) % 7);

  // Reset bedtime flag once per day (was tied to a noon check before).
  well_tucked_in_today_ = 0;

  // Round 5 Phase C2: bump active streak iff yesterday hit the daily
  // action goal; otherwise reset to 0.
  if (today_actions_ >= kDailyActionGoal) {
    if (active_streak_days_ < 9999) active_streak_days_++;
  } else {
    active_streak_days_ = 0;
  }

  today_day_index_     = day;
  today_happiness_sum_ = 0;
  today_samples_       = 0;
  today_actions_       = 0;
  walk_today_steps_    = 0;     // round 3: reset daily walk counter at midnight
  // Round 6 Phase 6I: stamp a daily seal whenever we cross into a new day.
  if (daily_seals_last_day_ != day) {
    daily_seals_last_day_ = day;
    if (daily_seals_total_ < 255) daily_seals_total_++;
  }
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
      if (trick_perf_[i] >= 10) unlock_achievement(AchievementId::Showstopper);
      // Round 6 Phase 6K: random tricks also count toward the chain.
      if (trick_chain_count_ == 0 ||
          last_tick_ms_ - trick_chain_first_ms_ > 15000) {
        trick_chain_first_ms_ = last_tick_ms_;
        trick_chain_count_    = 0;
      }
      if (trick_chain_count_ < 255) trick_chain_count_++;
      if (trick_chain_count_ >= 5) {
        if (trick_chain_runs_ < 255) trick_chain_runs_++;
        grant_biscuits(5);
        trick_chain_count_    = 0;
        trick_chain_first_ms_ = 0;
      }
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
  constexpr int last = (int)MenuTab::Shop;
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
  if (i == 15) return 0;           // Trade-bones row: priced in bones, not biscuits
  if (i < 19) return 10;           // bath toy (rubber duck / boat / fish)
  if (i == 19) return 30;          // Round 6 Phase 6D: auto-feeder item
  return 0;
}

bool Game::buy_item(uint8_t i) {
  // Special row: spend 5 bones for 1 biscuit. Priced in bones, not biscuits,
  // so the standard "biscuits_ < price" check doesn't apply.
  if (i == 15) {
    if (bones_collected_ < 5) return false;
    bones_collected_ -= 5;
    grant_biscuits(1);
    dirty_ = true;
    return true;
  }
  uint32_t price = shop_price(i);
  if (price == 0 || biscuits_ < price) return false;
  // Bath toy rows (16/17/18): set bit + activate.
  if (i >= 16 && i < 19) {
    uint8_t bit = (uint8_t)(1u << (i - 16));
    if (bath_toys_owned_ & bit) return false;
    bath_toys_owned_ |= bit;
    bath_toy_active_  = (uint8_t)(i - 16 + 1);   // 1..3
    biscuits_ -= price;
    dirty_ = true;
    return true;
  }
  // Round 6 Phase 6D: auto-feeder (row 19): one-time purchase.
  if (i == 19) {
    if (auto_feeder_owned_) return false;
    auto_feeder_owned_ = 1;
    biscuits_ -= price;
    dirty_ = true;
    return true;
  }
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
  // id 3 (party hat) -> EvolvedToAdult; ids 4-8 seasonal (auto-unlock
  // on Halloween / Christmas / St. Patrick's / Easter / Valentine's,
  // persistent thereafter).
  switch (id) {
    case 0: return true;  // bare
    case 1: return is_unlocked(achievements_, AchievementId::FirstPet);
    case 2: return is_unlocked(achievements_, AchievementId::Streak3Days);
    case 3: return is_unlocked(achievements_, AchievementId::EvolvedToAdult);
    case 4: return (seasonal_unlocks_ & 0x01) != 0;   // pumpkin
    case 5: return (seasonal_unlocks_ & 0x02) != 0;   // santa hat
    case 6: return (seasonal_unlocks_ & 0x04) != 0;   // shamrock collar
    case 7: return (seasonal_unlocks_ & 0x08) != 0;   // egg basket
    case 8: return (seasonal_unlocks_ & 0x10) != 0;   // heart bandana
    case 9:  return (halloween_costumes_unlocked_ & 0x01) != 0;  // witch hat
    case 10: return (halloween_costumes_unlocked_ & 0x02) != 0;  // ghost sheet
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
  // Best-friend bond: stamp a stable hash of the sync payload so the
  // Stats tab can show "Best friend: XXXX". Knuth's multiplicative hash
  // mixed over the first 7 bytes (excludes the CRC). Hash 0 is reserved
  // for "no bond" so bump if we happen to land on zero.
  uint32_t hash = 0x9E3779B9u;
  for (int i = 0; i < 7; ++i) hash = (hash ^ buf[i]) * 2654435761u;
  best_friend_hash_ = hash == 0 ? 0x9E3779B9u : hash;
  // Round 6 Phase 6J: push partner hash into leaderboard ring buffer
  // (dedupe against the last entry so back-to-back same-code applies
  // don't fill the buffer with duplicates).
  uint8_t last = (uint8_t)((leaderboard_head_ + 8 - 1) % 8);
  if (leaderboard_count_ == 0 || leaderboard_hashes_[last] != best_friend_hash_) {
    leaderboard_hashes_[leaderboard_head_] = best_friend_hash_;
    leaderboard_head_ = (uint8_t)((leaderboard_head_ + 1) % 8);
    if (leaderboard_count_ < 8) leaderboard_count_++;
  }
  unlock_achievement(AchievementId::Pawmates);
  dirty_ = true;
  return true;
}

// ---- Round 3 Phase 3D: Bedtime stories ----

const char* Game::bedtime_story_text() const {
  static const char* const kStories[8] = {
    "Once upon a time...",
    "A brave hound dog...",
    "...chased the moon...",
    "...found a bone...",
    "...rolled in grass...",
    "...made a friend...",
    "...slept by the fire.",
    "The end. Goodnight.",
  };
  return kStories[bedtime_story_idx_ % 8];
}

// ---- Round 3 Phase 3C: Gift treats ----

// Code format: 9 characters
//   "GIFT" + tier_digit + 3 hex anti-replay seed + 1 hex checksum
// Example: "GIFT13A7F2". Tier 0/1/2 = biscuit/bacon/steak.
// The hex chars use 0-9A-F. Checksum is the XOR of the 4 payload nibbles
// then ANDed with 0xF, displayed as a single hex digit. Anyone with the
// 5-char tail can validate without a roundtrip.
namespace {
inline char hex_digit(uint8_t v) {
  v &= 0xF;
  return v < 10 ? (char)('0' + v) : (char)('A' + v - 10);
}
inline int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
}  // namespace

bool Game::generate_gift_code(uint8_t tier, char* out_buf, int buf_len) {
  if (tier >= (uint8_t)TreatTier::COUNT) return false;
  if (treats_[tier] == 0) return false;          // need one to give one
  if (!out_buf || buf_len < 10) return false;
  uint32_t seed = rng_next() & 0xFFF;            // 3 hex digits of entropy
  uint8_t tnib = (uint8_t)(tier & 0xF);
  uint8_t n1   = (uint8_t)((seed >> 8) & 0xF);
  uint8_t n2   = (uint8_t)((seed >> 4) & 0xF);
  uint8_t n3   = (uint8_t)(seed & 0xF);
  uint8_t crc  = (uint8_t)(tnib ^ n1 ^ n2 ^ n3);
  out_buf[0] = 'G';
  out_buf[1] = 'I';
  out_buf[2] = 'F';
  out_buf[3] = 'T';
  out_buf[4] = hex_digit(tnib);
  out_buf[5] = hex_digit(n1);
  out_buf[6] = hex_digit(n2);
  out_buf[7] = hex_digit(n3);
  out_buf[8] = hex_digit(crc);
  out_buf[9] = '\0';
  return true;
}

bool Game::apply_gift_code(const char* code) {
  if (!code) return false;
  if (code[0] != 'G' || code[1] != 'I' || code[2] != 'F' || code[3] != 'T') return false;
  int t = hex_value(code[4]);
  int a = hex_value(code[5]);
  int b = hex_value(code[6]);
  int c = hex_value(code[7]);
  int crc_in = hex_value(code[8]);
  if (t < 0 || a < 0 || b < 0 || c < 0 || crc_in < 0) return false;
  if (t >= (int)TreatTier::COUNT) return false;
  uint8_t crc = (uint8_t)((t ^ a ^ b ^ c) & 0xF);
  if (crc != (uint8_t)crc_in) return false;
  // One redeemed gift per local day. If we have no clock yet, fall
  // through (allow). today_day_index_ == 0 means "no clock yet".
  if (today_day_index_ != 0 &&
      last_gift_received_day_ == today_day_index_) return false;
  treats_[t]++;
  if (today_day_index_ != 0) last_gift_received_day_ = today_day_index_;
  dirty_ = true;
  return true;
}

// Round 5 Phase D remainder: postcard message bank + code format.
//
// Code: "POST" + 5 hex chars
//   chars 4-5 = message id (2 hex = 8 bits; we mask to 4 bits / 0..15)
//   chars 6-7 = sender seed (anti-replay flavor; not validated)
//   char  8   = checksum (XOR of the 4 nibbles)
static const char* const kPostcards[16] = {
  "miss you, friend",
  "made a new friend",
  "took a great nap",
  "ran fast today",
  "found a stick",
  "love this weather",
  "saw a squirrel",
  "got a good belly rub",
  "ate a fancy treat",
  "rolled in the grass",
  "watched the sunset",
  "tail won't stop wagging",
  "dreamt of bones",
  "splashed a puddle",
  "snoozed in a sunbeam",
  "thinking of you",
};

const char* Game::postcard_message(uint8_t id) {
  if (id >= 16) return nullptr;
  return kPostcards[id];
}

bool Game::generate_postcard_code(uint8_t msg_id, char* out_buf, int buf_len) {
  if (msg_id >= 16) return false;
  if (!out_buf || buf_len < 10) return false;
  uint32_t seed = rng_next() & 0xFF;            // 2 hex digits
  uint8_t n0 = (uint8_t)(msg_id & 0x0F);        // low nibble (we only need 4 bits)
  uint8_t n1 = (uint8_t)((msg_id >> 4) & 0x0F); // high nibble (always 0 for 0..15)
  uint8_t n2 = (uint8_t)((seed >> 4) & 0x0F);
  uint8_t n3 = (uint8_t)(seed & 0x0F);
  uint8_t crc = (uint8_t)(n0 ^ n1 ^ n2 ^ n3);
  auto hex = [](uint8_t v) -> char {
    v &= 0xF;
    return v < 10 ? (char)('0' + v) : (char)('A' + v - 10);
  };
  out_buf[0] = 'P';
  out_buf[1] = 'O';
  out_buf[2] = 'S';
  out_buf[3] = 'T';
  out_buf[4] = hex(n0);
  out_buf[5] = hex(n1);
  out_buf[6] = hex(n2);
  out_buf[7] = hex(n3);
  out_buf[8] = hex(crc);
  out_buf[9] = '\0';
  return true;
}

bool Game::apply_postcard_code(const char* code) {
  if (!code) return false;
  if (code[0] != 'P' || code[1] != 'O' || code[2] != 'S' || code[3] != 'T') return false;
  auto hex_v = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };
  int a = hex_v(code[4]);
  int b = hex_v(code[5]);
  int c = hex_v(code[6]);
  int d = hex_v(code[7]);
  int crc_in = hex_v(code[8]);
  if (a < 0 || b < 0 || c < 0 || d < 0 || crc_in < 0) return false;
  uint8_t crc = (uint8_t)((a ^ b ^ c ^ d) & 0xF);
  if (crc != (uint8_t)crc_in) return false;
  uint8_t msg_id = (uint8_t)((b << 4) | a);
  if (msg_id >= 16) return false;
  last_postcard_msg_id_ = msg_id;
  if (postcards_received_ < 255) postcards_received_++;
  dirty_ = true;
  return true;
}

int Game::latest_achievement(int idx) const {
  // Walk achievement bits from highest to lowest. Append-only IDs
  // mean higher = added later = "more recent" in roster terms.
  if (idx < 0) return -1;
  int seen = 0;
  for (int i = kAchievementCount - 1; i >= 0; --i) {
    if (is_unlocked(achievements_, (AchievementId)i)) {
      if (seen == idx) return i;
      ++seen;
    }
  }
  return -1;
}

// ---- Round 3 Phase 1C: Daily quest + Pet horoscope ----

uint8_t Game::daily_quest_id() const {
  if (today_day_index_ == 0) return 0;     // no synced clock yet
  return (uint8_t)(today_day_index_ % 2);  // 0 = walk, 1 = happiness
}

uint32_t Game::daily_quest_progress() const {
  switch (daily_quest_id()) {
    case 0: return walk_today_steps_;
    case 1: return pet_.stats.happiness;
    default: return 0;
  }
}

uint32_t Game::daily_quest_goal() const {
  switch (daily_quest_id()) {
    case 0: return 30;
    case 1: return 90;
    default: return 0;
  }
}

const char* Game::daily_quest_text() const {
  switch (daily_quest_id()) {
    case 0: return "Walk 30 steps today";
    case 1: return "Reach happiness 90";
    default: return "";
  }
}

bool Game::daily_quest_awarded_today() const {
  return today_day_index_ != 0 &&
         daily_quest_awarded_day_ == today_day_index_;
}

void Game::update_daily_quest(uint64_t now_unix_ms) {
  (void)now_unix_ms;
  if (today_day_index_ == 0) return;
  // Round 6 Phase 6H: weekly challenge rolls over every 7 days; if the
  // current week is new, reset the progress accumulator. The reward is
  // payable once per week when progress >= target.
  uint32_t this_week = today_day_index_ / 7;
  if (this_week != weekly_last_awarded_week_ && weekly_challenge_complete()) {
    grant_biscuits(20);
    weekly_last_awarded_week_ = this_week;
    weekly_steps_progress_    = 0;
    dirty_ = true;
  }
  // Auto-reset progress on a new week even if the goal wasn't met.
  static uint32_t s_week_tracker = 0;
  if (s_week_tracker != this_week) {
    if (s_week_tracker != 0 && this_week > s_week_tracker &&
        weekly_last_awarded_week_ != this_week) {
      weekly_steps_progress_ = 0;
      dirty_ = true;
    }
    s_week_tracker = this_week;
  }

  if (daily_quest_awarded_today()) return;
  if (!daily_quest_complete()) return;
  grant_biscuits(5);
  daily_quest_awarded_day_ = today_day_index_;
  // Round 6 Phase 6F: push this quest id into the history ring.
  quest_history_[quest_history_head_] = daily_quest_id();
  quest_history_head_ = (uint8_t)((quest_history_head_ + 1) % 7);
  if (quest_history_count_ < 7) quest_history_count_++;
  dirty_ = true;
}

uint8_t Game::horoscope_id() const {
  if (today_day_index_ == 0) return 0;
  return (uint8_t)(today_day_index_ % 5);
}

const char* Game::horoscope_text() const {
  switch (horoscope_id()) {
    case 0: return "PLAYFUL";   // +happy on play
    case 1: return "SLEEPY";    // slower decay
    case 2: return "HUNGRY";    // +hunger on feed
    case 3: return "CURIOUS";   // better walk-finds
    case 4: return "LUCKY";     // +biscuit grants
    default: return "";
  }
}

// Round 6 Phase 6B: title text. If an earned title is chosen, use that;
// otherwise derive from trainer XP band.
const char* Game::trainer_title() const {
  if (chosen_title_id_ >= 1 && chosen_title_id_ <= 4 &&
      (earned_titles_mask_ & (1u << (chosen_title_id_ - 1)))) {
    switch (chosen_title_id_) {
      case 1: return "Bone Hunter";
      case 2: return "Soul Bond";
      case 3: return "Walker";
      case 4: return "Showstopper";
    }
  }
  uint32_t xp = trainer_xp_;
  if (xp >= 5000) return "Legend";
  if (xp >= 2000) return "Master";
  if (xp >= 500)  return "Pro";
  if (xp >= 100)  return "Trainer";
  return "Novice";
}

// Round 6 Phase 6B: bit0 Bone Hunter / 1 Soul Bond / 2 Walker / 3 Showstopper.
void Game::update_earned_titles() {
  uint8_t m = earned_titles_mask_;
  if (bones_collected_ >= 50)                              m |= 1u << 0;
  if (total_steps_     >= 500)                             m |= 1u << 2;
  if (achievements_ & (1ull << (int)AchievementId::Showstopper)) m |= 1u << 3;
  for (int i = 0; i < (int)Friend::COUNT; ++i) {
    if (friend_visits_[i] >= 25) { m |= 1u << 1; break; }
  }
  if (m != earned_titles_mask_) {
    earned_titles_mask_ = m;
    dirty_ = true;
  }
}

void Game::cycle_chosen_title() {
  // Auto -> next earned title -> auto -> ...
  for (int step = 1; step <= 5; ++step) {
    uint8_t next = (uint8_t)((chosen_title_id_ + step) % 5);  // 0..4
    if (next == 0 || (earned_titles_mask_ & (1u << (next - 1)))) {
      chosen_title_id_ = next;
      dirty_ = true;
      return;
    }
  }
}

// Round 6 Phase 6K: convenience setter for the wallpaper cycler.
void Game::cycle_scene_wallpaper() {
  uint8_t s = settings_.scene_id;
  if (s >= 8) return;
  scene_wallpaper_[s] = (uint8_t)((scene_wallpaper_[s] + 1) % 4);
  dirty_ = true;
}

// Round 6 Phase 6J: leaderboard read-out (age_idx 0 = most recent).
uint32_t Game::leaderboard_entry(uint8_t age_idx) const {
  if (age_idx >= leaderboard_count_) return 0;
  uint8_t i = (uint8_t)((leaderboard_head_ + 8 - 1 - age_idx) % 8);
  return leaderboard_hashes_[i];
}

// Round 6 Phase 6J: 6-line printable bio card for sharing.
const char* Game::trainer_photo_card() const {
  static char buf[256];
  unsigned bonded = 0;
  for (int i = 0; i < (int)Friend::COUNT; ++i)
    if (friend_bond_levels_[i] > 0) ++bonded;
  std::snprintf(buf, sizeof(buf),
                "%s, %s\n"
                "Trainer Lv %u  XP %u\n"
                "%u biscuits  %u bones\n"
                "%lu steps  %u tricks\n"
                "%u friends bonded\n"
                "Seals: %u  Achv: %u",
                pet_name_, trainer_title(),
                (unsigned)trainer_level(), (unsigned)trainer_xp_,
                (unsigned)biscuits_, (unsigned)bones_collected_,
                (unsigned long)total_steps_,
                (unsigned)__builtin_popcount(tricks_learned_),
                bonded,
                (unsigned)daily_seals_total_,
                (unsigned)__builtin_popcountll(achievements_));
  return buf;
}

// Round 6 Phase 6I: daily seal granted today?
bool Game::daily_seal_today() const {
  return today_day_index_ != 0 &&
         daily_seals_last_day_ == today_day_index_;
}

// Round 6 Phase 6I: rotating limited-time events. Each event runs for
// 7 days and the slot is keyed off the current week_index modulo the
// number of events. Event id 0 = no event (rest week).
uint8_t Game::current_event_id() const {
  if (today_day_index_ == 0) return 0;
  // Cycle through 5 slots: 0 rest, 1..4 themed events.
  return (uint8_t)((today_day_index_ / 7) % 5);
}

const char* Game::current_event_name() const {
  switch (current_event_id()) {
    case 0: return "";
    case 1: return "Fall Fest";
    case 2: return "Paw Pride Week";
    case 3: return "Winter Cheer";
    case 4: return "Spring Bloom";
    default: return "";
  }
}

uint8_t Game::event_days_remaining() const {
  if (today_day_index_ == 0) return 0;
  uint8_t day_in_week = (uint8_t)(today_day_index_ % 7);
  return (uint8_t)(7 - day_in_week);
}

// Round 6 Phase 6H: gift name lookup for the friend gift log.
const char* Game::gift_name(uint8_t id) {
  switch (id) {
    case 0: return "none";
    case 1: return "ball";
    case 2: return "treat";
    case 3: return "bone";
    case 4: return "sticker";
    case 5: return "biscuit";
    default: return "";
  }
}

bool Game::weekly_challenge_awarded() const {
  if (today_day_index_ == 0) return false;
  return weekly_last_awarded_week_ == today_day_index_ / 7;
}

// Round 6 Phase 6H: spend 100 XP to unlock perk bit (0..2).
bool Game::buy_perk(uint8_t bit) {
  if (bit > 2) return false;
  uint8_t mask = (uint8_t)(1u << bit);
  if (trainer_perks_mask_ & mask) return false;       // already owned
  if (trainer_xp_ < 100) return false;                // can't afford
  trainer_xp_ -= 100;
  trainer_perks_mask_ |= mask;
  dirty_ = true;
  return true;
}

// Round 6 Phase 6F: weekly XP bonus from active streak.
// 100 % baseline + 10 % per streak day, capped at 200 % (10-day streak).
uint16_t Game::xp_bonus_pct() const {
  uint16_t bonus = (uint16_t)active_streak_days_ * 10;
  if (bonus > 100) bonus = 100;
  return (uint16_t)(100 + bonus);
}

// Round 6 Phase 6F: quest history -- age_idx 0 is the most recent.
uint8_t Game::quest_history_entry(uint8_t age_idx) const {
  if (age_idx >= quest_history_count_) return 0xFF;
  uint8_t i = (uint8_t)((quest_history_head_ + 7 - 1 - age_idx) % 7);
  return quest_history_[i];
}

// Round 6 Phase 6F: birthday cake animation flag. Pending on the
// birthday morning until acknowledged via cake_seen() (we mark seen
// implicitly on the first day we recognize the birthday).
bool Game::birthday_cake_pending() const {
  if (!is_birthday_today_) return false;
  // Persisted as the low 8 bits of today_day_index_; one-shot per day.
  return ((uint8_t)today_day_index_) != birthday_cake_seen_day_;
}

// Round 6 Phase 6E: which friends have been dormant for >=3 days?
uint8_t Game::dormant_friends_mask() const {
  uint32_t today = today_day_index_;
  if (today == 0) return 0;
  uint8_t m = 0;
  for (int i = 0; i < (int)Friend::COUNT; ++i) {
    uint32_t last = friend_last_visit_day_[i];
    if (last == 0) continue;            // never visited; nothing to miss
    if (today >= last + 3) m |= (uint8_t)(1u << i);
  }
  return m;
}

// Round 6 Phase 6D: exercise 0..100 derived from today's walk steps.
// 500 steps = full bar (matches the Walker title threshold).
uint8_t Game::exercise_stat() const {
  uint32_t s = (uint32_t)walk_today_steps_ / 5;
  if (s > 100) s = 100;
  return (uint8_t)s;
}

uint32_t Game::vet_history_day(uint8_t age_idx) const {
  if (age_idx >= vet_history_count_) return 0;
  uint8_t i = (uint8_t)((vet_history_head_ + 5 - 1 - age_idx) % 5);
  return vet_history_days_[i];
}

// Round 6 Phase 6C: diary -- age_days 0 is yesterday, 6 is a week ago.
uint8_t Game::diary_entry(uint8_t age_days) const {
  if (age_days >= 7) return 0xFF;
  // diary_head_ points to the next write slot, so the most-recent entry
  // is at (diary_head_ - 1) mod 7.
  uint8_t i = (uint8_t)((diary_head_ + 7 - 1 - age_days) % 7);
  return diary_entries_[i];
}

// Round 6 Phase 6C: short one-line summaries for diary message ids 0..7.
const char* Game::diary_text(uint8_t id) {
  switch (id) {
    case 0: return "Joyful day with Bailey.";
    case 1: return "A pleasant day together.";
    case 2: return "An ordinary day.";
    case 3: return "Bailey was a bit bored.";
    case 4: return "Bailey missed you today.";
    case 5: return "A busy, hands-on day.";
    case 6: return "Long walks, happy paws.";
    case 7: return "A quiet, still day.";
    default: return nullptr;
  }
}

// Round 6 Phase 6B: 4-char engraving for accessory id 2 (blue collar).
// Always 4 chars + NUL; pads with spaces if pet_name is shorter.
const char* Game::collar_engraving() const {
  static char buf[5];
  for (int i = 0; i < 4; ++i) {
    char c = pet_name_[i];
    if (c == '\0') { buf[i] = ' '; }
    else if (c >= 'a' && c <= 'z') buf[i] = (char)(c - 32);
    else buf[i] = c;
  }
  buf[4] = '\0';
  return buf;
}

}  // namespace tama
