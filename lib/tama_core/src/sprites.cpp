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

uint8_t  g_pet[4][(int)PetPose::COUNT][W * H];  // stage x pose x pixels
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

// Ambient pose modes that further parameterize draw_bailey.
enum BehaviorMode {
  BM_Default = 0,
  BM_Sit     = 1,  // body lower, hind legs tucked
  BM_Bark    = 2,  // open mouth + head up
  BM_Pant    = 3,  // long tongue dangling
};

// Core hound silhouette: brown body with WHITE belly + paws + muzzle +
// chest blaze, dark patches around the eyes, tan eyebrow markings.
// Matches the real Bailey -- a tan/brown hound mix with white markings.
// `frame_lift` shifts the body up by a pixel for the breathing frame.
// `closed_eyes` for sleep, `tongue` for happy poses, `sad_mouth` for sad.
// `size_scale` controls puppy vs adult vs senior proportions.
// `mode` selects ambient pose variants (Sit / Bark / Pant).
void draw_bailey(uint8_t* buf,
                 int  frame_lift,
                 bool closed_eyes,
                 bool tongue,
                 bool sad_mouth,
                 float size_scale,
                 uint8_t body_color = BD,
                 uint8_t highlight  = HL,
                 BehaviorMode mode  = BM_Default) {
  std::memset(buf, TR, W * H);

  const int cx = W / 2;
  const int cy = H / 2 + 6 - frame_lift;

  const int body_rx = (int)(18 * size_scale);
  const int body_ry = (int)(12 * size_scale);
  const int head_r  = (int)(11 * size_scale);

  // Tail (curls behind right hip) -- brown body with a white tip.
  const int tail_len = (int)(14 * size_scale);
  for (int i = 0; i < tail_len; ++i) {
    int tx = cx + body_rx - 4 + i;
    int ty = cy - 2 - i / 2;
    uint8_t c = (i >= tail_len - 3) ? WH : body_color;
    pset(buf, W, H, tx,     ty, c);
    pset(buf, W, H, tx + 1, ty, c);
    pset(buf, W, H, tx,     ty + 1, c);
  }

  // Legs: WHITE all the way up (matches real Bailey's white legs + paws).
  auto draw_leg = [&](int x, int y, int w, int h) {
    fill_rect(buf, W, H, x, y, w, h, WH);
  };
  // Sit: hind legs tuck out of sight; front legs stay extended.
  if (mode != BM_Sit) {
    draw_leg(cx + 4, cy + body_ry - 2, 4, 6);
    draw_leg(cx + 9, cy + body_ry - 2, 4, 6);
  }
  // Front legs
  draw_leg(cx - 12, cy + body_ry - 2, 4, 7);
  draw_leg(cx -  7, cy + body_ry - 2, 4, 7);

  // Body
  fill_ellipse(buf, W, H, cx, cy, body_rx, body_ry, body_color);
  // White belly patch (larger than before, more of an oval to the side)
  fill_ellipse(buf, W, H, cx - 2, cy + 4, body_rx - 6, body_ry - 4, WH);
  // White chest blaze: a narrow vertical stripe from belly toward the neck
  for (int dy = -body_ry; dy <= body_ry - 5; ++dy) {
    pset(buf, W, H, cx - body_rx + 6, cy + dy, WH);
    pset(buf, W, H, cx - body_rx + 7, cy + dy, WH);
  }

  // Head (offset left/forward of body). For Bark, lift the head a couple
  // of pixels and tilt slightly so the open-mouth pose reads.
  const int hx = cx - body_rx + 4;
  int       hy = cy - body_ry + 2;
  if (mode == BM_Bark) hy -= 3;
  fill_ellipse(buf, W, H, hx, hy, head_r, head_r - 1, body_color);

  // Floppy ears (drooping past the jaw on both sides of the head) -- darker
  // brown for the classic bandit-mask hound look.
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

  // White muzzle (wraps lower face, larger than before)
  fill_ellipse(buf, W, H, hx, hy + 4, 7, 5, WH);

  // Dark patches around the eyes (bandit mask) - subtle, just a band.
  // Skips the center 3 columns so the white facial blaze can run through.
  for (int x = hx - 5; x <= hx + 5; ++x) {
    if (x >= hx - 1 && x <= hx + 1) continue;
    if ((x + hy) % 2 == 0) pset(buf, W, H, x, hy - 2, OL);
  }

  // White facial blaze: classic stripe running from the forehead down
  // between the eyes to the muzzle (the nose will overdraw on top later).
  for (int y = hy - head_r + 2; y <= hy + 2; ++y) {
    pset(buf, W, H, hx - 1, y, WH);
    pset(buf, W, H, hx,     y, WH);
    pset(buf, W, H, hx + 1, y, WH);
  }

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

  // Tan eyebrow markings (above each eye)
  pset(buf, W, H, hx - 4, hy - 3, highlight);
  pset(buf, W, H, hx - 3, hy - 3, highlight);
  pset(buf, W, H, hx + 3, hy - 3, highlight);
  pset(buf, W, H, hx + 4, hy - 3, highlight);

  // Nose (sits on white muzzle now)
  pset(buf, W, H, hx - 1, hy + 3, BK);
  pset(buf, W, H, hx,     hy + 3, BK);
  pset(buf, W, H, hx + 1, hy + 3, BK);
  pset(buf, W, H, hx,     hy + 4, BK);

  // Mouth + optional tongue / sad mouth / bark / pant
  if (mode == BM_Bark) {
    // Open mouth: black oval with pink tongue interior + motion lines.
    for (int dy = 5; dy <= 8; ++dy)
      for (int dx = -2; dx <= 2; ++dx)
        pset(buf, W, H, hx + dx, hy + dy, BK);
    pset(buf, W, H, hx,     hy + 7, PK);
    pset(buf, W, H, hx - 1, hy + 7, PK);
    pset(buf, W, H, hx + 1, hy + 7, PK);
    // motion lines on each side of the head (suggest a "WOOF!" burst)
    for (int dx = 0; dx < 4; ++dx) {
      pset(buf, W, H, hx - 8 - dx, hy - 2 - dx, OL);
      pset(buf, W, H, hx + 8 + dx, hy - 2 - dx, OL);
    }
  } else if (sad_mouth) {
    pset(buf, W, H, hx - 2, hy + 7, BK);
    pset(buf, W, H, hx - 1, hy + 6, BK);
    pset(buf, W, H, hx,     hy + 6, BK);
    pset(buf, W, H, hx + 1, hy + 6, BK);
    pset(buf, W, H, hx + 2, hy + 7, BK);
  } else {
    pset(buf, W, H, hx - 1, hy + 6, BK);
    pset(buf, W, H, hx,     hy + 6, BK);
    pset(buf, W, H, hx + 1, hy + 6, BK);
    if (tongue || mode == BM_Pant) {
      pset(buf, W, H, hx,     hy + 7, PK);
      pset(buf, W, H, hx + 1, hy + 7, PK);
      pset(buf, W, H, hx,     hy + 8, PK);
      if (mode == BM_Pant) {
        // long dangling tongue
        pset(buf, W, H, hx,     hy + 9,  PK);
        pset(buf, W, H, hx + 1, hy + 9,  PK);
        pset(buf, W, H, hx,     hy + 10, PK);
      }
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
    case PetPose::Sit:     draw_bailey(buf, 0, false, false, false, size_scale, BD, HL, BM_Sit);  break;
    case PetPose::Bark:    draw_bailey(buf, 0, false, false, false, size_scale, BD, HL, BM_Bark); break;
    case PetPose::Pant:    draw_bailey(buf, 0, false, true,  false, size_scale, BD, HL, BM_Pant); break;
    case PetPose::COUNT:   break;
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
    for (int p = 0; p < (int)PetPose::COUNT; ++p) {
      if (p == (int)PetPose::Gone)
        draw_pose(g_pet[s][p], PetPose::Gone, scale);
      else
        draw_pose(g_pet[s][p], (PetPose)p, scale);
    }
  }
  // Stage 3 (legacy Gone) sprites: all the gone pose for safety.
  for (int p = 0; p < (int)PetPose::COUNT; ++p) {
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
  if (p < 0 || p >= (int)PetPose::COUNT) p = 0;
  return g_pet[s][p];
}

const uint8_t* food_bowl_sprite() { return g_food; }
const uint8_t* ball_sprite()      { return g_ball; }
const uint8_t* poop_sprite()      { return g_poop; }
const uint8_t* bubble_sprite()    { return g_bubble; }
const uint8_t* zzz_sprite()       { return g_zzz; }
const uint8_t* heart_sprite()     { return g_heart; }

}  // namespace tama
