#include "tama/ui.h"

#include <cstdio>
#include <cstring>

#include "tama/achievements.h"
#include "tama/colors.h"
#include "tama/font_6x8.h"
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

const char* mood_text(Mood m) {
  switch (m) {
    case Mood::Happy:     return "Bailey is happy!";
    case Mood::Hungry:    return "Bailey is hungry";
    case Mood::Sad:       return "Bailey feels sad";
    case Mood::Dirty:     return "Bailey needs a bath";
    case Mood::Sleeping:  return "Zzz... napping";
    case Mood::MovingOut: return "Bailey moved in with...";  // family name appended later
    case Mood::Magic:     return "Bailey is young again!";
    case Mood::Gone:      // legacy, should not render
    case Mood::Neutral:
    default:              return "How is Bailey today?";
  }
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
void draw_accessory_overlay(Renderer& r, uint8_t accessory_id) {
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

void draw_pet_sprite(Renderer& r, const Pet& pet, uint32_t now_ms) {
  PetPose pose = PetPose::IdleA;

  switch (pet.current_action) {
    case Action::Eat:   pose = PetPose::Eating;  break;
    case Action::Play:  pose = PetPose::Playing; break;
    case Action::Clean: pose = PetPose::IdleA;   break;
    case Action::Pet:   pose = PetPose::IdleB;   break;
    case Action::None:
      switch (pet.mood) {
        case Mood::Sleeping:  pose = PetPose::Sleep; break;
        case Mood::Sad:       pose = PetPose::Sad;   break;
        case Mood::MovingOut: pose = PetPose::IdleB; break;  // happy walking off
        case Mood::Magic:     pose = PetPose::IdleA; break;  // calm transformation
        case Mood::Gone:      pose = PetPose::IdleA; break;  // legacy fallback
        default:
          pose = ((now_ms / kIdleFrameMs) & 1) ? PetPose::IdleB : PetPose::IdleA;
          break;
      }
      break;
  }

  // For MovingOut, slide Bailey off the right edge based on elapsed time.
  int draw_x = kPetX;
  if (pet.mood == Mood::MovingOut) {
    // The full transition is ~5s; slide over that interval.
    // We don't have direct access to transition_started_ms_ here, so use
    // age_ms low bits + now_ms parity for a deterministic position --
    // simpler: cosine-style cycle via now_ms gives a smooth slide.
    uint32_t cycle = now_ms % 5000;
    int max_dx = (kScreenW - kPetX);
    draw_x = kPetX + (int)(cycle * max_dx / 5000);
  }

  const uint8_t* sprite = pet_sprite(pet.stage, pose);
  r.drawSprite(draw_x, kPetY, kPetW, kPetH, sprite, kSpritePalette, kPetScale);

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
    case Action::Eat:
      r.drawSprite(kPetX + kPetDrawW - 8, kPetY + kPetDrawH - kAccessoryScale * 16 + 8,
                   16, 16, food_bowl_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::Play:
      r.drawSprite(kPetX - 4, kPetY + kPetDrawH - kAccessoryScale * 16,
                   16, 16, ball_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::Clean:
      r.drawSprite(kPetX + 16, kPetY - 4,
                   16, 16, bubble_sprite(), kSpritePalette, kAccessoryScale);
      r.drawSprite(kPetX + kPetDrawW - 36, kPetY + 8,
                   16, 16, bubble_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::Pet:
      r.drawSprite(kPetX + kPetDrawW / 2 - 16, kPetY - 4,
                   16, 16, heart_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::None:
      if (pet.mood == Mood::Sleeping)
        r.drawSprite(kPetX + kPetDrawW - 40, kPetY - 8,
                     16, 16, zzz_sprite(), kSpritePalette, kAccessoryScale);
      else if (pet.mood == Mood::Dirty)
        r.drawSprite(kPetX + kPetDrawW - 36, kPetY + kPetDrawH - 36,
                     16, 16, poop_sprite(), kSpritePalette, kAccessoryScale);
      break;
  }
}

// Weather overlay: light particles across the screen.
void draw_weather(Renderer& r, uint8_t weather, uint32_t now_ms) {
  if (weather == 0) return;
  uint32_t seed = 0x12345;
  if (weather == (uint8_t)2 /* Rain */) {
    for (int i = 0; i < 24; ++i) {
      seed = seed * 1664525u + 1013904223u;
      int x = (int)((seed >> 8) % kScreenW);
      int y = (int)(((seed >> 16) + now_ms / 50) % (kScreenH - kStatusH));
      r.drawVLine(x, y, 4, kSkyDeep);
    }
  } else if (weather == (uint8_t)3 /* Snow */) {
    for (int i = 0; i < 24; ++i) {
      seed = seed * 1664525u + 1013904223u;
      int x = (int)((seed >> 8) % kScreenW);
      int y = (int)(((seed >> 16) + now_ms / 80) % (kScreenH - kStatusH));
      r.fillRect(x, y, 2, 2, kWhite);
    }
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
  char msg_buf[48];
  const char* msg;
  if (pet.mood == Mood::MovingOut) {
    int idx = game.move_out_family_idx() & 7;
    std::snprintf(msg_buf, sizeof(msg_buf), "Moved in w/ the %s", kFamilyNames[idx]);
    msg = msg_buf;
  } else {
    msg = mood_text(pet.mood);
  }
  // Use scale 1 for the MovingOut/Magic message so the longer string fits.
  int scale = (pet.mood == Mood::MovingOut) ? 1 : 2;
  int tw = text_width(msg, scale);
  int tx = (kScreenW - tw) / 2;
  r.drawText(tx, y0 + (scale == 2 ? 8 : 12), msg, kWhite, scale);

  char info[40];
  uint32_t age_min = (uint32_t)(pet.age_ms / 60000ULL);
  std::snprintf(info, sizeof(info), "%s  %lum  S%u",
                stage_text(pet.stage), (unsigned long)age_min,
                (unsigned)game.streak_days());
  r.drawText(kScreenW - text_width(info, 1) - 4, y0 + kStatusH - 10, info, kGrayLight, 1);
}

void draw_menu_tabs(Renderer& r, const Game& game) {
#if BAILEY_MEMORIAL_WALL
  const char* labels[] = {"STATS", "BADGES", "OPTS", "SYNC", "MEM", "BAG", "SHOP"};
  const int   tab_ids[] = {0, 1, 2, 3, 4, 5, 6};
  constexpr int n_tabs = 7;
#else
  const char* labels[] = {"STATS", "BADGES", "OPTS", "SYNC", "BAG", "SHOP"};
  const int   tab_ids[] = {0, 1, 2, 3, 5, 6};
  constexpr int n_tabs = 6;
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

  std::snprintf(buf, sizeof(buf), "Stage : %s", stage_text(pet.stage));
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  uint32_t age_min = (uint32_t)(pet.age_ms / 60000ULL);
  std::snprintf(buf, sizeof(buf), "Age   : %lu min", (unsigned long)age_min);
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "Trait : %s", personality_name(game.personality()));
  r.drawText(x, y, buf, kPink, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "Streak: %u days  Pets: %lu",
                (unsigned)game.streak_days(), (unsigned long)game.total_pets());
  r.drawText(x, y, buf, kYellow, 1); y += 14;

  std::snprintf(buf, sizeof(buf), "Food  : %3u / 100", pet.stats.hunger);
  r.drawText(x, y, buf, kOrange, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Play  : %3u / 100", pet.stats.happiness);
  r.drawText(x, y, buf, kYellow, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Bath  : %3u / 100", pet.stats.cleanliness);
  r.drawText(x, y, buf, kBlue, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Rest  : %3u / 100", pet.stats.energy);
  r.drawText(x, y, buf, kGreen, 1); y += 14;

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

  // Diary auto-templated from yesterday's mood.
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

void draw_menu_achievements(Renderer& r, const Game& game) {
  int x0 = 22;
  int y0 = 14 + kStatsBarH + 22;
  int row = 0, col = 0;
  for (int i = 0; i < kAchievementCount; ++i) {
    bool unlocked = is_unlocked(game.achievements(), (AchievementId)i);
    int x = x0 + col * 100;
    int y = y0 + row * 14;
    r.fillRect(x, y, 8, 8, unlocked ? kYellow : kGrayDark);
    r.drawRect(x, y, 8, 8, kGrayLight);
    r.drawText(x + 12, y, achievement_name((AchievementId)i),
               unlocked ? kWhite : kGray, 1);
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

  std::snprintf(buf, sizeof(buf), "Auto-sleep: %s", s.auto_sleep ? "on" : "off");
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
  std::snprintf(buf, sizeof(buf), "Biscuits: %u", (unsigned)game.biscuits());
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
  const Row rows[15] = {
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
    {"Coat: Tan",     game.coat_pattern() == 1,      game.shop_price(11)},
    {"Coat: Brindle", game.coat_pattern() == 2,      game.shop_price(12)},
    {"Coat: Tri",     game.coat_pattern() == 3,      game.shop_price(13)},
    {"Coat: Black",   game.coat_pattern() == 4,      game.shop_price(14)},
  };
  // Show 6 rows around the cursor
  uint8_t cur = game.shop_cursor();
  int start = (cur > 2) ? (cur - 2) : 0;
  if (start > 9) start = 9;
  for (int i = 0; i < 6; ++i) {
    int idx = start + i;
    if (idx >= 15) break;
    bool sel = idx == cur;
    if (sel) r.fillRect(x - 2, y - 1, kScreenW - 28, 10, kGrayDark);
    std::snprintf(buf, sizeof(buf), "%c %s  %ub",
                  sel ? '>' : ' ', rows[idx].name, (unsigned)rows[idx].price);
    uint16_t fg = rows[idx].owned ? kGreen
                                  : (game.biscuits() >= rows[idx].price ? kWhite : kGrayDark);
    r.drawText(x, y, buf, fg, 1);
    y += 10;
  }
  y += 4;
  r.drawText(x, y, "A=buy  B=next  C=tab", kGray, 1);
}

void draw_menu_sync(Renderer& r, const Game& game_const) {
  // generate_sync_code mutates internal buffer; we need a mutable ref.
  auto& game = const_cast<Game&>(game_const);
  const char* code = game.generate_sync_code();
  int x = 22;
  int y = 14 + kStatsBarH + 22;
  r.drawText(x, y, "SHARE YOUR BAILEY", kYellow, 1); y += 16;
  r.drawText(x, y, "Type this code on the", kWhite, 1); y += 10;
  r.drawText(x, y, "web app to restore state:", kWhite, 1); y += 18;
  // Code (large)
  int tw = text_width(code, 2);
  r.drawText((kScreenW - tw) / 2, y, code, kYellow, 2);
  y += 24;
  r.drawText(x, y, "Or paste a code from web", kGray, 1); y += 10;
  r.drawText(x, y, "(typing on device: TODO)", kGray, 1);
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
  char buf[32];
  std::snprintf(buf, sizeof(buf), "WALK  %u/20  (B=step)", game.walk_steps());
  r.drawText((kScreenW - text_width(buf, 1)) / 2, y0 + 10, buf, kYellow, 1);
}

void draw_scene(Renderer& r, const Game& game, uint32_t now_ms) {
  const Pet& pet = game.pet();
  float daylight = game.daylight();

  draw_background(r, daylight);
  draw_scene_detail(r, game.settings().scene_id, daylight);

  r.fillRect(0, 0, kScreenW, kStatsBarH, kGrayDark);
  r.drawHLine(0, kStatsBarH, kScreenW, kGrayLight);
  draw_stats_bar(r, pet, game.clock_string());

  draw_pet_sprite(r, pet, now_ms);
  // Skip coat / accessory overlays during MovingOut (Bailey is sliding
  // off; the fixed-position overlays would be left behind).
  if (pet.mood != Mood::MovingOut) {
    draw_coat_accents(r, game.coat_pattern());
    draw_accessory_overlay(r, game.accessory_id());
  }
  draw_weather(r, (uint8_t)game.weather(), now_ms);

  // Birthday confetti goes UNDER the footer overlay so the message still reads.
  if (game.is_birthday()) draw_confetti(r, now_ms);

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
  }

  // NPC visitor: a small silhouette dog walks across the bottom of the
  // play area for 4 s.
  if (game.npc_visit_kind() != 0) {
    uint32_t elapsed = now_ms - game.npc_visit_ms();
    if (elapsed < 4000) {
      int x = -20 + (int)((kScreenW + 40) * elapsed / 4000);
      int y = kScreenH - kStatusH - 18;
      uint16_t cols[4] = {kBrown, kGrayDark, kBrownLight, kGray};
      uint16_t c = cols[(game.npc_visit_kind() - 1) % 4];
      // tiny dog: body + head + ear + legs
      r.fillRect(x, y, 14, 8, c);
      r.fillRect(x - 4, y - 4, 6, 6, c);   // head
      r.fillRect(x - 6, y - 2, 2, 5, c);   // ear
      r.fillRect(x + 1, y + 8, 2, 4, c);   // leg
      r.fillRect(x + 10, y + 8, 2, 4, c);  // leg
      r.fillRect(x - 7, y - 3, 1, 1, kBlack); // nose dot
      // Greet prompt
      r.drawText(8, kStatsBarH + 18, "Press to greet!", kYellow, 1);
    }
  }

  draw_footer(r, game);

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
  }

  r.drawText(14, kScreenH - kStatusH - 14, "Long-press: close  |  short: next tab",
             kGray, 1);
}

}  // namespace tama
