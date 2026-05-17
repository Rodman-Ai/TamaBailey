#pragma once

// =============================================================================
// Pin assignments for the Waveshare ESP32-S3-Touch-LCD-1.54.
//
// Verified from Waveshare's own demos in:
//   waveshareteam/ESP32-S3-Touch-LCD-1.54
//     examples/ESP32-S3-Touch-LCD-1.54-demo/Arduino-3.2.0/examples/
//       02_button_example/02_button_example.ino   -> button GPIOs
//       04_gfx_helloworld/04_gfx_helloworld.ino   -> ST7789 + backlight pins
//       08_lvgl_arduino_v8/08_lvgl_arduino_v8.ino -> CST816 touch pins
//
// Both the touch and non-touch 1.54" variants share the same display & button
// pin map; the touch variant additionally exposes the I2C touch panel.
// =============================================================================

// ----- ST7789 LCD (4-wire SPI, 240x240) -----
#define PIN_LCD_DC     45
#define PIN_LCD_CS     21
#define PIN_LCD_SCLK   38
#define PIN_LCD_MOSI   39
#define PIN_LCD_MISO   -1
#define PIN_LCD_RST    40
#define PIN_LCD_BL     46

// ----- Onboard buttons -----
// All three are wired switch-to-GND; use INPUT_PULLUP and treat LOW as pressed.
// BTN_A is also the BOOT button (GPIO0); it's safe to use at runtime.
#define PIN_BTN_A      0
#define PIN_BTN_B      5
#define PIN_BTN_C      4
#define BTN_ACTIVE_LOW 1

// ----- CST816 capacitive touch panel (touch variant only, I2C) -----
#define PIN_TOUCH_SDA  42
#define PIN_TOUCH_SCL  41
#define PIN_TOUCH_IRQ  48
#define PIN_TOUCH_RST  47

// ----- QMI8658 6-axis IMU (I2C, shared bus with touch + audio codec) -----
#define PIN_IMU_SDA    42
#define PIN_IMU_SCL    41
#define PIN_IMU_IRQ     6

// ----- Audio: ES8311 codec over I2C + I2S DAC -----
#define PIN_AUDIO_PA_CTRL   7
#define PIN_AUDIO_I2S_MCLK  8
#define PIN_AUDIO_I2S_BCLK  9
#define PIN_AUDIO_I2S_LRC  10
#define PIN_AUDIO_I2S_DOUT 12
#define PIN_AUDIO_I2C_SDA  42
#define PIN_AUDIO_I2C_SCL  41
