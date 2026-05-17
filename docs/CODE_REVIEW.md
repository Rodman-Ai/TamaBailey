# TamaBailey code review (pre-flash audit)

A read-through of the codebase before the first hardware flash.
Findings are split into **blocking** (fix before flashing),
**non-blocking** (worth knowing about), and **follow-ups** (deferred
work that's deliberately out of scope right now).

## Blocking

_None._ The build is ready to flash.

## Non-blocking (worth knowing)

### N1. ESP-side audio is a logging stub
`src/esp_audio.cpp` only `Serial.printf`s clip names. The board has an
ES8311 codec on I²C (SDA=42, SCL=41) wired to an I²S DAC (MCLK=8,
BCLK=9, DOUT=12, LRC=10) but we don't init the codec. Result: no sound
from the device. The browser version plays the same procedurally-
synthesized PCM clips through Web Audio. No functional impact on
gameplay.

### N2. ESP-side touch (resolved)
The CST816 capacitive panel is now driven by `src/esp_touch.{h,cpp}`
via `lewisxhe/SensorLib`'s `TouchDrvCSTXXX`. Polled at ~60 Hz from
`main.cpp::loop`. Taps on the pet -> `Input::PetTap`; drags ->
`Input::Stroke`; taps on the stats bar -> `Input::MenuToggle`. If the
chip doesn't answer at boot, the firmware logs and continues without
touch.

### N3. Wi-Fi credential placeholder check is brittle
`src/esp_clock.cpp::have_creds()` rejects the example SSID by checking
`WIFI_SSID[0] != 'y'`. That means any real SSID starting with the
letter `y` will be silently skipped. Low chance, but fix-up is a
two-line change (compare the full literal to `"your-ssid-here"`). Left
as-is because users who copy the example will normally replace the
whole string.

### N4. Memorial wall is gated off
`BAILEY_MEMORIAL_WALL=0` by default. The struct fields + ring buffer
still serialize (as zeros) so save format stays compatible. To enable:
add `-D BAILEY_MEMORIAL_WALL=1` to `build_flags` and rebuild.

### N5. Restart Input is undocumented
After the death-removal round, `Input::Restart` is still accepted
(maps to an instant `restart_pet(false)` -- handy for debugging /
manual reset) but no UI advertises it. Keyboard `r` in the web
shell.js still fires it. Intentional.

### N6. `LifeStage::Gone` and `Mood::Gone` still defined
Kept for save-format / sync-code backward compatibility -- a save from
an older build that contains `stage=Gone` will still deserialize. The
game never sets these values in the current build, and `mood_text` /
`draw_pet_sprite` fall through to harmless defaults.

### N7. PlatformIO library version pins
`platformio.ini` pins:
- `lovyan03/LovyanGFX@^1.2.0`
- `mathertel/OneButton@^2.6.1`

Both resolve from the public PlatformIO registry. `chain+` LDF mode
picks up `lib/tama_core/` automatically.

### N8. `default_16MB.csv` partition table
Comes from the espressif32 platform package; matches the N16R8 module
on the Waveshare board. PSRAM allocated via `qio_opi` memory_type.
The display back-buffer (240×240 RGB565 = 115 KB) prefers PSRAM and
falls back to internal RAM if PSRAM allocation fails -- see
`src/esp_renderer.cpp::EspRenderer::init`.

### N9. Save format size
`sizeof(SaveData)` is roughly 580 bytes (40 base + 540 after Round 2
additions). NVS per-entry cap is ~4 KB, so well within limits. Save
format version is **3**, with a clean v1→v2→v3 migrator in
`lib/tama_core/src/save.cpp::save_validate_and_migrate`.

### N10. Procedural-art startup cost
`sprites_init()` runs once at boot. The pet sprite buffers
(`g_pet[4][7][W*H]` -- about 21 KB) and accessory buffers (~1.5 KB)
sit in `.bss` of the tama_core translation unit -- no heap pressure
and no per-frame cost.

### N11. Audio clip buffers
`audio_clips.cpp` allocates each clip on the heap via `std::calloc`.
Total ~80 KB across the 10 clips at 22050 Hz mono int16. They're
allocated once at boot and never freed. Fine on the ESP32-S3 (with
PSRAM available). Note: since the ESP audio backend is a stub right
now, these buffers are unused on the device -- pure dead weight until
the ES8311 driver lands. Acceptable.

### N12. Naming overlap: `Wish::Treat` vs `Word::Treat` vs `INPUT.Treat`
All scoped to different namespaces / enum classes, so no compile
conflicts. The web `INPUT` map's `TreatGive: 15` is the actual input
code; the JS shell never uses a `Treat` name. Easy to confuse when
grepping. Flagged.

### N13. `update_walk` does double duty as the NPC visitor tick
For brevity it both ticks the NPC cameo state and handles walk-state
transitions. Comment in source notes this. If/when either grows
larger they should split.

## Follow-ups (deliberately out of scope)

- **Device ES8311 audio**: wire I²C codec init + I²S DMA streaming
  for the existing PCM clips. Spec already understood; just real work.
- **Device CST816 touch**: I²C driver + tap/drag region mapping to
  `Input::PetTap` / `Input::Stroke`. Spec understood.
- **Replace procedural Bailey with photo-traced pixel art**: the
  current art is parametric. Real pixel art would land in
  `scripts/png_to_sprite.py` flow.
- **NTP retry / Wi-Fi setup wizard**: silent fallback today.
- **CI workflow YAML audit**: only the firmware build's CI status
  needs eyes -- live observation post-push will catch any drift.

## Doc audit

- `README.md` — gameplay section updated to reflect no-death loops;
  build/flash section points to `docs/FLASHING.md`.
- `include/secrets.h.example` — documents the no-creds graceful path.
- `include/pins.h` — pin numbers verified from Waveshare's own demo
  sketches, with the source `.ino` files cited inline.
- `docs/FLASHING.md` — first-flash walkthrough including USB-C cable
  gotcha, download-mode dance, NVS wipe.
- This file (`docs/CODE_REVIEW.md`) — the audit you're reading.

Nothing else points at stale behavior.

## How this review was conducted

Hand-grep + read of all `lib/tama_core/**`, `src/**`, `web/**`, the
`platformio.ini`, `README.md`, and the `.github/workflows/` files. No
runtime profiling or static analyzer was run; that's a reasonable
next step if the firmware behaves oddly on hardware.
