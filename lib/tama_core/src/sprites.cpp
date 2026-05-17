// Procedurally-generated pixel art for Bailey the hound.
//
// Sprites live in static buffers populated once by sprites_init(). Every
// pose is drawn from the same primitives so the look stays consistent; tweak
// silhouette parameters in draw_bailey() to change all art at once.
//
// Color indices match kSpritePalette in tama/colors.h.

#include "tama/sprites.h"

#include <cstdint>
#include <cstring>

namespace tama {

namespace {

constexpr int W = kPetSpriteSize;
constexpr int H = kPetSpriteSize;
constexpr int A = kAccessorySize;

constexpr uint8_t TR = 0;   // transparent
constexpr uint8_t OL = 1;   // outline (dark brown)
constexpr uint8_t BD = 2;   // body (brown)
constexpr uint8_t HL = 3;   // highlight (light brown)
constexpr uint8_t BK = 4;   // black
constexpr uint8_t WH = 5;   // white
constexpr uint8_t PK = 6;   // pink
constexpr uint8_t CR = 7;   // cream / belly
constexpr uint8_t RD = 8;   // red
constexpr uint8_t GN = 9;   // green
constexpr uint8_t BL = 10;  // blue
constexpr uint8_t YL = 11;  // yellow
constexpr uint8_t OR = 12;  // orange
constexpr uint8_t GR = 13;  // gray
constexpr uint8_t DG = 14;  // dark gray
constexpr uint8_t LG = 15;  // light gray

uint8_t  g_pet[4][7][W * H];  // stage x pose x pixels
uint8_t  g_food[A * A];
uint8_t  g_ball[A * A];
uint8_t  g_poop[A * A];
uint8_t  g_bubble[A * A];
uint8_t  g_zzz[A * A];
uint8_t  g_heart[A * A];
bool     g_inited = false;

inline void pset(uint8_t* buf, int bw, int bh, int x, int y, uint8_t c) {
  if (x < 0 || x >= bw || y < 0 || y >= bh) return;
  buf[y * bw + x] = c;
}

inline uint8_t pget(const uint8_t* buf, int bw, int bh, int x, int y) {
  if (x < 0 || x >= bw || y < 0 || y >= bh) return TR;
  return buf[y * bw + x];
}

// Filled ellipse with optional outline.
void fill_ellipse(uint8_t* buf, int bw, int bh,
                  int cx, int cy, int rx, int ry,
                  uint8_t fill, uint8_t outline = TR) {
  for (int y = cy - ry; y <= cy + ry; ++y) {
    for (int x = cx - rx; x <= cx + rx; ++x) {
      int dx = x - cx, dy = y - cy;
      float d = (float)(dx * dx) / (rx * rx) + (float)(dy * dy) / (ry * ry);
      if (d <= 1.0f) {
        // outline ring: anything beyond 0.78 inside the ellipse
        if (outline != TR && d > 0.78f) pset(buf, bw, bh, x, y, outline);
        else                            pset(buf, bw, bh, x, y, fill);
      }
    }
  }
}

void fill_rect(uint8_t* buf, int bw, int bh,
               int x, int y, int w, int h, uint8_t c) {
  for (int j = 0; j < h; ++j)
    for (int i = 0; i < w; ++i)
      pset(buf, bw, bh, x + i, y + j, c);
}

void line(uint8_t* buf, int bw, int bh,
          int x0, int y0, int x1, int y1, uint8_t c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  while (true) {
    pset(buf, bw, bh, x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err * 2;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 <  dx) { err += dx; y0 += sy; }
  }
}

// Outline ring traced around already-set pixels: any background pixel
// touching the body color is replaced with outline color.
void thicken_outline(uint8_t* buf, int bw, int bh, uint8_t outline) {
  uint8_t tmp[W * H];
  std::memcpy(tmp, buf, bw * bh);
  for (int y = 0; y < bh; ++y) {
    for (int x = 0; x < bw; ++x) {
      if (tmp[y * bw + x] != TR) continue;
      bool touches = false;
      for (int dy = -1; dy <= 1 && !touches; ++dy)
        for (int dx = -1; dx <= 1 && !touches; ++dx)
          if ((dx || dy) && pget(tmp, bw, bh, x + dx, y + dy) != TR &&
              pget(tmp, bw, bh, x + dx, y + dy) != outline)
            touches = true;
      if (touches) pset(buf, bw, bh, x, y, outline);
    }
  }
}

// Core hound silhouette: long body, big floppy ears, droopy posture.
// `frame_lift` shifts the body up by a pixel for the breathing frame.
// `closed_eyes` for sleep, `tongue` for happy poses, `sad_mouth` for sad.
// `size_scale` controls puppy vs adult vs senior proportions.
void draw_bailey(uint8_t* buf,
                 int  frame_lift,
                 bool closed_eyes,
                 bool tongue,
                 bool sad_mouth,
                 float size_scale,
                 uint8_t body_color = BD,
                 uint8_t highlight  = HL) {
  std::memset(buf, TR, W * H);

  const int cx = W / 2;
  const int cy = H / 2 + 6 - frame_lift;

  const int body_rx = (int)(18 * size_scale);
  const int body_ry = (int)(12 * size_scale);
  const int head_r  = (int)(11 * size_scale);

  // Tail (curls behind right hip)
  for (int i = 0; i < (int)(14 * size_scale); ++i) {
    int tx = cx + body_rx - 4 + i;
    int ty = cy - 2 - i / 2;
    pset(buf, W, H, tx,     ty, body_color);
    pset(buf, W, H, tx + 1, ty, body_color);
    pset(buf, W, H, tx,     ty + 1, body_color);
  }

  // Back legs
  fill_rect(buf, W, H, cx + 4, cy + body_ry - 2, 4, 6, body_color);
  fill_rect(buf, W, H, cx + 9, cy + body_ry - 2, 4, 6, body_color);
  // Front legs
  fill_rect(buf, W, H, cx - 12, cy + body_ry - 2, 4, 7, body_color);
  fill_rect(buf, W, H, cx -  7, cy + body_ry - 2, 4, 7, body_color);

  // Body
  fill_ellipse(buf, W, H, cx, cy, body_rx, body_ry, body_color);
  // Cream belly patch
  fill_ellipse(buf, W, H, cx - 2, cy + 4, body_rx - 8, body_ry - 5, CR);

  // Head (offset left/forward of body)
  const int hx = cx - body_rx + 4;
  const int hy = cy - body_ry + 2;
  fill_ellipse(buf, W, H, hx, hy, head_r, head_r - 1, body_color);

  // Floppy ears (drooping past the jaw on both sides of the head)
  for (int i = 0; i < (int)(13 * size_scale); ++i) {
    int ex = hx - head_r + 1 - i / 3;
    int ey = hy - 2 + i;
    pset(buf, W, H, ex,     ey, OL);
    pset(buf, W, H, ex + 1, ey, body_color);
    pset(buf, W, H, ex + 2, ey, body_color);
    pset(buf, W, H, ex + 3, ey, highlight);
  }
  for (int i = 0; i < (int)(13 * size_scale); ++i) {
    int ex = hx + head_r - 4 + i / 3;
    int ey = hy - 1 + i;
    pset(buf, W, H, ex,     ey, highlight);
    pset(buf, W, H, ex + 1, ey, body_color);
    pset(buf, W, H, ex + 2, ey, body_color);
    pset(buf, W, H, ex + 3, ey, OL);
  }

  // Muzzle (cream patch on lower face)
  fill_ellipse(buf, W, H, hx, hy + 4, 6, 4, CR);

  // Eyes
  if (closed_eyes) {
    line(buf, W, H, hx - 5, hy - 1, hx - 2, hy, BK);
    line(buf, W, H, hx + 2, hy,     hx + 5, hy - 1, BK);
  } else {
    pset(buf, W, H, hx - 4, hy - 1, WH);
    pset(buf, W, H, hx - 3, hy - 1, BK);
    pset(buf, W, H, hx + 3, hy - 1, BK);
    pset(buf, W, H, hx + 4, hy - 1, WH);
    pset(buf, W, H, hx - 3, hy - 2, BK);
    pset(buf, W, H, hx + 3, hy - 2, BK);
  }

  // Nose
  pset(buf, W, H, hx - 1, hy + 3, BK);
  pset(buf, W, H, hx,     hy + 3, BK);
  pset(buf, W, H, hx + 1, hy + 3, BK);
  pset(buf, W, H, hx,     hy + 4, BK);

  // Mouth + optional tongue / sad mouth
  if (sad_mouth) {
    pset(buf, W, H, hx - 2, hy + 7, BK);
    pset(buf, W, H, hx - 1, hy + 6, BK);
    pset(buf, W, H, hx,     hy + 6, BK);
    pset(buf, W, H, hx + 1, hy + 6, BK);
    pset(buf, W, H, hx + 2, hy + 7, BK);
  } else {
    pset(buf, W, H, hx - 1, hy + 6, BK);
    pset(buf, W, H, hx,     hy + 6, BK);
    pset(buf, W, H, hx + 1, hy + 6, BK);
    if (tongue) {
      pset(buf, W, H, hx,     hy + 7, PK);
      pset(buf, W, H, hx + 1, hy + 7, PK);
    }
  }

  thicken_outline(buf, W, H, OL);
}

void draw_accessory_food() {
  std::memset(g_food, TR, A * A);
  // Bowl
  for (int y = 9; y < 14; ++y)
    for (int x = 1; x < 15; ++x)
      g_food[y * A + x] = (y == 9 ? OL : (y == 13 ? OL : GR));
  // Kibble
  g_food[8 * A + 4] = YL;
  g_food[8 * A + 5] = OR;
  g_food[8 * A + 7] = YL;
  g_food[8 * A + 9] = OR;
  g_food[8 * A + 11] = YL;
  // Outline
  for (int x = 1; x < 15; ++x) { g_food[9 * A + x] = OL; g_food[13 * A + x] = OL; }
  for (int y = 9; y < 14; ++y) { g_food[y * A + 1] = OL; g_food[y * A + 14] = OL; }
}

void draw_accessory_ball() {
  std::memset(g_ball, TR, A * A);
  int cx = 8, cy = 9, r = 5;
  for (int y = cy - r; y <= cy + r; ++y) {
    for (int x = cx - r; x <= cx + r; ++x) {
      int dx = x - cx, dy = y - cy;
      int d2 = dx * dx + dy * dy;
      if (d2 <= r * r) {
        g_ball[y * A + x] = ((x + y) & 1) ? GN : RD;
        if (d2 > (r - 1) * (r - 1)) g_ball[y * A + x] = OL;
      }
    }
  }
}

void draw_accessory_poop() {
  std::memset(g_poop, TR, A * A);
  // Stacked swirls
  fill_ellipse(g_poop, A, A, 8, 12, 5, 2, DG, OL);
  fill_ellipse(g_poop, A, A, 8,  9, 4, 2, DG, OL);
  fill_ellipse(g_poop, A, A, 8,  6, 3, 2, DG, OL);
  // Two little flies
  g_poop[2 * A + 12] = DG;
  g_poop[3 * A + 13] = DG;
}

void draw_accessory_bubble() {
  std::memset(g_bubble, TR, A * A);
  fill_ellipse(g_bubble, A, A, 7,  7, 4, 4, BL, WH);
  fill_ellipse(g_bubble, A, A, 12, 11, 2, 2, BL, WH);
  fill_ellipse(g_bubble, A, A, 3, 12, 2, 2, BL, WH);
}

void draw_accessory_zzz() {
  std::memset(g_zzz, TR, A * A);
  // Three rising Z's
  auto draw_z = [&](int x, int y, int s) {
    for (int i = 0; i < s; ++i) {
      g_zzz[(y) * A + (x + i)] = WH;        // top
      g_zzz[(y + s - 1) * A + (x + i)] = WH; // bottom
      g_zzz[(y + s - 1 - i) * A + (x + i)] = WH; // diagonal
    }
  };
  draw_z(8, 1, 3);
  draw_z(4, 5, 4);
  draw_z(0, 10, 5);
}

void draw_accessory_heart() {
  std::memset(g_heart, TR, A * A);
  fill_ellipse(g_heart, A, A, 5, 6, 3, 3, RD, OL);
  fill_ellipse(g_heart, A, A, 10, 6, 3, 3, RD, OL);
  for (int y = 6; y <= 13; ++y) {
    int w = 7 - (y - 6);
    for (int x = 8 - w / 2; x <= 8 + w / 2; ++x) {
      g_heart[y * A + x] = ((x == 8 - w / 2) || (x == 8 + w / 2)) ? OL : RD;
    }
  }
}

void draw_pose(uint8_t* buf, PetPose pose, float size_scale) {
  switch (pose) {
    case PetPose::IdleA:   draw_bailey(buf, 0, false, false, false, size_scale); break;
    case PetPose::IdleB:   draw_bailey(buf, 1, false, false, false, size_scale); break;
    case PetPose::Eating:  draw_bailey(buf, 0, false, false, false, size_scale); break;
    case PetPose::Playing: draw_bailey(buf, 1, false, true,  false, size_scale); break;
    case PetPose::Sleep:   draw_bailey(buf, 0, true,  false, false, size_scale); break;
    case PetPose::Sad:     draw_bailey(buf, 0, false, false, true,  size_scale); break;
    case PetPose::Gone: {
      std::memset(buf, TR, W * H);
      // Tombstone
      fill_rect(buf, W, H, 16, 16, 16, 22, GR);
      // Round top
      fill_ellipse(buf, W, H, 24, 16, 8, 8, GR);
      // Outline
      thicken_outline(buf, W, H, OL);
      // RIP text crude
      pset(buf, W, H, 19, 23, BK); pset(buf, W, H, 20, 23, BK);
      pset(buf, W, H, 19, 24, BK); pset(buf, W, H, 19, 25, BK);
      pset(buf, W, H, 22, 23, BK); pset(buf, W, H, 23, 23, BK);
      pset(buf, W, H, 22, 24, BK); pset(buf, W, H, 22, 25, BK);
      pset(buf, W, H, 25, 23, BK); pset(buf, W, H, 26, 23, BK);
      pset(buf, W, H, 26, 24, BK); pset(buf, W, H, 25, 25, BK);
      // Grass
      for (int x = 10; x < 38; ++x) pset(buf, W, H, x, 40, GN);
      break;
    }
  }
}

}  // namespace

void sprites_init() {
  if (g_inited) return;
  g_inited = true;

  static const float kStageScale[3] = {0.85f, 1.00f, 0.95f};

  for (int s = 0; s < 3; ++s) {
    float scale = kStageScale[s];
    for (int p = 0; p < 6; ++p) {
      draw_pose(g_pet[s][p], (PetPose)p, scale);
    }
    // Gone always renders the tombstone
    draw_pose(g_pet[s][(int)PetPose::Gone], PetPose::Gone, scale);
  }
  // Stage 3 (Gone) sprites: all the gone pose.
  for (int p = 0; p < 7; ++p) {
    draw_pose(g_pet[3][p], PetPose::Gone, 1.0f);
  }

  draw_accessory_food();
  draw_accessory_ball();
  draw_accessory_poop();
  draw_accessory_bubble();
  draw_accessory_zzz();
  draw_accessory_heart();
}

const uint8_t* pet_sprite(LifeStage stage, PetPose pose) {
  int s = (int)stage;
  int p = (int)pose;
  if (s < 0 || s > 3) s = 0;
  if (p < 0 || p > 6) p = 0;
  return g_pet[s][p];
}

const uint8_t* food_bowl_sprite() { return g_food; }
const uint8_t* ball_sprite()      { return g_ball; }
const uint8_t* poop_sprite()      { return g_poop; }
const uint8_t* bubble_sprite()    { return g_bubble; }
const uint8_t* zzz_sprite()       { return g_zzz; }
const uint8_t* heart_sprite()     { return g_heart; }

}  // namespace tama
