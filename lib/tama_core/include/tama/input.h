#pragma once

#include <cstdint>

namespace tama {

enum class Input : uint8_t {
  None             = 0,
  Feed             = 1,
  Play             = 2,
  Clean            = 3,
  MenuToggle       = 4,
  PetTap           = 5,
  Restart          = 6,
  Stroke           = 7,
  MenuNext         = 8,
  // Phase 2
  CycleScene       = 9,
  CycleCoat        = 10,
  CycleAccessory   = 11,
  TakePhoto        = 12,  // Phase 3
  MicTrigger       = 13,  // Phase 3: a loud sound was heard
  // Round 2 inputs
  Walk             = 14,  // start a walk (or step during walking)
  TreatGive        = 15,  // give one treat (cycles through tiers based on stock)
  Brush            = 16,  // brush Bailey -- small cleanliness bump
  CycleToy         = 17,  // cycle active toy
  CycleAge         = 18,  // demo: cycle Puppy/Adult/Senior on demand
  ImuFlick         = 19,  // IMU forward flick (throw / advance fetch)
  // Voice / menu trick commands
  VoiceSit         = 20,
  VoiceCome        = 21,
  VoiceHighFive    = 22,
  VoiceRollOver    = 23,
  VoiceJump        = 24,
  Bedtime          = 25,  // manual tuck-in (sets well_tucked_in_today)
  MenuCursorNext   = 26,  // move action-tab cursor down
  // Friends visit (named dogs play with Bailey)
  PlayWithFriend         = 27,  // random friend
  PlayWithFriendOllie    = 28,
  PlayWithFriendMitchell = 29,
  PlayWithFriendEnzo     = 30,
  PlayWithFriendLincoln  = 31,
  PlayWithFriendRuben    = 32,
  PlayWithFriendFrancie  = 33,
  PlayWithFriendBomi     = 34,
  PlayWithFriendNoshy    = 35,
  // Round 3 inputs
  ImuShake               = 36,  // physical shake of the device
  TradeBones             = 37,  // spend 5 bones for 1 biscuit (Shop row)
  CycleBathToy           = 38,  // cycle active bath toy (0..3)
  HideSeek               = 39,  // play a round of hide & seek
  // Round 5 Phase B remainder: mini-game triggers.
  Fish                   = 40,  // start fishing (Beach scene)
  MemoryPaws             = 41,  // start memory mini-game
  TugOfWar               = 42,  // start tug-of-war timing game
  ChaseStick             = 43,  // fetch variant in Forest scene
  VetVisit               = 44,  // cure-via-vet animation when sick
  // Round 6 Phase 6K: mini-games + wallpaper customization.
  PumpkinTap             = 45,  // Halloween-only rhythm-tap mini-game tap
  CycleWallpaper         = 46,  // cycle scene wallpaper variant 0..3
  // Round 6 Phase 6L: 3 more rhythm-tap mini-games.
  SnowballThrow          = 47,  // winter-scene snowball-fight tap
  PetalCatch             = 48,  // cherry-blossom-day petal catch tap
  GroomBrush             = 49,  // detailed grooming rhythm tap
  // Round 6 Phase 6M: 2 more rhythm-tap mini-games.
  RhythmTap              = 50,  // 4-button beat tap (always available)
  AppleBob               = 51,  // Halloween-only apple-bobbing tap
  // Round 6 Phase 6N: painting / drawing mini-game.
  PaintCellCycle         = 52,  // cycle color at the current cursor cell
  PaintCursorNext        = 53,  // advance the painting cursor 0..15
  // Chrome (header + footer) visibility toggle via vertical swipe.
  HideChrome             = 54,  // swipe-up-on-top OR swipe-down-on-bottom
  ShowChrome             = 55,  // swipe-down-on-top OR swipe-up-on-bottom
  // System: request a deep-sleep / device-off (hw-gated in main.cpp).
  PowerOff               = 56,
};

}  // namespace tama
