// PetManager: deterministic internal state of the character, all stats in 0..100.
#pragma once
#include "types.h"

namespace cc {

struct PetStats {
  float happiness = 70, energy = 85, curiosity = 55, attention = 60, affection = 45;
  float stress = 10, sleepiness = 10, hunger = 20, fun = 60, health = 100;
};

enum class PetStatus : uint8_t { Content, Hungry, Tired, Bored, Joyful, Asleep, Irritated };
enum class PetEvent : uint8_t {
  Tap, DoubleTap, LongTouch, Stroke, Feed, Pet, Play, GoSleep, WakeUp, Shake, Jerk, FaceDown, Lift,
  LoudNoise, Clap, Voice, GameWin, GameLose, Notification, RemoteAction
};
enum class Personality : uint8_t { Playful, Calm, Grumpy, Shy, Count };
const char* petStatusName(PetStatus s);
const char* personalityName(Personality p);   // protocol name
const char* personalityTitle(Personality p);  // UI name

class PetManager {
 public:
  void setPersonality(Personality p) { personality_ = p; }
  void setAlertness(float a) { alert_ = clampf(a, 0, 1); }  // 0 deep night, 1 midday
  Personality personality() const { return personality_; }
  void tick(float dtMs);          // dt in simulated time
  void applyOffline(uint32_t seconds);  // time passed while switched off
  void event(PetEvent e);
  void setAsleep(bool s) { asleep_ = s; }
  bool asleep() const { return asleep_; }
  bool irritatedByRepeats() const { return repeats_ >= 5; }

  PetStats stats;
  PetStatus status() const;
  Emotion moodEmotion() const;    // what the character feels when nothing is happening
  float idleSeconds() const { return idle_; }
  bool wantsSleep() const { return !asleep_ && (stats.sleepiness >= 88 || stats.energy <= 8); }
  bool wantsWake() const { return asleep_ && stats.energy >= 92 && stats.sleepiness <= 12 && alert_ > 0.3f; }
  bool set(const char* name, float value);
  float get(const char* name, bool* ok = nullptr) const;
  static const char* const kStatNames[10];

 private:
  void clampAll();
  Personality personality_ = Personality::Playful;
  bool asleep_ = false;
  float idle_ = 0, alert_ = 0.8f;
  float repeatWindow_ = 0;
  int repeats_ = 0;
  mutable Emotion lastMood_ = Emotion::Neutral;
};

}  // namespace cc
