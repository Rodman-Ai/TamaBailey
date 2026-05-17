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
uint8_t  g_friend[(int)Friend::COUNT][(int)PetPose::COUNT][W * H];
uint8_t  g_food[A * A];
uint8_t  g_food_empty[A * A];
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
                 uint8_t body_color    = BD,
                 uint8_t highlight     = HL,
                 BehaviorMode mode     = BM_Default,
                 int  ear_length       = 13,
                 int  body_rx_delta    = 0,
                 int  tail_length      = 14,
                 bool has_chest_blaze  = true,
                 uint8_t belly_color   = WH,
                 bool pointy_ears      = false) {
  std::memset(buf, TR, W * H);

  const int cx = W / 2;
  const int cy = H / 2 + 6 - frame_lift;

  const int body_rx = (int)(18 * size_scale) + body_rx_delta;
  const int body_ry = (int)(12 * size_scale);
  const int head_r  = (int)(11 * size_scale);

  // Tail (curls behind right hip) -- body color with a white tip.
  const int tail_len = (int)(tail_length * size_scale);
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
  // Sit: hind legs render as short stubby paws tucked under the body
  // (height 3) instead of being hidden entirely.
  int hind_h = (mode == BM_Sit) ? 3 : 6;
  draw_leg(cx + 4, cy + body_ry - 2, 4, hind_h);
  draw_leg(cx + 9, cy + body_ry - 2, 4, hind_h);
  // Front legs
  draw_leg(cx - 12, cy + body_ry - 2, 4, 7);
  draw_leg(cx -  7, cy + body_ry - 2, 4, 7);

  // Body
  fill_ellipse(buf, W, H, cx, cy, body_rx, body_ry, body_color);
  // Belly patch -- skip when the belly color matches the body (e.g.
  // Enzo's solid rott silhouette, Ruben's solid PWD silhouette).
  if (belly_color != body_color) {
    fill_ellipse(buf, W, H, cx - 2, cy + 4, body_rx - 6, body_ry - 4, belly_color);
  }
  // Chest blaze: a narrow vertical white stripe from belly to the neck.
  if (has_chest_blaze) {
    for (int dy = -body_ry; dy <= body_ry - 5; ++dy) {
      pset(buf, W, H, cx - body_rx + 6, cy + dy, WH);
      pset(buf, W, H, cx - body_rx + 7, cy + dy, WH);
    }
  }

  // Head (offset left/forward of body). For Bark, lift the head a couple
  // of pixels and tilt slightly so the open-mouth pose reads.
  const int hx = cx - body_rx + 4;
  int       hy = cy - body_ry + 2;
  if (mode == BM_Bark) hy -= 3;
  fill_ellipse(buf, W, H, hx, hy, head_r, head_r - 1, body_color);

  if (pointy_ears) {
    // Pointy upright ears (Jindo-style) -- two small triangles pointing
    // up from the top of the head.
    int len = (int)(7 * size_scale);
    for (int i = 0; i < len; ++i) {
      int width = len - i;
      // Left ear
      int lx = hx - head_r + 2;
      for (int j = 0; j < width; ++j) {
        pset(buf, W, H, lx + j, hy - head_r + 1 - i, body_color);
      }
      pset(buf, W, H, lx, hy - head_r + 1 - i, OL);
      pset(buf, W, H, lx + width - 1, hy - head_r + 1 - i, OL);
      // Right ear
      int rx = hx + head_r - 2 - width + 1;
      for (int j = 0; j < width; ++j) {
        pset(buf, W, H, rx + j, hy - head_r + 1 - i, body_color);
      }
      pset(buf, W, H, rx, hy - head_r + 1 - i, OL);
      pset(buf, W, H, rx + width - 1, hy - head_r + 1 - i, OL);
    }
  } else {
    // Floppy ears (drooping past the jaw on both sides of the head).
    int len = (int)(ear_length * size_scale);
    for (int i = 0; i < len; ++i) {
      int ex = hx - head_r + 1 - i / 3;
      int ey = hy - 2 + i;
      pset(buf, W, H, ex,     ey, OL);
      pset(buf, W, H, ex + 1, ey, body_color);
      pset(buf, W, H, ex + 2, ey, body_color);
      pset(buf, W, H, ex + 3, ey, highlight);
    }
    for (int i = 0; i < len; ++i) {
      int ex = hx + head_r - 4 + i / 3;
      int ey = hy - 1 + i;
      pset(buf, W, H, ex,     ey, highlight);
      pset(buf, W, H, ex + 1, ey, body_color);
      pset(buf, W, H, ex + 2, ey, body_color);
      pset(buf, W, H, ex + 3, ey, OL);
    }
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

void draw_accessory_food_empty() {
  std::memset(g_food_empty, TR, A * A);
  // Bowl interior only -- no kibble on top.
  for (int y = 10; y < 14; ++y)
    for (int x = 2; x < 14; ++x)
      g_food_empty[y * A + x] = GR;
  // Outline
  for (int x = 1; x < 15; ++x) { g_food_empty[9 * A + x] = OL; g_food_empty[13 * A + x] = OL; }
  for (int y = 9; y < 14; ++y) { g_food_empty[y * A + 1] = OL; g_food_empty[y * A + 14] = OL; }
  // Subtle shadow inside the empty bowl.
  g_food_empty[11 * A + 7] = DG;
  g_food_empty[11 * A + 8] = DG;
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

// Per-friend overlays applied AFTER draw_bailey paints the base body.
// Each overlay only modifies pixels that already belong to the body
// (so silhouette + outline stay intact).
static void overlay_brindle(uint8_t* buf, int /*w*/, int /*h*/) {
  // Ollie: diagonal dark-brown stripes on body pixels.
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      uint8_t c = buf[y * W + x];
      if (c != BD && c != HL) continue;
      if (((x + y) & 7) < 2) buf[y * W + x] = OL;   // every ~7th diag = stripe
    }
  }
}

static void overlay_tan_points(uint8_t* buf, int /*w*/, int /*h*/) {
  // Enzo: classic rott tan markings on legs + a tan eyebrow patch.
  // Replace WH (leg paws) with HL (tan), and stamp tan brows above the
  // eye band.
  for (int y = H * 7 / 10; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      uint8_t c = buf[y * W + x];
      if (c == WH) buf[y * W + x] = HL;
    }
  }
  // Tan brow stripes near where draw_bailey put the bandit mask.
  const int cx = W / 2;
  const int hy = H / 2 + 6 - 12 + 2 - 3;  // matches the brow line in draw_bailey
  for (int dx = -5; dx <= -3; ++dx) buf[hy * W + (cx - 8 + dx + 4)] = HL;
  for (int dx =  3; dx <=  5; ++dx) buf[hy * W + (cx - 8 + dx + 4)] = HL;
}

static void overlay_tuxedo(uint8_t* buf, int /*w*/, int /*h*/) {
  // Francie: Boston Terrier tuxedo -- white wedge on the upper chest
  // and dark patches around the eyes (the BT mask). Bailey's white
  // belly + paws stay; we just add the chest wedge + eye darkening.
  const int cx = W / 2;
  const int cy = H / 2 + 6;
  // White chest wedge
  for (int y = cy - 6; y <= cy + 4; ++y) {
    int half = 2 + (cy + 4 - y) / 2;
    for (int x = cx - half; x <= cx + half; ++x) {
      if (buf[y * W + x] != TR) pset(buf, W, H, x, y, WH);
    }
  }
  // Darken around the eye band (mask look)
  int hx = cx - 14;
  int hy = cy - 10;
  for (int dx = -6; dx <= 6; ++dx) {
    int x = hx + dx;
    if ((dx + 6) & 1) continue;
    if (buf[(hy - 1) * W + x] != TR) pset(buf, W, H, x, hy - 1, OL);
  }
}

static void overlay_nose_star(uint8_t* buf, int /*w*/, int /*h*/) {
  // Noshy: 3-px white cross over the nose tip. The nose is drawn at
  // (hx-1..hx+1, hy+3) + (hx, hy+4) inside draw_bailey -- compute the
  // same coordinates here.
  const int cx = W / 2;
  const int cy = H / 2 + 6;
  const int body_rx = (int)(18 * 1.0f);
  const int hx = cx - body_rx + 4;
  const int hy = cy - 12 + 2;
  pset(buf, W, H, hx,     hy + 3, WH);
  pset(buf, W, H, hx - 1, hy + 4, WH);
  pset(buf, W, H, hx,     hy + 4, WH);
  pset(buf, W, H, hx + 1, hy + 4, WH);
  pset(buf, W, H, hx,     hy + 5, WH);
}

static void overlay_curly_coat(uint8_t* buf, int /*w*/, int /*h*/) {
  // Ruben: tight curls -- flip ~25% of body pixels to outline color in
  // a high-frequency checker pattern, suggesting curly fur without
  // collapsing the silhouette.
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      uint8_t c = buf[y * W + x];
      if (c != BD && c != HL && c != DG && c != GR) continue;
      if ((((x * 5) ^ (y * 3)) & 3) == 0) buf[y * W + x] = OL;
    }
  }
}

static void overlay_long_fur(uint8_t* buf, int /*w*/, int /*h*/) {
  // Lincoln: add a soft fluff ring around the existing outline by
  // dropping a sparse OL halo just outside the silhouette.
  uint8_t tmp[W * H];
  std::memcpy(tmp, buf, W * H);
  for (int y = 1; y < H - 1; ++y) {
    for (int x = 1; x < W - 1; ++x) {
      if (tmp[y * W + x] != TR) continue;
      // Adjacent to outline AND every 3rd-pixel sparse pattern.
      bool touch = (tmp[(y - 1) * W + x] == OL ||
                    tmp[(y + 1) * W + x] == OL ||
                    tmp[y * W + (x - 1)] == OL ||
                    tmp[y * W + (x + 1)] == OL);
      if (touch && ((x + y * 3) % 4) == 0) buf[y * W + x] = OL;
    }
  }
}

void draw_friend_pose(uint8_t* buf, Friend f, PetPose pose) {
  float scale = friend_size_scale(f);
  // Per-friend skin: body / highlight / blaze / belly / ear-style.
  uint8_t body, hi, belly = WH;
  bool    blaze       = true;
  bool    pointy_ear  = false;
  switch (f) {
    case Friend::Ollie:    body = BD; hi = HL; break;
    case Friend::Mitchell: body = WH; hi = GR; break;
    case Friend::Enzo:     body = DG; hi = OL; blaze = false; belly = DG; break;
    case Friend::Lincoln:  body = YL; hi = OR; belly = CR; break;
    case Friend::Ruben:    body = DG; hi = GR; blaze = false; belly = DG; break;
    case Friend::Francie:  body = BK; hi = DG; break;  // tuxedo overlay covers the rest
    case Friend::Bomi:     body = HL; hi = OR; blaze = false; pointy_ear = true; break;
    case Friend::Noshy:    body = DG; hi = GR; blaze = false; belly = DG; break;
    default:               body = BD; hi = HL; break;
  }

  // Map PetPose -> the same BehaviorMode + flags draw_pose would pick.
  int  lift     = (pose == PetPose::IdleB || pose == PetPose::Playing) ? 1 : 0;
  bool eyes_sh  = (pose == PetPose::Sleep);
  bool tongue   = (pose == PetPose::Playing || pose == PetPose::Pant);
  bool sad      = (pose == PetPose::Sad);
  BehaviorMode mode =
      (pose == PetPose::Sit)  ? BM_Sit  :
      (pose == PetPose::Bark) ? BM_Bark :
      (pose == PetPose::Pant) ? BM_Pant :
                                BM_Default;

  if (pose == PetPose::Gone) {
    draw_bailey(buf, 0, false, false, false, scale, body, hi, BM_Default,
                /*ear_len*/13, /*body_rx_delta*/0, /*tail_len*/14,
                blaze, belly, pointy_ear);
  } else {
    draw_bailey(buf, lift, eyes_sh, tongue, sad, scale, body, hi, mode,
                /*ear_len*/13, /*body_rx_delta*/0, /*tail_len*/14,
                blaze, belly, pointy_ear);
  }

  switch (f) {
    case Friend::Ollie:    overlay_brindle(buf, W, H); break;
    case Friend::Enzo:     overlay_tan_points(buf, W, H); break;
    case Friend::Lincoln:  overlay_long_fur(buf, W, H); break;
    case Friend::Ruben:    overlay_curly_coat(buf, W, H); break;
    case Friend::Francie:  overlay_tuxedo(buf, W, H); break;
    case Friend::Noshy:    overlay_curly_coat(buf, W, H);
                           overlay_nose_star(buf, W, H); break;
    case Friend::Mitchell:
    case Friend::Bomi:     break;  // body color + ears do the work
    default: break;
  }
}

// Per-life-stage Bailey config (ear length, body trim, tail length).
struct BaileyConfig {
  int ear_length;
  int body_rx_delta;
  int tail_length;
};

static BaileyConfig bailey_config_for_stage(int stage) {
  // stage: 0=Puppy, 1=Adult, 2=Senior, 3=legacy Gone
  switch (stage) {
    case 0:  return BaileyConfig{13,  0, 14};
    case 1:  return BaileyConfig{ 9, -2, 18};
    case 2:  return BaileyConfig{ 9, -2, 18};
    default: return BaileyConfig{13,  0, 14};
  }
}

void draw_pose(uint8_t* buf, PetPose pose, float size_scale,
               int stage = 0) {
  BaileyConfig c = bailey_config_for_stage(stage);
  switch (pose) {
    case PetPose::IdleA:   draw_bailey(buf, 0, false, false, false, size_scale,
                                       BD, HL, BM_Default,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::IdleB:   draw_bailey(buf, 1, false, false, false, size_scale,
                                       BD, HL, BM_Default,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::Eating:  draw_bailey(buf, 0, false, false, false, size_scale,
                                       BD, HL, BM_Default,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::Playing: draw_bailey(buf, 1, false, true,  false, size_scale,
                                       BD, HL, BM_Default,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::Sleep:   draw_bailey(buf, 0, true,  false, false, size_scale,
                                       BD, HL, BM_Default,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::Sad:     draw_bailey(buf, 0, false, false, true,  size_scale,
                                       BD, HL, BM_Default,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::Sit:     draw_bailey(buf, 0, false, false, false, size_scale,
                                       BD, HL, BM_Sit,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::Bark:    draw_bailey(buf, 0, false, false, false, size_scale,
                                       BD, HL, BM_Bark,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
    case PetPose::Pant:    draw_bailey(buf, 0, false, true,  false, size_scale,
                                       BD, HL, BM_Pant,
                                       c.ear_length, c.body_rx_delta, c.tail_length); break;
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
        draw_pose(g_pet[s][p], PetPose::Gone, scale, s);
      else
        draw_pose(g_pet[s][p], (PetPose)p, scale, s);
    }
  }
  // Stage 3 (legacy Gone) sprites: all the gone pose for safety.
  for (int p = 0; p < (int)PetPose::COUNT; ++p) {
    draw_pose(g_pet[3][p], PetPose::Gone, 1.0f);
  }

  // Friend sprites (Ollie/Mitchell/Enzo/Lincoln) -- all poses.
  for (int fi = 0; fi < (int)Friend::COUNT; ++fi) {
    for (int p = 0; p < (int)PetPose::COUNT; ++p) {
      draw_friend_pose(g_friend[fi][p], (Friend)fi, (PetPose)p);
    }
  }

  draw_accessory_food();
  draw_accessory_food_empty();
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

const uint8_t* friend_sprite(Friend f, PetPose pose) {
  int fi = (int)f;
  int p  = (int)pose;
  if (fi < 0 || fi >= (int)Friend::COUNT) fi = 0;
  if (p  < 0 || p  >= (int)PetPose::COUNT) p = 0;
  return g_friend[fi][p];
}

const uint8_t* food_bowl_sprite()       { return g_food; }
const uint8_t* food_bowl_empty_sprite() { return g_food_empty; }
const uint8_t* ball_sprite()      { return g_ball; }
const uint8_t* poop_sprite()      { return g_poop; }
const uint8_t* bubble_sprite()    { return g_bubble; }
const uint8_t* zzz_sprite()       { return g_zzz; }
const uint8_t* heart_sprite()     { return g_heart; }

}  // namespace tama
