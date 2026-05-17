#pragma once

#include <cstdint>

#include "tama/renderer.h"

namespace bailey {

// 240x240 RGB565 back-buffer. present() converts to RGBA8888 and ships
// the resulting bytes to JS, which calls putImageData() on the canvas.
class WebRenderer final : public tama::Renderer {
 public:
  WebRenderer();

  int width()  const override { return 240; }
  int height() const override { return 240; }

  void clear(uint16_t color) override;
  void fillRect(int x, int y, int w, int h, uint16_t color) override;
  void drawRect(int x, int y, int w, int h, uint16_t color) override;
  void drawHLine(int x, int y, int w, uint16_t color) override;
  void drawVLine(int x, int y, int h, uint16_t color) override;
  void drawPixel(int x, int y, uint16_t color) override;
  void drawSprite(int x, int y, int w, int h,
                  const uint8_t* data, const uint16_t* palette, int scale) override;
  void drawText(int x, int y, const char* text, uint16_t fg, int scale) override;
  void present() override;

 private:
  static constexpr int kW = 240;
  static constexpr int kH = 240;
  uint16_t back_[kW * kH];
  uint8_t  front_rgba_[kW * kH * 4];

  inline void pixel(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= kW || y >= kH) return;
    back_[y * kW + x] = c;
  }
};

}  // namespace bailey
