// EmotionEngine: decides which emotion is active. Rules:
//  - only transitions from the table are legal; others are routed through intermediate states
//    (e.g. Sleeping -> WakingUp -> Happy), so the character never jumps chaotically;
//  - every emotion has a minimum dwell time, only a stronger request can cut it short;
//  - a timed emotion falls back to the mood computed from the pet state.
#pragma once
#include "types.h"

namespace cc {

enum class EmotionPriority : uint8_t { Mood = 0, Idle = 1, Reaction = 2, Strong = 3, Forced = 4 };

class EmotionEngine {
 public:
  void reset(Emotion e) { cur_ = e; route_ = e; final_ = e; age_ = 0; hold_ = 0; prio_ = EmotionPriority::Mood; changed_ = true; }
  // Returns false if the request was rejected by priority/dwell rules.
  bool request(Emotion e, EmotionPriority p, float holdMs = 0);
  void setMood(Emotion e) { mood_ = e; }
  void update(float dtMs);
  Emotion current() const { return cur_; }
  Emotion mood() const { return mood_; }
  bool consumeChanged() { bool c = changed_; changed_ = false; return c; }
  int transitionCount() const { return transitions_; }
  EmotionPriority enteredPriority() const { return enteredPrio_; }

  static bool allowed(Emotion from, Emotion to);
  static float minDwellMs(Emotion e);
  // First hop of the shortest legal route, or `to` when already adjacent.
  static Emotion nextHop(Emotion from, Emotion to);

 private:
  void enter(Emotion e);
  Emotion cur_ = Emotion::Neutral, route_ = Emotion::Neutral, final_ = Emotion::Neutral, mood_ = Emotion::Neutral;
  float age_ = 0, hold_ = 0, pendingHold_ = 0;
  EmotionPriority prio_ = EmotionPriority::Mood, pendingPrio_ = EmotionPriority::Mood;
  EmotionPriority enteredPrio_ = EmotionPriority::Forced;
  bool changed_ = true;
  int transitions_ = 0;
};

}  // namespace cc
