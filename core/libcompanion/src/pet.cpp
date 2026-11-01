#include "companion/pet.h"
#include <string.h>

namespace cc {

const char* const PetManager::kStatNames[10] = {"happiness", "energy", "curiosity", "attention", "affection",
                                                "stress", "sleepiness", "hunger", "fun", "health"};

const char* petStatusName(PetStatus s) {
  static const char* const n[] = {"content", "hungry", "tired", "bored", "joyful", "asleep", "irritated"};
  return n[(int)s];
}
const char* personalityName(Personality p) {
  static const char* const n[] = {"playful", "calm", "grumpy", "shy"};
  return n[(int)p];
}
const char* personalityTitle(Personality p) {
  static const char* const n[] = {"Игривый", "Спокойный", "Ворчун", "Застенчивый"};
  return n[(int)p];
}

// How each character scales reactions: joy gain, stress gain, boredom speed.
struct Traits { float joy, stress, boredom; };
static const Traits kTraits[] = {{1.2f, 0.8f, 1.3f}, {0.9f, 0.5f, 0.6f}, {0.7f, 1.5f, 1.f}, {1.f, 1.3f, 0.8f}};

void PetManager::clampAll() {
  float* f = &stats.happiness;
  for (int i = 0; i < 10; i++) f[i] = clampf(f[i], 0, 100);
}

void PetManager::tick(float dtMs) {
  float s = dtMs * 0.001f;
  const Traits& tr = kTraits[(int)personality_];
  idle_ += s;
  repeatWindow_ -= s;
  if (repeatWindow_ <= 0) { repeats_ = 0; repeatWindow_ = 0; }

  stats.hunger += s * 0.0115f;                 // full to starving in ~2.4 hours
  if (asleep_) {
    stats.energy += s * 0.055f;                // full recharge in ~30 min
    stats.sleepiness -= s * 0.07f;
    stats.stress -= s * 0.05f;
    stats.hunger -= s * 0.004f;
  } else {
    stats.energy -= s * 0.0085f;               // ~3 hours awake
    stats.sleepiness += s * (0.006f + (100 - stats.energy) * 0.00012f);
    float bored = idle_ > 60 ? 1.f : 0.25f;
    stats.fun -= s * 0.012f * bored * tr.boredom;
    stats.attention -= s * 0.02f * bored * tr.boredom;
    stats.affection -= s * 0.0018f * bored;
    stats.curiosity += s * (idle_ > 120 ? -0.01f : 0.01f);
    stats.stress -= s * 0.035f;
  }
  // Happiness drifts toward what the other needs imply; it is a result, not an input.
  float target = 55 + 0.2f * (stats.fun - 50) + 0.2f * (stats.affection - 50) - 0.3f * stats.stress * 0.5f -
                 (stats.hunger > 60 ? (stats.hunger - 60) * 0.5f : 0) - (stats.energy < 25 ? (25 - stats.energy) * 0.4f : 0);
  stats.happiness += (target - stats.happiness) * clampf(s * 0.0035f, 0, 1);
  // Health falls only under sustained neglect and recovers slowly when needs are met.
  if (stats.hunger > 85 || stats.energy < 5 || stats.stress > 90) stats.health -= s * 0.01f;
  else if (stats.hunger < 60) stats.health += s * 0.004f;
  clampAll();
}

void PetManager::applyOffline(uint32_t seconds) {
  // The creature "sleeps" while the device is off, so a weekend away is not a disaster.
  if (seconds > 8 * 3600) seconds = 8 * 3600;
  bool was = asleep_;
  asleep_ = true;
  for (uint32_t i = 0; i < seconds / 60; i++) tick(60000.f);
  asleep_ = was;
  idle_ = 0;
}

void PetManager::event(PetEvent e) {
  const Traits& tr = kTraits[(int)personality_];
  bool interaction = e != PetEvent::LoudNoise && e != PetEvent::Clap && e != PetEvent::Voice && e != PetEvent::Notification;
  if (interaction) idle_ = 0;
  auto repeat = [&] {
    repeats_++;
    repeatWindow_ = 8;
    return repeats_ >= 5;
  };
  switch (e) {
    case PetEvent::Tap:
      if (repeat()) { stats.stress += 5 * tr.stress; stats.happiness -= 1; }
      else { stats.attention += 4; stats.curiosity += 1; stats.fun += 0.5f; }
      break;
    case PetEvent::DoubleTap:
      if (repeat()) { stats.stress += 7 * tr.stress; }
      else { stats.fun += 3 * tr.joy; stats.happiness += 2 * tr.joy; stats.attention += 5; }
      break;
    case PetEvent::LongTouch:
    case PetEvent::Stroke:
      stats.affection += 3 * tr.joy; stats.happiness += 2 * tr.joy; stats.stress -= 4; stats.attention += 3;
      break;
    case PetEvent::Pet:
      stats.affection += 8 * tr.joy; stats.happiness += 5 * tr.joy; stats.stress -= 8; stats.attention += 6;
      break;
    case PetEvent::Feed:
      stats.hunger -= 35; stats.happiness += 4; stats.energy += 4; stats.attention += 4;
      if (stats.hunger <= 0) stats.stress += 3;   // overfed
      break;
    case PetEvent::Play:
      stats.fun += 18 * tr.joy; stats.happiness += 6 * tr.joy; stats.energy -= 6; stats.hunger += 4; stats.attention += 8;
      break;
    case PetEvent::GoSleep: asleep_ = true; stats.sleepiness += 5; break;
    case PetEvent::WakeUp: asleep_ = false; break;
    case PetEvent::Shake: stats.stress += 12 * tr.stress; stats.energy -= 1; stats.curiosity += 2; break;
    case PetEvent::Jerk: stats.stress += 8 * tr.stress; break;
    case PetEvent::FaceDown: stats.sleepiness += 6; break;
    case PetEvent::Lift: stats.attention += 6; stats.sleepiness -= 3; break;
    case PetEvent::LoudNoise: stats.stress += 6 * tr.stress; stats.attention += 6; stats.curiosity += 2; break;
    case PetEvent::Clap: stats.attention += 5; stats.curiosity += 3; break;
    case PetEvent::Voice: stats.attention += 1.5f; stats.affection += 0.2f; break;
    case PetEvent::GameWin: stats.happiness += 8 * tr.joy; stats.fun += 10 * tr.joy; stats.energy -= 2; break;
    case PetEvent::GameLose: stats.fun += 4; stats.happiness -= 2; stats.energy -= 2; break;
    case PetEvent::Notification: stats.attention += 3; stats.curiosity += 2; break;
    case PetEvent::RemoteAction: stats.attention += 2; break;
  }
  clampAll();
}

PetStatus PetManager::status() const {
  if (asleep_) return PetStatus::Asleep;
  if (stats.stress >= 65) return PetStatus::Irritated;
  if (stats.hunger >= 72) return PetStatus::Hungry;
  if (stats.energy <= 20 || stats.sleepiness >= 75) return PetStatus::Tired;
  if (stats.happiness >= 82 && stats.fun >= 55) return PetStatus::Joyful;
  if (stats.fun <= 25 || stats.attention <= 18) return PetStatus::Bored;
  return PetStatus::Content;
}

Emotion PetManager::moodEmotion() const {
  // Hysteresis: the first matching rule wins, so similar stats always give the same answer.
  switch (status()) {
    case PetStatus::Asleep: return Emotion::Sleeping;
    case PetStatus::Irritated: return stats.stress >= 82 ? Emotion::Angry : Emotion::Annoyed;
    case PetStatus::Hungry: return stats.hunger >= 88 ? Emotion::Sad : Emotion::Annoyed;
    case PetStatus::Tired: return Emotion::Sleepy;
    case PetStatus::Joyful: return stats.affection >= 80 ? Emotion::Affection : Emotion::Happy;
    case PetStatus::Bored: return Emotion::Bored;
    case PetStatus::Content: break;
  }
  if (stats.happiness < 28) return Emotion::Sad;
  if (idle_ > 45 && stats.curiosity >= 70) return Emotion::Curious;
  if (idle_ > 300) return Emotion::Bored;
  return Emotion::Neutral;
}

bool PetManager::set(const char* name, float value) {
  float* f = &stats.happiness;
  for (int i = 0; i < 10; i++)
    if (!strcmp(name, kStatNames[i])) { f[i] = clampf(value, 0, 100); return true; }
  return false;
}

float PetManager::get(const char* name, bool* ok) const {
  const float* f = &stats.happiness;
  for (int i = 0; i < 10; i++)
    if (!strcmp(name, kStatNames[i])) { if (ok) *ok = true; return f[i]; }
  if (ok) *ok = false;
  return 0;
}

}  // namespace cc
