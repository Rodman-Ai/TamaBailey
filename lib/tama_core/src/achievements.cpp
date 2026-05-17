#include "tama/achievements.h"

namespace tama {

namespace {
struct Entry { const char* name; const char* desc; };

const Entry kEntries[kAchievementCount] = {
  /* FirstFeed         */ {"First Bite",     "Fed Bailey for the first time"},
  /* FirstPlay         */ {"Playtime",       "Played with Bailey for the first time"},
  /* FirstClean        */ {"Squeaky",        "Bathed Bailey for the first time"},
  /* FirstPet          */ {"Good Pup",       "Petted Bailey for the first time"},
  /* FullStats         */ {"Picture Perfect","All four stats at 100"},
  /* Petted100         */ {"Best Friend",    "Petted Bailey 100 times"},
  /* Streak3Days       */ {"Loyal",          "3-day visit streak"},
  /* Streak7Days       */ {"Devoted",        "7-day visit streak"},
  /* EvolvedToAdult    */ {"Grown Up",       "Bailey reached Adult"},
  /* EvolvedToSenior   */ {"Wise Hound",     "Bailey reached Senior"},
  /* LearnedFirstTrick */ {"Smart Pup",      "Learned a trick"},
  /* LearnedAllTricks  */ {"Show Dog",       "Learned every trick"},
  /* FetchPro          */ {"Fetch Pro",      "Caught 10 fetches"},
  /* ScenicTour        */ {"Scenic Tour",    "Visited every scene"},
  /* WeatheredTheStorm */ {"Storm Watcher",  "Cared for Bailey through bad weather"},
  /* Dapper            */ {"Dapper",         "Equipped your first accessory"},
  /* SurvivedSickness  */ {"Bedside Manner", "Nursed Bailey back to health"},
  /* HonoredAncestor   */ {"Legacy",         "Inherited a trait from a past Bailey"},
  /* PhotoFan          */ {"Photo Fan",      "Took a photo"},
  /* CalledByName      */ {"Here, Boy!",     "Got Bailey's attention with your voice"},
  /* BirthdayBoy       */ {"Birthday Boy",   "Visited Bailey on his birthday"},
  /* WalkOfALifetime   */ {"Walk a Mile",    "Took 100 lifetime steps with Bailey"},
  /* WishGranter       */ {"Wish Granter",   "Fulfilled 5 of Bailey's wishes"},
  /* BiscuitTycoon     */ {"Biscuit Tycoon", "Earned 50 biscuits"},
  /* SeasonalGreetings */ {"Season's Best",  "Visited on a holiday"},
};
}  // namespace

const char* achievement_name(AchievementId id) {
  int i = static_cast<int>(id);
  if (i < 0 || i >= kAchievementCount) return "?";
  return kEntries[i].name;
}

const char* achievement_desc(AchievementId id) {
  int i = static_cast<int>(id);
  if (i < 0 || i >= kAchievementCount) return "?";
  return kEntries[i].desc;
}

}  // namespace tama
