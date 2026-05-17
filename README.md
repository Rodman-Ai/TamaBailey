# TamaBailey

A Tamagotchi-style desk pet of my dog Bailey, running on a Waveshare
ESP32-S3 1.54" LCD development board.

This README covers the **scaffold / hello-world** stage. Gameplay (stats,
decay, sprite animation, evolution, Preferences persistence) is not yet
implemented — flash this first to confirm the display and buttons work.

## Hardware

- **Board:** [Waveshare ESP32-S3 1.54" LCD Development Board](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.54)
  (ESP32-S3, 16 MB flash, 8 MB PSRAM)
- **Display:** 240×240 IPS, ST7789 driver, 4-wire SPI, 262K color
- **Inputs:** 3 onboard buttons (one is the BOOT button on GPIO0)
- **USB:** native USB CDC for serial + flashing (no FTDI bridge)

### GPIO pin assignments

All pin numbers live in [`include/pins.h`](include/pins.h). The LCD defaults
mirror the closely-related ESP32-S3-LCD-1.47 and the button defaults are
`GPIO0` (BOOT) + `GPIO1` + `GPIO2`. **These have not been confirmed against
the 1.54" schematic** — please verify them once and edit that one file if
anything is wrong. The hello-world prints all 3 button states to serial so
you can spot a mis-mapped button immediately.

## Setup

1. Install [PlatformIO](https://platformio.org/install) — either the VS Code
   extension or the standalone CLI (`pip install platformio`).
2. Clone this repo and open it in PlatformIO.
3. Copy the Wi-Fi credentials template (not used by the hello-world, but
   needed once we wire up future features):
   ```sh
   cp include/secrets.h.example include/secrets.h
   # then edit include/secrets.h with your SSID/password
   ```
   `include/secrets.h` is gitignored.

## Build & flash

```sh
pio run                 # compile
pio run -t upload       # compile + flash over USB
pio device monitor      # 115200 baud serial monitor
```

If `pio run -t upload` can't find the board or reset stalls, see
[Download mode](#download-mode) below.

## Download mode

The ESP32-S3 auto-resets into the bootloader over USB CDC, so you usually
don't need to do anything manual. If upload fails:

1. Hold **BOOT**.
2. Tap **RESET** (also called **EN**).
3. Release **BOOT**.

The board is now in download mode — re-run `pio run -t upload`. Tap
**RESET** once more after flashing to run the firmware.

## Verifying the hello-world

After flashing, you should see:

- **On the display:** centered text `Hello, pet!`, a magenta accent line
  below it, and three labelled tiles (`A` `B` `C`) at the bottom. A tile
  fills green when you hold its button.
- **On serial (115200 baud):** a line every 100 ms like
  ```
  BTN A=1 B=1 C=1
  ```
  A `1` means released, `0` means pressed.

### Troubleshooting

- **Screen is blank / backlight off:** the LCD pins in `include/pins.h`
  don't match your board. Cross-check against the schematic on the
  [Waveshare wiki](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.54)
  and edit the file.
- **Wrong button lights up when you press one:** swap the `PIN_BTN_A` /
  `PIN_BTN_B` / `PIN_BTN_C` values in `include/pins.h`.
- **Nothing on serial:** make sure `monitor_speed` matches (115200) and
  that you've granted permission to the USB serial device on your OS.

## Repository layout

```
TamaBailey/
├── platformio.ini           # PlatformIO config (esp32-s3-devkitc-1, Arduino)
├── src/main.cpp             # hello-world sketch
├── include/
│   ├── pins.h               # all GPIO defines — verify against schematic
│   └── secrets.h.example    # Wi-Fi credentials template
├── lib/                     # project-private libraries (empty for now)
├── assets/sprites/          # pet sprite art (empty for now)
└── docs/                    # design notes (empty for now)
```
