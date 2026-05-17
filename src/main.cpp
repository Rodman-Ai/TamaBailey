// TamaBailey -- Tamagotchi-style desk pet of Bailey the hound.
//
// Wires hardware adapters (LovyanGFX, OneButton, Preferences, Wi-Fi+NTP,
// ES8311 audio, QMI8658 IMU) into the platform-agnostic game core in
// lib/tama_core/.

#include <Arduino.h>

#include "esp_audio.h"
#include "esp_clock.h"
#include "esp_imu.h"
#include "esp_input.h"
#include "esp_mic.h"
#include "esp_renderer.h"
#include "esp_storage.h"
#include "esp_touch.h"
#include "tama/game.h"

static bailey::LGFX_ST7789_240x240 lcd;
static bailey::EspRenderer*        renderer = nullptr;
static bailey::EspStorage          storage;
static bailey::EspClock            clock_;
static bailey::EspSpeaker          speaker;
static bailey::EspImu              imu;
static bailey::EspTouch            touch;
static bailey::EspMic              mic;
static tama::Game                  game;
static bailey::EspInput*           input = nullptr;

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println("TamaBailey starting up");

  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(220);

  static bailey::EspRenderer r(lcd);
  renderer = &r;
  if (!renderer->init()) {
    Serial.println("FATAL: failed to allocate display back-buffer");
    while (true) delay(1000);
  }

  bool audio_ok = speaker.begin();
  mic.begin();
  bool imu_ok   = imu.begin();
  touch.begin();
  clock_.begin();
  game.init(storage, millis(), &clock_, &speaker);
  // Surface init status as a small bottom-left HUD chip for debug.
  game.set_hw_audio_status(audio_ok);
  game.set_hw_imu_status(imu_ok);
  lcd.setBrightness(game.settings().brightness);

  static bailey::EspInput in(game);
  input = &in;
  input->begin();
}

void loop() {
  input->tick();
  clock_.poll();

  uint32_t now = millis();
  imu.poll(now, game);
  touch.poll(now, game);
  mic.poll(now, game);
  game.tick(now);
  game.draw(*renderer);
  renderer->present();
  game.maybe_save(storage);

  delay(16);
}
