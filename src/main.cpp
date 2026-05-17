// TamaBailey -- Tamagotchi-style desk pet of Bailey the hound.
//
// This file just wires the hardware adapters into the platform-agnostic
// game core. All gameplay lives in lib/tama_core/, which also compiles to
// WebAssembly (see web/) for browser play via GitHub Pages.

#include <Arduino.h>

#include "esp_input.h"
#include "esp_renderer.h"
#include "esp_storage.h"
#include "tama/game.h"

static bailey::LGFX_ST7789_240x240 lcd;
static bailey::EspRenderer*        renderer = nullptr;
static bailey::EspStorage          storage;
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

  game.init(storage, millis());

  static bailey::EspInput in(game);
  input = &in;
  input->begin();
}

void loop() {
  input->tick();

  uint32_t now = millis();
  game.tick(now);
  game.draw(*renderer);
  renderer->present();
  game.maybe_save(storage);

  delay(16);  // ~60 fps cap; the heavy lift is the back-buffer push
}
