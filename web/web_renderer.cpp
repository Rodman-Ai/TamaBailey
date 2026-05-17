#include "web_renderer.h"

#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "tama/font_6x8.h"

namespace bailey {

WebRenderer::WebRenderer() {
  std::memset(back_, 0, sizeof(back_));
  std::memset(front_rgba_, 0, sizeof(front_rgba_));
}

void WebRenderer::clear(uint16_t c) {
  // Word fill
  for (int i = 0; i < kW * kH; ++i) back_[i] = c;
}

void WebRenderer::fillRect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > kW) w = kW - x;
  if (y + h > kH) h = kH - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; ++j) {
    uint16_t* row = &back_[(y + j) * kW + x];
    for (int i = 0; i < w; ++i) row[i] = c;
  }
}

void WebRenderer::drawRect(int x, int y, int w, int h, uint16_t c) {
  drawHLine(x, y,         w, c);
  drawHLine(x, y + h - 1, w, c);
  drawVLine(x,         y, h, c);
  drawVLine(x + w - 1, y, h, c);
}

void WebRenderer::drawHLine(int x, int y, int w, uint16_t c) {
  if (y < 0 || y >= kH) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > kW) w = kW - x;
  for (int i = 0; i < w; ++i) back_[y * kW + x + i] = c;
}

void WebRenderer::drawVLine(int x, int y, int h, uint16_t c) {
  if (x < 0 || x >= kW) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > kH) h = kH - y;
  for (int j = 0; j < h; ++j) back_[(y + j) * kW + x] = c;
}

void WebRenderer::drawPixel(int x, int y, uint16_t c) { pixel(x, y, c); }

void WebRenderer::drawSprite(int x, int y, int w, int h,
                             const uint8_t* data, const uint16_t* palette,
                             int scale) {
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      uint8_t idx = data[j * w + i];
      if (idx == 0) continue;
      uint16_t col = palette[idx];
      if (scale == 1) pixel(x + i, y + j, col);
      else fillRect(x + i * scale, y + j * scale, scale, scale, col);
    }
  }
}

void WebRenderer::drawText(int x, int y, const char* text, uint16_t fg, int scale) {
  if (!text) return;
  int cx = x;
  while (*text) {
    char c = *text++;
    int idx = (unsigned char)c - 0x20;
    if (idx < 0 || idx >= 96) idx = '?' - 0x20;
    const uint8_t* glyph = tama::kFont6x8[idx];
    for (int col = 0; col < 6; ++col) {
      uint8_t bits = glyph[col];
      for (int row = 0; row < 8; ++row) {
        if (bits & (1u << row)) {
          if (scale == 1) pixel(cx + col, y + row, fg);
          else fillRect(cx + col * scale, y + row * scale, scale, scale, fg);
        }
      }
    }
    cx += 6 * scale;
  }
}

void WebRenderer::present() {
  // RGB565 -> RGBA8888
  for (int i = 0; i < kW * kH; ++i) {
    uint16_t c = back_[i];
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >>  5) & 0x3F) << 2;
    uint8_t b = ( c        & 0x1F) << 3;
    front_rgba_[i * 4 + 0] = r | (r >> 5);
    front_rgba_[i * 4 + 1] = g | (g >> 6);
    front_rgba_[i * 4 + 2] = b | (b >> 5);
    front_rgba_[i * 4 + 3] = 0xFF;
  }
#ifdef __EMSCRIPTEN__
  EM_ASM({
    if (typeof Module === 'undefined' || !Module.baileyPresent) return;
    Module.baileyPresent($0, $1, $2);
  }, front_rgba_, kW, kH);
#endif
}

}  // namespace bailey
