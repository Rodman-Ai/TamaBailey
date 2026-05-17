#pragma once

#include <cstdint>

namespace tama {

// Hardware-agnostic drawing interface.
//
// Coordinates are in display pixels (origin top-left). Colors are RGB565.
// Sprites are 8-bit palette indices into kSpritePalette (colors.h);
// index 0 = transparent.
class Renderer {
 public:
  virtual ~Renderer() = default;

  virtual int width()  const = 0;
  virtual int height() const = 0;

  virtual void clear(uint16_t color) = 0;
  virtual void fillRect(int x, int y, int w, int h, uint16_t color) = 0;
  virtual void drawRect(int x, int y, int w, int h, uint16_t color) = 0;
  virtual void drawHLine(int x, int y, int w, uint16_t color) = 0;
  virtual void drawVLine(int x, int y, int h, uint16_t color) = 0;
  virtual void drawPixel(int x, int y, uint16_t color) = 0;

  // Draw an indexed sprite. `data` has `w*h` bytes; each byte is a
  // palette index. `palette` has 16 RGB565 entries; index 0 is treated
  // as transparent.
  virtual void drawSprite(int x, int y, int w, int h,
                          const uint8_t* data,
                          const uint16_t* palette,
                          int scale = 1) = 0;

  // Draw a string using the built-in 6x8 bitmap font, scaled.
  virtual void drawText(int x, int y, const char* text,
                        uint16_t fg, int scale = 1) = 0;

  // Push the back-buffer to the visible screen (if applicable).
  virtual void present() = 0;
};

}  // namespace tama
