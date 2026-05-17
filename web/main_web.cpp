// Emscripten entry: runs the same tama::Game as the ESP32, with web-side
// renderer / clock / speaker / storage.

#include <cstdint>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#define EMSCRIPTEN_KEEPALIVE
static double emscripten_get_now() { return 0.0; }
#endif

#include "tama/game.h"
#include "web_audio.h"
#include "web_clock.h"
#include "web_renderer.h"
#include "web_storage.h"

namespace {
bailey::WebRenderer renderer;
bailey::WebStorage  storage;
bailey::WebClock    clock_;
bailey::WebSpeaker  speaker;
tama::Game          game;
bool                spectator_mode = false;
}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
void bailey_input(int code) {
  if (spectator_mode) return;  // spectator can watch but not interact
  game.enqueue(static_cast<tama::Input>(code));
}

EMSCRIPTEN_KEEPALIVE
void bailey_init() {
  uint32_t now = (uint32_t)emscripten_get_now();
  game.init(storage, now, &clock_, &speaker);
}

EMSCRIPTEN_KEEPALIVE
int bailey_apply_sync_code(const char* code) {
  return game.apply_sync_code(code) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
const char* bailey_generate_sync_code() {
  return game.generate_sync_code();
}

EMSCRIPTEN_KEEPALIVE
void bailey_set_spectator(int on) {
  spectator_mode = on != 0;
}

EMSCRIPTEN_KEEPALIVE
void bailey_set_weather(int w) {
  if (w < 0 || w > 3) return;
  game.set_weather((tama::Weather)w);
}

// Lets the JS shell query a stat without exporting the full Game object.
// Returns -1 if invalid index. Indices: 0=hunger 1=happiness 2=clean 3=energy.
EMSCRIPTEN_KEEPALIVE
int bailey_get_stat(int idx) {
  const auto& p = game.pet();
  switch (idx) {
    case 0: return p.stats.hunger;
    case 1: return p.stats.happiness;
    case 2: return p.stats.cleanliness;
    case 3: return p.stats.energy;
    default: return -1;
  }
}

EMSCRIPTEN_KEEPALIVE
void bailey_frame() {
  uint32_t now = (uint32_t)emscripten_get_now();
  game.tick(now);
  game.draw(renderer);
  renderer.present();
  if (!spectator_mode) game.maybe_save(storage);
}

}  // extern "C"

#ifndef __EMSCRIPTEN__
int main() { return 0; }
#endif
