#include "esp_power.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include "pins.h"

namespace bailey {

void enter_deep_sleep(int wake_gpio) {
  // Drop the LCD backlight first so the user sees the screen go dark
  // before the chip actually halts.
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, LOW);

  // Wake on the side button (active-low: wake when the pin reads 0).
  esp_sleep_enable_ext0_wakeup((gpio_num_t)wake_gpio, 0);
  esp_deep_sleep_start();
  // unreachable
}

}  // namespace bailey
