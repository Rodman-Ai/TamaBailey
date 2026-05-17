#include "esp_renderer.h"

#include "pins.h"
#include "tama/font_6x8.h"

namespace bailey {

LGFX_ST7789_240x240::LGFX_ST7789_240x240() {
  {
    auto cfg = bus_.config();
    cfg.spi_host    = SPI2_HOST;
    cfg.spi_mode    = 0;
    cfg.freq_write  = 40000000;
    cfg.freq_read   = 16000000;
    cfg.spi_3wire   = false;
    cfg.use_lock    = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk    = PIN_LCD_SCLK;
    cfg.pin_mosi    = PIN_LCD_MOSI;
    cfg.pin_miso    = PIN_LCD_MISO;
    cfg.pin_dc      = PIN_LCD_DC;
    bus_.config(cfg);
    panel_.setBus(&bus_);
  }
  {
    auto cfg = panel_.config();
    cfg.pin_cs           = PIN_LCD_CS;
    cfg.pin_rst          = PIN_LCD_RST;
    cfg.pin_busy         = -1;
    cfg.panel_width      = 240;
    cfg.panel_height     = 240;
    cfg.offset_x         = 0;
    cfg.offset_y         = 0;
    cfg.offset_rotation  = 0;
    cfg.dummy_read_pixel = 8;
    cfg.dummy_read_bits  = 1;
    cfg.readable         = false;
    cfg.invert           = true;
    cfg.rgb_order        = false;
    cfg.dlen_16bit       = false;
    cfg.bus_shared       = false;
    panel_.config(cfg);
  }
  {
    auto cfg = light_.config();
    cfg.pin_bl      = PIN_LCD_BL;
    cfg.invert      = false;
    cfg.freq        = 12000;
    cfg.pwm_channel = 7;
    light_.config(cfg);
    panel_.setLight(&light_);
  }
  setPanel(&panel_);
}

EspRenderer::EspRenderer(LGFX_ST7789_240x240& lcd) : lcd_(lcd), canvas_(&lcd) {}

bool EspRenderer::init() {
  canvas_.setColorDepth(16);
  // Prefer PSRAM for the back-buffer (~115 KB).
  canvas_.setPsram(true);
  if (!canvas_.createSprite(240, 240)) {
    canvas_.setPsram(false);
    if (!canvas_.createSprite(240, 240)) return false;
  }
  canvas_.fillScreen(0);
  return true;
}

void EspRenderer::clear(uint16_t color)                                   { canvas_.fillScreen(color); }
void EspRenderer::fillRect(int x, int y, int w, int h, uint16_t c)        { canvas_.fillRect(x, y, w, h, c); }
void EspRenderer::drawRect(int x, int y, int w, int h, uint16_t c)        { canvas_.drawRect(x, y, w, h, c); }
void EspRenderer::drawHLine(int x, int y, int w, uint16_t c)              { canvas_.drawFastHLine(x, y, w, c); }
void EspRenderer::drawVLine(int x, int y, int h, uint16_t c)              { canvas_.drawFastVLine(x, y, h, c); }
void EspRenderer::drawPixel(int x, int y, uint16_t c)                     { canvas_.drawPixel(x, y, c); }

void EspRenderer::drawSprite(int x, int y, int w, int h,
                             const uint8_t* data, const uint16_t* palette,
                             int scale) {
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      uint8_t idx = data[j * w + i];
      if (idx == 0) continue;  // transparent
      uint16_t col = palette[idx];
      if (scale == 1) {
        canvas_.drawPixel(x + i, y + j, col);
      } else {
        canvas_.fillRect(x + i * scale, y + j * scale, scale, scale, col);
      }
    }
  }
}

void EspRenderer::drawText(int x, int y, const char* text, uint16_t fg, int scale) {
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
          if (scale == 1) canvas_.drawPixel(cx + col, y + row, fg);
          else            canvas_.fillRect(cx + col * scale, y + row * scale, scale, scale, fg);
        }
      }
    }
    cx += 6 * scale;
  }
}

void EspRenderer::present() {
  canvas_.pushSprite(0, 0);
}

}  // namespace bailey
