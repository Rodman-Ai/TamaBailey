#pragma once

#include <cstdint>

namespace tama {

// RGB565 colors used by the UI.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBlack       = rgb(  0,   0,   0);
constexpr uint16_t kWhite       = rgb(255, 255, 255);
constexpr uint16_t kSky         = rgb(135, 206, 235);
constexpr uint16_t kSkyDeep     = rgb( 70, 130, 180);
constexpr uint16_t kGrass       = rgb( 80, 160,  80);
constexpr uint16_t kGrassDark   = rgb( 40, 110,  40);
constexpr uint16_t kBrownDark   = rgb( 60,  35,  10);
constexpr uint16_t kBrown       = rgb(140,  80,  35);
constexpr uint16_t kBrownLight  = rgb(200, 140,  80);
constexpr uint16_t kCream       = rgb(245, 220, 175);
constexpr uint16_t kPink        = rgb(255, 150, 170);
constexpr uint16_t kRed         = rgb(220,  40,  40);
constexpr uint16_t kOrange      = rgb(245, 150,  30);
constexpr uint16_t kYellow      = rgb(245, 215,  40);
constexpr uint16_t kGreen       = rgb( 90, 200,  90);
constexpr uint16_t kBlue        = rgb( 90, 150, 245);
constexpr uint16_t kGray        = rgb(140, 140, 140);
constexpr uint16_t kGrayDark    = rgb( 70,  70,  70);
constexpr uint16_t kGrayLight   = rgb(210, 210, 210);
constexpr uint16_t kHeartRed    = rgb(245,  80, 100);

// 16-entry indexed palette used by sprites. Index 0 is transparent.
inline constexpr uint16_t kSpritePalette[16] = {
  /* 0 transparent (sentinel)        */ kBlack,
  /* 1 outline                       */ kBrownDark,
  /* 2 body                          */ kBrown,
  /* 3 highlight                     */ kBrownLight,
  /* 4 black (eyes/nose)             */ kBlack,
  /* 5 white (eye whites / Zzz)      */ kWhite,
  /* 6 pink (tongue / ear inside)    */ kPink,
  /* 7 cream (belly / muzzle)        */ kCream,
  /* 8 red (food / collar)           */ kRed,
  /* 9 green (ball / grass)          */ kGreen,
  /*10 blue (water / soap)           */ kBlue,
  /*11 yellow (food / sun)           */ kYellow,
  /*12 orange (sad)                  */ kOrange,
  /*13 gray (poop / stone)           */ kGray,
  /*14 dark gray                     */ kGrayDark,
  /*15 light gray                    */ kGrayLight,
};

constexpr uint8_t kSpriteTransparent = 0;

}  // namespace tama
