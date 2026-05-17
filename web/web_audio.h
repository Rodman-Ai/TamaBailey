#pragma once

#include <cstdint>

#include "tama/audio.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace bailey {

// Plays clips via the Web Audio API. EM_ASM hands the JS side a pointer
// into wasm heap (int16 PCM) and the volume + sample rate.
class WebSpeaker final : public tama::Speaker {
 public:
  void playClip(tama::ClipId clip, uint8_t volume = 100) override {
    int n = 0;
    const int16_t* pcm = tama::pcm_clip(clip, n);
    if (!pcm || n <= 0) return;
#ifdef __EMSCRIPTEN__
    EM_ASM({
      if (typeof Module !== 'undefined' && Module.baileyPlay) {
        Module.baileyPlay($0, $1, $2, $3);
      }
    }, pcm, n, tama::kAudioSampleRate, volume);
#else
    (void)volume;
#endif
  }
};

}  // namespace bailey
