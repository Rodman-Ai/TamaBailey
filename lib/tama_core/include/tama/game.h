#pragma once

#include <cstdint>

#include "tama/achievements.h"
#include "tama/audio.h"
#include "tama/clock.h"
#include "tama/friends.h"
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

// Memorial wall UI + history ring-buffer write on death. Disabled by
// default; rebuild with -D BAILEY_MEMORIAL_WALL=1 to bring it back.
// The MenuTab::Memorial enum value stays defined either way (for ABI
// stability and the switch fall-through in ui.cpp); only the active
// rendering / writing code is gated.
#ifndef BAILEY_MEMORIAL_WALL
#define BAILEY_MEMORIAL_WALL 0
#endif

// Base decay/regen tunables (multiplier defaults to settings.decay_mult/10).
#if BAILEY_FAST_DECAY
constexpr uint32_t kBaseMsPerHungerPoint      =   7200;
constexpr uint32_t kBaseMsPerHappinessPoint   =  10800;
constexpr uint32_t kBaseMsPerCleanlinessPoint =  14400;
constexpr uint32_t kBaseMsPerEnergyRegen      =   3000;
constexpr uint64_t kHealthyForAdult           = (uint64_t)24 * 60 * 1000;
constexpr uint64_t kHealthyForSenior          = (uint64_t)96 * 60 * 1000;
constexpr uint64_t kNeglectForMoveOut         = (uint64_t)60 * 1000;       // 60 s
constexpr uint64_t kSeniorLoopMs              = (uint64_t)96 * 60 * 1000;  // another 96 min as senior
#else
constexpr uint32_t kBaseMsPerHungerPoint      = 432000;
constexpr uint32_t kBaseMsPerHappinessPoint   = 648000;
constexpr uint32_t kBaseMsPerCleanlinessPoint = 864000;
constexpr uint32_t kBaseMsPerEnergyRegen      = 180000;
constexpr uint64_t kHealthyForAdult           = (uint64_t)24 * 3600 * 1000;
constexpr uint64_t kHealthyForSenior          = (uint64_t)96 * 3600 * 1000;
constexpr uint64_t kNeglectForMoveOut         = (uint64_t)60 * 60 * 1000;  // 60 min
constexpr uint64_t kSeniorLoopMs              = (uint64_t)96 * 3600 * 1000; // another 96 h
#endif

// How long the MovingOut / Magic transition plays before the restart fires.
constexpr uint32_t kTransitionMs              = 5000;

// How long a friend visits when invited. Random ambient visits use this
// same value. Tripled from 12 s so visits feel like an event, not a
// drive-by.
constexpr uint32_t kFriendVisitMs             = 36000;

// Maximum number of friend dogs that can be visiting Bailey at once.
constexpr int      kMaxVisitors               = 2;

constexpr uint32_t kEnergyCostPlay   = 10;
constexpr uint32_t kActionEatBoost   = 30;
constexpr uint32_t kActionPlayBoost  = 30;
constexpr uint32_t kActionCleanBoost = 60;
constexpr uint32_t kActionPetBoost   =  5;
constexpr uint32_t kActionStrokeBoost = 1;  // continuous stroke
constexpr uint32_t kStreakBonus      = 10;  // daily streak happiness bump

constexpr uint32_t kActionEatDurationMs   = 1500;
constexpr uint32_t kActionPlayDurationMs  = 1800;
constexpr uint32_t kActionCleanDurationMs = 1800;
constexpr uint32_t kActionPetDurationMs   =  600;

constexpr uint32_t kPetCooldownMs   = 12000;
constexpr uint32_t kStrokeCooldownMs = 250;
constexpr uint32_t kSaveIntervalMs  = 30000;
constexpr uint32_t kIdleFrameMs     = 700;

// Cap on offline catch-up: capped to 7 days so a long absence doesn't
// auto-kill Bailey faster than they could actually be neglected.
constexpr uint64_t kMaxOfflineCatchupMs = (uint64_t)7 * 24 * 3600 * 1000;

enum class GameMode : uint8_t {
  Idle           = 0,
  FetchAiming    = 1,
  FetchInFlight  = 2,
  FetchCatching  = 3,
  FetchResult    = 4,
  PickingCoat    = 5,
  PhotoMode      = 6,
  Walking        = 7,   // Round 2: Nintendogs-style walk
};

// Round 2: 5 toys -- ball is unlocked by default. Index also indexes
// into save's toy_owned bitmask.
enum class Toy : uint8_t {
  Ball = 0, Frisbee = 1, Rope = 2, SqueakyDuck = 3, Stick = 4, COUNT = 5,
};
const char* toy_name(Toy t);
constexpr uint8_t kAllToysMask = (1u << (int)Toy::COUNT) - 1;

// Round 2: 3 tiers of treats.
enum class TreatTier : uint8_t { Biscuit = 0, Bacon = 1, Steak = 2, COUNT = 3 };
const char* treat_name(TreatTier t);

// Round 2: current "want" that Bailey is signaling.
enum class Wish : uint8_t {
  None = 0, Treat = 1, Walk = 2, Pet = 3, Fetch = 4, Brush = 5,
};
const char* wish_name(Wish w);

// Round 2: 5 learned words mirroring trick milestones.
enum class Word : uint8_t {
  Name = 0, Sit = 1, Outside = 2, Treat = 3, Bedtime = 4, COUNT = 5,
};
const char* word_name(Word w);

// Tricks (Phase 2). Auto-learned at age milestones (no rhythm mini-game).
enum class Trick : uint8_t {
  Sit       = 0,
  Shake     = 1,
  RollOver  = 2,
  Speak     = 3,
  Spin      = 4,
  COUNT     = 5,
};
const char* trick_name(Trick t);
constexpr uint8_t kAllTricksMask = (1u << (int)Trick::COUNT) - 1;
// Age (ms) at which each trick auto-learns. Scaled by BAILEY_FAST_DECAY.
uint64_t trick_age_threshold(Trick t);

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
  uint8_t   tricks_learned() const { return tricks_learned_; }
  uint64_t  fetch_catches()  const { return fetch_catches_; }
  uint32_t  fetch_state_ms() const { return mode_started_ms_; }
  // Round 2 inspection
  uint32_t  biscuits()       const { return biscuits_; }
  uint8_t   toys_owned()     const { return toy_owned_; }
  Toy       active_toy()     const { return (Toy)active_toy_; }
  uint8_t   treats(TreatTier t) const { return treats_[(int)t]; }
  Wish      current_wish()   const { return (Wish)wish_; }
  uint16_t  walk_steps()     const { return walk_steps_; }
  uint64_t  total_steps()    const { return total_steps_; }
  uint8_t   vocab_learned()  const { return vocab_learned_; }
  uint16_t  trick_perf(Trick t) const { return trick_perf_[(int)t]; }
  Trick     favorite_trick() const;          // most-performed (Sit if tie)
  uint8_t   mood_history(uint8_t day_back) const;  // 0=yesterday .. 6=7 days ago
  bool      is_birthday()    const { return is_birthday_today_; }
  bool      tucked_in()      const { return well_tucked_in_today_ != 0; }
  uint32_t  transition_started_ms() const { return transition_started_ms_; }
  uint8_t   move_out_family_idx()   const { return move_out_family_idx_; }
  uint8_t   ambient_behavior()      const { return ambient_behavior_; }
  int16_t   ambient_x_offset()      const { return ambient_x_offset_; }
  uint32_t  ambient_started_ms()    const { return ambient_started_ms_; }
  uint8_t   shop_cursor()    const { return shop_cursor_; }
  uint8_t   npc_visit_kind(int slot = 0) const {
    if (slot < 0 || slot >= kMaxVisitors) return 0;
    return slot == 0 ? npc_visit_kind_ : npc_visit_kind2_;
  }
  uint32_t  npc_visit_ms(int slot = 0) const {
    if (slot < 0 || slot >= kMaxVisitors) return 0;
    return slot == 0 ? npc_visit_ms_ : npc_visit_ms2_;
  }
  // Active holiday: 0 none, 1 birthday, 2 halloween, 3 christmas
  uint8_t   active_holiday() const { return active_holiday_; }
  // Manually override the weather (e.g. from a wttr.in fetch on the web).
  void      set_weather(Weather w) { weather_ = (uint8_t)w; dirty_ = true; }

  // Equip accessory (no-op if id not unlocked). 0 = unequip.
  void equip_accessory(uint8_t id);
  void choose_coat(uint8_t id);
  bool accessory_unlocked(uint8_t id) const;

  // Background tint from 0.0 (deep night) to 1.0 (full day), based on
  // local clock if synced, else millis-based 24-min cycle.
  float     daylight() const { return daylight_; }
  // Local-time HH:MM string for the status bar; empty if clock not synced.
  const char* clock_string() const { return clock_str_; }

  // --- Settings menus interact via these ---
  enum class MenuTab : uint8_t {
    // Actions is intentionally first so the menu always opens onto it
    // and the tab bar renders it leftmost.
    Actions = 0, Stats = 1, Achievements = 2, Settings = 3,
    Sync = 4, Memorial = 5, Inventory = 6, Shop = 7,
  };
  static MenuTab next_menu_tab(MenuTab cur);
  uint8_t actions_cursor()  const { return actions_cursor_; }
  uint8_t actions_submenu() const { return actions_submenu_; }
  // Voice / menu trick currently being performed.
  // 0 = none, else (1+(int)VoiceX - (int)VoiceSit).
  uint8_t voice_trick_kind()   const { return voice_trick_kind_; }
  uint32_t voice_trick_started_ms() const { return voice_trick_started_ms_; }
  uint32_t friend_visits(Friend f) const {
    int i = (int)f; if (i < 0 || i >= (int)Friend::COUNT) return 0;
    return friend_visits_[i];
  }

  // Round 2 Phase 2 helpers
  bool buy_item(uint8_t shop_index);   // returns true on success
  uint32_t shop_price(uint8_t shop_index) const;
  uint8_t memorial_count() const { return memorial_count_; }
  const SaveData::MemorialEntry& memorial_entry(uint8_t idx) const { return memorial_[idx % 5]; }
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
  // Phase 2
  void update_fetch_mode(uint32_t now_ms);
  void update_weather(uint64_t now_unix_ms);
  void update_sickness(uint32_t dt_ms);
  void update_tricks();
  void try_cure_sickness();
  // Round 2
  void update_walk(uint32_t now_ms);
  void update_wish(uint32_t now_ms);
  void update_birthday(uint64_t now_unix_ms);
  void update_bedtime(uint64_t now_unix_ms);
  void update_ambient(uint32_t now_ms);
  void update_vocab();
  void grant_biscuits(uint32_t n);
  void fulfill_wish_if_matches(Wish what);
  void roll_over_day_if_needed(uint64_t now_unix_ms);
  void perform_random_trick();
  // xorshift32; seeded in init() and stepped per draw. Used for ambient
  // friend-visit spawns where a previous hash-of-now scheme produced a
  // visible bias (the same friend would keep showing up).
  uint32_t rng_next();
  // Death-removal helpers
  void enter_transition(Mood m);   // sets mood + stamps transition_started_ms_
  void restart_pet(bool magic);    // resets pet, preserves achievements / biscuits / etc

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
  MenuTab  menu_tab_         = MenuTab::Actions;
  GameMode mode_             = GameMode::Idle;
  uint32_t mode_started_ms_  = 0;
  uint32_t last_weather_roll_day_ = 0;
  uint32_t sick_started_ms_  = 0;
  uint32_t last_fetch_result_ = 0;  // 1 = caught, 2 = missed, 0 = none

  // Round 2 state
  uint32_t biscuits_                  = 0;
  uint8_t  toy_owned_                 = 1;   // ball owned by default
  uint8_t  active_toy_                = 0;
  uint8_t  treats_[3]                 = {0,0,0};
  uint8_t  wish_                      = 0;   // Wish enum
  uint64_t wish_started_ms_           = 0;
  uint32_t birthday_celebrated_day_   = 0;
  uint8_t  well_tucked_in_today_      = 0;
  uint8_t  vocab_learned_             = 0;
  uint16_t trick_perf_[5]             = {0,0,0,0,0};
  uint64_t total_steps_               = 0;
  uint8_t  mood_history_[7]           = {0,0,0,0,0,0,0};
  uint8_t  mood_history_head_         = 0;
  uint16_t walk_steps_                = 0;
  uint16_t walk_target_               = 0;
  bool     is_birthday_today_         = false;
  uint32_t last_wish_check_day_       = 0;
  // Round 2 Phase 2 transient state
  uint32_t npc_visit_ms_              = 0;  // slot 0: when current NPC visit started
  uint8_t  npc_visit_kind_            = 0;  // slot 0: 1..N = friend_id+1, 0 = none
  uint32_t npc_visit_ms2_             = 0;  // slot 1
  uint8_t  npc_visit_kind2_           = 0;  // slot 1
  uint8_t  shop_cursor_               = 0;
  uint8_t  active_holiday_            = 0;
  uint8_t  actions_cursor_            = 0;
  uint8_t  actions_submenu_           = 0;   // 0 = main, 1 = tricks
  uint8_t  voice_trick_kind_          = 0;
  uint32_t voice_trick_started_ms_    = 0;
  uint32_t friend_visits_[(int)Friend::COUNT] = {0,0,0,0,0,0,0,0};
  // Round 2 Phase 3 transient state
  uint32_t today_day_index_           = 0;
  uint32_t today_happiness_sum_       = 0;
  uint16_t today_samples_             = 0;
  uint16_t today_actions_             = 0;
  // Death-removal: timestamp when MovingOut / Magic transition started.
  uint32_t transition_started_ms_     = 0;
  uint8_t  move_out_family_idx_       = 0;   // which surname to show
  // Ambient behavior state machine.
  uint8_t  ambient_behavior_          = 0;   // 0 stand, 1 walk, 2 sit, 3 pant, 4 bark
  uint32_t ambient_started_ms_        = 0;
  int16_t  ambient_x_offset_          = 0;
  int8_t   ambient_walk_dir_          = 1;
  bool     in_transition() const {
    return pet_.mood == Mood::MovingOut || pet_.mood == Mood::Magic;
  }

  float    daylight_         = 1.0f;
  char     clock_str_[16]    = {0};
  Clock*   clock_            = nullptr;
  Speaker* speaker_          = nullptr;

  Input    queued_[16]       = {};
  uint8_t  queued_head_      = 0;
  uint8_t  queued_tail_      = 0;

  char     sync_code_buf_[16] = {0};

  // xorshift32 state for ambient spawn rolls; seeded in init().
  uint32_t rng_state_         = 0;
};

}  // namespace tama
