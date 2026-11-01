// Audio: sound pack reader, software mixer (AudioManager) and emotion-to-sound logic (SoundEmotionEngine).
// The mixer produces PCM on whichever thread drives the audio output; firmware feeds I2S with it and
// the emulator feeds the PC sound card, so priorities, fades and queueing behave identically.
#include <atomic>
#pragma once
#include "types.h"

namespace cc {

enum class SoundCategory : uint8_t { Emotion, Interaction, System, Game, Count };
constexpr int kSoundCategoryCount = (int)SoundCategory::Count;
const char* soundCategoryName(SoundCategory c);

// Pack layout (little endian), produced by tools/gen_sounds.py:
//   "CSP1" u32 count, then count entries { char name[32]; u32 offset; u32 samples; }, then 16 kHz mono s16 PCM.
class SoundPack {
 public:
  bool load(const uint8_t* data, uint32_t size);
  int count() const { return count_; }
  int find(const char* name) const;   // e.g. "emotions/happy"; -1 if absent
  const char* name(int i) const;
  const int16_t* pcm(int i, uint32_t* samples) const;
  float durationMs(int i) const;
 private:
  const uint8_t* data_ = nullptr;
  uint32_t size_ = 0;
  int count_ = 0;
};

class AudioManager {
 public:
  static constexpr int kVoices = 2;   // 0: emotional voice, 1: interface effects
  void setPack(const SoundPack* p) { pack_ = p; }

  // Control side, called from the logic thread. Lock-free: commands go through a ring to the mixer.
  void play(int sound, SoundCategory cat, float gain = 1.0f, int priority = 1);
  void playPcm(const int16_t* pcm, uint32_t samples);   // streamed AI reply; buffer must outlive playback
  void stopAll();
  void setVolume(int percent) { volumeTarget_.store(clampi(percent, 0, 100) / 100.f); }  // smooth ramp in mixer
  int volume() const { return (int)(volumeTarget_.load() * 100 + 0.5f); }
  void setMuted(bool m) { muted_ = m; }
  bool muted() const { return muted_; }
  void setCategoryEnabled(SoundCategory c, bool on) { catOn_[(int)c] = on; }
  bool categoryEnabled(SoundCategory c) const { return catOn_[(int)c]; }
  bool isPlaying() const { return active_.load() != 0; }
  int lastSound() const { return last_.load(); }
  uint32_t playCount() const { return playCount_; }

  // Audio side: fills out with mono samples. Never blocks and never allocates.
  void mix(int16_t* out, int samples);

 private:
  struct Cmd { int16_t sound; int16_t priority; uint8_t cat; uint8_t kind; float gain; const int16_t* pcm; uint32_t n; };
  struct Voice { const int16_t* pcm = nullptr; uint32_t n = 0, pos = 0; float gain = 0; int prio = 0; int fadeOut = 0; int fadeIn = 0; };
  struct Pending { const int16_t* pcm = nullptr; uint32_t n = 0; float gain = 0; int prio = 0; bool valid = false; };
  void push(const Cmd& c);
  void start(Voice& v, Pending& pend, const int16_t* pcm, uint32_t n, float gain, int prio);
  const SoundPack* pack_ = nullptr;
  static constexpr int kRing = 16;
  Cmd ring_[kRing];
  std::atomic<uint32_t> head_{0}, tail_{0};
  std::atomic<float> volumeTarget_{0.7f};
  std::atomic<int> active_{0}, last_{-1};
  float volume_ = 0.7f;
  bool muted_ = false;
  bool catOn_[kSoundCategoryCount] = {true, true, true, true};
  Voice voices_[kVoices];
  Pending pending_[kVoices];
  uint32_t playCount_ = 0;
};

// Connects emotions and sound: one primary and one variant per emotion, individual gain and a cooldown,
// plays only on entering an emotion, never loops while the emotion lasts.
class SoundEmotionEngine {
 public:
  void attach(AudioManager* a, const SoundPack* p);
  void onEmotionCue(Emotion e, float nowMs);
  void resetCooldowns();  // an explicit command from the user or a test panel must always be heard
  // Interface events, resolved by name ("interactions/tap").
  void play(const char* name, float nowMs = -1);
  int emotionSound(Emotion e, int variant) const { return ids_[(int)e][variant]; }
  float gainOf(Emotion e) const;
  float cooldownOf(Emotion e) const;
  bool enabled = true;
 private:
  AudioManager* audio_ = nullptr;
  const SoundPack* pack_ = nullptr;
  int ids_[kEmotionCount][2];
  float lastAt_[kEmotionCount];
  uint8_t next_[kEmotionCount];
};

}  // namespace cc
