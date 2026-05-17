#pragma once

#include <cstdint>

namespace tama {

enum class ClipId : uint8_t {
  Yip       = 0,  // feed
  Wuff      = 1,  // play
  Splash    = 2,  // clean
  Heart     = 3,  // pet
  Snore     = 4,  // sleeping
  Whimper   = 5,  // sad
  Sneeze    = 6,  // sick
  Fanfare   = 7,  // evolution
  Achieve   = 8,  // achievement unlocked
  Sad       = 9,  // gone
  COUNT     = 10,
};

class Speaker {
 public:
  virtual ~Speaker() = default;
  virtual void playClip(ClipId clip, uint8_t volume = 100) = 0;
  virtual bool available() const { return true; }
};

// Synthesize all clips into 16-bit signed PCM @ 22050 Hz mono. Each clip
// is a malloc'd buffer; pcm_clip() returns it (and its sample count).
// Call audio_init() once before using; safe to call multiple times.
void           audio_init();
const int16_t* pcm_clip(ClipId clip, int& num_samples);
constexpr int  kAudioSampleRate = 22050;

}  // namespace tama
