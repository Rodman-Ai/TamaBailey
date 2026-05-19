#pragma once

namespace bailey {

// Cut the LCD backlight, configure the given GPIO as the wake source
// (active-low, EXT0), and call esp_deep_sleep_start(). Never returns
// on success. wake_gpio must be one of the RTC-capable GPIOs (BTN_A
// on this board is GPIO 0, which is RTC-capable).
void enter_deep_sleep(int wake_gpio);

}  // namespace bailey
