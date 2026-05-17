// TamaBailey — hello-world hardware check.
// Confirms the 240x240 ST7789 display draws text and that all 3 onboard
// buttons are wired to the expected GPIOs. No gameplay yet.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Arduino.h>

#include "pins.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = PIN_LCD_SCLK;
      cfg.pin_mosi    = PIN_LCD_MOSI;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = PIN_LCD_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
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
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl      = PIN_LCD_BL;
      cfg.invert      = false;
      cfg.freq        = 12000;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

static LGFX lcd;

static const uint8_t kBtnPins[3] = {PIN_BTN_A, PIN_BTN_B, PIN_BTN_C};
static const char    kBtnLabel[3] = {'A', 'B', 'C'};

static bool readPressed(uint8_t pin) {
  int level = digitalRead(pin);
  return BTN_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
}

static void drawScreen(const bool pressed[3]) {
  lcd.startWrite();
  lcd.fillScreen(TFT_BLACK);

  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.drawString("Hello, pet!", 120, 90);

  lcd.drawFastHLine(40, 118, 160, TFT_MAGENTA);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  lcd.drawString("Buttons:", 120, 150);

  lcd.setFont(&fonts::Font4);
  for (int i = 0; i < 3; ++i) {
    int x = 60 + i * 60;
    int y = 195;
    uint16_t fg = pressed[i] ? TFT_BLACK  : TFT_WHITE;
    uint16_t bg = pressed[i] ? TFT_GREEN  : TFT_BLACK;
    if (pressed[i]) {
      lcd.fillRoundRect(x - 22, y - 22, 44, 44, 6, bg);
    } else {
      lcd.drawRoundRect(x - 22, y - 22, 44, 44, 6, TFT_DARKGREY);
    }
    lcd.setTextColor(fg, bg);
    lcd.drawString(String(kBtnLabel[i]), x, y);
  }
  lcd.endWrite();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("TamaBailey hello-world");

  for (uint8_t i = 0; i < 3; ++i) {
    pinMode(kBtnPins[i], BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  }

  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(200);

  bool initial[3] = {false, false, false};
  drawScreen(initial);
}

void loop() {
  static bool last[3]      = {false, false, false};
  static uint32_t nextTick = 0;

  uint32_t now = millis();
  if (now < nextTick) return;
  nextTick = now + 100;

  bool pressed[3];
  for (int i = 0; i < 3; ++i) pressed[i] = readPressed(kBtnPins[i]);

  Serial.printf("BTN A=%d B=%d C=%d\n",
                pressed[0] ? 1 : 0,
                pressed[1] ? 1 : 0,
                pressed[2] ? 1 : 0);

  bool changed = false;
  for (int i = 0; i < 3; ++i) {
    if (pressed[i] != last[i]) {
      changed = true;
      last[i] = pressed[i];
    }
  }
  if (changed) drawScreen(pressed);
}
