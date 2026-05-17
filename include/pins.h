#pragma once

// =============================================================================
// TODO: VERIFY AGAINST BOARD SCHEMATIC
// -----------------------------------------------------------------------------
// These pin assignments are STARTING DEFAULTS for the Waveshare
// ESP32-S3 1.54" LCD Development Board (240x240 ST7789, 3 onboard buttons).
//
// The LCD pins below mirror the closely-related Waveshare ESP32-S3-LCD-1.47
// (same product family, similar layout). They have NOT been confirmed from
// the 1.54" schematic — Waveshare's wiki/PDF was not reachable from the
// environment where this scaffold was generated.
//
// Before assuming a hardware fault, cross-check these values against the
// schematic at: https://www.waveshare.com/wiki/ESP32-S3-LCD-1.54
//
// The hello-world in src/main.cpp prints all 3 button states to serial,
// so you can immediately identify which physical button maps to which
// GPIO and swap the PIN_BTN_* defines below if needed.
// =============================================================================

// ----- ST7789 LCD (4-wire SPI) -----
#define PIN_LCD_MOSI   45
#define PIN_LCD_SCLK   40
#define PIN_LCD_CS     42
#define PIN_LCD_DC     41
#define PIN_LCD_RST    39
#define PIN_LCD_BL     48

// ----- Onboard buttons -----
// GPIO0 is the BOOT button — universal on ESP32-S3 dev boards and safe to use
// at runtime (it only matters during reset). GPIO1 / GPIO2 are placeholders
// for the other two onboard buttons; verify against the schematic.
#define PIN_BTN_A      0
#define PIN_BTN_B      1
#define PIN_BTN_C      2

// Buttons are wired switch-to-GND with INPUT_PULLUP; pressed = LOW.
#define BTN_ACTIVE_LOW 1
