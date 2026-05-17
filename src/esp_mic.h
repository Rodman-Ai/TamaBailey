#pragma once

#include <cstdint>

#include "tama/game.h"

namespace bailey {

// I2S microphone adapter using the ES8311 codec's mic input. Runs a
// short-time-energy VAD over the captured PCM and segments utterances.
// Each closed utterance gets handed to a heuristic classifier that
// picks one of the five voice commands by counting syllable bursts and
// inspecting the energy envelope's tail. NOT a real speech recognizer
// -- it will misfire; the menu has the same five tricks for
// fallback. ESP-SR / Multinet is a follow-up.
class EspMic {
 public:
  bool begin();
  bool ok() const { return ok_; }
  void poll(uint32_t now_ms, tama::Game& game);

 private:
  // Closed-utterance classifier; emits an Input::Voice* on Game.
  void classify_and_dispatch(tama::Game& game);

  bool      ok_                = false;
  uint32_t  last_poll_ms_      = 0;
  uint32_t  noise_floor_q15_   = 200;   // adaptive floor in q15 energy units
  uint8_t   silence_streak_    = 0;
  bool      voiced_            = false;

  // Utterance feature ring (per-frame at 16 ms).
  static constexpr int kMaxFrames = 140;     // ~2.2 s of audio
  uint16_t  frame_energy_[kMaxFrames] = {};  // q15-ish energy magnitude
  uint16_t  frame_zcr_[kMaxFrames]    = {};
  int       frame_count_              = 0;
  uint32_t  utterance_started_ms_     = 0;
  uint32_t  last_dispatch_ms_         = 0;
};

}  // namespace bailey
