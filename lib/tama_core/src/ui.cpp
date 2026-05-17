#include "tama/ui.h"

#include <cstdio>

#include "tama/colors.h"
#include "tama/font_6x8.h"
#include "tama/game.h"  // for kIdleFrameMs and tunables
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

void draw_stat_bar(Renderer& r, int x, int y, int w, int h,
                   uint8_t value, uint16_t fill_color, const char* label) {
  r.fillRect(x, y, w, h, kBlack);
  r.drawRect(x - 1, y - 1, w + 2, h + 2, kGrayLight);
  int filled = (w - 2) * value / 100;
  if (filled > 0) r.fillRect(x + 1, y + 1, filled, h - 2, fill_color);
  // Label above
  r.drawText(x, y - 10, label, kGrayLight, 1);
}

void draw_stats_bar(Renderer& r, const Pet& pet) {
  // 4 stats, 4 columns
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
}

void draw_pet_sprite(Renderer& r, const Pet& pet, uint32_t now_ms) {
  PetPose pose = PetPose::IdleA;

  // Action poses override mood-driven idle while the action animation runs.
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
          // Idle breathing: alternate frames.
          pose = ((now_ms / kIdleFrameMs) & 1) ? PetPose::IdleB : PetPose::IdleA;
          break;
      }
      break;
  }

  const uint8_t* sprite = pet_sprite(pet.stage, pose);
  r.drawSprite(kPetX, kPetY, kPetW, kPetH, sprite, kSpritePalette, kPetScale);

  // Accessories
  switch (pet.current_action) {
    case Action::Eat:
      r.drawSprite(kPetX + kPetDrawW - 8, kPetY + kPetDrawH - kAccessorySize * kAccessoryScale + 8,
                   kAccessorySize, kAccessorySize, food_bowl_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::Play:
      r.drawSprite(kPetX - 4, kPetY + kPetDrawH - kAccessorySize * kAccessoryScale,
                   kAccessorySize, kAccessorySize, ball_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::Clean:
      r.drawSprite(kPetX + 16, kPetY - 4,
                   kAccessorySize, kAccessorySize, bubble_sprite(), kSpritePalette, kAccessoryScale);
      r.drawSprite(kPetX + kPetDrawW - 36, kPetY + 8,
                   kAccessorySize, kAccessorySize, bubble_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::Pet:
      r.drawSprite(kPetX + kPetDrawW / 2 - 16, kPetY - 4,
                   kAccessorySize, kAccessorySize, heart_sprite(), kSpritePalette, kAccessoryScale);
      break;
    case Action::None:
      if (pet.mood == Mood::Sleeping)
        r.drawSprite(kPetX + kPetDrawW - 40, kPetY - 8,
                     kAccessorySize, kAccessorySize, zzz_sprite(), kSpritePalette, kAccessoryScale);
      else if (pet.mood == Mood::Dirty)
        r.drawSprite(kPetX + kPetDrawW - 36, kPetY + kPetDrawH - 36,
                     kAccessorySize, kAccessorySize, poop_sprite(), kSpritePalette, kAccessoryScale);
      break;
  }
}

void draw_footer(Renderer& r, const Pet& pet) {
  int y0 = kScreenH - kStatusH;
  r.fillRect(0, y0, kScreenW, kStatusH, kGrayDark);
  r.drawHLine(0, y0, kScreenW, kGrayLight);

  const char* msg = mood_text(pet.mood);
  int tw = text_width(msg, 2);
  int tx = (kScreenW - tw) / 2;
  r.drawText(tx, y0 + 8, msg, kWhite, 2);

  // Stage + age indicator on bottom right
  char info[24];
  uint32_t age_min = (uint32_t)(pet.age_ms / 60000ULL);
  std::snprintf(info, sizeof(info), "%s  %lum", stage_text(pet.stage),
                (unsigned long)age_min);
  r.drawText(kScreenW - text_width(info, 1) - 4, y0 + kStatusH - 10, info, kGrayLight, 1);
}

}  // namespace

bool point_on_pet(int x, int y) {
  return x >= kPetX && x < kPetX + kPetDrawW &&
         y >= kPetY && y < kPetY + kPetDrawH;
}

bool point_on_stats_bar(int /*x*/, int y) {
  return y >= 0 && y < kStatsBarH;
}

void draw_scene(Renderer& r, const Pet& pet, uint32_t now_ms) {
  // Sky gradient strip + grass strip background
  r.fillRect(0, 0, kScreenW, kScreenH, kSky);
  // Floor band where Bailey sits
  int floor_y = kPetY + kPetSpriteSize * kPetScale - 6;
  r.fillRect(0, floor_y, kScreenW, kScreenH - floor_y - kStatusH, kGrass);
  r.drawHLine(0, floor_y, kScreenW, kGrassDark);

  // Top stats panel
  r.fillRect(0, 0, kScreenW, kStatsBarH, kGrayDark);
  r.drawHLine(0, kStatsBarH, kScreenW, kGrayLight);
  draw_stats_bar(r, pet);

  draw_pet_sprite(r, pet, now_ms);
  draw_footer(r, pet);
}

void draw_menu_overlay(Renderer& r, const Pet& pet) {
  // Translucent-ish background (we don't have alpha; draw a panel)
  int pad = 14;
  r.fillRect(pad, pad + kStatsBarH, kScreenW - pad * 2,
             kScreenH - kStatusH - kStatsBarH - pad * 2, kBlack);
  r.drawRect(pad, pad + kStatsBarH, kScreenW - pad * 2,
             kScreenH - kStatusH - kStatsBarH - pad * 2, kYellow);

  char buf[40];
  int x = pad + 8;
  int y = pad + kStatsBarH + 8;
  r.drawText(x, y, "BAILEY STATUS", kYellow, 2); y += 22;

  std::snprintf(buf, sizeof(buf), "Stage : %s",
                pet.stage == LifeStage::Puppy ? "Puppy" :
                pet.stage == LifeStage::Adult ? "Adult" :
                pet.stage == LifeStage::Senior ? "Senior" : "Gone");
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  uint32_t age_min = (uint32_t)(pet.age_ms / 60000ULL);
  std::snprintf(buf, sizeof(buf), "Age   : %lu min", (unsigned long)age_min);
  r.drawText(x, y, buf, kWhite, 1); y += 12;

  std::snprintf(buf, sizeof(buf), "Food  : %3u / 100", pet.stats.hunger);
  r.drawText(x, y, buf, kOrange, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Play  : %3u / 100", pet.stats.happiness);
  r.drawText(x, y, buf, kYellow, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Bath  : %3u / 100", pet.stats.cleanliness);
  r.drawText(x, y, buf, kBlue, 1); y += 12;
  std::snprintf(buf, sizeof(buf), "Rest  : %3u / 100", pet.stats.energy);
  r.drawText(x, y, buf, kGreen, 1); y += 14;

  uint32_t healthy_min = (uint32_t)(pet.healthy_streak_ms / 60000ULL);
  std::snprintf(buf, sizeof(buf), "Healthy: %lu min", (unsigned long)healthy_min);
  r.drawText(x, y, buf, kGrayLight, 1); y += 12;

  r.drawText(x, y + 4, "Long-press to close", kGray, 1);
}

}  // namespace tama
