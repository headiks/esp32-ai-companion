// Shared basic types. No platform dependencies: this library builds for ESP32-S3 and for desktop.
#pragma once
#include <stdint.h>
#include <math.h>

namespace cc {

constexpr int kScreenW = 466;
constexpr int kScreenH = 466;
constexpr int kSampleRate = 16000;

enum class Emotion : uint8_t {
  Neutral, Happy, VeryHappy, Sad, Angry, Annoyed, Surprised, Curious, Thinking,
  Sleepy, Sleeping, WakingUp, Scared, Affection, Bored, Attentive, Confused, Excited,
  Mischievous, Shy, Proud, Dizzy, Yawning, Cold, Hot, Dreamy, Focused, Winking,
  Smug, Skeptical, Guilty, Embarrassed, Hopeful, Lonely, Grateful, Jealous, Determined, Terrified, Disgusted,
  Hungry, Starstruck, Laughing, Crying, Relieved, Sneaky, Nostalgic, Playful, Amazed, Exhausted, Suspicious,
  Count
};
constexpr int kEmotionCount = (int)Emotion::Count;
enum class Weather : uint8_t { Clear, Clouds, Rain, Snow, Storm, Fog, Wind, Count };
const char* emotionName(Emotion e);               // protocol name, e.g. "very_happy"
bool emotionFromName(const char* s, Emotion* out);

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

// Small deterministic PRNG, so tests and emulator runs are reproducible for a given seed.
class Rng {
 public:
  explicit Rng(uint32_t seed = 0x1234567u) : s_(seed ? seed : 1) {}
  void seed(uint32_t v) { s_ = v ? v : 1; }
  uint32_t next() { s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5; return s_; }
  int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
  float unit() { return (next() & 0xFFFFFF) / 16777215.0f; }
  float rangef(float lo, float hi) { return lo + (hi - lo) * unit(); }
 private:
  uint32_t s_;
};

}  // namespace cc
