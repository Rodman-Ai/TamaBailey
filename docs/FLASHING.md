# Flashing TamaBailey over USB-C

Step-by-step for getting the firmware onto the Waveshare
ESP32-S3-Touch-LCD-1.54 board the first time.

## 0. What you need

- The Waveshare ESP32-S3-Touch-LCD-1.54 board.
- A USB-C cable that supports **data** (not charge-only). The
  difference matters — a charge-only cable will power the board but
  the computer won't see a serial device.
- Python 3.9+ and `pip`.
- 5-10 minutes.

## 1. Install PlatformIO

PlatformIO drives the toolchain (compilers, partition table, flash
upload) so you don't have to. One-time install:

```sh
pip install --upgrade platformio
```

Verify:

```sh
pio --version
```

## 2. Plug in the board

Connect via USB-C. The board enumerates as a USB CDC serial device:

| OS     | Looks like                              |
|--------|-----------------------------------------|
| macOS  | `/dev/cu.usbmodem<digits>`              |
| Linux  | `/dev/ttyACM0` (or higher)              |
| Windows| `COMx` (Device Manager → Ports)         |

Quick check:

```sh
pio device list
```

You should see a device with USB VID `303A` (Espressif). If not, see
[Troubleshooting](#troubleshooting).

## 3. Build and flash

From the repo root:

```sh
pio run -e esp32-s3-lcd-1_54 -t upload
```

This compiles the firmware and pushes it to the board over USB CDC. On
the first run PlatformIO will download the ESP32 platform package and
the LovyanGFX + OneButton libraries (~200 MB, cached after this).

When upload finishes the board auto-resets and the display lights up.

## 4. Watch the serial output

```sh
pio device monitor
```

Press `Ctrl-T` then `Ctrl-X` to exit. First-boot output looks like:

```
TamaBailey starting up
[clock] no Wi-Fi credentials; staying unsynced (game uses synthetic day/night)
[audio] Yip @ vol 70             # printed when you press Feed
```

Press buttons A / B / C (top to bottom on the board) and you'll see
`[audio] Yip / Wuff / Splash` lines confirming the action loop is live.

## 5. Optional: Wi-Fi + NTP for real-world time

Without Wi-Fi the firmware runs fine; it uses a synthetic 24-minute
day/night cycle and skips offline-decay catch-up.

To get a real local time + daily-streak tracking:

```sh
cp include/secrets.h.example include/secrets.h
```

Edit `include/secrets.h` and fill in your real SSID / password. The
file is gitignored so it won't leak. Re-flash and the serial log will
show:

```
[clock] connecting Wi-Fi: <ssid>
[clock] NTP synced: 1763404129
```

within a few seconds of join.

## 6. Optional: demo build (fast decay)

Want to see stat decay + evolutions + the move-out narrative inside
one session?

```sh
pio run -e esp32-s3-lcd-1_54-fast -t upload
```

This collapses the 12-24 h timers into 12-24 minutes; everything
else is identical to the classic build.

---

## Troubleshooting

**Board not detected.**
- Try a different USB-C cable. The most common cause is a charge-only
  cable. A cable that came with a phone or a known data USB-C cable
  will work.
- On Linux: `groups | grep dialout`. If missing, `sudo usermod -aG
  dialout $USER`, then log out / in.
- On macOS: try a different USB-C port; some hubs eat the data lines.

**`A fatal error occurred: Failed to connect to ESP32-S3`.**
The board didn't enter download mode in time. Force it:

1. Hold the **BOOT** button (one of the three tactiles next to RESET).
2. Tap the **RESET** button.
3. Release **BOOT**.
4. Re-run `pio run -t upload` within a few seconds.

After flash, tap **RESET** once to run the new firmware.

**Display blank / dim.**
- Pin map check: `include/pins.h` is verified for the Waveshare
  ESP32-S3-Touch-LCD-1.54 (both the touch and non-touch variants of
  the 1.54" board share the same LCD pinout). If you have a different
  board, edit the pin defines.
- The backlight defaults to brightness 220/255. If your unit is dim,
  open the in-game Options menu and bump it.

**Serial monitor shows nothing.**
- Confirm baud rate 115200 (it's the default for `pio device monitor`).
- Confirm the board is running the firmware (tap RESET — you should
  briefly see the boot ROM message before our "TamaBailey starting up"
  line).
- Confirm `ARDUINO_USB_CDC_ON_BOOT=1` is in `platformio.ini`
  `build_flags` (it is by default).

**Reset Bailey from scratch.**
Normal `pio run -t upload` preserves Bailey's state in NVS. To wipe:

```sh
pio run -t erase           # wipes the entire flash including NVS
pio run -e esp32-s3-lcd-1_54 -t upload
```

**Stop the monitor cleanly.**
`Ctrl-T` then `Ctrl-X` exits `pio device monitor` without leaving the
serial port held.

**Audio: I don't hear anything.**
The board has an ES8311 audio codec but the driver isn't wired up in
this build (logging stub only). You'll see `[audio] Yip ...` in the
serial log when sounds would play. Real device audio is a tracked
follow-up; the browser version already has full audio.

**Touch on the device.**
Tapping Bailey on the LCD pets him (heart pop + Wuff bark + small
happiness bump). Dragging across him strokes him continuously. Tapping
the dark stats bar at the top toggles the status menu. The browser
version's touch behaves the same.

## What success looks like

When everything's working you should see:

- Bailey idle-animating on screen (brown body, white belly + paws +
  muzzle).
- Top stats bar with four colored meters: food, play, bath, rest.
- Bottom status banner with a mood string like "How is Bailey today?".
- Three onboard buttons each play their action animation when pressed,
  with a serial-log line confirming.
- Long-pressing any button (~0.8 s) opens the tabbed status menu;
  press buttons to cycle tabs.
- If Wi-Fi is set up, an HH:MM clock appears top-right and the
  background sky changes tint with real time of day.
