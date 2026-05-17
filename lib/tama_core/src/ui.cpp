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

const char* mood_text(Mood m) {
  switch (m) {
    case Mood::Happy:    return "Bailey is happy!";
    case Mood::Hungry:   return "Bailey is hungry";
    case Mood::Sad:      return "Bailey feels sad";
    case Mood::Dirty:    return "Bailey needs a bath";
    case Mood::Sleeping: return "Zzz... napping";
    case Mood::Gone:     return "Hold a button to start over";
    case Mood::Neutral:
    default:             return "How is Bailey today?";
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
        case Mood::Sleeping: pose = PetPose::Sleep; break;
        case Mood::Sad:      pose = PetPose::Sad;   break;
        case Mood::Gone:     pose = PetPose::Gone;  break;
        default:
          pose = ((now_ms / kIdleFrameMs) & 1) ? PetPose::IdleB : PetPose::IdleA;
          break;
      }
      break;
  }

  const uint8_t* sprite = pet_sprite(pet.stage, pose);
  r.drawSprite(kPetX, kPetY, kPetW, kPetH, sprite, kSpritePalette, kPetScale);

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

void draw_footer(Renderer& r, const Pet& pet, uint16_t streak_days) {
  int y0 = kScreenH - kStatusH;
  r.fillRect(0, y0, kScreenW, kStatusH, kGrayDark);
  r.drawHLine(0, y0, kScreenW, kGrayLight);

  const char* msg = mood_text(pet.mood);
  int tw = text_width(msg, 2);
  int tx = (kScreenW - tw) / 2;
  r.drawText(tx, y0 + 8, msg, kWhite, 2);

  char info[40];
  uint32_t age_min = (uint32_t)(pet.age_ms / 60000ULL);
  std::snprintf(info, sizeof(info), "%s  %lum  S%u",
                stage_text(pet.stage), (unsigned long)age_min, (unsigned)streak_days);
  r.drawText(kScreenW - text_width(info, 1) - 4, y0 + kStatusH - 10, info, kGrayLight, 1);
}

void draw_menu_tabs(Renderer& r, const Game& game) {
  const char* labels[4] = {"STATS", "BADGES", "OPTIONS", "SYNC"};
  int x = 14;
  int y = 14 + kStatsBarH;
  for (int i = 0; i < 4; ++i) {
    bool active = (int)game.menu_tab() == i;
    uint16_t bg = active ? kYellow : kGrayDark;
    uint16_t fg = active ? kBlack  : kGrayLight;
    int w = text_width(labels[i], 1) + 8;
    r.fillRect(x, y, w, 12, bg);
    r.drawRect(x, y, w, 12, kYellow);
    r.drawText(x + 4, y + 2, labels[i], fg, 1);
    x += w + 2;
  }
}

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
  r.drawText(x, y, buf, kGreen, 1);
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

  // Tricks learned
  uint8_t tl = game.tricks_learned();
  r.drawText(x, y, "Tricks    :", kWhite, 1); y += 10;
  for (int i = 0; i < (int)Trick::COUNT; ++i) {
    bool got = (tl & (1u << i)) != 0;
    r.drawText(x + 12, y, trick_name((Trick)i),
               got ? kGreen : kGrayDark, 1);
    y += 10;
  }
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

void draw_scene(Renderer& r, const Game& game, uint32_t now_ms) {
  const Pet& pet = game.pet();
  float daylight = game.daylight();

  draw_background(r, daylight);
  draw_scene_detail(r, game.settings().scene_id, daylight);

  r.fillRect(0, 0, kScreenW, kStatsBarH, kGrayDark);
  r.drawHLine(0, kStatsBarH, kScreenW, kGrayLight);
  draw_stats_bar(r, pet, game.clock_string());

  draw_pet_sprite(r, pet, now_ms);
  draw_coat_accents(r, game.coat_pattern());
  draw_accessory_overlay(r, game.accessory_id());
  draw_weather(r, (uint8_t)game.weather(), now_ms);
  draw_footer(r, pet, game.streak_days());

  // Mode-specific overlays
  if (game.mode() != GameMode::Idle && game.mode() != GameMode::PickingCoat)
    draw_fetch(r, game, now_ms);
  if (game.is_sick())
    draw_sickness_overlay(r);
  if (game.mode() == GameMode::PickingCoat)
    draw_coat_picker(r, game);
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
  }

  r.drawText(14, kScreenH - kStatusH - 14, "Long-press: close  |  short: next tab",
             kGray, 1);
}

}  // namespace tama
