#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "tama/renderer.h"

namespace bailey {

class LGFX_ST7789_240x240 : public lgfx::LGFX_Device {
 public:
  LGFX_ST7789_240x240();
 private:
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI      bus_;
  lgfx::Light_PWM    light_;
};

// Renders the tama::Renderer primitives into a 240x240 RGB565 back-buffer
// sprite; present() pushes it to the real LCD with a single DMA transfer.
class EspRenderer final : public tama::Renderer {
 public:
  EspRenderer(LGFX_ST7789_240x240& lcd);
  bool init();   // must be called after lcd.init()

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
  LGFX_ST7789_240x240& lcd_;
  lgfx::LGFX_Sprite    canvas_;
};

}  // namespace bailey
