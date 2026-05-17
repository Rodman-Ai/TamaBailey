# TamaBailey

A Tamagotchi-style virtual pet of my hound dog **Bailey**. Runs on a
Waveshare ESP32-S3-Touch-LCD-1.54 *and* in your browser via WebAssembly.

The same C++ game logic powers both -- there's no port and no drift.
Hardware adapters live in `src/` (LovyanGFX + buttons + optional touch +
Preferences); the browser adapter lives in `web/` (canvas 2D +
localStorage). Both call into a hardware-agnostic core in `lib/tama_core/`.

## Play it in your browser

After the first push to `main`, the workflow in
`.github/workflows/pages.yml` builds and deploys the WebAssembly bundle
to GitHub Pages. Visit:

> `https://<owner>.github.io/TamaBailey/`

**One-time Pages setup:** in repo Settings -> Pages, set *Source* to
"GitHub Actions". That's it; future pushes deploy automatically.

## Hardware

- **Board:** [Waveshare ESP32-S3-Touch-LCD-1.54](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.54)
  (ESP32-S3, 16 MB flash, 8 MB PSRAM)
- **Display:** 240x240 IPS, ST7789 driver, 4-wire SPI
- **Buttons:** 3 onboard tactile buttons (GPIO 0 / 5 / 4, active low)
- **Touch (optional):** CST816 capacitive panel on I2C (GPIO 42/41/48/47)
- **USB:** native USB CDC for serial + flashing

All pin numbers live in [`include/pins.h`](include/pins.h) and were
verified from Waveshare's own demo sketches at
[`waveshareteam/ESP32-S3-Touch-LCD-1.54`](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.54).

## Gameplay

| Input            | Action                                               |
|------------------|------------------------------------------------------|
| BTN A / `A`      | **Feed** Bailey (+30 food)                           |
| BTN B / `B`      | **Play** with Bailey (+30 play, -10 rest)            |
| BTN C / `C`      | **Clean** Bailey (+60 bath)                          |
| Long-press (~0.8 s) any button | Toggle **status menu**                 |
| Tap pet (touch / canvas click) | **Pet** Bailey (+5 play, 12 s cooldown) |
| Tap stats bar                  | Toggle status menu                     |
| `R` (web)                      | Restart when Bailey is gone            |

- Four stats (food / play / bath / rest) decay over real time.
- Rest regenerates while Bailey isn't actively playing.
- Mood is driven by the stats: happy, hungry, dirty, sleeping, sad, gone.
- Keep all stats >= 30 for 24 h to evolve Puppy -> Adult, 96 h -> Senior.
- Neglect all stats to 0 for 60 minutes and Bailey is gone. Long-press
  any button to hatch a fresh puppy.

For testing, build with `-D BAILEY_FAST_DECAY=1` (or use the
`esp32-s3-lcd-1_54-fast` PIO env) to collapse the 12 h / 24 h timers
into 12 min / 24 min.

## Build and flash (device)

```sh
# Install PlatformIO CLI
pip install platformio

# Compile and flash (classic Tamagotchi pace)
pio run -e esp32-s3-lcd-1_54 -t upload

# Serial monitor
pio device monitor

# Demo build with fast stat decay
pio run -e esp32-s3-lcd-1_54-fast -t upload
```

### Download mode

The ESP32-S3 auto-resets into the bootloader over USB CDC. If a flash
fails: hold **BOOT**, tap **RESET**, release **BOOT**, then re-run the
upload.

### Wi-Fi credentials (not used in MVP, but wired up)

```sh
cp include/secrets.h.example include/secrets.h
# Edit include/secrets.h with your SSID / password.
# include/secrets.h is gitignored.
```

## Build and run the web version locally

```sh
# Install and activate emsdk first (https://emscripten.org/docs/getting_started/downloads.html)
cd path/to/emsdk && ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh

# Build the bundle
cd path/to/TamaBailey
bash web/build.sh

# Serve and open
python3 -m http.server -d web/dist 8000
# -> http://localhost:8000
```

Save state is stored in `localStorage` under `tama_bailey_save_v1`.

## Replacing the placeholder art with your own

The sprites you see by default are drawn procedurally by
[`lib/tama_core/src/sprites.cpp`](lib/tama_core/src/sprites.cpp).
To swap in your own pixel art:

1. Draw a 48x48 PNG (transparent background) of each pose you want to
   replace. Stick to the 16-color palette in `tama/colors.h` for the
   best fidelity.
2. Convert each PNG to a C++ indexed-color array:
   ```sh
   pip install Pillow
   python scripts/png_to_sprite.py assets/sprites/bailey_idle_a.png \
     --name bailey_idle_a \
     --output lib/tama_core/src/sprites_user.cpp
   ```
3. Wire your generated arrays into `pet_sprite()` by editing
   `lib/tama_core/src/sprites.cpp` -- replace the relevant procedural
   draw call with a `std::memcpy` from your array.

Re-flash the device and rebuild the web bundle; both sides pick up the
new art.

## Repository layout

```
TamaBailey/
├── platformio.ini              # ESP32-S3 / Arduino / LovyanGFX build
├── include/
│   ├── pins.h                  # verified Waveshare 1.54" pin map
│   └── secrets.h.example       # Wi-Fi creds template (unused in MVP)
├── src/                        # ESP32 adapter
│   ├── main.cpp
│   ├── esp_renderer.{h,cpp}    # LovyanGFX-backed renderer
│   ├── esp_storage.{h,cpp}     # Preferences-backed storage
│   └── esp_input.{h,cpp}       # OneButton-backed input
├── lib/tama_core/              # PURE C++17 GAME CORE (no Arduino/Em deps)
│   ├── include/tama/*.h        # game, pet, stats, input, renderer ...
│   └── src/*.cpp               # game logic, sprites, font, UI
├── web/                        # browser adapter (Emscripten target)
│   ├── main_web.cpp            # bailey_init / bailey_frame entry points
│   ├── web_renderer.{h,cpp}    # canvas back-buffer
│   ├── web_storage.h           # localStorage via EM_ASM
│   ├── index.html style.css shell.js
│   └── build.sh                # emcc wrapper
├── scripts/png_to_sprite.py    # PNG -> indexed C++ array
├── .github/workflows/
│   ├── firmware.yml            # PlatformIO build on push/PR
│   └── pages.yml               # Wasm build + Pages deploy on main
├── assets/sprites/             # your own art lives here (empty)
└── docs/
```

## What's in (Phase 2)

- **Fetch mini-game** -- when Bailey is an Adult or older and not sick,
  pressing Play (B / `B`) starts a fetch flow. The ball arcs out, comes
  back, and you have to time a second B press during the catch window
  for the full happiness bonus + a fetch-skill increment.
- **Trick learning** -- Bailey auto-learns tricks at age milestones
  (Sit / Shake / Roll Over / Speak / Spin); a Clever personality halves
  the timers. Tricks list appears in the Options menu.
- **Multiple scenes** -- living room (default), backyard, dog park,
  each with its own ambient detail. Cycle with the *Change scene*
  button in the web app.
- **Weather** -- rolls once per real-world day (sunny / cloudy / rain /
  snow). Rain accelerates bath decay; snow slows rest regen.
- **Accessories** -- bandana, blue collar, party hat -- each unlocked
  by an achievement. Cycle with *Change accessory*.
- **Coat patterns** -- 5 hound coats; Bailey prompts to pick one on
  first evolution to Adult.
- **Sickness + recovery** -- if food AND bath both stay below 20 for
  too long, Bailey gets sick (red border, sneeze). A bath cures him.

## What's in (Phase 1)

- **Web audio** -- bark / wuff / splash / heart / snore / whimper /
  sneeze / fanfare / achievement / sad clips are synthesized in C++ and
  played via the Web Audio API on the browser side.
- **Wi-Fi + NTP** on device: fill in `include/secrets.h` (copy from the
  `.example`) and Bailey gets a real day/night cycle and offline decay
  catch-up.
- **Day/night cycle** with sky / grass tinting and a moon at night
  / sun at noon. Falls back to a 24-minute synthetic loop if NTP is
  unavailable.
- **Settings menu** -- volume, brightness, decay-rate multiplier,
  timezone offset, auto-sleep, mic toggle.
- **Achievements** -- 20 unlockables shown in a Badges tab.
- **Daily login streak** -- visit on consecutive days for a happiness
  bonus.
- **Sync code** -- shareable 13-char alphanumeric code (or
  `?bailey=<code>` URL) that snapshots state between device and web.

## Known limitations (deferred)

- **Device audio (ES8311 codec)** -- Phase 1 ships an audio abstraction
  and synthesized PCM clips that work in the browser; the device's
  ES8311+I2S integration lands in a follow-up. Logging-only stub for now.
- **Touch petting on device** -- web supports click + drag to pet; the
  device's CST816 driver lands in Phase 2.
- One pet per save slot at a time (memorial wall in Phase 3 preserves
  previous lives).
- Placeholder art -- replace via the workflow above.
