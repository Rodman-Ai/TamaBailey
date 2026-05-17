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
      if (pet.mood == Mood::Sleeping)
        r.drawSprite(draw_x + kPetDrawW - 40, kPetY - 8,
                     16, 16, zzz_sprite(), kSpritePalette, kAccessoryScale);
      else if (pet.mood == Mood::Dirty)
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
    case 4: {  // Bedroom: bed + pillow + nightstand + lamp
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
  } else if (v0 != 0 && v1 != 0) {
    std::snprintf(msg_buf, sizeof(msg_buf), "%s and %s are visiting",
                  friend_name((Friend)(v0 - 1)),
                  friend_name((Friend)(v1 - 1)));
    msg = msg_buf;
  } else if (v0 != 0) {
    std::snprintf(msg_buf, sizeof(msg_buf), "%s is visiting",
                  friend_name((Friend)(v0 - 1)));
    msg = msg_buf;
  } else {
    msg = mood_text(pet.mood);
  }
  // Custom-formatted strings (msg_buf) render at scale 1 to fit; the
  // mood-text fast path keeps scale 2.
  int scale = (msg == msg_buf) ? 1 : 2;
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
    y += 12;
  }
  if (game.horoscope_text()[0] != '\0') {
    std::snprintf(buf, sizeof(buf), "Today : %s", game.horoscope_text());
    r.drawText(x, y, buf, kPink, 1); y += 12;
  }

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
  const Row rows[16] = {
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
    {"Trade 5 bones", false,                          0},  // priced in bones, not biscuits
  };
  // Show 6 rows around the cursor
  uint8_t cur = game.shop_cursor();
  int start = (cur > 2) ? (cur - 2) : 0;
  if (start > 10) start = 10;
  for (int i = 0; i < 6; ++i) {
    int idx = start + i;
    if (idx >= 16) break;
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
  static const char* const kMain[8] = {
    "Go for a walk", "Play fetch", "Give treat", "Brush",
    "Switch toy", "Bedtime", "Tricks >",
    "Play with a friend >",
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
    rows = kMain;    n_rows = 8;  header = "ACTIONS";
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
  char buf[40];
  std::snprintf(buf, sizeof(buf), "WALK %u/20  B=step C=stop", game.walk_steps());
  r.drawText((kScreenW - text_width(buf, 1)) / 2, y0 + 10, buf, kYellow, 1);
  std::snprintf(buf, sizeof(buf), "today: %u steps  bones: %u",
                (unsigned)game.walk_today_steps(),
                (unsigned)game.bones_collected());
  r.drawText((kScreenW - text_width(buf, 1)) / 2, y0 + 22, buf, kGrayLight, 1);
  // Transient "FOUND!" popup for the last 1500 ms after a walk item-find.
  uint32_t since = game.last_tick_ms() - game.last_walk_find_ms();
  if (game.last_walk_find_kind() != 0 && since < 1500) {
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

  r.fillRect(0, 0, kScreenW, kStatsBarH, kGrayDark);
  r.drawHLine(0, kStatsBarH, kScreenW, kGrayLight);
  draw_stats_bar(r, pet, game.clock_string());

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
    if (game.npc_visit_kind(slot) == 0) continue;
    uint32_t elapsed = now_ms - game.npc_visit_ms(slot);
    if (elapsed >= kFriendVisitMs) continue;
    Friend f = (Friend)((game.npc_visit_kind(slot) - 1) % (int)Friend::COUNT);
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
    PetPose fpose = ((now_ms / 350 + slot) & 1) ? PetPose::IdleB : PetPose::IdleA;
    r.drawSprite(fx, fy, kPetW, kPetH,
                 friend_sprite(f, fpose), kSpritePalette, kPetScale);
    vis[slot] = {true, f, fx, fy, elapsed, slot};
  }

  draw_pet_sprite(r, pet, now_ms, game);
  if (pet.mood != Mood::MovingOut) {
    draw_coat_accents(r, game.coat_pattern());
    draw_accessory_overlay(r, game.accessory_id());
  }
  draw_weather(r, (uint8_t)game.weather(), now_ms);

  // Now draw the visitor labels + hearts ON TOP of Bailey so the name
  // strip is still readable when there's overlap.
  bool any_visitor = false;
  for (int slot = 0; slot < kMaxVisitors; ++slot) {
    if (!vis[slot].active) continue;
    any_visitor = true;
    if (vis[slot].elapsed < 2000) {
      const char* name = friend_name(vis[slot].f);
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
    case Game::MenuTab::Actions:   draw_menu_actions(r, game); break;
  }

  r.drawText(14, kScreenH - kStatusH - 14, "Long-press: close  |  short: next tab",
             kGray, 1);
}

}  // namespace tama
