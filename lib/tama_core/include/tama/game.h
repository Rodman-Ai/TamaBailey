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

// Sleep schedule window (24h clock). During these hours, decay slows 50%
// and Bailey defaults to Sleeping mood (when auto_sleep is on).
constexpr uint8_t  kBedtimeHour               = 22;
constexpr uint8_t  kWakeHour                  = 7;

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
  Fog    = 4,    // Round 4
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
  uint64_t  achievements() const { return achievements_; }
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
  uint32_t  last_tick_ms()          const { return last_tick_ms_; }
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
  // Round 3: walk-find HUD popup + bones counter + daily-steps.
  uint32_t  bones_collected() const { return bones_collected_; }
  uint16_t  walk_today_steps() const { return walk_today_steps_; }
  // 0 = no recent find, 1 = bone, 2 = toy unlocked, 3 = treat (biscuit-flavored)
  uint8_t   last_walk_find_kind() const { return last_walk_find_kind_; }
  uint32_t  last_walk_find_ms()   const { return last_walk_find_ms_; }

  // Round 3 Phase 2A: Gourmet buff (treat-recipe combo).
  bool      gourmet_active() const {
    return last_tick_ms_ < gourmet_until_ms_;
  }
  uint32_t  gourmet_remaining_ms() const {
    return gourmet_active() ? gourmet_until_ms_ - last_tick_ms_ : 0;
  }

  // Round 3 Phase 3: Best-friend bond (paired via sync code).
  uint32_t best_friend_hash() const { return best_friend_hash_; }

  // Round 3 Phase 3G: Seasonal accessories (auto-unlocked on holiday).
  uint8_t  seasonal_unlocks() const { return seasonal_unlocks_; }
  // Round 3 Phase 3H: Bath toys.
  uint8_t  bath_toys_owned() const  { return bath_toys_owned_; }
  uint8_t  bath_toy_active() const  { return bath_toy_active_; }
  // Round 3 Phase 3L: Hide & Seek.
  uint16_t hide_seek_wins() const   { return hide_seek_wins_; }
  // Last hide-seek outcome (0 none / 1 win / 2 peek / 3 miss) + when.
  uint8_t  hide_seek_last_outcome() const { return hide_seek_last_outcome_; }
  uint32_t hide_seek_last_ms()      const { return hide_seek_last_ms_; }
  bool     hide_seek_active() const {
    // Latent fix: also require an outcome to have actually been set,
    // otherwise the 3-s window is "active" at game start (when
    // last_tick_ms_ > 0 and hide_seek_last_ms_ is still 0) and the
    // footer renders the default "Not there!" message spuriously.
    return hide_seek_last_outcome_ != 0 &&
           last_tick_ms_ - hide_seek_last_ms_ < 3000;
  }

  // Round 3 Phase 3F: Walk dig mini-game.
  uint16_t  dig_successes()      const { return dig_successes_; }
  bool      dig_prompt_active()  const {
    return last_tick_ms_ < dig_prompt_until_ms_;
  }

  // Round 3 Phase 3D: Bedtime story playback.
  uint16_t  stories_heard() const { return stories_heard_; }
  bool      bedtime_story_active() const {
    return last_tick_ms_ < bedtime_story_until_ms_;
  }
  const char* bedtime_story_text() const;

  // Round 3 Phase 3C: Tamagotchi-Connect-style treat gifting.
  // Writes a 9-char "GIFTXXXXX" code into a caller-owned buffer (>=10
  // chars). Returns false if `tier` is invalid or there are no treats
  // of that tier to gift (the give-side must own at least one).
  // Generating a code does NOT decrement the inventory; that happens
  // when the RECEIVER applies the code. Players are on the honor
  // system for the giver to also use their own treat.
  bool generate_gift_code(uint8_t tier, char* out_buf, int buf_len);
  // Redeems a gift code. One redeem per local day allowed; returns
  // false if the code is malformed or today's quota is already used.
  bool apply_gift_code(const char* code);

  // Round 3 Phase 3A: Achievement showcase. Returns the achievement id
  // of the i-th (0..2) MOST RECENTLY EARNED unlocked achievement, or
  // -1 if fewer than i+1 are unlocked. We auto-pick by enum-id order
  // (highest = most recent) since Round 2/3 IDs were appended.
  int latest_achievement(int idx) const;

  // Hardware init status for on-screen debug indicator. 0 = unknown
  // (default, e.g. on web), 1 = OK, 2 = FAIL. Set by the platform
  // adapter after the corresponding subsystem's begin() returns.
  enum HwStatus : uint8_t { HwUnknown = 0, HwOk = 1, HwFail = 2 };
  uint8_t   hw_imu_status()    const { return hw_imu_status_; }
  uint8_t   hw_audio_status()  const { return hw_audio_status_; }
  void      set_hw_imu_status(bool ok)   { hw_imu_status_   = ok ? HwOk : HwFail; }
  void      set_hw_audio_status(bool ok) { hw_audio_status_ = ok ? HwOk : HwFail; }

  // Round 4: time-of-day + day index exposed for footer phrase
  // rotation. today_day_index() is 0 when no clock has been synced.
  uint32_t  today_day_index() const { return today_day_index_; }
  uint8_t   current_hour()    const { return current_hour_; }
  bool      have_local_hour() const { return have_local_hour_; }
  // Round 4 Phase 3: tomorrow's deterministic weather forecast.
  Weather   tomorrow_weather() const;
  // Round 5 Phase D1: New Year fireworks active flag.
  bool      fireworks_active() const {
    return last_tick_ms_ < new_year_fireworks_until_ms_;
  }
  // Round 5 Phase C remainder: daily login wheel.
  // Returns true if a spin is available (today != last_login_wheel_day
  // AND today_day_index != 0). Reward enum: 0 = 5 biscuits, 1 = 3
  // bones, 2 = 1 biscuit treat, 3 = 1 bacon treat, 4 = 1 random sticker.
  bool      wheel_available()   const;
  uint8_t   last_wheel_reward() const { return last_wheel_reward_; }
  // Spin and apply the reward. Returns the reward id (0..4) or 255
  // if the wheel isn't currently available (already spun today or no
  // clock yet).
  uint8_t   spin_wheel();

  // Round 5 Phase D2: photo flash + mystery visitor.
  bool      photo_flash_active() const {
    return last_tick_ms_ < photo_flash_until_ms_;
  }

  // Round 5 Phase B: trick combo + firefly mini-game.
  // Combo: 3 distinct tricks within 30 s -> +20 % happiness on actions
  // for 60 s. Stacks multiplicatively with the Gourmet buff.
  bool      trick_combo_active() const {
    return last_tick_ms_ < trick_combo_until_ms_;
  }
  uint32_t  trick_combo_remaining_ms() const {
    return trick_combo_active() ? trick_combo_until_ms_ - last_tick_ms_ : 0;
  }
  // Firefly: appears during low-daylight hours and despawns after
  // 3 s. PetTap during the window catches it.
  bool      firefly_active() const {
    return firefly_spawn_ms_ != 0 &&
           last_tick_ms_ - firefly_spawn_ms_ < 3000;
  }
  int16_t   firefly_x() const { return firefly_x_; }
  int16_t   firefly_y() const { return firefly_y_; }
  uint16_t  fireflies_caught() const { return fireflies_caught_; }
  // The mystery visitor is a non-canonical guest. When npc_visit_kind()
  // returns kMysteryVisitorKind it should render as a silhouette and
  // the footer shows "A mystery dog visits...".
  static constexpr uint8_t kMysteryVisitorKind = 255;
  bool      mystery_visitor_active() const {
    return npc_visit_kind_ == kMysteryVisitorKind ||
           npc_visit_kind2_ == kMysteryVisitorKind;
  }

  // Round 5 Phase A: personalization. Pet name + birthday are
  // persisted and configurable. Defaults are "Bailey" + Jan 13.
  const char* pet_name()       const { return pet_name_; }
  uint8_t     birthday_month() const { return birthday_month_; }
  uint8_t     birthday_day()   const { return birthday_day_; }
  void        set_pet_name(const char* name);
  void        set_birthday(uint8_t month, uint8_t day);

  // Round 5 Phase A2: collectible stickers + wallpaper poster.
  // Stickers: bit0=paw / bit1=star / bit2=bone / bit3=heart / bit4=fire.
  // Auto-unlocked when the matching achievement fires.
  uint8_t     stickers_unlocked() const { return stickers_unlocked_; }
  uint8_t     wall_poster()       const { return wall_poster_; }
  void        cycle_wall_poster()       { wall_poster_ = (wall_poster_ + 1) % 4; dirty_ = true; }

  // Round 5 Phase C1: progression + lifetime counters.
  // XP awarded per user action; level = sqrt(xp / 10), capped at 30.
  uint32_t  trainer_xp()      const { return trainer_xp_; }
  uint32_t  trainer_level()   const;     // derived from xp
  uint64_t  time_played_ms()  const { return time_played_ms_; }

  // Round 5 Phase C2: daily action goal + active streak.
  // today_actions count resets on day rollover; goal is hardcoded 5.
  static constexpr uint16_t kDailyActionGoal = 5;
  uint16_t  today_actions()       const { return today_actions_; }
  uint16_t  active_streak_days()  const { return active_streak_days_; }

  // Round 5 Phase C2: derived skill stats (no persistence). Each is
  // 0..100. Intelligence reflects trick mastery, stamina reflects
  // lifetime walking, charm reflects total pets.
  uint8_t   skill_intelligence() const;
  uint8_t   skill_stamina()      const;
  uint8_t   skill_charm()        const;

  // Round 3 Phase 1C: daily quest + pet horoscope.
  // Quest types rotate by today_day_index_ % 2.
  // Returns 0 when no synced clock (so UI can hide the quest line).
  uint8_t   daily_quest_id()        const;
  uint32_t  daily_quest_progress()  const;
  uint32_t  daily_quest_goal()      const;
  const char* daily_quest_text()    const;
  bool      daily_quest_complete()  const { return daily_quest_progress() >= daily_quest_goal(); }
  bool      daily_quest_awarded_today() const;
  // Horoscope: 0..4 picked from today_day_index_ % 5. Returns 0 when no clock.
  uint8_t   horoscope_id()    const;
  const char* horoscope_text() const;
  // Manually override the weather (e.g. from a wttr.in fetch on the web).
  void      set_weather(Weather w) {
    uint8_t nw = (uint8_t)w;
    if (nw != weather_) {
      prev_weather_         = weather_;
      last_weather_change_ms_ = last_tick_ms_;
    }
    weather_ = nw;
    dirty_ = true;
  }
  // Round 4: previous weather + transition timestamp -- used by the
  // rainbow / puddle-fade overlays.
  uint8_t   prev_weather()           const { return prev_weather_; }
  uint32_t  last_weather_change_ms() const { return last_weather_change_ms_; }
  uint32_t  last_lightning_ms()      const { return last_lightning_ms_; }
  void      set_last_lightning_ms(uint32_t ms) { last_lightning_ms_ = ms; }
  // Returns true if a lightning bolt fires THIS tick (renderer paints
  // the flash; the audio cue is fired here).
  void      maybe_trigger_lightning(uint32_t now_ms);
  // Round 4: periodic snore audio cue while pet is Sleeping (every 6 s).
  void      maybe_trigger_snore(uint32_t now_ms);
  uint32_t  last_snore_ms() const { return last_snore_ms_; }
  // Round 5 Phase B: firefly spawn rolled each tick during low daylight.
  void      maybe_spawn_firefly(uint32_t now_ms);

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
  uint64_t achievements_  = 0;
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

  // Round 3 Phase 1 transient + collectible state.
  uint32_t bones_collected_   = 0;   // persisted (save v7)
  uint16_t walk_today_steps_  = 0;   // persisted (save v7)
  uint8_t  last_walk_find_kind_ = 0; // transient, no save
  uint32_t last_walk_find_ms_   = 0; // transient, no save
  uint8_t  current_hour_        = 12; // cached from clock, used by sleep
                                      // schedule + horoscope hooks; transient
  bool     have_local_hour_     = false;
  bool     is_sleep_hour() const;

  // Daily quest: which day_index we last awarded biscuits for. Persisted.
  uint32_t daily_quest_awarded_day_ = 0;
  void update_daily_quest(uint64_t now_unix_ms);

  // Best-friend bond: 32-bit hash of the last sync code consumed.
  uint32_t best_friend_hash_ = 0;

  // Gift treats: today_day_index of last redeemed gift. 0 = never.
  uint32_t last_gift_received_day_ = 0;

  // Treat recipe combo: bitmask of tiers eaten in the last 60s.
  // Transient: not persisted across save/load.
  uint32_t combo_window_start_ms_ = 0;
  uint8_t  combo_mask_            = 0;
  uint32_t gourmet_until_ms_      = 0;

  // Bedtime stories: persistent count + transient bubble timer.
  uint16_t stories_heard_           = 0;
  uint32_t bedtime_story_until_ms_  = 0;
  uint8_t  bedtime_story_idx_       = 0;

  // Walk dig mini-game: persistent success count + transient
  // dig-prompt timer (cleared when A scores or window expires).
  uint16_t dig_successes_           = 0;
  uint32_t dig_prompt_until_ms_     = 0;

  // Seasonal accessory unlocks (bitmap): bit0 pumpkin, bit1 santa,
  // bit2 shamrock. Persistent once granted (wear year-round).
  uint8_t  seasonal_unlocks_        = 0;

  // Bath toys: owned bitmap (bit0 duck, bit1 boat, bit2 fish) +
  // active selection (0..3).
  uint8_t  bath_toys_owned_         = 0;
  uint8_t  bath_toy_active_         = 0;

  // Hide & Seek: persistent win count + transient last-outcome state
  // (1=win/2=peek/3=miss) for the 3 s flash overlay in the footer.
  uint16_t hide_seek_wins_          = 0;
  uint8_t  hide_seek_last_outcome_  = 0;
  uint32_t hide_seek_last_ms_       = 0;

  // Round 4: weather transition + lightning timing (transient).
  uint8_t  prev_weather_            = 0;
  uint32_t last_weather_change_ms_  = 0;
  uint32_t last_lightning_ms_       = 0;
  uint32_t last_snore_ms_           = 0;
  // Once-per-Christmas auto-scene flag (transient): which day_index
  // we last auto-switched to Snow Park.
  uint32_t last_xmas_auto_scene_day_ = 0;

  // Round 5 Phase D1: once-per-year New Year fireworks timer (transient).
  uint32_t last_new_year_day_          = 0;
  uint32_t new_year_fireworks_until_ms_ = 0;

  // Round 5 Phase D2: photo-snap flash timer (transient).
  uint32_t photo_flash_until_ms_       = 0;

  // Round 5 Phase B: trick combo bonus (transient).
  uint8_t  recent_tricks_mask_         = 0;  // bits 0..4 -> Trick enum
  uint32_t recent_tricks_first_ms_     = 0;
  uint32_t trick_combo_until_ms_       = 0;
  // Firefly mini-game: spawn position + timestamp (transient) +
  // lifetime catch count (persisted v21).
  uint32_t firefly_spawn_ms_           = 0;
  int16_t  firefly_x_                  = 0;
  int16_t  firefly_y_                  = 0;
  uint16_t fireflies_caught_           = 0;

  // Hardware init status (transient, set by the platform adapter).
  uint8_t  hw_imu_status_   = HwUnknown;
  uint8_t  hw_audio_status_ = HwUnknown;

  // Round 5 Phase A: personalization (persisted v17).
  char     pet_name_[12]    = {'B', 'a', 'i', 'l', 'e', 'y', 0};
  uint8_t  birthday_month_  = 1;
  uint8_t  birthday_day_    = 13;

  // Round 5 Phase A2: decor + sticker collection (persisted v18).
  uint8_t  stickers_unlocked_ = 0;
  uint8_t  wall_poster_       = 0;

  // Round 5 Phase C1: progression + lifetime counters (persisted v19).
  uint32_t trainer_xp_     = 0;
  uint64_t time_played_ms_ = 0;
  void     award_xp(uint32_t n);

  // Round 5 Phase C2: persistent active-streak counter (v20).
  uint16_t active_streak_days_ = 0;

  // Round 5 Phase C remainder: daily login wheel (persisted v22).
  uint32_t last_login_wheel_day_ = 0;
  uint8_t  last_wheel_reward_    = 0;
};

}  // namespace tama
