// TamaBailey -- Tamagotchi-style desk pet of Bailey the hound.
//
// Wires hardware adapters (LovyanGFX, OneButton, Preferences, Wi-Fi+NTP,
// stub speaker) into the platform-agnostic game core in lib/tama_core/.

#include <Arduino.h>

#include "esp_audio.h"
#include "esp_clock.h"
#include "esp_input.h"
#include "esp_renderer.h"
#include "esp_storage.h"
#include "tama/game.h"

static bailey::LGFX_ST7789_240x240 lcd;
static bailey::EspRenderer*        renderer = nullptr;
static bailey::EspStorage          storage;
static bailey::EspClock            clock_;
static bailey::EspSpeaker          speaker;
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

  clock_.begin();
  game.init(storage, millis(), &clock_, &speaker);
  // Apply persisted brightness.
  lcd.setBrightness(game.settings().brightness);

  static bailey::EspInput in(game);
  input = &in;
  input->begin();
}

void loop() {
  input->tick();
  clock_.poll();

  uint32_t now = millis();
  game.tick(now);
  game.draw(*renderer);
  renderer->present();
  game.maybe_save(storage);

  delay(16);
}
