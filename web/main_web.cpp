// Emscripten entry: runs the same tama::Game as the ESP32, with a
// WebRenderer that pushes pixels into an HTML canvas via JS.

#include <cstdint>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#define EMSCRIPTEN_KEEPALIVE
static double emscripten_get_now() { return 0.0; }
#endif

#include "tama/game.h"
#include "web_renderer.h"
#include "web_storage.h"

namespace {
bailey::WebRenderer renderer;
bailey::WebStorage  storage;
tama::Game          game;
}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
void bailey_input(int code) {
  game.enqueue(static_cast<tama::Input>(code));
}

EMSCRIPTEN_KEEPALIVE
void bailey_init() {
  uint32_t now = (uint32_t)emscripten_get_now();
  game.init(storage, now);
}

EMSCRIPTEN_KEEPALIVE
void bailey_frame() {
  uint32_t now = (uint32_t)emscripten_get_now();
  game.tick(now);
  game.draw(renderer);
  renderer.present();
  game.maybe_save(storage);
}

}  // extern "C"

#ifndef __EMSCRIPTEN__
int main() { return 0; }  // sanity-build path for native compile checks
#endif
