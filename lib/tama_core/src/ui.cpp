#include "tama/ui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "tama/achievements.h"
#include "tama/colors.h"
#include "tama/font_6x8.h"
#include "tama/friends.h"
#include "tama/game.h"
#include "tama/sprites.h"

namespace tama {

namespace {

constexpr int kPetW = kPetSpriteSize;
constexpr int kPetH = kPetSpriteSize;
constexpr int kPetScale = 3;
constexpr int kPetDrawW = kPetW * kPetScale;
constexpr int kPetDrawH = kPetH * kPetScale;
constexpr int kPetX = (kScreenW - kPetDrawW) / 2;
constexpr int kPetY = kStatsBarH + 8;
constexpr int kAccessoryScale = 2;

// Surname pool for the move-out narrative beat -- cosmetic only,
// picked by Game::move_out_family_idx().
static const char* const kFamilyNames[8] = {
  "Nakamuras", "Patels", "Kowalskis", "Diazes",
  "OBriens", "Yamamotos", "Schmidts", "Akinyemis"
};

// Round 4 + Round 5: per-mood phrase banks (10 each). Index rotates
// as `(today_day_index + minute) % 10` so the footer changes every
// minute and each day starts at a different offset. With the 2-line
// 20-char-wrap footer, phrases up to ~40 chars wrap cleanly.
static const char* const kHappyBank[10] = {
  "Bailey is happy!",
  "Bailey wags his tail.",
  "Bailey is having a great day!",
  "Bailey's nose is wiggling.",
  "Bailey is in a good mood.",
  "Bailey did a zoomie!",
  "Bailey snorts happily",
  "Bailey grins at you",
  "Bailey thumps the floor",
  "Bailey is feeling fine",
};
static const char* const kHungryBank[10] = {
  "Bailey is hungry",
  "Bailey's tummy rumbles",
  "Bailey could really eat",
  "Bailey eyes the food bowl",
  "Bailey wants a snack",
  "Bailey sniffs the air",
  "Bailey paws the bowl",
  "Bailey eyes your snack",
  "Bailey is starving",
  "Bailey wants a treat",
};
static const char* const kSadBank[10] = {
  "Bailey feels sad",
  "Bailey looks gloomy",
  "Bailey misses you",
  "Bailey needs cheering up",
  "Bailey is sulking",
  "Bailey hides his face",
  "Bailey huddles in a ball",
  "Bailey wants attention",
  "Bailey lets out a sigh",
  "Bailey misses the park",
};
static const char* const kDirtyBank[10] = {
  "Bailey needs a bath",
  "Bailey is muddy!",
  "Bailey smells funky",
  "Bailey's coat is grubby",
  "Bailey wants a wash",
  "Bailey rolled in dirt",
  "Bailey's paws are mucky",
  "Bailey shakes off dust",
  "Bailey wants a scrub",
  "Bailey needs a brush",
};
static const char* const kSleepingBank[10] = {
  "Zzz... napping",
  "Bailey is dreaming",
  "Bailey is fast asleep",
  "Bailey is curled up",
  "Bailey snoozes peacefully",
  "Bailey is out cold",
  "Bailey twitches a paw",
  "Bailey dreams of bones",
  "Bailey snorts softly",
  "Bailey is napping deep",
};
static const char* const kNeutralBank[10] = {
  "How is Bailey today?",
  "Bailey looks around",
  "Bailey is just chillin'",
  "Bailey ponders life",
  "Bailey is taking it easy",
  "Bailey lies in a sunbeam",
  "Bailey watches a fly",
  "Bailey is just being",
  "Bailey listens to the wind",
  "Bailey waits politely",
};

// Weather-themed overrides for the Happy bank. Picked when weather
// matches AND the player is on an "even" rotation tick so the bank
// still rotates rather than locking on one phrase.
static const char* happy_for_weather(Weather w) {
  switch (w) {
    case Weather::Rain:   return "Bailey splashes in a puddle!";
    case Weather::Snow:   return "Bailey rolls in the snow!";
    case Weather::Cloudy: return "Bailey enjoys the cool breeze.";
    case Weather::Fog:    return "Bailey sniffs through the mist.";
    case Weather::Sunny:
    default:              return nullptr;   // no override, use bank
  }
}
static const char* sad_for_weather(Weather w) {
  if (w == Weather::Rain) return "Bailey shivers in the rain";
  if (w == Weather::Fog)  return "Bailey can't see his friends";
  return nullptr;
}
static const char* sleeping_for_weather(Weather w) {
  if (w == Weather::Rain) return "Bailey listens to the rain";
  if (w == Weather::Snow) return "Bailey is bundled up asleep";
  return nullptr;
}

// Round 4 Phase 3 + Round 5: thought-bubble phrase bank. Picked by
// day_index and the current 30-second window so two consecutive ticks
// return the same string. User-requested phrases land at the head.
static const char* const kThoughts[16] = {
  "thinking about chicken",
  "thinking about salmon",
  "thinking about fetch",
  "thinking about pee",
  "thinking about toys",
  "dreaming of a nice walk",
  "thinking of bones...",
  "remembering a friend",
  "wondering about treats",
  "watching a butterfly",
  "listening for footsteps",
  "dreaming of belly rubs",
  "planning a zoomie",
  "missing the park",
  "counting birds",
  "remembering puppy days",
};

// Round 5 Phase A: replace the first occurrence of "Bailey" in `src`
// with `name`. If `name` is empty or equals "Bailey", just copies the
// source (so default-named pets pay no extra cost beyond the strlen).
// out_cap includes space for the null terminator.
static void apply_pet_name(char* out, int out_cap,
                           const char* src, const char* name) {
  if (out_cap <= 0) return;
  out[0] = '\0';
  if (!src) return;
  const char* needle = "Bailey";
  const int needle_len = 6;
  // Fast path: no rename in effect.
  if (!name || name[0] == '\0' ||
      (name[0] == 'B' && name[1] == 'a' && name[2] == 'i' &&
       name[3] == 'l' && name[4] == 'e' && name[5] == 'y' &&
       name[6] == '\0')) {
    int n = 0;
    while (src[n] && n < out_cap - 1) { out[n] = src[n]; ++n; }
    out[n] = '\0';
    return;
  }
  // Locate the first "Bailey" substring.
  const char* hit = nullptr;
  for (const char* p = src; *p; ++p) {
    bool match = true;
    for (int i = 0; i < needle_len; ++i) {
      if (p[i] != needle[i]) { match = false; break; }
    }
    if (match) { hit = p; break; }
  }
  if (!hit) {
    int n = 0;
    while (src[n] && n < out_cap - 1) { out[n] = src[n]; ++n; }
    out[n] = '\0';
    return;
  }
  int pos = 0;
  // Copy [src .. hit).
  for (const char* p = src; p < hit && pos < out_cap - 1; ++p, ++pos)
    out[pos] = *p;
  // Copy name.
  for (int i = 0; name[i] && pos < out_cap - 1; ++i, ++pos)
    out[pos] = name[i];
  // Copy [hit + needle_len .. end).
  for (const char* p = hit + needle_len; *p && pos < out_cap - 1; ++p, ++pos)
    out[pos] = *p;
  out[pos] = '\0';
}

const char* mood_text_variant(const Game& game) {
  const Mood m = game.pet().mood;
  // MovingOut and Magic are special-cased in draw_footer; if we get
  // here something is off but we return a safe value.
  if (m == Mood::MovingOut) return "Bailey moved in with...";
  if (m == Mood::Magic)     return "Bailey is young again!";
  if (m == Mood::Gone)      return "How is Bailey today?";

  // Round 4 Phase 3: birthday overrides every other mood line.
  if (game.is_birthday()) return "Happy birthday, Bailey!";

  // Round 4 Phase 3: random thought bubble for 3 s every 30 s in
  // idle. Skipped when the urgent moods (Sad/Hungry/Dirty) are on
  // so emergencies still surface.
  if (game.pet().current_action == Action::None &&
      m != Mood::Sad && m != Mood::Hungry && m != Mood::Dirty) {
    uint32_t cycle = game.last_tick_ms() % 30000;
    if (cycle < 3000) {
      uint32_t idx = (game.today_day_index() + game.last_tick_ms() / 30000)
                     % 16;
      return kThoughts[idx];
    }
  }

  // Rotation index: minute-of-game-time + day-of-year. Without a
  // synced clock, today_day_index_ stays 0, but the minute term still
  // rotates so the bank cycles. Mod 10 = bank size.
  uint32_t minute = game.last_tick_ms() / 60000u;
  uint32_t idx    = (game.today_day_index() + minute) % 10;

  // Weather-flavored picks on even minutes (so the basic bank still
  // shows up half the time).
  Weather w = game.weather();
  if ((minute & 1) == 0) {
    const char* wo = nullptr;
    if      (m == Mood::Happy)    wo = happy_for_weather(w);
    else if (m == Mood::Sad)      wo = sad_for_weather(w);
    else if (m == Mood::Sleeping) wo = sleeping_for_weather(w);
    if (wo) return wo;
  }

  switch (m) {
    case Mood::Happy:    return kHappyBank[idx];
    case Mood::Hungry:   return kHungryBank[idx];
    case Mood::Sad:      return kSadBank[idx];
    case Mood::Dirty:    return kDirtyBank[idx];
    case Mood::Sleeping: return kSleepingBank[idx];
    case Mood::Neutral:
    default:             return kNeutralBank[idx];
  }
}

// Time-of-day prefix prepended to the footer mood line.
const char* time_of_day_prefix(const Game& game) {
  if (!game.have_local_hour()) return "";
  uint8_t h = game.current_hour();
  if (h < 6 || h >= 22) return "Late: ";
  if (h < 12)           return "Morning: ";
  if (h < 18)           return "";
  return "Evening: ";
}


const char* stage_text(LifeStage s) {
  switch (s) {
    case LifeStage::Puppy:  return "Puppy";
    case LifeStage::Adult:  return "Adult";
    case LifeStage::Senior: return "Senior";
    case LifeStage::Gone:   return "Gone";
  }
  return "?";
}

// Linearly interpolate RGB565 -> RGB565 by alpha 0..1
uint16_t mix(uint16_t a, uint16_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r = ar + (int)((br - ar) * t);
  int g = ag + (int)((bg - ag) * t);
  int bc = ab + (int)((bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bc);
}

void draw_stat_bar(Renderer& r, int x, int y, int w, int h,
                   uint8_t value, uint16_t fill_color, const char* label) {
  r.fillRect(x, y, w, h, kBlack);
  r.drawRect(x - 1, y - 1, w + 2, h + 2, kGrayLight);
  int filled = (w - 2) * value / 100;
  if (filled > 0) r.fillRect(x + 1, y + 1, filled, h - 2, fill_color);
  r.drawText(x, y - 10, label, kGrayLight, 1);
}

void draw_stats_bar(Renderer& r, const Pet& pet, const char* clock_str) {
  const int margin = 6;
  const int bar_h  = 6;
  const int bar_w  = (kScreenW - margin * 5) / 4;
  int x = margin;
  int y = 18;
  struct B { const char* label; uint8_t value; uint16_t color; };
  B bars[4] = {
    {"FOOD",  pet.stats.hunger,      kOrange},
    {"PLAY",  pet.stats.happiness,   kYellow},
    {"BATH",  pet.stats.cleanliness, kBlue},
    {"REST",  pet.stats.energy,      kGreen},
  };
  for (int i = 0; i < 4; ++i) {
    draw_stat_bar(r, x, y, bar_w, bar_h, bars[i].value, bars[i].color, bars[i].label);
    x += bar_w + margin;
  }
  if (clock_str && clock_str[0]) {
    int tw = text_width(clock_str, 1);
    r.drawText(kScreenW - tw - 2, 2, clock_str, kGrayLight, 1);
  }
}

// Round 3 Phase 3A: 3 mini-badges for the most-recently unlocked
// achievements, painted in the top-left corner of the stats bar.
// Rendered after draw_stats_bar so the badges sit on top of nothing.
void draw_achievement_showcase(Renderer& r, const Game& game) {
  for (int i = 0; i < 3; ++i) {
    int id = game.latest_achievement(i);
    if (id < 0) break;
    int bx = 2 + i * 8;
    r.fillRect(bx, 2, 6, 6, kYellow);
    r.drawRect(bx, 2, 6, 6, kGrayDark);
  }
}

// Background tint depending on time of day.
void draw_background(Renderer& r, float daylight) {
  uint16_t sky_day   = kSky;
  uint16_t sky_night = rgb(15, 20, 50);   // deep blue
  uint16_t grass_day = kGrass;
  uint16_t grass_night = rgb(20, 50, 25); // dark green
  uint16_t sky   = mix(sky_night,   sky_day,   daylight);
  uint16_t grass = mix(grass_night, grass_day, daylight);

  r.fillRect(0, 0, kScreenW, kScreenH, sky);
  int floor_y = kPetY + kPetDrawH - 6;
  r.fillRect(0, floor_y, kScreenW, kScreenH - floor_y - kStatusH, grass);
  r.drawHLine(0, floor_y, kScreenW, mix(rgb(8, 30, 15), kGrassDark, daylight));

  // Moon when very dark
  if (daylight < 0.3f) {
    r.fillRect(180, 8 + kStatsBarH, 16, 16, rgb(240, 240, 220));
    r.fillRect(178, 10 + kStatsBarH, 4, 4, rgb(15, 20, 50));  // crescent
  }
  // Sun when very bright
  else if (daylight > 0.85f) {
    r.fillRect(186, 8 + kStatsBarH, 14, 14, kYellow);
  }
}

// Accessory drawn over the pet's head/neck. accessory_id meanings:
//   1 = red bandana, 2 = blue collar, 3 = party hat
// engraving (optional) is a 4-char tag rendered on the blue-collar tag.
void draw_accessory_overlay(Renderer& r, uint8_t accessory_id,
                            const char* engraving = nullptr) {
  if (accessory_id == 0) return;
  int cx = kPetX + kPetDrawW / 2;
  int cy = kPetY + 24 * kPetScale;
  switch (accessory_id) {
    case 1: { // red bandana around neck
      for (int i = -10; i <= 10; ++i) {
        for (int j = 0; j < 4; ++j) {
          r.fillRect(cx + i*kPetScale - 1, cy + j*kPetScale,
                     kPetScale, kPetScale, kRed);
        }
      }
      // knot
      r.fillRect(cx + 9*kPetScale, cy + 2*kPetScale,
                 4*kPetScale, 5*kPetScale, kRed);
      break;
    }
    case 2: { // blue collar
      for (int i = -8; i <= 8; ++i)
        r.fillRect(cx + i*kPetScale - 1, cy + 1*kPetScale,
                   kPetScale, kPetScale * 2, kBlue);
      r.fillRect(cx - 1, cy + 3*kPetScale, kPetScale * 2, kPetScale * 2, kYellow);
      // Round 6 Phase 6B: engraved tag (4 chars from pet_name).
      if (engraving) {
        r.drawText(cx + 2 * kPetScale - 8, cy + 3 * kPetScale + 4,
                   engraving, kBlack, 1);
      }
      break;
    }
    case 3: { // party hat above head
      int hx = kPetX + 15 * kPetScale;
      int hy = kPetY - 4;
      for (int row = 0; row < 14; ++row) {
        int width = (14 - row);
        for (int col = 0; col < width; ++col)
          r.fillRect(hx + col*kPetScale + (14-width)*kPetScale/2,
                     hy + row*kPetScale, kPetScale, kPetScale, kPink);
      }
      // pom-pom
      r.fillRect(hx + 6*kPetScale, hy - 4, kPetScale*3, kPetScale*3, kYellow);
      break;
    }
    case 4: { // pumpkin hat: round orange dome with green stem
      int hx = kPetX + 10 * kPetScale;
      int hy = kPetY + 0;
      // Dome (12 wide x 8 tall ovalish).
      for (int row = 0; row < 8; ++row) {
        int inset = (row < 2) ? 2 : (row > 5 ? 1 : 0);
        for (int col = inset; col < 12 - inset; ++col)
          r.fillRect(hx + col*kPetScale, hy + row*kPetScale,
                     kPetScale, kPetScale, kOrange);
      }
      // Vertical ridges (darker orange).
      r.fillRect(hx + 5*kPetScale, hy + 1*kPetScale, kPetScale, kPetScale*6, kBrownDark);
      r.fillRect(hx + 7*kPetScale, hy + 1*kPetScale, kPetScale, kPetScale*6, kBrownDark);
      // Stem.
      r.fillRect(hx + 5*kPetScale, hy - 2*kPetScale, kPetScale*2, kPetScale*2, kGrassDark);
      break;
    }
    case 5: { // santa hat: red triangle + white pom + white brim
      int hx = kPetX + 12 * kPetScale;
      int hy = kPetY - 2;
      for (int row = 0; row < 10; ++row) {
        int width = (10 - row);
        for (int col = 0; col < width; ++col)
          r.fillRect(hx + col*kPetScale + (10-width)*kPetScale/2,
                     hy + row*kPetScale, kPetScale, kPetScale, kRed);
      }
      // White brim.
      r.fillRect(hx - 1, hy + 9*kPetScale, kPetScale*12, kPetScale*2, kWhite);
      // White pom.
      r.fillRect(hx + 4*kPetScale, hy - 3, kPetScale*3, kPetScale*3, kWhite);
      break;
    }
    case 6: { // shamrock collar: green strap with leaf charm
      for (int i = -8; i <= 8; ++i)
        r.fillRect(cx + i*kPetScale - 1, cy + 1*kPetScale,
                   kPetScale, kPetScale * 2, kGrassDark);
      // Leaf charm (three small bumps).
      r.fillRect(cx - 2*kPetScale, cy + 3*kPetScale, kPetScale*2, kPetScale*2, kGreen);
      r.fillRect(cx + 0,             cy + 3*kPetScale, kPetScale*2, kPetScale*2, kGreen);
      r.fillRect(cx - 1*kPetScale,   cy + 5*kPetScale, kPetScale*2, kPetScale*2, kGreen);
      break;
    }
    case 7: { // Easter: yellow egg basket carried at the neck, 3 eggs
      // Brown basket weave.
      int bx = kPetX + 8 * kPetScale;
      int by = kPetY + 18 * kPetScale;
      r.fillRect(bx,     by + 4, 14, 8, kBrownDark);
      r.fillRect(bx + 1, by + 3, 12, 1, kBrownLight);
      // Handle (arc).
      r.fillRect(bx + 1, by - 1, 2, 4, kBrownDark);
      r.fillRect(bx + 11, by - 1, 2, 4, kBrownDark);
      r.fillRect(bx + 3, by - 2, 8, 1, kBrownDark);
      // Three eggs (white / pink / blue).
      r.fillRect(bx + 2, by, 3, 4, kWhite);
      r.fillRect(bx + 6, by, 3, 4, kPink);
      r.fillRect(bx + 10, by, 3, 4, kBlue);
      break;
    }
    case 8: { // Valentine's: pink bandana with a white heart
      for (int i = -10; i <= 10; ++i) {
        for (int j = 0; j < 4; ++j) {
          r.fillRect(cx + i*kPetScale - 1, cy + j*kPetScale,
                     kPetScale, kPetScale, kPink);
        }
      }
      // Knot.
      r.fillRect(cx + 9*kPetScale, cy + 2*kPetScale,
                 4*kPetScale, 5*kPetScale, kPink);
      // White heart in the middle of the chest band.
      r.fillRect(cx - 2, cy + 1*kPetScale,     2, 1, kWhite);
      r.fillRect(cx + 1, cy + 1*kPetScale,     2, 1, kWhite);
      r.fillRect(cx - 3, cy + 1*kPetScale + 1, 7, 1, kWhite);
      r.fillRect(cx - 2, cy + 1*kPetScale + 2, 5, 1, kWhite);
      r.fillRect(cx - 1, cy + 1*kPetScale + 3, 3, 1, kWhite);
      r.drawPixel(cx,    cy + 1*kPetScale + 4, kWhite);
      break;
    }
  }
}

// Coat recolor: overlay a translucent-ish coat tint by replacing body
// pixels in the rendered region. We do this cheaply by drawing thin
// vertical accents on top of the body silhouette.
void draw_coat_accents(Renderer& r, uint8_t coat_pattern) {
  if (coat_pattern == 0) return;
  uint16_t accent;
  switch (coat_pattern) {
    case 1: accent = kBrownDark;       break;  // brindle stripes
    case 2: accent = rgb(40, 25, 10);  break;  // tri-color (very dark)
    case 3: accent = rgb(190, 70, 40); break;  // red
    case 4: accent = rgb(20, 15, 10);  break;  // black-and-tan
    default: return;
  }
  // Stripes/spots over body region
  int sx = kPetX + 14 * kPetScale;
  int sy = kPetY + 26 * kPetScale;
  int sw = 20 * kPetScale;
  int sh = 14 * kPetScale;
  for (int i = 0; i < sw; i += 8) {
    r.fillRect(sx + i, sy, kPetScale, sh, accent);
  }
}

// ============= Action animations (Feed / Play / Bath) =====================

// Small deterministic noise -- gives each particle a unique offset / speed
// while staying frame-stable for a given (seed, idx).
static inline uint32_t pnoise(uint32_t seed, uint32_t idx) {
  uint32_t v = (seed ^ (idx * 2654435761u)) * 1664525u + 1013904223u;
  return v;
}

// Decoration: when this action is running, draw the choreographed
// particles + accessories. Helpers are below.
static void draw_feed_choreography(Renderer& r, const Pet& pet,
                                   uint32_t now_ms, int pet_x);
static void draw_play_choreography(Renderer& r, const Pet& pet,
                                   uint32_t now_ms, int pet_x);
static void draw_bath_choreography(Renderer& r, const Pet& pet,
                                   uint32_t now_ms, int pet_x);

void draw_pet_sprite(Renderer& r, const Pet& pet, uint32_t now_ms,
                     const Game& game) {
  PetPose pose = PetPose::IdleA;

  switch (pet.current_action) {
    case Action::Eat: {
      // 3 head bobs across the munch phase (300..975 ms).
      uint32_t elapsed = now_ms - pet.action_started_ms;
      bool head_down = (elapsed > 300 && elapsed < 975 &&
                        ((elapsed / 110) & 1));
      pose = head_down ? PetPose::IdleA : PetPose::IdleB;
      break;
    }
    case Action::Play: {
      // Wiggle frame -- alternate every ~80ms.
      uint32_t elapsed = now_ms - pet.action_started_ms;
      pose = ((elapsed / 80) & 1) ? PetPose::IdleA : PetPose::IdleB;
      break;
    }
    case Action::Clean: pose = PetPose::Pant;    break;
    case Action::Pet: {
      // Voice / menu trick overrides the default pet pose with a
      // per-trick visual.
      uint8_t kind = game.voice_trick_kind();
      uint32_t elapsed = now_ms - pet.action_started_ms;
      switch (kind) {
        case 1: pose = PetPose::Sit; break;                       // Sit
        case 2: pose = ((elapsed / 200) & 1) ? PetPose::IdleA     // Come
                                              : PetPose::IdleB;
                break;
        case 3: pose = PetPose::Bark; break;                      // High five
        case 4: {                                                 // Roll over
          uint32_t phase = (elapsed / 150) % 4;
          pose = (phase == 0) ? PetPose::IdleA :
                 (phase == 1) ? PetPose::Sleep :
                 (phase == 2) ? PetPose::IdleB : PetPose::IdleA;
          break;
        }
        case 5: pose = PetPose::IdleB; break;                     // Jump
        default: pose = PetPose::IdleB; break;
      }
      break;
    }
    case Action::None:
      switch (pet.mood) {
        case Mood::Sleeping:  pose = PetPose::Sleep; break;
        case Mood::Sad:       pose = PetPose::Sad;   break;
        case Mood::MovingOut: pose = PetPose::IdleB; break;  // happy walking off
        case Mood::Magic:     pose = PetPose::IdleA; break;  // calm transformation
        case Mood::Gone:      pose = PetPose::IdleA; break;  // legacy fallback
        default: {
          // Ambient behavior takes priority over breathing while idle.
          switch (game.ambient_behavior()) {
            case 1: // walking -- alternate breathing frames faster
              pose = ((now_ms / 250) & 1) ? PetPose::IdleB : PetPose::IdleA;
              break;
            case 2: pose = PetPose::Sit;     break;
            case 3: pose = PetPose::Pant;    break;
            case 4: pose = PetPose::Bark;    break;
            case 5: // run -- alternate frames very fast
              pose = ((now_ms / 100) & 1) ? PetPose::IdleB : PetPose::IdleA;
              break;
            case 6: pose = PetPose::Sleep;   break;   // lie down
            case 7: // scenery interact -- walk while moving, sit while settled
              if (game.ambient_x_offset() == 50) pose = PetPose::Sit;
              else pose = ((now_ms / 250) & 1) ? PetPose::IdleB : PetPose::IdleA;
              break;
            default:
              pose = ((now_ms / kIdleFrameMs) & 1) ? PetPose::IdleB : PetPose::IdleA;
              break;
          }
          break;
        }
      }
      break;
  }

  // For MovingOut, slide Bailey off the right edge based on elapsed time.
  // Otherwise apply the ambient walking x-offset (zero outside walks).
  int draw_x = kPetX;
  if (pet.mood == Mood::MovingOut) {
    uint32_t cycle = now_ms % 5000;
    int max_dx = (kScreenW - kPetX);
    draw_x = kPetX + (int)(cycle * max_dx / 5000);
  } else {
    draw_x = kPetX + game.ambient_x_offset();
    // Bath shake-off: small horizontal jiggle in the back half of the
    // Clean action (t01 >= 0.55).
    if (pet.current_action == Action::Clean) {
      uint32_t elapsed = now_ms - pet.action_started_ms;
      if (elapsed > (kActionCleanDurationMs * 55) / 100 &&
          elapsed < (kActionCleanDurationMs * 80) / 100) {
        draw_x += ((elapsed / 60) & 1) ? -2 : 2;
      }
    }
    // Round 4: rain shake-off -- 1.5 s wet-fur jiggle every ~10 s
    // while idle in Rain weather.
    if (pet.current_action == Action::None &&
        game.weather() == Weather::Rain) {
      uint32_t cycle = now_ms % 10000;
      if (cycle < 1500) {
        draw_x += ((cycle / 80) & 1) ? -2 : 2;
      }
      // Round 4 Phase 3: wet-fur drip behind Bailey while he walks.
      // 1-2 px blue dot drawn at the floor at his previous x position.
      if (game.ambient_x_offset() != 0 && ((now_ms / 200) & 3) == 0) {
        int drip_x = draw_x + (game.ambient_x_offset() > 0 ? -4 : kPetDrawW + 2);
        int drip_y = kPetY + kPetDrawH - 4;
        r.fillRect(drip_x, drip_y, 1, 2, kBlue);
      }
    }
    // Voice-trick "Come": slide Bailey 18 px toward the center over the
    // first half of the action, then back.
    if (pet.current_action == Action::Pet && game.voice_trick_kind() == 2) {
      uint32_t elapsed = now_ms - pet.action_started_ms;
      float u = (float)elapsed / 600.0f;  // 0..1 over the duration
      if (u > 1.0f) u = 1.0f;
      float tri = 1.0f - fabsf(u * 2.0f - 1.0f);   // 0..1..0
      draw_x += (int)(tri * 18);
    }
    // Clamp so the sprite stays on screen.
    if (draw_x < 4) draw_x = 4;
    if (draw_x + kPetDrawW > kScreenW - 4) draw_x = kScreenW - 4 - kPetDrawW;
  }

  // Voice-trick "Jump": parabolic Y offset.
  int draw_y = kPetY;
  if (pet.current_action == Action::Pet && game.voice_trick_kind() == 5) {
    uint32_t elapsed = now_ms - pet.action_started_ms;
    float u = (float)elapsed / 600.0f;
    if (u > 1.0f) u = 1.0f;
    float lift = 4.0f * u * (1.0f - u);   // 0..1..0 (peak at u=0.5)
    draw_y -= (int)(lift * 28);
  }

  const uint8_t* sprite = pet_sprite(pet.stage, pose);
  r.drawSprite(draw_x, draw_y, kPetW, kPetH, sprite, kSpritePalette, kPetScale);

  // Magic transition: scatter twinkles over the pet.
  if (pet.mood == Mood::Magic) {
    uint32_t seed = 0xBABE;
    for (int i = 0; i < 12; ++i) {
      seed = seed * 1664525u + 1013904223u + now_ms / 100;
      int sx = kPetX + (int)((seed >> 4) % kPetDrawW);
      int sy = kPetY + (int)((seed >> 12) % kPetDrawH);
      r.fillRect(sx, sy, 2, 2, kYellow);
      r.fillRect(sx - 1, sy + 1, 1, 1, kWhite);
    }
  }

  switch (pet.current_action) {
    case Action::Eat:   draw_feed_choreography(r, pet, now_ms, draw_x); break;
    case Action::Play:  draw_play_choreography(r, pet, now_ms, draw_x); break;
    case Action::Clean: draw_bath_choreography(r, pet, now_ms, draw_x); break;
    case Action::Pet:
      r.drawSprite(draw_x + kPetDrawW / 2 - 16, kPetY - 4,
                   16, 16, heart_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::None:
      if (pet.mood == Mood::Sleeping) {
        // Round 4: snore intensity -- the Z's pulse small/med/large
        // on a slow 600ms cycle, drifting up slightly too.
        int  zz_phase = (now_ms / 600) % 3;
        int  zz_scale = kAccessoryScale + (zz_phase == 2 ? 1 : 0);
        int  zz_dy    = (zz_phase == 0) ? 0 : (zz_phase == 1) ? -2 : -4;
        r.drawSprite(draw_x + kPetDrawW - 40, kPetY - 8 + zz_dy,
                     16, 16, zzz_sprite(), kSpritePalette, zz_scale);
      } else if (pet.mood == Mood::Dirty)
        r.drawSprite(draw_x + kPetDrawW - 36, kPetY + kPetDrawH - 36,
                     16, 16, poop_sprite(), kSpritePalette, kAccessoryScale);
      break;
  }
}

// ----- Feed: bowl slides in, head bobs, kibble pops, tongue lick --------
static void draw_feed_choreography(Renderer& r, const Pet& pet,
                                   uint32_t now_ms, int pet_x) {
  uint32_t elapsed = now_ms - pet.action_started_ms;
  float t = (float)elapsed / (float)tama::kActionEatDurationMs;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  // Bowl position: slides in 0.00-0.20, parks 0.20-0.85, slides out 0.85-1.00.
  const int bowl_parked_x = pet_x + kPetDrawW - 12;
  const int bowl_off_x    = kScreenW + 4;
  const int bowl_y        = kPetY + kPetDrawH - kAccessoryScale * 16 + 10;
  int bowl_x;
  bool empty = false;
  if (t < 0.20f) {
    float u = t / 0.20f;             // 0..1
    float ease = u * (2.0f - u);     // quadratic ease-out
    bowl_x = (int)(bowl_off_x + (bowl_parked_x - bowl_off_x) * ease);
  } else if (t < 0.85f) {
    bowl_x = bowl_parked_x;
  } else {
    float u = (t - 0.85f) / 0.15f;   // 0..1
    float ease = u * u;              // ease-in
    bowl_x = (int)(bowl_parked_x + (bowl_off_x - bowl_parked_x) * ease);
    empty = true;
  }
  const uint8_t* bowl = empty ? tama::food_bowl_empty_sprite()
                              : tama::food_bowl_sprite();
  r.drawSprite(bowl_x, bowl_y, 16, 16, bowl, kSpritePalette, kAccessoryScale);

  // Kibble particles during the munch phase (0.20-0.65). 8 particles
  // arc up from the bowl rim, fade by disappearing past their arc.
  if (t >= 0.20f && t < 0.65f) {
    float phase = (t - 0.20f) / 0.45f;
    for (int i = 0; i < 8; ++i) {
      uint32_t n = pnoise(0xF00D, i);
      float launch = (float)(n & 0xFF) / 255.0f;      // 0..1 when in window
      if (phase < launch) continue;
      float local = (phase - launch) * 2.5f;
      if (local > 1.0f) continue;
      // Arc parameters per particle
      int   bowl_cx = bowl_x + 8;
      int   bowl_cy = bowl_y + 4;
      float dx_max  = 10.0f + (float)((n >> 8) & 0x0F);
      float peak    = 12.0f + (float)((n >> 12) & 0x07);
      int   dir     = ((n >> 16) & 1) ? -1 : 1;
      int   px = bowl_cx + (int)(dir * dx_max * local);
      int   py = bowl_cy - (int)(peak * 4 * local * (1.0f - local));
      uint16_t col = ((n >> 20) & 1) ? kYellow : kOrange;
      r.fillRect(px, py, 2, 2, col);
    }
  }

  // Tongue + heart sparkles during the lick phase (0.65-0.85).
  if (t >= 0.65f && t < 0.85f) {
    int hx = pet_x + 18 * kPetScale;
    int hy = kPetY  + 19 * kPetScale;
    // Tongue lick: small pink curl in front of mouth.
    r.fillRect(hx + 1, hy + 1, 3, 2, kPink);
    r.fillRect(hx + 3, hy - 1, 2, 2, kPink);
    // 3 hearts drifting up
    float phase = (t - 0.65f) / 0.20f;
    for (int i = 0; i < 3; ++i) {
      uint32_t n = pnoise(0xBEEF, i);
      float local = phase - (float)(n & 0x3F) / 255.0f;
      if (local < 0 || local > 1) continue;
      int hxp = pet_x + 18 * kPetScale + ((i - 1) * 12);
      int hyp = kPetY + 8 - (int)(local * 24);
      r.fillRect(hxp, hyp, 3, 2, kHeartRed);
      r.fillRect(hxp - 1, hyp + 1, 5, 2, kHeartRed);
      r.fillRect(hxp,     hyp + 3, 3, 1, kHeartRed);
    }
  }
}

// ----- Play (puppy path): ball drops, bounces, rolls, hearts up ---------
static void draw_play_choreography(Renderer& r, const Pet& pet,
                                   uint32_t now_ms, int pet_x) {
  uint32_t elapsed = now_ms - pet.action_started_ms;
  float t = (float)elapsed / (float)tama::kActionPlayDurationMs;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  // Ground reference for the ball -- below Bailey's paws so the rolling
  // ball never crosses his leg row.
  const int ground_y = kPetY + kPetDrawH + 4;
  const int catch_x  = pet_x + 6;     // where Bailey "catches" the ball

  if (t < 0.18f) {
    // Phase 1: ball drops from above. Quadratic acceleration.
    float u = t / 0.18f;
    int ball_y = -16 + (int)((ground_y + 16) * u * u);
    int ball_x = catch_x + 60;
    r.drawSprite(ball_x, ball_y, 16, 16, tama::ball_sprite(),
                 kSpritePalette, kAccessoryScale);
  } else if (t < 0.55f) {
    // Phase 2/3: bouncing. Two damped bounces, each ~half the previous.
    float u = (t - 0.18f) / 0.37f;
    // Two bounces: first 0..0.55, second 0.55..1
    float peak, bphase;
    if (u < 0.55f) { peak = 30; bphase = u / 0.55f; }
    else            { peak = 16; bphase = (u - 0.55f) / 0.45f; }
    // half-sine arc for each bounce
    float h = peak * sinf(3.14159f * bphase);
    int ball_y = ground_y - (int)h;
    int ball_x = catch_x + 60 - (int)(60 * u * 0.4f);
    // Squash-stretch: flatter on bounce frames (bphase < 0.05 or > 0.95)
    bool impact = (bphase < 0.08f || bphase > 0.92f);
    if (impact) {
      r.fillRect(ball_x + 2, ball_y + 10, 12, 4, kGreen);
      r.drawRect(ball_x + 2, ball_y + 10, 12, 4, kBlack);
    } else {
      r.drawSprite(ball_x, ball_y, 16, 16, tama::ball_sprite(),
                   kSpritePalette, kAccessoryScale);
    }
  } else if (t < 0.80f) {
    // Phase 4: ball rolls in horizontally to Bailey.
    float u = (t - 0.55f) / 0.25f;
    int ball_x = catch_x + 36 - (int)(36 * u);
    int ball_y = ground_y - 2;
    r.drawSprite(ball_x, ball_y, 16, 16, tama::ball_sprite(),
                 kSpritePalette, kAccessoryScale);
  } else {
    // Phase 5: ball "caught" -- hearts float up.
    float phase = (t - 0.80f) / 0.20f;
    for (int i = 0; i < 3; ++i) {
      uint32_t n = pnoise(0xCAFE, i);
      float local = phase - (float)(n & 0x3F) / 255.0f;
      if (local < 0 || local > 1) continue;
      int hxp = pet_x + 18 * kPetScale + ((i - 1) * 14);
      int hyp = kPetY - 4 - (int)(local * 28);
      r.fillRect(hxp,     hyp,     3, 2, kHeartRed);
      r.fillRect(hxp - 1, hyp + 1, 5, 2, kHeartRed);
      r.fillRect(hxp,     hyp + 3, 3, 1, kHeartRed);
    }
  }
}

// ----- Bath: shower, foam, shake, sparkles ------------------------------
static void draw_bath_choreography(Renderer& r, const Pet& pet,
                                   uint32_t now_ms, int pet_x) {
  uint32_t elapsed = now_ms - pet.action_started_ms;
  float t = (float)elapsed / (float)tama::kActionCleanDurationMs;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  const int top_y    = kStatsBarH + 4;
  const int ground_y = kPetY + kPetDrawH - 8;

  // Phase 1: shower (0.00-0.30).
  if (t < 0.30f) {
    float phase = t / 0.30f;
    for (int i = 0; i < 10; ++i) {
      uint32_t n = pnoise(0xDB0B, i);
      float launch = (float)(n & 0xFF) / 255.0f;
      float local  = phase - launch * 0.4f;
      if (local < 0 || local > 1) continue;
      int dx = pet_x + (int)((n >> 8) % kPetDrawW);
      int dy = top_y + (int)((ground_y - top_y) * local);
      r.drawVLine(dx, dy, 3, kBlue);
      r.drawPixel(dx + 1, dy + 2, kSkyDeep);
    }
  }

  // Phase 2: foaming bubbles (0.15-0.65). Bubbles overlap droplet tail.
  if (t >= 0.15f && t < 0.65f) {
    float phase = (t - 0.15f) / 0.50f;
    for (int i = 0; i < 6; ++i) {
      uint32_t n = pnoise(0xB0BB, i);
      float launch = (float)(n & 0x3F) / 100.0f;   // 0..0.64
      if (phase < launch) continue;
      float local = (phase - launch) / 0.5f;
      if (local > 1) local = 1;
      // Grow scale 1 -> 2 across first 30 %, drift up the rest.
      // Spawn band positioned ABOVE the leg row so bubbles foam around
      // Bailey's body/head, not over his paws.
      int   scale = 1 + (int)(local * 1.5f);
      int   bx    = pet_x + ((n >> 6) % kPetDrawW);
      int   by    = (kPetY + 18 + (int)((n >> 10) % 60))
                    - (int)(local * 36.0f);
      r.drawSprite(bx, by, 16, 16, tama::bubble_sprite(),
                   kSpritePalette, scale);
    }
  }

  // Phase 3: shake-off droplet burst (0.55-0.80). Droplets fling out radially.
  if (t >= 0.55f && t < 0.80f) {
    float phase = (t - 0.55f) / 0.25f;
    int   cx = pet_x + kPetDrawW / 2;
    int   cy = kPetY + kPetDrawH / 2;
    for (int i = 0; i < 8; ++i) {
      uint32_t n  = pnoise(0xD1DA, i);
      float ang   = ((float)(n & 0xFFFF) / 65535.0f) * 6.2831853f;
      float dist  = phase * 36.0f + 6;
      int   dx = cx + (int)(cosf(ang) * dist);
      int   dy = cy + (int)(sinf(ang) * dist);
      r.drawPixel(dx,     dy,     kBlue);
      r.drawPixel(dx + 1, dy,     kSkyDeep);
      r.drawPixel(dx,     dy + 1, kSkyDeep);
    }
  }

  // Phase 4: sparkles (0.78-1.00).
  if (t >= 0.78f) {
    float phase = (t - 0.78f) / 0.22f;
    for (int i = 0; i < 6; ++i) {
      uint32_t n = pnoise(0x5A55, i);
      float launch = (float)(n & 0x3F) / 64.0f;
      float local  = phase - launch * 0.6f;
      if (local < 0 || local > 0.5f) continue;     // each sparkle lives ~120ms
      int sx = pet_x + ((n >> 6) % kPetDrawW);
      int sy = kPetY  + ((n >> 14) % kPetDrawH);
      // Cross-shaped white sparkle.
      r.fillRect(sx - 2, sy,     5, 1, kWhite);
      r.fillRect(sx,     sy - 2, 1, 5, kWhite);
      r.drawPixel(sx,    sy,        kYellow);
    }
  }
}

// Weather overlay: light particles across the screen.
// Round 4: rainbow arc across the upper half, painted for a window
// after Rain -> Sunny.
static void draw_rainbow(Renderer& r) {
  static const uint16_t bands[6] = {
      kRed, kOrange, kYellow, kGreen, kBlue, kPink};
  int cx = kScreenW / 2;
  int cy = kScreenH - 40;     // off-screen-low so the arc curves over
  for (int b = 0; b < 6; ++b) {
    int rr = 130 - b * 4;
    // sample the upper semicircle once per pixel column
    for (int dx = -rr; dx <= rr; dx += 2) {
      // y = cy - sqrt(rr*rr - dx*dx), approximated:
      int dx2 = dx * dx;
      int rr2 = rr * rr;
      if (dx2 > rr2) continue;
      int v = rr2 - dx2;
      // crude integer sqrt good enough for a 130-radius arc
      int s = 0;
      while ((s + 1) * (s + 1) <= v) ++s;
      int x = cx + dx;
      int y = cy - s;
      if (y < kStatsBarH + 2 || y > kScreenH - kStatusH - 30) continue;
      r.fillRect(x, y, 2, 1, bands[b]);
    }
  }
}

// Round 4: 3 diagonal yellow streaks from the sun corner.
static void draw_sun_rays(Renderer& r, uint32_t now_ms) {
  int sx = kScreenW - 20;
  int sy = kStatsBarH + 14;
  for (int i = 0; i < 3; ++i) {
    int dx = -40 - i * 18;
    int dy = (i % 2 ? 1 : -1) * (4 + i * 3);
    int len = 30 + i * 6;
    // Twinkle: vary intensity by frame.
    uint16_t col = ((now_ms / 120 + i) & 1) ? kYellow : kOrange;
    for (int k = 0; k < len; ++k) {
      int x = sx + dx + k;
      int y = sy + (dy * k) / len;
      r.drawPixel(x, y, col);
    }
  }
}

// Round 4: fog overlay -- translucent gray checkerboard across the
// play area. Cheap dither using a 2x2 pattern.
static void draw_fog_overlay(Renderer& r, uint32_t now_ms) {
  int floor_y = kPetY + kPetDrawH - 6;
  uint16_t haze = mix(kGrayDark, kWhite, 0.7f);
  int phase = (now_ms / 200) & 1;
  for (int y = kStatsBarH + 1; y < floor_y; y += 1) {
    for (int x = 0; x < kScreenW; x += 2) {
      if (((x + y + phase) & 3) == 0) r.drawPixel(x, y, haze);
    }
  }
}

void draw_weather(Renderer& r, const Game& game, uint32_t now_ms) {
  uint8_t weather = (uint8_t)game.weather();
  int floor_y = kPetY + kPetDrawH - 6;
  uint32_t seed = 0x12345;

  // Lightning flash: full-screen white veil for 80 ms after a strike.
  if (weather == (uint8_t)Weather::Rain &&
      now_ms - game.last_lightning_ms() < 80) {
    r.fillRect(0, kStatsBarH + 1, kScreenW,
               kScreenH - kStatsBarH - kStatusH - 1, kWhite);
  }

  if (weather == (uint8_t)Weather::Rain) {
    // 36 falling streaks + a splash dot on the floor where each lands.
    for (int i = 0; i < 36; ++i) {
      seed = seed * 1664525u + 1013904223u;
      int x = (int)((seed >> 8) % kScreenW);
      int y = (int)(((seed >> 16) + now_ms / 50) % (kScreenH - kStatusH));
      r.drawVLine(x, y, 4, kSkyDeep);
      // Splash mark just above the floor for streaks that landed.
      if (y > floor_y - 6 && y < floor_y) {
        r.fillRect(x - 1, floor_y, 3, 1, kBlue);
      }
    }
    // Up to 4 puddles at fixed scene positions.
    static const int kPuddleX[4] = {30, 90, 150, 200};
    for (int i = 0; i < 4; ++i) {
      r.fillRect(kPuddleX[i], floor_y + 2, 10, 2, kBlue);
      r.drawPixel(kPuddleX[i] - 1, floor_y + 2, kSkyDeep);
      r.drawPixel(kPuddleX[i] + 10, floor_y + 2, kSkyDeep);
    }
  } else if (weather == (uint8_t)Weather::Snow) {
    // Snowflakes drift sideways via a sin-ish wobble; size shrinks
    // near the floor for a tiny depth cue.
    for (int i = 0; i < 32; ++i) {
      seed = seed * 1664525u + 1013904223u;
      int base_x = (int)((seed >> 8) % kScreenW);
      int y = (int)(((seed >> 16) + now_ms / 80) % (kScreenH - kStatusH));
      // Triangle-wave wobble across +-4 px, period ~1.6s.
      int phase = (int)((now_ms / 100) + i * 17) % 16;
      int wobble = phase < 8 ? phase - 4 : 12 - phase;
      int x = base_x + wobble;
      int sz = (y > floor_y - 24) ? 1 : 2;
      r.fillRect(x, y, sz, sz, kWhite);
    }
  } else if (weather == (uint8_t)Weather::Fog) {
    draw_fog_overlay(r, now_ms);
  }

  // After Rain -> Sunny (within 30 s), paint the rainbow.
  if (weather == (uint8_t)Weather::Sunny &&
      game.prev_weather() == (uint8_t)Weather::Rain &&
      now_ms - game.last_weather_change_ms() < 30000) {
    draw_rainbow(r);
  }
  // Sunny + bright daylight -> sun rays.
  if (weather == (uint8_t)Weather::Sunny && game.daylight() > 0.85f) {
    draw_sun_rays(r, now_ms);
  }
}

// Scene-specific ambient detail.
void draw_scene_detail(Renderer& r, uint8_t scene_id, float daylight) {
  int floor_y = kPetY + kPetDrawH - 6;
  switch (scene_id) {
    case 1: {  // Backyard: fence + tree
      uint16_t fence = mix(rgb(60, 40, 20), rgb(140, 90, 50), daylight);
      for (int x = 0; x < kScreenW; x += 18) {
        r.fillRect(x, floor_y - 14, 4, 14, fence);
      }
      r.fillRect(0, floor_y - 2, kScreenW, 2, fence);
      // Tree on right
      uint16_t trunk = mix(rgb(40, 25, 10), rgb(120, 70, 35), daylight);
      uint16_t leaf  = mix(rgb(20, 60, 25), kGreen, daylight);
      r.fillRect(210, floor_y - 30, 6, 30, trunk);
      r.fillRect(196, floor_y - 50, 36, 22, leaf);
      break;
    }
    case 2: {  // Dog park: bench + sign
      uint16_t bench = mix(rgb(50, 30, 20), rgb(160, 100, 60), daylight);
      r.fillRect(8, floor_y - 12, 60, 4, bench);
      r.fillRect(10, floor_y - 12, 3, 12, bench);
      r.fillRect(63, floor_y - 12, 3, 12, bench);
      // Sign
      r.fillRect(170, floor_y - 28, 30, 14, kBrownLight);
      r.drawText(174, floor_y - 24, "PARK", kBrownDark, 1);
      r.fillRect(183, floor_y - 14, 4, 14, kBrownDark);
      break;
    }
    case 3: {  // Beach: sand band, ocean, sun, palm tree
      uint16_t sand   = mix(rgb(120, 90, 50), rgb(240, 215, 150), daylight);
      uint16_t ocean  = mix(rgb(10, 30, 90),  rgb(70, 150, 220),  daylight);
      uint16_t foam   = mix(rgb(200, 220, 230), kWhite,            daylight);
      // Ocean horizon strip
      r.fillRect(0, floor_y - 22, kScreenW, 10, ocean);
      r.drawHLine(0, floor_y - 12, kScreenW, foam);
      // Sand floor band
      r.fillRect(0, floor_y - 2, kScreenW, kScreenH - floor_y - kStatusH + 2, sand);
      // Sun
      uint16_t sun = mix(rgb(245, 180, 80), kYellow, daylight);
      r.fillRect(184, kStatsBarH + 10, 14, 14, sun);
      // Palm tree (trunk + 4 fronds)
      uint16_t trunk = mix(rgb(60, 35, 10), rgb(120, 75, 30), daylight);
      uint16_t frond = mix(rgb(20, 90, 30), kGreen, daylight);
      r.fillRect(28, floor_y - 36, 4, 36, trunk);
      r.fillRect(16, floor_y - 38, 12, 3, frond);
      r.fillRect(30, floor_y - 38, 14, 3, frond);
      r.fillRect(20, floor_y - 42, 8, 3, frond);
      r.fillRect(30, floor_y - 42, 10, 3, frond);
      break;
    }
    case 4: {  // Bedroom: bed + pillow + nightstand + lamp + dog house
      uint16_t floor = mix(rgb(80, 50, 30), rgb(170, 130, 90), daylight);
      r.fillRect(0, floor_y - 2, kScreenW, kScreenH - floor_y - kStatusH + 2, floor);
      // Bed (right side)
      uint16_t blanket = mix(rgb(60, 30, 80), rgb(160, 110, 200), daylight);
      uint16_t pillow  = mix(rgb(180, 180, 200), kWhite,           daylight);
      r.fillRect(140, floor_y - 22, 92, 22, blanket);
      r.fillRect(146, floor_y - 26, 24, 8, pillow);
      // Bed frame
      uint16_t frame = mix(rgb(40, 25, 10), rgb(120, 80, 40), daylight);
      r.drawHLine(140, floor_y - 22, 92, frame);
      r.drawVLine(140, floor_y - 22, 22, frame);
      // Nightstand (left)
      r.fillRect(12, floor_y - 16, 22, 16, frame);
      // Lamp on nightstand
      uint16_t lamp = mix(rgb(120, 100, 30), kYellow, daylight);
      r.fillRect(20, floor_y - 26, 6, 10, lamp);
      r.fillRect(17, floor_y - 30, 12, 4, lamp);
      // Round 5 Phase A2: small red dog house in the bottom-left.
      // 28 px wide, 18 tall, with a triangular roof + dark doorway arch.
      uint16_t house_body = mix(rgb(110, 30, 25), kRed,   daylight);
      uint16_t house_roof = mix(rgb(70,  20, 18), rgb(170, 70, 55), daylight);
      r.fillRect(46, floor_y - 18, 28, 18, house_body);
      // Triangular roof (4 rows widening then narrowing)
      for (int row = 0; row < 6; ++row) {
        int w  = 24 - row * 2;
        int xs = 46 + 2 + row;
        r.fillRect(xs, floor_y - 24 + row, w, 1, house_roof);
      }
      // Doorway arch
      r.fillRect(56, floor_y - 12, 8, 12, rgb(20, 10, 8));
      r.drawPixel(55, floor_y - 11, rgb(20, 10, 8));
      r.drawPixel(64, floor_y - 11, rgb(20, 10, 8));
      break;
    }
    case 5: {  // Kitchen: tile floor + fridge + food bag
      // Checker tile floor
      uint16_t tileA = mix(rgb(150, 150, 160), rgb(210, 210, 220), daylight);
      uint16_t tileB = mix(rgb(120, 120, 130), rgb(180, 180, 190), daylight);
      for (int y = floor_y - 2; y < kScreenH - kStatusH; y += 6) {
        for (int x = 0; x < kScreenW; x += 16) {
          r.fillRect(x,      y, 8, 6, tileA);
          r.fillRect(x + 8,  y, 8, 6, tileB);
        }
      }
      // Fridge (left)
      uint16_t fridge = mix(rgb(170, 170, 180), kWhite, daylight);
      uint16_t handle = mix(rgb(60, 60, 65), rgb(140, 140, 150), daylight);
      r.fillRect(8, kStatsBarH + 8, 36, floor_y - kStatsBarH - 8, fridge);
      r.drawRect(8, kStatsBarH + 8, 36, floor_y - kStatsBarH - 8, handle);
      r.drawHLine(8, kStatsBarH + 38, 36, handle);
      r.fillRect(38, kStatsBarH + 20, 3, 12, handle);
      r.fillRect(38, kStatsBarH + 46, 3, 12, handle);
      // Dog food bag (right)
      uint16_t bag = mix(rgb(90, 60, 20), rgb(190, 130, 50), daylight);
      r.fillRect(196, floor_y - 28, 28, 28, bag);
      r.drawText(199, floor_y - 22, "BAILEY", kWhite, 1);
      r.drawText(202, floor_y - 12, "FOOD",  kWhite, 1);
      break;
    }
    case 6: {  // Forest: trees + ferns
      uint16_t moss  = mix(rgb(20, 60, 30), rgb(60, 130, 60), daylight);
      r.fillRect(0, floor_y - 2, kScreenW, kScreenH - floor_y - kStatusH + 2, moss);
      uint16_t trunk = mix(rgb(40, 25, 10), rgb(100, 70, 35), daylight);
      uint16_t leaf  = mix(rgb(10, 70, 20), rgb(50, 140, 50), daylight);
      // Three trees of varying height
      int trees_x[3]   = {10, 110, 200};
      int trees_h[3]   = {62, 72, 56};
      for (int i = 0; i < 3; ++i) {
        int tx = trees_x[i];
        int th = trees_h[i];
        r.fillRect(tx, floor_y - th, 6, th, trunk);
        // Canopy (overlapping ellipses)
        r.fillRect(tx - 12, floor_y - th - 4, 30, 18, leaf);
        r.fillRect(tx - 8,  floor_y - th - 12, 22, 14, leaf);
      }
      // Ferns at floor
      for (int x = 16; x < kScreenW; x += 36) {
        r.fillRect(x, floor_y - 6, 3, 6, leaf);
        r.fillRect(x - 4, floor_y - 4, 3, 4, leaf);
        r.fillRect(x + 4, floor_y - 4, 3, 4, leaf);
      }
      break;
    }
    case 7: {  // Snow park: snow + bare trees + snowman
      uint16_t snow = mix(rgb(170, 180, 200), kWhite, daylight);
      r.fillRect(0, floor_y - 2, kScreenW, kScreenH - floor_y - kStatusH + 2, snow);
      uint16_t trunk = mix(rgb(30, 20, 15), rgb(80, 60, 40), daylight);
      // Two bare trees
      const int bare_x[2] = {30, 180};
      for (int b = 0; b < 2; ++b) {
        int tx = bare_x[b];
        r.fillRect(tx, floor_y - 38, 4, 38, trunk);
        // Branches
        r.fillRect(tx - 8, floor_y - 30, 8, 2, trunk);
        r.fillRect(tx + 4, floor_y - 26, 10, 2, trunk);
        r.fillRect(tx - 6, floor_y - 22, 6, 2, trunk);
        r.fillRect(tx + 4, floor_y - 18, 8, 2, trunk);
      }
      // Snowman: two stacked white circles + dot eyes + carrot nose
      int sx = 120, sy = floor_y - 22;
      r.fillRect(sx - 6, sy,      12, 10, kWhite);     // body
      r.drawRect(sx - 6, sy,      12, 10, kGrayDark);
      r.fillRect(sx - 4, sy - 8,   8,  8, kWhite);     // head
      r.drawRect(sx - 4, sy - 8,   8,  8, kGrayDark);
      r.drawPixel(sx - 2, sy - 6,  kBlack);
      r.drawPixel(sx + 1, sy - 6,  kBlack);
      r.fillRect(sx,     sy - 4,   2,  1, kOrange);
      // Snowflakes drifting
      for (int i = 0; i < 12; ++i) {
        int fx = (i * 23) % kScreenW;
        int fy = (kStatsBarH + 4) + (i * 11) % (floor_y - kStatsBarH - 10);
        r.drawPixel(fx, fy, kWhite);
      }
      break;
    }
    case 0:
    default: {  // Living room: couch + window
      uint16_t couch = mix(rgb(40, 60, 90), rgb(120, 160, 220), daylight);
      r.fillRect(8, floor_y - 18, 70, 18, couch);
      r.fillRect(0, floor_y - 24, 14, 24, couch);
      r.fillRect(72, floor_y - 24, 14, 24, couch);
      // Window
      uint16_t frame = mix(rgb(20, 15, 10), rgb(120, 80, 40), daylight);
      r.fillRect(160, kStatsBarH + 12, 60, 40, frame);
      r.fillRect(163, kStatsBarH + 15, 54, 34, mix(rgb(20, 20, 40), kSky, daylight));
      r.drawHLine(163, kStatsBarH + 31, 54, frame);
      r.drawVLine(189, kStatsBarH + 15, 34, frame);
      break;
    }
  }
}

void draw_footer(Renderer& r, const Game& game) {
  const Pet& pet = game.pet();
  int y0 = kScreenH - kStatusH;
  r.fillRect(0, y0, kScreenW, kStatusH, kGrayDark);
  r.drawHLine(0, y0, kScreenW, kGrayLight);

  // MovingOut footer renders as "Bailey moved in with the <Family>."
  // Active visits override the mood text with "<Name> is visiting" /
  // "<Name1> and <Name2> are visiting".
  char msg_buf[48];
  const char* msg;
  uint8_t v0 = game.npc_visit_kind(0);
  uint8_t v1 = game.npc_visit_kind(1);
  if (pet.mood == Mood::MovingOut) {
    int idx = game.move_out_family_idx() & 7;
    std::snprintf(msg_buf, sizeof(msg_buf), "Moved in w/ the %s", kFamilyNames[idx]);
    msg = msg_buf;
  } else if (v0 == Game::kMysteryVisitorKind ||
             v1 == Game::kMysteryVisitorKind) {
    std::snprintf(msg_buf, sizeof(msg_buf), "A mystery dog visits...");
    msg = msg_buf;
  } else if (v0 != 0 && v1 != 0) {
    std::snprintf(msg_buf, sizeof(msg_buf), "%s and %s are visiting",
                  friend_name((Friend)(v0 - 1)),
                  friend_name((Friend)(v1 - 1)));
    msg = msg_buf;
  } else if (v0 != 0) {
    std::snprintf(msg_buf, sizeof(msg_buf), "%s is visiting",
                  friend_name((Friend)(v0 - 1)));
    msg = msg_buf;
  } else if (game.hide_seek_active()) {
    const char* h = game.hide_seek_last_outcome() == 1 ? "Found Bailey!"
                  : game.hide_seek_last_outcome() == 2 ? "Just a peek..."
                  :                                        "Not there!";
    std::snprintf(msg_buf, sizeof(msg_buf), "%s", h);
    msg = msg_buf;
  } else if (game.bedtime_story_active()) {
    std::snprintf(msg_buf, sizeof(msg_buf), "%s", game.bedtime_story_text());
    msg = msg_buf;
  } else if (game.gourmet_active()) {
    uint32_t rem = game.gourmet_remaining_ms() / 1000;
    std::snprintf(msg_buf, sizeof(msg_buf), "GOURMET %u:%02u",
                  (unsigned)(rem / 60), (unsigned)(rem % 60));
    msg = msg_buf;
  } else if (game.trick_combo_active()) {
    uint32_t rem = game.trick_combo_remaining_ms() / 1000;
    std::snprintf(msg_buf, sizeof(msg_buf), "TRICK COMBO %u:%02u",
                  (unsigned)(rem / 60), (unsigned)(rem % 60));
    msg = msg_buf;
  } else {
    // Round 4: rotating mood-text bank + time-of-day prefix.
    // Round 5: substitute custom pet name into the body.
    const char* body   = mood_text_variant(game);
    const char* prefix = time_of_day_prefix(game);
    static char body_named[48];
    apply_pet_name(body_named, sizeof(body_named), body, game.pet_name());
    if (prefix[0] != '\0') {
      std::snprintf(msg_buf, sizeof(msg_buf), "%s%s", prefix, body_named);
      msg = msg_buf;
    } else {
      // Copy into msg_buf so the rename is preserved across the wrap
      // logic below (which reads `msg` and only `msg_buf` is owned).
      std::snprintf(msg_buf, sizeof(msg_buf), "%s", body_named);
      msg = msg_buf;
    }
  }
  // Round 5 revision: drop the bottom-right Stage/age/streak chip
  // (still visible in the Stats tab) and bring back the bigger
  // scale-2 mood text. 2-line word-wrap stays so long phrases like
  // "Bailey is having a great day!" don't clip. Cap at 19 chars per
  // line -- 20 * 12 px = 240 px = edge-to-edge at scale 2, so 19
  // leaves a small margin.
  constexpr int kMaxLineChars = 19;
  int msg_len = 0;
  while (msg[msg_len] != '\0' && msg_len < 47) ++msg_len;
  if (msg_len <= kMaxLineChars) {
    // Fits on one line, centered in the 36 px footer (scale-2 text
    // is 16 px tall, sits at y0+10..y0+26).
    int tw = text_width(msg, 2);
    int tx = (kScreenW - tw) / 2;
    r.drawText(tx, y0 + 10, msg, kWhite, 2);
  } else {
    // Find the split point: last space in [0, kMaxLineChars], else
    // hard-break at kMaxLineChars.
    int split = kMaxLineChars;
    for (int i = kMaxLineChars; i > 0; --i) {
      if (msg[i] == ' ') { split = i; break; }
    }
    char line1[24], line2[24];
    int l1 = split;
    if (l1 > 23) l1 = 23;
    for (int i = 0; i < l1; ++i) line1[i] = msg[i];
    line1[l1] = '\0';
    int start2 = split;
    while (msg[start2] == ' ') ++start2;
    int l2 = msg_len - start2;
    if (l2 > 23) l2 = 23;
    for (int i = 0; i < l2; ++i) line2[i] = msg[start2 + i];
    line2[l2] = '\0';
    int tw1 = text_width(line1, 2);
    int tw2 = text_width(line2, 2);
    r.drawText((kScreenW - tw1) / 2, y0 + 2,  line1, kWhite, 2);
    r.drawText((kScreenW - tw2) / 2, y0 + 18, line2, kWhite, 2);
  }
}

void draw_menu_tabs(Renderer& r, const Game& game) {
  // Tab IDs correspond to the MenuTab enum AFTER the Actions=0
  // reorder. ACT renders first so the leftmost tab matches the
  // menu's default-open tab.
#if BAILEY_MEMORIAL_WALL
  const char* labels[] = {"ACT", "STA", "BDG", "OPT", "SYN", "MEM", "BAG", "SHP"};
  const int   tab_ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
  constexpr int n_tabs = 8;
#else
  const char* labels[] = {"ACT", "STA", "BDG", "OPT", "SYN", "BAG", "SHP"};
  const int   tab_ids[] = {0, 1, 2, 3, 4, 6, 7};
  constexpr int n_tabs = 7;
#endif
  int x = 6;
  int y = 14 + kStatsBarH;
  for (int i = 0; i < n_tabs; ++i) {
    bool active = (int)game.menu_tab() == tab_ids[i];
    uint16_t bg = active ? kYellow : kGrayDark;
    uint16_t fg = active ? kBlack  : kGrayLight;
    int w = text_width(labels[i], 1) + 4;
    r.fillRect(x, y, w, 12, bg);
    r.drawRect(x, y, w, 12, kYellow);
    r.drawText(x + 2, y + 2, labels[i], fg, 1);
    x += w + 1;
  }
}

#if BAILEY_MEMORIAL_WALL
void draw_menu_memorial(Renderer& r, const Game& game) {
  int x = 22;
  int y = 14 + kStatsBarH + 22;
  r.drawText(x, y, "MEMORIAL WALL", kYellow, 1); y += 12;
  uint8_t n = game.memorial_count();
  if (n == 0) {
    r.drawText(x, y, "No past Baileys yet.", kGray, 1);
    return;
  }
  char buf[40];
  const char* stage_str[] = {"Puppy","Adult","Senior","Gone"};
  for (uint8_t i = 0; i < n && i < 5; ++i) {
    const auto& e = game.memorial_entry(i);
    const char* stage = (e.peak_stage <= 3) ? stage_str[e.peak_stage] : "?";
    int badges = __builtin_popcount(e.achievements_mask);
    std::snprintf(buf, sizeof(buf), "#%u  %s  %lum  %dB",
                  i + 1, stage, (unsigned long)e.age_minutes, badges);
    r.drawText(x, y, buf, kWhite, 1);
    y += 10;
  }
}
#endif  // BAILEY_MEMORIAL_WALL

void draw_menu_stats(Renderer& r, const Pet& pet, const Game& game) {
  int x = 22;
  int y = 14 + kStatsBarH + 18;
  char buf[40];

  // Round 3 (post-audit): the upper info block uses 10 px line spacing
  // instead of 12, to keep the Mood sparkline + Yesterday diary on
  // screen when every conditional row is present (synced clock + best
  // friend + bedtime stories).
  constexpr int kInfoStep = 10;

  // Round 3: daily quest line (only if a synced clock has set a day index).
  if (game.daily_quest_goal() > 0) {
    const char* check = game.daily_quest_awarded_today() ? " (done!)" :
                        (game.daily_quest_complete()    ? " (claim)"  : "");
    std::snprintf(buf, sizeof(buf), "Quest : %s %u/%u%s",
                  game.daily_quest_text(),
                  (unsigned)game.daily_quest_progress(),
                  (unsigned)game.daily_quest_goal(), check);
    r.drawText(x, y, buf,
               game.daily_quest_awarded_today() ? kGreen : kYellow, 1);
    y += kInfoStep;
  }
  if (game.horoscope_text()[0] != '\0') {
    std::snprintf(buf, sizeof(buf), "Today : %s", game.horoscope_text());
    r.drawText(x, y, buf, kPink, 1); y += kInfoStep;
  }

  std::snprintf(buf, sizeof(buf), "Stage : %s", stage_text(pet.stage));
  r.drawText(x, y, buf, kWhite, 1); y += kInfoStep;

  uint32_t age_min = (uint32_t)(pet.age_ms / 60000ULL);
  std::snprintf(buf, sizeof(buf), "Age   : %lu min", (unsigned long)age_min);
  r.drawText(x, y, buf, kWhite, 1); y += kInfoStep;

  std::snprintf(buf, sizeof(buf), "Trait : %s", personality_name(game.personality()));
  r.drawText(x, y, buf, kPink, 1); y += kInfoStep;

  std::snprintf(buf, sizeof(buf), "Streak: %u days  Pets: %lu",
                (unsigned)game.streak_days(), (unsigned long)game.total_pets());
  r.drawText(x, y, buf, kYellow, 1); y += kInfoStep;

  // Round 5 Phase C1: trainer level + lifetime play time.
  std::snprintf(buf, sizeof(buf), "Trainer Lv %u  (%u XP)",
                (unsigned)game.trainer_level(), (unsigned)game.trainer_xp());
  r.drawText(x, y, buf, kGreen, 1); y += kInfoStep;
  // Round 6 Phase 6B: trainer title (auto by XP, or chosen earned title).
  std::snprintf(buf, sizeof(buf), "Title : %s", game.trainer_title());
  r.drawText(x, y, buf, kYellow, 1); y += kInfoStep;
  // Round 6 Phase 6F: XP bonus from active streak.
  if (game.xp_bonus_pct() > 100) {
    std::snprintf(buf, sizeof(buf), "XP boost: +%u%%",
                  (unsigned)(game.xp_bonus_pct() - 100));
    r.drawText(x, y, buf, kGreen, 1); y += kInfoStep;
  }
  {
    uint64_t mins = game.time_played_ms() / 60000ULL;
    uint64_t hrs  = mins / 60ULL;
    std::snprintf(buf, sizeof(buf), "Played: %luh %02lum",
                  (unsigned long)hrs, (unsigned long)(mins % 60));
    r.drawText(x, y, buf, kGrayLight, 1); y += kInfoStep;
  }
  // Round 5 Phase C2: daily action goal + active streak.
  {
    uint16_t today = game.today_actions();
    uint16_t cap   = Game::kDailyActionGoal;
    bool met = today >= cap;
    std::snprintf(buf, sizeof(buf), "Today: %u/%u%s  Active: %ud",
                  (unsigned)(today > cap ? cap : today), (unsigned)cap,
                  met ? " *" : "",
                  (unsigned)game.active_streak_days());
    r.drawText(x, y, buf, met ? kGreen : kYellow, 1); y += kInfoStep;
  }
  // Round 5 Phase C2: derived skill stats (IQ / stamina / charm), 0..100.
  {
    std::snprintf(buf, sizeof(buf), "IQ %u  Stamina %u  Charm %u",
                  game.skill_intelligence(), game.skill_stamina(),
                  game.skill_charm());
    r.drawText(x, y, buf, kPink, 1); y += kInfoStep;
  }
  // Round 5 Phase C3: hall of fame compact records line. Top counters
  // surfaced together: fetch catches / dig successes / hide-seek wins.
  {
    std::snprintf(buf, sizeof(buf), "Best: %luF %uD %uH",
                  (unsigned long)game.fetch_catches(),
                  (unsigned)game.dig_successes(),
                  (unsigned)game.hide_seek_wins());
    r.drawText(x, y, buf, kYellow, 1); y += kInfoStep;
  }
  // Round 6 Phase 6D: vet history -- last cure age in days.
  if (game.vet_history_count() > 0) {
    uint32_t today = game.today_day_index();
    uint32_t last  = game.vet_history_day(0);
    uint32_t ago   = (today > last) ? (today - last) : 0;
    std::snprintf(buf, sizeof(buf), "Vet: %u cures (last %ud ago)",
                  (unsigned)game.vet_history_count(), (unsigned)ago);
    r.drawText(x, y, buf, kRed, 1); y += kInfoStep;
  }
  // Round 6 Phase 6F: most-recent completed quest from history.
  if (game.quest_history_count() > 0) {
    uint8_t qid = game.quest_history_entry(0);
    std::snprintf(buf, sizeof(buf), "Last quest: #%u (%u done)",
                  (unsigned)qid, (unsigned)game.quest_history_count());
    r.drawText(x, y, buf, kGreen, 1); y += kInfoStep;
  }
  // Round 5 Phase D remainder: last postcard received.
  if (game.last_postcard_msg_id() < 16) {
    const char* m = Game::postcard_message(game.last_postcard_msg_id());
    if (m) {
      std::snprintf(buf, sizeof(buf), "Card: %s", m);
      r.drawText(x, y, buf, kPink, 1); y += kInfoStep;
    }
  }
  // Round 5 Phase C remainder: daily login wheel state.
  {
    static const char* const kWheelName[5] = {
      "5 biscuits", "3 bones", "biscuit treat", "bacon treat", "sticker",
    };
    if (game.wheel_available()) {
      r.drawText(x, y, "Wheel: SPIN!", kGreen, 1);
    } else if (game.today_day_index() != 0) {
      uint8_t reward = game.last_wheel_reward();
      if (reward < 5) {
        std::snprintf(buf, sizeof(buf), "Wheel: %s", kWheelName[reward]);
        r.drawText(x, y, buf, kGrayLight, 1);
      } else {
        r.drawText(x, y, "Wheel: claimed", kGrayLight, 1);
      }
    }
    y += kInfoStep;
  }

  // Round 3: best-friend bond from the last sync code consumed.
  if (game.best_friend_hash() != 0) {
    std::snprintf(buf, sizeof(buf), "Best friend: %04X",
                  (unsigned)(game.best_friend_hash() & 0xFFFFu));
    r.drawText(x, y, buf, kPink, 1); y += kInfoStep;
  }
  // Round 6 Phase 6B: aggregate friend-bond hearts. Only render once
  // any bond has been earned (saves a row on a fresh save).
  {
    uint16_t total = 0;
    for (int i = 0; i < (int)Friend::COUNT; ++i)
      total += game.friend_bond((Friend)i);
    if (total > 0) {
      std::snprintf(buf, sizeof(buf), "Bonds: %u/40 hearts", (unsigned)total);
      r.drawText(x, y, buf, kPink, 1); y += kInfoStep;
    }
  }
  // Round 6 Phase 6E: soul-bonded friend.
  if (game.soul_bond_friend_id() < (int)Friend::COUNT) {
    static const char* const kFriendName[8] = {
      "Ollie", "Mitchell", "Enzo", "Lincoln",
      "Ruben", "Francie", "Bomi", "Noshy",
    };
    std::snprintf(buf, sizeof(buf), "Soul bond: %s",
                  kFriendName[game.soul_bond_friend_id()]);
    r.drawText(x, y, buf, kPink, 1); y += kInfoStep;
  }
  // Round 6 Phase 6E: dormant friends -- "miss" hint.
  {
    uint8_t dormant = game.dormant_friends_mask();
    if (dormant != 0) {
      int n = 0;
      for (int i = 0; i < 8; ++i) if (dormant & (1u << i)) ++n;
      std::snprintf(buf, sizeof(buf), "Miss: %d friends 3+ days", n);
      r.drawText(x, y, buf, kYellow, 1); y += kInfoStep;
    }
  }
  if (game.stories_heard() > 0) {
    std::snprintf(buf, sizeof(buf), "Stories: %u", (unsigned)game.stories_heard());
    r.drawText(x, y, buf, kGrayLight, 1); y += kInfoStep;
  }
  // Round 4 Phase 3: tomorrow's weather forecast.
  {
    auto wname = [](Weather w) -> const char* {
      switch (w) {
        case Weather::Sunny:  return "SUNNY";
        case Weather::Cloudy: return "CLOUDY";
        case Weather::Rain:   return "RAIN";
        case Weather::Snow:   return "SNOW";
        case Weather::Fog:    return "FOG";
        default:              return "?";
      }
    };
    std::snprintf(buf, sizeof(buf), "Today: %s  Tmrw: %s",
                  wname(game.weather()), wname(game.tomorrow_weather()));
    r.drawText(x, y, buf, kSky, 1); y += kInfoStep;
  }
  y += 2;

  std::snprintf(buf, sizeof(buf), "Food  : %3u / 100", pet.stats.hunger);
  r.drawText(x, y, buf, kOrange, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Play  : %3u / 100", pet.stats.happiness);
  r.drawText(x, y, buf, kYellow, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Bath  : %3u / 100", pet.stats.cleanliness);
  r.drawText(x, y, buf, kBlue, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Rest  : %3u / 100", pet.stats.energy);
  r.drawText(x, y, buf, kGreen, 1); y += 12;
  // Round 6 Phase 6A: health + weight indicators.
  std::snprintf(buf, sizeof(buf), "Health: %3u / 100", (unsigned)game.health_stat());
  r.drawText(x, y, buf, kRed, 1); y += 12;
  {
    uint8_t w = game.pet_weight();
    const char* wl = w < 30 ? "Slim" : (w > 70 ? "Chubby" : "Normal");
    std::snprintf(buf, sizeof(buf), "Weight: %-6s (%3u)", wl, (unsigned)w);
    r.drawText(x, y, buf, kPink, 1); y += 12;
  }
  // Round 6 Phase 6D: exercise score (from today's walk steps).
  std::snprintf(buf, sizeof(buf), "Exer  : %3u / 100%s",
                (unsigned)game.exercise_stat(),
                game.auto_feeder_owned() ? "  AF" : "");
  r.drawText(x, y, buf, kSky, 1); y += 14;

  // Round 2: biscuits + active toy + treat counts
  std::snprintf(buf, sizeof(buf), "Biscuits: %u   Steps: %lu",
                (unsigned)game.biscuits(), (unsigned long)game.total_steps());
  r.drawText(x, y, buf, kYellow, 1); y += 10;
  std::snprintf(buf, sizeof(buf), "Toy:%s  Treats:%u/%u/%u",
                toy_name(game.active_toy()),
                game.treats(TreatTier::Biscuit),
                game.treats(TreatTier::Bacon),
                game.treats(TreatTier::Steak));
  r.drawText(x, y, buf, kWhite, 1); y += 14;

  // 7-day mood sparkline + one-line yesterday summary.
  r.drawText(x, y, "Mood (7d):", kGrayLight, 1);
  int sx = x + 60, sy = y - 1;
  for (int d = 6; d >= 0; --d) {
    uint8_t v = game.mood_history((uint8_t)d);
    int h = (v * 9) / 100;
    if (h < 1 && v > 0) h = 1;
    uint16_t col = v >= 70 ? kGreen : (v >= 40 ? kYellow : kRed);
    r.fillRect(sx + (6 - d) * 10, sy + (9 - h), 8, h, col);
    r.drawRect(sx + (6 - d) * 10, sy, 8, 10, kGrayDark);
  }
  y += 14;

  // Diary auto-templated from yesterday's mood. Round 6 Phase 6C
  // prefers the persisted diary entry (richer message bank); falls
  // back to the legacy mood-only summary if the diary is empty.
  uint8_t did = game.diary_entry(0);
  if (did != 0xFF && Game::diary_text(did) != nullptr) {
    std::snprintf(buf, sizeof(buf), "Yesterday: %s", Game::diary_text(did));
    r.drawText(x, y, buf, kGrayLight, 1);
  } else {
    uint8_t y1 = game.mood_history(0);
    if (y1 > 0) {
      const char* desc =
        y1 >= 80 ? "Great day with Bailey." :
        y1 >= 60 ? "Bailey had a good day." :
        y1 >= 40 ? "Bailey felt OK yesterday." :
        y1 >= 20 ? "Bailey seemed bored." :
                   "Bailey felt lonely.";
      std::snprintf(buf, sizeof(buf), "Yesterday: %s", desc);
      r.drawText(x, y, buf, kGrayLight, 1);
    }
  }
}

// Round 5 Phase C3: tier of an achievement (0..3 = none / bronze /
// silver / gold). Derived live from the underlying counter so the
// player's "tier ladder" updates without any save bump.
static uint8_t achievement_tier(const Game& game, AchievementId id) {
  if (!is_unlocked(game.achievements(), id)) return 0;
  // Default tier for unlocked-but-not-counter-based achievements: bronze.
  switch (id) {
    case AchievementId::Petted100: {
      uint64_t n = game.total_pets();
      return n >= 1000 ? 3 : (n >= 500 ? 2 : 1);
    }
    case AchievementId::FetchPro: {
      uint64_t n = game.fetch_catches();
      return n >= 100 ? 3 : (n >= 50 ? 2 : 1);
    }
    case AchievementId::WalkOfALifetime: {
      uint64_t n = game.total_steps();
      return n >= 1000 ? 3 : (n >= 500 ? 2 : 1);
    }
    case AchievementId::BiscuitTycoon: {
      uint32_t n = game.biscuits();
      return n >= 500 ? 3 : (n >= 200 ? 2 : 1);
    }
    case AchievementId::Streak7Days: {
      uint16_t n = game.streak_days();
      return n >= 30 ? 3 : (n >= 14 ? 2 : 1);
    }
    case AchievementId::MasterDigger: {
      uint16_t n = game.dig_successes();
      return n >= 100 ? 3 : (n >= 50 ? 2 : 1);
    }
    case AchievementId::HideAndSeekChamp: {
      uint16_t n = game.hide_seek_wins();
      return n >= 50 ? 3 : (n >= 20 ? 2 : 1);
    }
    case AchievementId::Goodnight: {
      uint16_t n = game.stories_heard();
      return n >= 100 ? 3 : (n >= 50 ? 2 : 1);
    }
    default: return 1;   // bronze for any unlocked one-shot.
  }
}

void draw_menu_achievements(Renderer& r, const Game& game) {
  int x0 = 22;
  int y0 = 14 + kStatsBarH + 22;
  int row = 0, col = 0;
  for (int i = 0; i < kAchievementCount; ++i) {
    AchievementId id = (AchievementId)i;
    bool unlocked = is_unlocked(game.achievements(), id);
    int x = x0 + col * 100;
    int y = y0 + row * 14;
    r.fillRect(x, y, 8, 8, unlocked ? kYellow : kGrayDark);
    r.drawRect(x, y, 8, 8, kGrayLight);
    r.drawText(x + 12, y, achievement_name(id),
               unlocked ? kWhite : kGray, 1);
    // Round 5: tier dots. Up to 3 small 3x3 squares next to the
    // unlocked badge: bronze / silver / gold based on the counter.
    uint8_t tier = unlocked ? achievement_tier(game, id) : 0;
    static const uint16_t kTierColors[3] = {
      rgb(180, 110, 60), kGrayLight, kYellow,  // bronze / silver / gold
    };
    for (int t = 0; t < 3; ++t) {
      uint16_t c = (t < tier) ? kTierColors[t] : kGrayDark;
      r.fillRect(x + 90 + t * 4, y + 2, 3, 3, c);
    }
    ++col;
    if (col >= 2) { col = 0; ++row; }
  }
}

void draw_menu_options(Renderer& r, const Game& game) {
  int x = 22;
  int y = 14 + kStatsBarH + 22;
  char buf[40];
  const auto& s = game.settings();

  std::snprintf(buf, sizeof(buf), "Volume    : %3u%%", s.volume);
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "Brightness: %3u", s.brightness);
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "Decay     : %u.%ux", s.decay_mult / 10, s.decay_mult % 10);
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "TZ offset : %d min", (int)s.tz_offset_min);
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "Auto-sleep: %s  (%02u-%02u)",
                s.auto_sleep ? "on" : "off",
                (unsigned)kBedtimeHour, (unsigned)kWakeHour);
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "Mic       : %s", s.mic_enabled ? "on" : "off");
  r.drawText(x, y, buf, kWhite, 1); y += 14;

  // Tricks learned, with the favorite starred
  uint8_t tl = game.tricks_learned();
  Trick fav = game.favorite_trick();
  bool any_perf = false;
  for (int i = 0; i < (int)Trick::COUNT; ++i) any_perf |= (game.trick_perf((Trick)i) > 0);
  r.drawText(x, y, "Tricks    :", kWhite, 1); y += 10;
  for (int i = 0; i < (int)Trick::COUNT; ++i) {
    bool got = (tl & (1u << i)) != 0;
    bool starred = got && any_perf && (Trick)i == fav;
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%s %s%s",
                  starred ? "*" : " ",
                  trick_name((Trick)i),
                  starred ? " (favorite)" : "");
    r.drawText(x + 12, y, buf, got ? kGreen : kGrayDark, 1);
    y += 10;
  }
  // Vocabulary
  uint8_t voc = game.vocab_learned();
  if (voc) {
    y += 4;
    r.drawText(x, y, "Words     :", kWhite, 1); y += 10;
    for (int i = 0; i < (int)Word::COUNT; ++i) {
      bool got = (voc & (1u << i)) != 0;
      r.drawText(x + 12, y, word_name((Word)i),
                 got ? kPink : kGrayDark, 1);
      y += 10;
    }
  }
}

void draw_menu_inventory(Renderer& r, const Game& game) {
  int x = 16;
  int y = 14 + kStatsBarH + 22;
  char buf[40];
  r.drawText(x, y, "INVENTORY", kYellow, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Biscuits: %u   Bones: %u",
                (unsigned)game.biscuits(), (unsigned)game.bones_collected());
  r.drawText(x, y, buf, kYellow, 1); y += 12;
  r.drawText(x, y, "Toys:", kWhite, 1); y += 10;
  for (int i = 0; i < (int)Toy::COUNT; ++i) {
    bool owned = (game.toys_owned() & (1u << i)) != 0;
    bool active = ((int)game.active_toy() == i);
    std::snprintf(buf, sizeof(buf), "  %s %s%s",
                  owned ? (active ? "*" : "+") : "-",
                  toy_name((Toy)i),
                  active ? " (active)" : "");
    r.drawText(x, y, buf, owned ? kWhite : kGrayDark, 1);
    y += 10;
  }
  y += 4;
  r.drawText(x, y, "Treats:", kWhite, 1); y += 10;
  for (int i = 0; i < (int)TreatTier::COUNT; ++i) {
    std::snprintf(buf, sizeof(buf), "  %s x%u", treat_name((TreatTier)i), game.treats((TreatTier)i));
    r.drawText(x, y, buf, kWhite, 1);
    y += 10;
  }
}

void draw_menu_shop(Renderer& r, const Game& game) {
  int x = 12;
  int y = 14 + kStatsBarH + 20;
  char buf[40];
  std::snprintf(buf, sizeof(buf), "SHOP   Biscuits: %u", (unsigned)game.biscuits());
  r.drawText(x, y, buf, kYellow, 1); y += 12;

  struct Row { const char* name; bool owned; uint32_t price; };
  // Catalog mirrors Game::buy_item indices.
  const Row rows[20] = {
    {"Ball",          (game.toys_owned() & 1)  != 0, game.shop_price(0)},
    {"Frisbee",       (game.toys_owned() & 2)  != 0, game.shop_price(1)},
    {"Rope",          (game.toys_owned() & 4)  != 0, game.shop_price(2)},
    {"Squeaky",       (game.toys_owned() & 8)  != 0, game.shop_price(3)},
    {"Stick",         (game.toys_owned() & 16) != 0, game.shop_price(4)},
    {"Red bandana",   game.accessory_unlocked(1),    game.shop_price(5)},
    {"Blue collar",   game.accessory_unlocked(2),    game.shop_price(6)},
    {"Party hat",     game.accessory_unlocked(3),    game.shop_price(7)},
    {"Biscuit treat", false,                          game.shop_price(8)},
    {"Bacon treat",   false,                          game.shop_price(9)},
    {"Steak treat",   false,                          game.shop_price(10)},
    {"Tan +feed",     game.coat_pattern() == 1,      game.shop_price(11)},
    {"Brindle +nrg",  game.coat_pattern() == 2,      game.shop_price(12)},
    {"Tri +cuddle",   game.coat_pattern() == 3,      game.shop_price(13)},
    {"Black +bones",  game.coat_pattern() == 4,      game.shop_price(14)},
    {"Trade 5 bones", false,                          0},   // priced in bones
    {"Rubber duck",   (game.bath_toys_owned() & 1) != 0, game.shop_price(16)},
    {"Toy boat",      (game.bath_toys_owned() & 2) != 0, game.shop_price(17)},
    {"Toy fish",      (game.bath_toys_owned() & 4) != 0, game.shop_price(18)},
    {"Auto-feeder",   game.auto_feeder_owned(),         game.shop_price(19)},
  };
  // Show 6 rows around the cursor
  uint8_t cur = game.shop_cursor();
  int start = (cur > 2) ? (cur - 2) : 0;
  if (start > 14) start = 14;
  for (int i = 0; i < 6; ++i) {
    int idx = start + i;
    if (idx >= 20) break;
    bool sel = idx == cur;
    if (sel) r.fillRect(x - 2, y - 1, kScreenW - 28, 10, kGrayDark);
    if (idx == 15) {
      // Trade-bones row: priced in bones (5 -> 1 biscuit).
      std::snprintf(buf, sizeof(buf), "%c %s  5 bones",
                    sel ? '>' : ' ', rows[idx].name);
      uint16_t fg = game.bones_collected() >= 5 ? kWhite : kGrayDark;
      r.drawText(x, y, buf, fg, 1);
    } else {
      std::snprintf(buf, sizeof(buf), "%c %s  %ub",
                    sel ? '>' : ' ', rows[idx].name, (unsigned)rows[idx].price);
      uint16_t fg = rows[idx].owned ? kGreen
                                    : (game.biscuits() >= rows[idx].price ? kWhite : kGrayDark);
      r.drawText(x, y, buf, fg, 1);
    }
    y += 10;
  }
  y += 4;
  r.drawText(x, y, "A=buy  B=next  C=tab", kGray, 1);
}

void draw_menu_actions(Renderer& r, const Game& game) {
  static const char* const kMain[10] = {
    "Play with a friend >", "Tricks >",
    "Change scene", "Change hat",
    "Go for a walk", "Play fetch", "Give treat", "Brush",
    "Switch toy", "Bedtime",
  };
  static const char* const kTricks[6] = {
    "Sit", "Come", "High five", "Roll over", "Jump", "< Back",
  };
  static const char* const kFriends[10] = {
    "Random", "Ollie", "Mitchell", "Enzo", "Lincoln",
    "Ruben", "Francie", "Bomi", "Noshy", "< Back",
  };
  uint8_t sub = game.actions_submenu();
  const char* const* rows;
  int                n_rows;
  const char*        header;
  if (sub == 1) {
    rows = kTricks;  n_rows = 6;  header = "TRICKS";
  } else if (sub == 2) {
    rows = kFriends; n_rows = 10; header = "VISIT FRIENDS";
  } else {
    rows = kMain;    n_rows = 10; header = "ACTIONS";
  }

  int x = 14;
  int y = 14 + kStatsBarH + 20;
  r.drawText(x, y, header, kYellow, 1); y += 12;
  uint8_t cur = game.actions_cursor() % n_rows;
  int start  = (cur > 4) ? (cur - 4) : 0;
  if (start > n_rows - 8) start = (n_rows > 8) ? (n_rows - 8) : 0;
  for (int i = 0; i < 8; ++i) {
    int idx = start + i;
    if (idx >= n_rows) break;
    bool sel = (idx == cur);
    if (sel) r.fillRect(x - 2, y - 1, kScreenW - 28, 10, kGrayDark);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%c %s", sel ? '>' : ' ', rows[idx]);
    r.drawText(x, y, buf, sel ? kYellow : kWhite, 1);
    y += 10;
  }
  y += 4;
  r.drawText(x, y, "A=do  B=next  C=tab", kGray, 1);
}

void draw_menu_sync(Renderer& r, const Game& game_const) {
  // generate_sync_code mutates internal buffer; we need a mutable ref.
  auto& game = const_cast<Game&>(game_const);
  const char* code = game.generate_sync_code();
  int x = 22;
  int y = 14 + kStatsBarH + 22;
  r.drawText(x, y, "SHARE YOUR BAILEY", kYellow, 1); y += 14;
  r.drawText(x, y, "Type this code or scan", kWhite, 1); y += 10;
  r.drawText(x, y, "the pattern:", kWhite, 1); y += 14;
  // Code (medium)
  int tw = text_width(code, 1);
  r.drawText((kScreenW - tw) / 2, y, code, kYellow, 1);
  y += 12;
  // Round 5 Phase D remainder: QR-style display. Render a 12x12 grid
  // of black/white cells derived from a deterministic hash of the
  // sync code -- not a real QR (would need the full Reed-Solomon
  // encoder) but visually distinctive and stable per save state so
  // someone could in principle photograph it and we'd hash-match.
  {
    constexpr int kCells = 12;
    constexpr int kCellSize = 8;
    constexpr int kQuiet = 4;
    int qx = (kScreenW - kCells * kCellSize) / 2;
    int qy = y;
    // White quiet zone background.
    r.fillRect(qx - kQuiet, qy - kQuiet,
               kCells * kCellSize + 2 * kQuiet,
               kCells * kCellSize + 2 * kQuiet, kWhite);
    // Hash each cell from the sync code.
    uint32_t h = 0x9E3779B9u;
    for (const char* p = code; *p; ++p) h = (h ^ (uint8_t)*p) * 2654435761u;
    for (int row = 0; row < kCells; ++row) {
      for (int col = 0; col < kCells; ++col) {
        // Corner finder squares (top-left, top-right, bottom-left) like a real QR.
        bool finder = (row < 3 && col < 3) ||
                      (row < 3 && col >= kCells - 3) ||
                      (row >= kCells - 3 && col < 3);
        bool on;
        if (finder) {
          // Solid 3x3 with 1-px white ring at the middle row/col edges.
          int br = (row < 3) ? row : (kCells - 1 - row);
          int bc = (col < 3) ? col : (kCells - 1 - col);
          if (col >= kCells - 3) bc = (kCells - 1 - col);
          on = (br == 0 || br == 2) || (bc == 0 || bc == 2);
        } else {
          // Pseudo-random cell colored by the rotated hash.
          uint32_t bit = (h >> ((row * kCells + col) & 31)) & 1;
          on = (bit != 0);
        }
        if (on) {
          r.fillRect(qx + col * kCellSize, qy + row * kCellSize,
                     kCellSize, kCellSize, kBlack);
        }
      }
    }
  }
}

}  // namespace

bool point_on_pet(int x, int y) {
  return x >= kPetX && x < kPetX + kPetDrawW &&
         y >= kPetY && y < kPetY + kPetDrawH;
}
bool point_on_stats_bar(int /*x*/, int y) {
  return y >= 0 && y < kStatsBarH;
}

// Render fetch state -- the ball arc + a catch-window prompt.
void draw_fetch(Renderer& r, const Game& game, uint32_t now_ms) {
  GameMode m = game.mode();
  (void)m;
  // We use a free flag via local time-since
  uint32_t elapsed = now_ms - game.fetch_state_ms();
  if (m == GameMode::FetchAiming) {
    r.drawText(kStatsBarH + 4, kStatsBarH + 4, "Aim...", kYellow, 1);
    return;
  }
  if (m == GameMode::FetchInFlight) {
    // Ball arcing right-to-far-right; show as moving green circle
    float t = (float)elapsed / 1000.0f;
    if (t > 1.0f) t = 1.0f;
    int x = (int)(kPetX + kPetDrawW + t * 80);
    int y = (int)(kPetY + kPetDrawH - 20 - 60 * (1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f)));
    r.fillRect(x, y, 6, 6, kGreen);
    r.drawRect(x - 1, y - 1, 8, 8, kBlack);
    return;
  }
  if (m == GameMode::FetchCatching) {
    // Ball flying back, prompt to press B
    float t = (float)elapsed / 500.0f;
    if (t > 1.0f) t = 1.0f;
    int x = (int)(kPetX + kPetDrawW + 80 - t * 100);
    int y = (int)(kPetY + kPetDrawH - 30 - 30 * (1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f)));
    r.fillRect(x, y, 6, 6, kGreen);
    r.drawRect(x - 1, y - 1, 8, 8, kBlack);
    r.drawText(kStatsBarH + 4, kStatsBarH + 4, "PRESS B!", kYellow, 2);
    return;
  }
  if (m == GameMode::FetchResult) {
    bool hit = game.fetch_catches() > 0 && elapsed < 900;
    (void)hit;
    // For result, indicate caught or missed by checking last action
    r.drawText(kStatsBarH + 4, kStatsBarH + 4,
               game.pet().current_action == Action::Play ? "GOT IT!" : "missed",
               game.pet().current_action == Action::Play ? kGreen : kOrange, 2);
  }
}

void draw_coat_picker(Renderer& r, const Game& game) {
  int pad = 12;
  r.fillRect(pad, pad + kStatsBarH, kScreenW - pad * 2, 120, kBlack);
  r.drawRect(pad, pad + kStatsBarH, kScreenW - pad * 2, 120, kYellow);
  r.drawText(pad + 6, kStatsBarH + pad + 4, "Pick Bailey's coat!", kYellow, 1);

  const char* names[5] = {"Tan", "Brindle", "Tri", "Red", "Black & Tan"};
  uint16_t swatches[5] = {kBrown, kBrownDark, rgb(40,25,10), rgb(190,70,40), rgb(20,15,10)};
  for (int i = 0; i < 5; ++i) {
    int x = pad + 6 + (i % 3) * 70;
    int y = kStatsBarH + pad + 22 + (i / 3) * 36;
    bool active = (game.coat_pattern() == i);
    r.fillRect(x, y, 16, 16, swatches[i]);
    r.drawRect(x, y, 16, 16, active ? kYellow : kGrayLight);
    r.drawText(x + 20, y + 4, names[i], kWhite, 1);
  }
  r.drawText(pad + 6, kStatsBarH + pad + 100,
             "B = next, hold to confirm", kGray, 1);
}

void draw_sickness_overlay(Renderer& r) {
  // Subtle red border + sneeze marker
  r.drawRect(0, kStatsBarH, kScreenW, kScreenH - kStatsBarH - kStatusH, kRed);
  r.drawRect(1, kStatsBarH + 1, kScreenW - 2, kScreenH - kStatsBarH - kStatusH - 2, kRed);
  r.drawText(kScreenW - 60, kStatsBarH + 6, "SICK!", kRed, 1);
}

// Wish / dream / thought bubble centered horizontally above Bailey.
static void draw_thought_bubble(Renderer& r, const char* text, uint16_t color) {
  int tw = text_width(text, 1) + 8;
  int bx = kPetX + (kPetDrawW - tw) / 2;
  int by = kPetY - 16;
  // Bubble body
  r.fillRect(bx, by, tw, 12, kWhite);
  r.drawRect(bx, by, tw, 12, kGrayDark);
  r.fillRect(bx + tw / 2 - 2, by + 12, 4, 3, kWhite);
  r.fillRect(bx + tw / 2,     by + 15, 2, 2, kWhite);
  r.drawText(bx + 4, by + 2, text, color, 1);
}

// Confetti dots scattered across the upper half (deterministic per now_ms
// window so each frame looks different but stable per frame).
static void draw_confetti(Renderer& r, uint32_t now_ms) {
  uint32_t seed = 0xC0FFEE;
  uint16_t cols[5] = {kRed, kYellow, kGreen, kBlue, kPink};
  for (int i = 0; i < 30; ++i) {
    seed = seed * 1664525u + 1013904223u + now_ms / 200;
    int x = (int)((seed >> 4) % kScreenW);
    int y = (int)(((seed >> 12) % (kScreenH - kStatusH - kStatsBarH)) + kStatsBarH);
    uint16_t c = cols[(seed >> 20) % 5];
    r.fillRect(x, y, 2, 3, c);
  }
}

// Walking overlay: Bailey's silhouette is drawn at the normal position but
// we render a small progress + step counter.
static void draw_walk_progress(Renderer& r, const Game& game) {
  // Progress bar at top of the play area.
  int x0 = 20, y0 = kStatsBarH + 4, w = kScreenW - 40;
  r.fillRect(x0, y0, w, 5, kBlack);
  r.drawRect(x0 - 1, y0 - 1, w + 2, 7, kGrayLight);
  int fill = w * game.walk_steps() / 20;
  if (fill > 0) r.fillRect(x0, y0, fill, 5, kGreen);
  char buf[40];
  std::snprintf(buf, sizeof(buf), "WALK %u/20  B=step C=stop", game.walk_steps());
  r.drawText((kScreenW - text_width(buf, 1)) / 2, y0 + 10, buf, kYellow, 1);
  std::snprintf(buf, sizeof(buf), "today: %u steps  bones: %u",
                (unsigned)game.walk_today_steps(),
                (unsigned)game.bones_collected());
  r.drawText((kScreenW - text_width(buf, 1)) / 2, y0 + 22, buf, kGrayLight, 1);
  // Dig prompt overlay: shown for the prompt's 1.5 s window.
  if (game.dig_prompt_active()) {
    const char* msg = "DIG!  [A]";
    int tx = (kScreenW - text_width(msg, 2)) / 2;
    r.drawText(tx, y0 + 36, msg, kRed, 2);
  }
  // Transient "FOUND!" popup for the last 1500 ms after a walk item-find.
  uint32_t since = game.last_tick_ms() - game.last_walk_find_ms();
  if (game.last_walk_find_kind() != 0 && since < 1500 && !game.dig_prompt_active()) {
    const char* what = game.last_walk_find_kind() == 1 ? "bone"
                     : game.last_walk_find_kind() == 2 ? "new toy"
                     :                                    "treat";
    std::snprintf(buf, sizeof(buf), "FOUND: %s!", what);
    int tx = (kScreenW - text_width(buf, 2)) / 2;
    r.drawText(tx, y0 + 36, buf, kGreen, 2);
  }
}

void draw_scene(Renderer& r, const Game& game, uint32_t now_ms) {
  const Pet& pet = game.pet();
  float daylight = game.daylight();

  draw_background(r, daylight);
  draw_scene_detail(r, game.settings().scene_id, daylight);

  // Round 5 Phase A2: bedroom wall poster (only in scene 4). One of 4
  // designs cycled via game.cycle_wall_poster(). Drawn above the bed.
  if (game.settings().scene_id == 4) {
    int px = 150, py = kStatsBarH + 14;
    int pw = 60, ph = 36;
    r.fillRect(px, py, pw, ph, mix(rgb(180, 160, 220), kWhite, daylight));
    r.drawRect(px - 1, py - 1, pw + 2, ph + 2, mix(rgb(60, 40, 20), kBrownDark, daylight));
    switch (game.wall_poster()) {
      case 0: {  // Pet portrait (yellow circle = stand-in for Bailey)
        r.fillRect(px + 22, py + 6, 16, 14, kYellow);
        r.fillRect(px + 26, py + 22, 8, 6, kYellow);
        r.drawText(px + 14, py + ph - 9, "GOOD DOG", kBrownDark, 1);
        break;
      }
      case 1: {  // Paw print
        r.fillRect(px + 26, py + 10, 8, 10, mix(rgb(60, 40, 20), kBrownDark, daylight));
        r.fillRect(px + 20, py +  6, 4, 4, mix(rgb(60, 40, 20), kBrownDark, daylight));
        r.fillRect(px + 28, py +  4, 4, 4, mix(rgb(60, 40, 20), kBrownDark, daylight));
        r.fillRect(px + 36, py +  6, 4, 4, mix(rgb(60, 40, 20), kBrownDark, daylight));
        break;
      }
      case 2: {
        r.drawText(px + 8, py + 12, "I LOVE", kRed, 1);
        r.drawText(px + 14, py + 22, "DOGS", kRed, 2);
        break;
      }
      case 3:
      default: {
        r.drawText(px + 8, py + 12, "ADVENTURE", kBlue, 1);
        r.drawText(px + 14, py + 22, "AWAITS!", kBlue, 1);
        break;
      }
    }
    // Round 5 Phase A remainder: pet bed type swap (top of the
    // existing blanket position). Draws a colored 22x8 strip just
    // below the bed sprite based on the player's choice.
    int floor_y = kPetY + kPetDrawH - 6;
    int bx = 90, by = floor_y - 8;
    switch (game.bed_type()) {
      case 0: {  // basket -- woven brown rim
        r.fillRect(bx, by + 2, 36, 6, mix(rgb(80, 50, 25), rgb(180, 130, 70), daylight));
        r.drawRect(bx, by + 2, 36, 6, kBrownDark);
        // weave hatching
        for (int dx = 2; dx < 36; dx += 4)
          r.drawPixel(bx + dx, by + 4, kBrownDark);
        break;
      }
      case 1: {  // kennel pad -- gray rectangle
        r.fillRect(bx, by, 36, 8, mix(rgb(70, 70, 80), kGrayLight, daylight));
        r.drawRect(bx, by, 36, 8, kGrayDark);
        break;
      }
      case 2: {  // blanket pile -- soft pink mound
        r.fillRect(bx + 2, by + 2, 32, 6, mix(rgb(150, 80, 110), kPink, daylight));
        // top tuft
        r.fillRect(bx + 12, by, 12, 3, mix(rgb(160, 100, 130), kPink, daylight));
        break;
      }
    }
    // Food bowl in the corner of the bedroom (next to the night-
    // stand). Color cycled by bowl_color: blue / red / silver.
    {
      uint16_t bowl_outer, bowl_inner;
      switch (game.bowl_color()) {
        case 1:  bowl_outer = kRed;       bowl_inner = mix(rgb(70, 0, 0), kRed, daylight); break;
        case 2:  bowl_outer = kGrayLight; bowl_inner = mix(rgb(40, 40, 40), kGrayLight, daylight); break;
        case 0:
        default: bowl_outer = kBlue;      bowl_inner = mix(rgb(0, 0, 80), kBlue, daylight); break;
      }
      int bwx = 38, bwy = floor_y - 6;
      r.fillRect(bwx, bwy + 1, 14, 4, bowl_outer);   // bowl body
      r.fillRect(bwx + 2, bwy + 4, 10, 1, bowl_inner);
      r.drawPixel(bwx - 1, bwy + 2, bowl_outer);
      r.drawPixel(bwx + 14, bwy + 2, bowl_outer);
      // brown kibble dot
      r.fillRect(bwx + 5, bwy + 1, 4, 2, kBrownDark);
    }
  }

  r.fillRect(0, 0, kScreenW, kStatsBarH, kGrayDark);
  r.drawHLine(0, kStatsBarH, kScreenW, kGrayLight);
  draw_stats_bar(r, pet, game.clock_string());
  draw_achievement_showcase(r, game);
  // Round 5 Phase A2: sticker badges on the right edge under the stats
  // bar. Up to 5 unlocked stickers as 6x6 colored dots.
  {
    uint8_t mask = game.stickers_unlocked();
    static const uint16_t kColors[5] = {kBrownDark, kYellow, kWhite, kRed, kOrange};
    int sy = kStatsBarH + 4;
    for (int i = 0; i < 5; ++i) {
      if ((mask >> i) & 1) {
        int sx = kScreenW - 10;
        r.fillRect(sx, sy + i * 8, 6, 6, kColors[i]);
        r.drawRect(sx, sy + i * 8, 6, 6, kGrayDark);
      }
    }
  }

  // Draw any visiting friends FIRST so Bailey draws on top of them --
  // his silhouette (especially the left-side hind leg) stays whole even
  // when slot 0 parks against the left edge and overlaps him.
  struct VisitorRender {
    bool     active;
    Friend   f;
    int      fx, fy;
    uint32_t elapsed;
    int      slot;
  };
  VisitorRender vis[kMaxVisitors] = {};
  for (int slot = 0; slot < kMaxVisitors; ++slot) {
    vis[slot].active = false;
    vis[slot].slot   = slot;
    uint8_t kind = game.npc_visit_kind(slot);
    if (kind == 0) continue;
    uint32_t elapsed = now_ms - game.npc_visit_ms(slot);
    if (elapsed >= kFriendVisitMs) continue;
    bool mystery = (kind == Game::kMysteryVisitorKind);
    Friend f = mystery ? Friend::Ollie
                       : (Friend)((kind - 1) % (int)Friend::COUNT);
    int parked_x = (slot == 0) ? 4 : (kScreenW - kPetDrawW - 4);
    int off_x    = (slot == 0) ? -kPetDrawW : kScreenW;
    int fx;
    if (elapsed < 800) {
      float u = (float)elapsed / 800.0f;
      fx = (int)(off_x + (parked_x - off_x) * u * (2.0f - u));
    } else if (elapsed > kFriendVisitMs - 800) {
      float u = (float)(kFriendVisitMs - elapsed) / 800.0f;
      fx = (int)(off_x + (parked_x - off_x) * u * (2.0f - u));
    } else {
      fx = parked_x;
    }
    int fy = kPetY + (int)((1.0f - friend_size_scale(f)) * kPetDrawH);
    if (mystery) {
      // Mystery visitor: solid black silhouette + question mark in
      // its position instead of a named friend sprite.
      r.fillRect(fx + 8, fy + 12, kPetDrawW - 16, kPetDrawH - 18, kBlack);
      r.fillRect(fx + 18, fy + 4, kPetDrawW - 36, 16, kBlack);
      r.drawText(fx + kPetDrawW / 2 - 3, fy + 10, "?", kWhite, 2);
    } else {
      PetPose fpose = ((now_ms / 350 + slot) & 1) ? PetPose::IdleB : PetPose::IdleA;
      r.drawSprite(fx, fy, kPetW, kPetH,
                   friend_sprite(f, fpose), kSpritePalette, kPetScale);
    }
    vis[slot] = {true, f, fx, fy, elapsed, slot};
  }

  draw_pet_sprite(r, pet, now_ms, game);
  if (pet.mood != Mood::MovingOut) {
    draw_coat_accents(r, game.coat_pattern());
    draw_accessory_overlay(r, game.accessory_id(),
                           game.accessory_id() == 2 ? game.collar_engraving() : nullptr);
  }
  draw_weather(r, game, now_ms);

  // Now draw the visitor labels + hearts ON TOP of Bailey so the name
  // strip is still readable when there's overlap.
  bool any_visitor = false;
  for (int slot = 0; slot < kMaxVisitors; ++slot) {
    if (!vis[slot].active) continue;
    any_visitor = true;
    if (vis[slot].elapsed < 2000) {
      bool mystery = (game.npc_visit_kind(slot) == Game::kMysteryVisitorKind);
      const char* name = mystery ? "???" : friend_name(vis[slot].f);
      int tw = text_width(name, 1);
      int lx = vis[slot].fx + (kPetDrawW - tw) / 2;
      if (lx < 2) lx = 2;
      if (lx + tw + 4 > kScreenW) lx = kScreenW - tw - 4;
      r.fillRect(lx - 2, vis[slot].fy - 12, tw + 4, 10, kBlack);
      r.drawText(lx, vis[slot].fy - 11, name, kYellow, 1);
    }
    if (((now_ms / 600 + slot) & 1) &&
        vis[slot].elapsed > 500 && vis[slot].elapsed < kFriendVisitMs - 500) {
      int hx = (slot == 0) ? ((vis[slot].fx + kPetDrawW + kPetX) / 2)
                           : ((vis[slot].fx + kPetX + kPetDrawW) / 2);
      int hy = kPetY + 18 - (int)((now_ms / 30) % 24);
      r.fillRect(hx,     hy,     3, 2, kHeartRed);
      r.fillRect(hx - 1, hy + 1, 5, 2, kHeartRed);
      r.fillRect(hx,     hy + 3, 3, 1, kHeartRed);
    }
  }
  if (any_visitor) {
    r.drawText(8, kStatsBarH + 18, "Press to greet!", kYellow, 1);
  }

  // Round 3: water bowl rendered at Bailey's destination during the
  // scenery-interact ambient behavior. Small blue oval with rim.
  if (game.ambient_behavior() == 7) {
    int bowl_x = kPetX + 50 + 8;       // matches target +50 offset
    int bowl_y = kPetY + kPetDrawH - 8;
    r.fillRect(bowl_x,     bowl_y,     14, 4, kBlue);     // water
    r.drawRect(bowl_x - 1, bowl_y - 1, 16, 6, kGrayLight); // rim
  }

  // Round 3: bath-toy floater during the Clean action. Bobs gently up
  // and down beside Bailey's head while the bath choreography runs.
  if (pet.current_action == Action::Clean && game.bath_toy_active() != 0) {
    int bx = kPetX + kPetDrawW + 4;
    int by = kPetY + 16 + (int)((now_ms / 80) % 6);
    switch (game.bath_toy_active()) {
      case 1: // rubber duck: yellow body + orange beak + black eye dot
        r.fillRect(bx,     by,     8, 6, kYellow);
        r.fillRect(bx + 2, by - 3, 5, 4, kYellow);   // head
        r.fillRect(bx + 5, by - 2, 3, 2, kOrange);   // beak
        r.fillRect(bx + 4, by - 2, 1, 1, kBlack);    // eye
        break;
      case 2: // toy boat: brown hull + white sail
        r.fillRect(bx,     by + 1, 10, 4, kBrownDark);
        r.fillRect(bx + 4, by - 5, 1, 7, kBrownDark);   // mast
        r.fillRect(bx + 5, by - 5, 4, 5, kWhite);       // sail
        break;
      case 3: // toy fish: pink body + tail fork
        r.fillRect(bx,     by,     8, 4, kPink);
        r.fillRect(bx + 8, by - 1, 2, 2, kPink);        // upper fin
        r.fillRect(bx + 8, by + 3, 2, 2, kPink);        // lower fin
        r.fillRect(bx + 2, by + 1, 1, 1, kBlack);       // eye
        break;
    }
  }

  // Birthday confetti goes UNDER the footer overlay so the message still reads.
  // Round 5 Phase B: firefly spawn. Tiny pulsing yellow dot at the
  // game's recorded x/y for the 3 s the bug is in the air.
  if (game.firefly_active()) {
    int fx = game.firefly_x();
    int fy = game.firefly_y();
    int  bob = (int)((now_ms / 80) % 6) - 3;   // -3..+2 px drift
    uint16_t col = ((now_ms / 120) & 1) ? kYellow : kOrange;
    r.fillRect(fx + bob, fy, 3, 3, col);
    // Soft glow halo
    r.drawPixel(fx + bob - 1, fy + 1, col);
    r.drawPixel(fx + bob + 3, fy + 1, col);
  }

  if (game.is_birthday()) draw_confetti(r, now_ms);
  // Round 5 Phase D2: 1 s gold-frame flash after a Take-Photo press.
  if (game.photo_flash_active()) {
    int y0 = kStatsBarH + 1;
    int yh = kScreenH - kStatsBarH - kStatusH - 1;
    // 4 px gold border around the play area.
    for (int b = 0; b < 4; ++b) {
      r.drawRect(b, y0 + b, kScreenW - b * 2, yh - b * 2, kYellow);
    }
  }
  // Round 5 Phase D1: New Year fireworks -- 3 colored bursts radiating
  // from random points across the upper half during the 5 s window
  // after the Jan 1 roll-over.
  if (game.fireworks_active()) {
    static const uint16_t burst_cols[3] = {kRed, kYellow, kBlue};
    for (int b = 0; b < 3; ++b) {
      uint32_t seed = (now_ms / 100 + b * 7919u) * 2654435761u;
      int bcx = 30 + (int)((seed >> 8) % (kScreenW - 60));
      int bcy = kStatsBarH + 10 + (int)((seed >> 16) % 60);
      // Radiating dots.
      for (int k = 0; k < 12; ++k) {
        float ang = (float)k * 0.523598f;     // 30 deg increments
        // Cheap sin/cos approximation via small table.
        static const int8_t cs[12] = { 10, 9, 5, 0,-5,-9,-10,-9,-5, 0, 5, 9};
        static const int8_t sn[12] = {  0, 5, 9,10, 9, 5,  0,-5,-9,-10,-9,-5};
        (void)ang;
        int dx = (int)(cs[k]);
        int dy = (int)(sn[k]);
        r.fillRect(bcx + dx, bcy + dy, 2, 2, burst_cols[b]);
      }
      r.fillRect(bcx - 1, bcy - 1, 3, 3, burst_cols[b]);
    }
  }

  // Round 4 Phase 2B: weather/mood overlays painted ON TOP of Bailey
  // and any visitor labels.
  {
    int head_x = kPetX + kPetDrawW / 2;
    int head_y = kPetY - 6;
    Weather w = game.weather();

    // Pet umbrella during Rain (always visible when Rain active).
    if (w == Weather::Rain) {
      // Yellow dome (semicircle) + small pole.
      for (int dx = -10; dx <= 10; ++dx) {
        int h = (int)(8.0f * (1.0f - (dx * dx) / 100.0f));
        for (int dy = 0; dy < h; ++dy)
          r.drawPixel(head_x + dx, head_y - dy - 2, kYellow);
      }
      // Rim outline.
      r.drawHLine(head_x - 10, head_y - 1, 21, kOrange);
      // Pole.
      for (int dy = 0; dy < 5; ++dy)
        r.drawPixel(head_x, head_y + dy, kBrownDark);
    }

    // Snowflake on the nose every ~10 s for 1 s during Snow.
    if (w == Weather::Snow) {
      uint32_t phase = (now_ms / 1000) % 10;
      if (phase == 0) {
        int nx = kPetX + kPetDrawW / 2 - 8;     // a little left of center for Bailey's snout
        int ny = kPetY + kPetDrawH / 2 - 4;
        r.fillRect(nx, ny, 2, 2, kWhite);
        r.drawPixel(nx - 1, ny, kSkyDeep);
        r.drawPixel(nx + 2, ny + 1, kSkyDeep);
      }
    }

    // Mood emoji bubble: every 5 s, hover above the head for 1 s.
    if (pet.current_action == Action::None &&
        ((now_ms / 1000) % 5) == 0) {
      uint16_t col = kWhite;
      const char* glyph = "";
      switch (pet.mood) {
        case Mood::Happy:    col = kHeartRed; glyph = "*"; break;  // heart proxy
        case Mood::Sleeping: col = kGrayLight; glyph = "Z"; break;
        case Mood::Hungry:   col = kOrange; glyph = "!"; break;
        case Mood::Dirty:    col = kBrownDark; glyph = "~"; break;
        case Mood::Sad:      col = kBlue; glyph = "."; break;
        default: break;
      }
      if (glyph[0]) {
        // Small bubble background.
        r.fillRect(head_x + 6, head_y - 12, 10, 9, kWhite);
        r.drawRect(head_x + 6, head_y - 12, 10, 9, kGrayDark);
        r.drawText(head_x + 9, head_y - 10, glyph, col, 1);
      }
    }
  }

  // Holiday-specific decor: Halloween pumpkin, Christmas wreath / lights.
  if (game.active_holiday() == 2) {
    // Halloween pumpkin in bottom-left
    r.fillRect(12, kScreenH - kStatusH - 18, 14, 12, kOrange);
    r.drawRect(12, kScreenH - kStatusH - 18, 14, 12, kRed);
    r.fillRect(17, kScreenH - kStatusH - 22, 4, 4, kGreen);
    // jack-o-lantern eyes
    r.fillRect(15, kScreenH - kStatusH - 14, 2, 2, kBlack);
    r.fillRect(21, kScreenH - kStatusH - 14, 2, 2, kBlack);
  } else if (game.active_holiday() == 3) {
    // Christmas: string of lights along top of stats bar
    uint16_t cols[5] = {kRed, kGreen, kYellow, kBlue, kPink};
    for (int x = 4; x < kScreenW; x += 8) {
      r.fillRect(x, kStatsBarH + 1, 3, 3, cols[(x / 8) % 5]);
    }
  } else if (game.active_holiday() == 8) {
    // Round 6 Phase 6C: Cherry Blossom Day -- pink petals drift down.
    // Petal positions are time-shifted so they appear to fall.
    uint32_t t = now_ms / 80;
    for (int k = 0; k < 14; ++k) {
      int seed = k * 2654435761u;
      int px = ((seed >> 8) & 0xFF) % kScreenW;
      int py = ((seed >> 4) + (int)t) % (kScreenH - kStatsBarH - 30);
      r.fillRect(px, kStatsBarH + py, 3, 3, kPink);
    }
  } else if (game.active_holiday() == 9) {
    // Round 6 Phase 6F: Day of Dogs -- paw-print confetti.
    uint32_t t = now_ms / 100;
    for (int k = 0; k < 10; ++k) {
      int seed = k * 2654435761u;
      int px = ((seed >> 8) + (int)t) % kScreenW;
      int py = ((seed >> 12) & 0x7F) + kStatsBarH + 8;
      r.fillRect(px, py, 4, 4, kYellow);
      r.fillRect(px + 1, py - 2, 2, 2, kYellow);
    }
  }
  // Round 6 Phase 6F: birthday cake animation (simple 3-stage stack
  // drawn near the bottom of the playfield; UI marks it seen below
  // so it self-clears within a few seconds).
  if (game.birthday_cake_pending()) {
    int cx = kScreenW / 2;
    int by = kScreenH - kStatusH - 20;
    // Phase progresses with now_ms: 0..1500 stage 1, 1500..3000 stage 2,
    // 3000+ stage 3 (candle lit), then mark seen.
    uint32_t phase = (now_ms / 1500) % 3;
    // Plate
    r.fillRect(cx - 14, by + 8, 28, 2, kGrayLight);
    // Bottom layer (always).
    r.fillRect(cx - 12, by, 24, 8, kPink);
    if (phase >= 1) {
      // Middle layer
      r.fillRect(cx - 8, by - 6, 16, 6, kYellow);
    }
    if (phase >= 2) {
      // Candle + flame
      r.fillRect(cx - 1, by - 12, 2, 6, kWhite);
      r.fillRect(cx - 1, by - 14, 2, 2, kOrange);
    }
  }

  draw_footer(r, game);

  // Round 4: hardware init status chip in the bottom-left. Gated
  // behind BAILEY_DEBUG_HW_HUD so the default build hides it; flip
  // `-D BAILEY_DEBUG_HW_HUD=1` in platformio.ini build_flags to
  // re-enable for troubleshooting.
#ifndef BAILEY_DEBUG_HW_HUD
#define BAILEY_DEBUG_HW_HUD 0
#endif
#if BAILEY_DEBUG_HW_HUD
  if (game.hw_imu_status() != Game::HwUnknown ||
      game.hw_audio_status() != Game::HwUnknown) {
    auto status_str = [](uint8_t s) -> const char* {
      return s == Game::HwOk ? "OK" : s == Game::HwFail ? "FAIL" : "--";
    };
    auto status_col = [](uint8_t s) -> uint16_t {
      return s == Game::HwOk ? kGreen : s == Game::HwFail ? kRed : kGrayLight;
    };
    int x = 2;
    int y = kScreenH - kStatusH - 10;
    r.drawText(x,      y, "IMU:", kGrayLight, 1);
    r.drawText(x + 24, y, status_str(game.hw_imu_status()),
               status_col(game.hw_imu_status()), 1);
    int gap = (game.hw_imu_status() == Game::HwFail) ? 48 : 40;
    r.drawText(x + gap,      y, "AUD:", kGrayLight, 1);
    r.drawText(x + gap + 24, y, status_str(game.hw_audio_status()),
               status_col(game.hw_audio_status()), 1);
  }
#endif

  // Mode-specific overlays
  if (game.mode() == GameMode::Walking)
    draw_walk_progress(r, game);
  else if (game.mode() != GameMode::Idle && game.mode() != GameMode::PickingCoat)
    draw_fetch(r, game, now_ms);
  if (game.is_sick())
    draw_sickness_overlay(r);
  if (game.mode() == GameMode::PickingCoat)
    draw_coat_picker(r, game);

  // Wish / dream bubble (does not show during fetch / walk / sick / menu).
  if (game.mode() == GameMode::Idle && !game.is_sick()) {
    if (pet.mood == Mood::Sleeping) {
      const char* dreams[] = {"...bone", "...ball", "...treat", "...you"};
      draw_thought_bubble(r, dreams[(now_ms / 1500) % 4], kSkyDeep);
    } else if (game.current_wish() != Wish::None) {
      draw_thought_bubble(r, wish_name(game.current_wish()), kPink);
    } else if (game.is_birthday()) {
      draw_thought_bubble(r, "Birthday!", kHeartRed);
    }
  }
}

void draw_menu_overlay(Renderer& r, const Game& game) {
  const Pet& pet = game.pet();
  int pad = 8;
  r.fillRect(pad, pad + kStatsBarH, kScreenW - pad * 2,
             kScreenH - kStatusH - kStatsBarH - pad * 2, kBlack);
  r.drawRect(pad, pad + kStatsBarH, kScreenW - pad * 2,
             kScreenH - kStatusH - kStatsBarH - pad * 2, kYellow);
  draw_menu_tabs(r, game);

  switch (game.menu_tab()) {
    case Game::MenuTab::Stats:        draw_menu_stats(r, pet, game); break;
    case Game::MenuTab::Achievements: draw_menu_achievements(r, game); break;
    case Game::MenuTab::Settings:     draw_menu_options(r, game); break;
    case Game::MenuTab::Sync:         draw_menu_sync(r, game); break;
    case Game::MenuTab::Memorial:
#if BAILEY_MEMORIAL_WALL
      draw_menu_memorial(r, game);
#else
      draw_menu_stats(r, pet, game);
#endif
      break;
    case Game::MenuTab::Inventory: draw_menu_inventory(r, game); break;
    case Game::MenuTab::Shop:      draw_menu_shop(r, game); break;
    case Game::MenuTab::Actions:   draw_menu_actions(r, game); break;
  }

  r.drawText(14, kScreenH - kStatusH - 14, "Long-press: close  |  short: next tab",
             kGray, 1);
}

}  // namespace tama
