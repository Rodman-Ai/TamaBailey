#pragma once

#include <cstdint>

namespace tama {

// Bit positions in the achievements bitmask. Append; do NOT reorder.
enum class AchievementId : uint8_t {
  FirstFeed        =  0,
  FirstPlay        =  1,
  FirstClean       =  2,
  FirstPet         =  3,
  FullStats        =  4,   // all four stats at 100 simultaneously
  Petted100        =  5,
  Streak3Days      =  6,
  Streak7Days      =  7,
  EvolvedToAdult   =  8,
  EvolvedToSenior  =  9,
  LearnedFirstTrick = 10,  // Phase 2
  LearnedAllTricks  = 11,  // Phase 2
  FetchPro          = 12,  // Phase 2: caught 10 fetches
  ScenicTour        = 13,  // Phase 2: visited all 3 scenes
  WeatheredTheStorm = 14,  // Phase 2: cared for Bailey during rain/snow
  Dapper            = 15,  // Phase 2: equipped first accessory
  SurvivedSickness  = 16,  // Phase 2
  HonoredAncestor   = 17,  // Phase 3: inherited a trait
  PhotoFan          = 18,  // Phase 3: took a photo
  CalledByName      = 19,  // Phase 3: triggered via mic
  // Round 2 additions
  BirthdayBoy       = 20,
  WalkOfALifetime   = 21,  // 100 lifetime steps
  WishGranter       = 22,  // fulfilled 5 wishes
  BiscuitTycoon     = 23,  // 50 biscuits earned lifetime
  SeasonalGreetings = 24,  // visited on any holiday
  PlayDate          = 25,  // first time a named friend visited
  Socialite         = 26,  // hosted all four friends at least once
  // Round 3 additions
  Pawmates          = 27,  // bonded with another player's Bailey via sync
  Goodnight         = 28,  // 10 bedtime stories told
  MasterDigger      = 29,  // 10 successful walk-digs
  Showstopper       = 30,  // mastered any single trick (perf >= 10)
};

constexpr int kAchievementCount = 31;
static_assert(kAchievementCount <= 32, "bitmask is 32 bits");

const char* achievement_name(AchievementId id);
const char* achievement_desc(AchievementId id);

// NOTE: Arduino.h defines a `bit(b)` macro that would clobber any function
// called `bit` in this namespace. Use `achievement_bit` instead.
inline uint32_t achievement_bit(AchievementId id) {
  return 1u << static_cast<uint8_t>(id);
}
inline bool is_unlocked(uint32_t mask, AchievementId id) {
  return (mask & achievement_bit(id)) != 0;
}
// Returns true if this was a NEW unlock.
inline bool unlock(uint32_t& mask, AchievementId id) {
  uint32_t b = achievement_bit(id);
  if (mask & b) return false;
  mask |= b;
  return true;
}

}  // namespace tama
