#include "esp_audio.h"

#include <Arduino.h>

namespace bailey {

namespace {
const char* clip_name(tama::ClipId c) {
  switch (c) {
    case tama::ClipId::Yip:     return "Yip";
    case tama::ClipId::Wuff:    return "Wuff";
    case tama::ClipId::Splash:  return "Splash";
    case tama::ClipId::Heart:   return "Heart";
    case tama::ClipId::Snore:   return "Snore";
    case tama::ClipId::Whimper: return "Whimper";
    case tama::ClipId::Sneeze:  return "Sneeze";
    case tama::ClipId::Fanfare: return "Fanfare";
    case tama::ClipId::Achieve: return "Achieve";
    case tama::ClipId::Sad:     return "Sad";
    default:                    return "?";
  }
}
}

void EspSpeaker::playClip(tama::ClipId clip, uint8_t volume) {
  // Phase 1: log only. Real ES8311+I2S playback lands in a follow-up.
  Serial.printf("[audio] %s @ vol %u\n", clip_name(clip), volume);
}

}  // namespace bailey
