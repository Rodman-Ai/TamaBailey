// Synthesize all sound clips into 16-bit PCM buffers at startup.
// All synthesis is mathematical (square + sine + noise + envelope) so we
// don't ship any binary blobs.

#include "tama/audio.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace tama {

namespace {

constexpr int   SR     = kAudioSampleRate;
constexpr float TWO_PI = 6.28318530718f;

struct Clip {
  int16_t* data;
  int      n;
};

Clip g_clips[(int)ClipId::COUNT] = {};
bool g_inited = false;
uint32_t g_rng = 0xC0FFEEu;

inline uint32_t lcg() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng; }
inline float    noise() { return ((int32_t)(lcg() & 0xFFFF) - 32768) / 32768.0f; }

inline float env_ar(int i, int n, float attack_frac, float release_frac) {
  float t = (float)i / (float)n;
  if (t < attack_frac)            return t / attack_frac;
  if (t > 1.0f - release_frac)    return (1.0f - t) / release_frac;
  return 1.0f;
}

Clip make(int dur_ms) {
  int n = SR * dur_ms / 1000;
  Clip c{(int16_t*)std::calloc(n, sizeof(int16_t)), n};
  return c;
}

inline void add_square(Clip& c, float freq, float amp, int start_ms, int len_ms,
                       float attack = 0.05f, float release = 0.2f) {
  int s = SR * start_ms / 1000;
  int n = SR * len_ms / 1000;
  if (s < 0) s = 0;
  if (s + n > c.n) n = c.n - s;
  for (int i = 0; i < n; ++i) {
    float phase = std::fmod((float)i * freq / SR, 1.0f);
    float v = (phase < 0.5f ? 1.0f : -1.0f) * amp * env_ar(i, n, attack, release);
    int32_t s16 = (int32_t)c.data[s + i] + (int32_t)(v * 24000);
    if (s16 >  32767) s16 =  32767;
    if (s16 < -32768) s16 = -32768;
    c.data[s + i] = (int16_t)s16;
  }
}

inline void add_sine(Clip& c, float freq, float amp, int start_ms, int len_ms,
                     float attack = 0.05f, float release = 0.2f) {
  int s = SR * start_ms / 1000;
  int n = SR * len_ms / 1000;
  if (s < 0) s = 0;
  if (s + n > c.n) n = c.n - s;
  for (int i = 0; i < n; ++i) {
    float v = std::sin(TWO_PI * freq * i / SR) * amp * env_ar(i, n, attack, release);
    int32_t s16 = (int32_t)c.data[s + i] + (int32_t)(v * 24000);
    if (s16 >  32767) s16 =  32767;
    if (s16 < -32768) s16 = -32768;
    c.data[s + i] = (int16_t)s16;
  }
}

inline void add_noise(Clip& c, float amp, int start_ms, int len_ms,
                      float attack = 0.05f, float release = 0.2f) {
  int s = SR * start_ms / 1000;
  int n = SR * len_ms / 1000;
  if (s < 0) s = 0;
  if (s + n > c.n) n = c.n - s;
  for (int i = 0; i < n; ++i) {
    float v = noise() * amp * env_ar(i, n, attack, release);
    int32_t s16 = (int32_t)c.data[s + i] + (int32_t)(v * 24000);
    if (s16 >  32767) s16 =  32767;
    if (s16 < -32768) s16 = -32768;
    c.data[s + i] = (int16_t)s16;
  }
}

// Frequency sweep using square wave
inline void add_sweep_square(Clip& c, float f0, float f1, float amp,
                             int start_ms, int len_ms,
                             float attack = 0.05f, float release = 0.2f) {
  int s = SR * start_ms / 1000;
  int n = SR * len_ms / 1000;
  if (s < 0) s = 0;
  if (s + n > c.n) n = c.n - s;
  float phase = 0.0f;
  for (int i = 0; i < n; ++i) {
    float t = (float)i / n;
    float freq = f0 + (f1 - f0) * t;
    phase += freq / SR;
    if (phase >= 1.0f) phase -= 1.0f;
    float v = (phase < 0.5f ? 1.0f : -1.0f) * amp * env_ar(i, n, attack, release);
    int32_t s16 = (int32_t)c.data[s + i] + (int32_t)(v * 24000);
    if (s16 >  32767) s16 =  32767;
    if (s16 < -32768) s16 = -32768;
    c.data[s + i] = (int16_t)s16;
  }
}

void build_yip() {
  Clip c = make(180);
  add_sweep_square(c, 900, 1500, 0.55f, 0, 90);
  add_sweep_square(c, 1500, 1100, 0.45f, 90, 90, 0.05f, 0.5f);
  g_clips[(int)ClipId::Yip] = c;
}

void build_wuff() {
  Clip c = make(320);
  add_sweep_square(c, 500, 380, 0.6f, 0, 140);
  add_sweep_square(c, 700, 500, 0.55f, 170, 150, 0.04f, 0.5f);
  g_clips[(int)ClipId::Wuff] = c;
}

void build_splash() {
  Clip c = make(380);
  for (int seg = 0; seg < 3; ++seg) {
    add_noise(c, 0.45f, 50 * seg, 130, 0.1f, 0.4f);
    add_sine(c, 1500.0f + 200 * seg, 0.25f, 50 * seg, 130);
  }
  g_clips[(int)ClipId::Splash] = c;
}

void build_heart() {
  Clip c = make(220);
  add_sine(c, 740.0f, 0.5f, 0, 90,   0.05f, 0.6f);
  add_sine(c, 988.0f, 0.5f, 110, 110, 0.05f, 0.6f);
  g_clips[(int)ClipId::Heart] = c;
}

void build_snore() {
  Clip c = make(900);
  // Long low rumble that dips
  add_sweep_square(c, 130, 90, 0.4f, 0, 500, 0.2f, 0.3f);
  add_sweep_square(c, 200, 140, 0.3f, 500, 400, 0.1f, 0.5f);
  // Wet noise overlay
  add_noise(c, 0.12f, 0, 900, 0.1f, 0.3f);
  g_clips[(int)ClipId::Snore] = c;
}

void build_whimper() {
  Clip c = make(500);
  add_sweep_square(c, 800, 400, 0.4f, 0, 200);
  add_sweep_square(c, 600, 350, 0.4f, 220, 280, 0.04f, 0.6f);
  g_clips[(int)ClipId::Whimper] = c;
}

void build_sneeze() {
  Clip c = make(260);
  add_noise(c, 0.7f, 0, 80, 0.05f, 0.2f);
  add_sweep_square(c, 1100, 500, 0.5f, 60, 180, 0.05f, 0.5f);
  g_clips[(int)ClipId::Sneeze] = c;
}

void build_fanfare() {
  Clip c = make(800);
  // C E G C arpeggio
  add_square(c, 523, 0.45f, 0,   180);
  add_square(c, 659, 0.45f, 180, 180);
  add_square(c, 784, 0.45f, 360, 180);
  add_square(c, 1047, 0.55f, 540, 260, 0.05f, 0.5f);
  g_clips[(int)ClipId::Fanfare] = c;
}

void build_achieve() {
  Clip c = make(280);
  add_square(c, 988, 0.45f, 0,   120);
  add_square(c, 1319, 0.50f, 120, 160, 0.05f, 0.5f);
  g_clips[(int)ClipId::Achieve] = c;
}

void build_sad() {
  Clip c = make(900);
  add_sweep_square(c, 600, 200, 0.45f, 0,   400);
  add_sweep_square(c, 400, 180, 0.4f,  410, 480, 0.04f, 0.6f);
  g_clips[(int)ClipId::Sad] = c;
}

// Outdoor chirp: two upward sine pips at bird-call pitch.
void build_birds() {
  Clip c = make(380);
  add_sine(c, 2200.0f, 0.35f, 0,   90, 0.02f, 0.4f);
  add_sine(c, 2600.0f, 0.30f, 110, 70, 0.02f, 0.4f);
  add_sine(c, 1900.0f, 0.30f, 220, 90, 0.02f, 0.4f);
  g_clips[(int)ClipId::Birds] = c;
}

// Beach wave: low whoosh built from noise + slow sine dip.
void build_waves() {
  Clip c = make(700);
  add_noise(c, 0.18f, 0,   500, 0.15f, 0.5f);
  add_sine (c, 180.0f, 0.25f, 50, 600,  0.1f, 0.6f);
  g_clips[(int)ClipId::Waves] = c;
}

// Snow scene gust: filtered noise with a wide envelope.
void build_wind() {
  Clip c = make(800);
  add_noise(c, 0.22f, 0, 800, 0.2f, 0.7f);
  g_clips[(int)ClipId::Wind] = c;
}

// Thunder: deep low-frequency rumble with a slow attack and a long
// tail. Layers a sweep below 200 Hz with broad noise.
void build_thunder() {
  Clip c = make(1200);
  add_sweep_square(c, 80, 50, 0.55f, 0,   900, 0.25f, 0.6f);
  add_sweep_square(c, 60, 40, 0.4f,  100, 1100, 0.3f, 0.6f);
  add_noise(c, 0.18f, 0, 1200, 0.3f, 0.7f);
  g_clips[(int)ClipId::Thunder] = c;
}

// Snore (loud): a longer, breathier sigh than the existing Snore.
// Used for the periodic snore cue during the Sleeping pose.
void build_snore_loud() {
  Clip c = make(900);
  add_sweep_square(c, 200, 110, 0.35f, 0, 450, 0.15f, 0.5f);
  add_noise(c, 0.10f, 0, 900, 0.2f, 0.6f);
  g_clips[(int)ClipId::SnoreLoud] = c;
}

}  // namespace

void audio_init() {
  if (g_inited) return;
  g_inited = true;
  build_yip();
  build_wuff();
  build_splash();
  build_heart();
  build_snore();
  build_whimper();
  build_sneeze();
  build_fanfare();
  build_achieve();
  build_sad();
  build_birds();
  build_waves();
  build_wind();
  build_thunder();
  build_snore_loud();
}

const int16_t* pcm_clip(ClipId clip, int& num_samples) {
  int i = (int)clip;
  if (i < 0 || i >= (int)ClipId::COUNT || !g_clips[i].data) {
    num_samples = 0;
    return nullptr;
  }
  num_samples = g_clips[i].n;
  return g_clips[i].data;
}

}  // namespace tama
