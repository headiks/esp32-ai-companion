#include "companion/emotion.h"
#include <string.h>

namespace cc {

static const char* const kNames[kEmotionCount] = {
    "neutral", "happy", "very_happy", "sad", "angry", "annoyed", "surprised", "curious", "thinking",
    "sleepy", "sleeping", "waking_up", "scared", "affection", "bored", "attentive", "confused", "excited"};

const char* emotionName(Emotion e) { return (int)e < kEmotionCount ? kNames[(int)e] : "neutral"; }

bool emotionFromName(const char* s, Emotion* out) {
  for (int i = 0; i < kEmotionCount; i++)
    if (!strcmp(s, kNames[i])) { *out = (Emotion)i; return true; }
  return false;
}

#define B(e) (1u << (int)Emotion::e)
static constexpr uint32_t kAll = (1u << kEmotionCount) - 1;
// Falling asleep goes through Sleepy; a sleeper may only wake up (or be startled awake via WakingUp).
static constexpr uint32_t kAwake = kAll & ~B(Sleeping) & ~B(WakingUp);

static const uint32_t kAllowed[kEmotionCount] = {
    /*Neutral*/   kAwake,
    /*Happy*/     kAwake,
    /*VeryHappy*/ kAwake & ~B(Sleepy) & ~B(Bored),
    /*Sad*/       kAwake & ~B(VeryHappy) & ~B(Excited),
    /*Angry*/     kAwake & ~B(Affection) & ~B(VeryHappy) & ~B(Sleepy),
    /*Annoyed*/   kAwake & ~B(VeryHappy),
    /*Surprised*/ kAwake & ~B(Sleepy),
    /*Curious*/   kAwake,
    /*Thinking*/  kAwake,
    /*Sleepy*/    B(Sleeping) | B(Neutral) | B(Bored) | B(Attentive) | B(Surprised) | B(Scared) | B(Happy) | B(Sad) | B(Annoyed) | B(Curious),
    /*Sleeping*/  B(WakingUp),
    /*WakingUp*/  kAwake & ~B(Angry),
    /*Scared*/    kAwake & ~B(VeryHappy) & ~B(Affection) & ~B(Sleepy),
    /*Affection*/ kAwake & ~B(Angry),
    /*Bored*/     kAwake,
    /*Attentive*/ kAwake,
    /*Confused*/  kAwake & ~B(Sleepy),
    /*Excited*/   kAwake & ~B(Sleepy) & ~B(Sad),
};
#undef B

// Short pops stay visible long enough to read; moods last longer so they do not flicker.
static const uint16_t kDwell[kEmotionCount] = {
    /*Neutral*/ 400, /*Happy*/ 900, /*VeryHappy*/ 1200, /*Sad*/ 1400, /*Angry*/ 1000, /*Annoyed*/ 900,
    /*Surprised*/ 650, /*Curious*/ 800, /*Thinking*/ 900, /*Sleepy*/ 1200, /*Sleeping*/ 1500, /*WakingUp*/ 1100,
    /*Scared*/ 800, /*Affection*/ 1200, /*Bored*/ 1200, /*Attentive*/ 500, /*Confused*/ 900, /*Excited*/ 900};

bool EmotionEngine::allowed(Emotion from, Emotion to) {
  return from == to || (kAllowed[(int)from] >> (int)to) & 1u;
}

float EmotionEngine::minDwellMs(Emotion e) { return kDwell[(int)e]; }

Emotion EmotionEngine::nextHop(Emotion from, Emotion to) {
  if (allowed(from, to)) return to;
  // Breadth first search; the table is tiny and requests are rare.
  int8_t parent[kEmotionCount];
  memset(parent, -1, sizeof(parent));
  uint8_t queue[kEmotionCount];
  int head = 0, tail = 0;
  queue[tail++] = (uint8_t)from;
  parent[(int)from] = (int8_t)from;
  while (head < tail) {
    int u = queue[head++];
    for (int v = 0; v < kEmotionCount; v++) {
      if (parent[v] >= 0 || !((kAllowed[u] >> v) & 1u)) continue;
      parent[v] = (int8_t)u;
      queue[tail++] = (uint8_t)v;
      if (v == (int)to) {
        int w = v;
        while (parent[w] != (int)from) w = parent[w];
        return (Emotion)w;
      }
    }
  }
  return from;
}

void EmotionEngine::enter(Emotion e) {
  if (e == cur_) return;
  cur_ = e;
  age_ = 0;
  changed_ = true;
  transitions_++;
}

bool EmotionEngine::request(Emotion e, EmotionPriority p, float holdMs) {
  if (e == final_ && p <= prio_) { if (holdMs > hold_) hold_ = holdMs; return true; }
  bool dwellOver = age_ >= minDwellMs(cur_);
  if (p != EmotionPriority::Forced) {
    if (p < prio_ && hold_ > 0) return false;           // a stronger timed emotion is playing
    if (!dwellOver && p <= prio_) return false;         // too soon: avoid flicker
  }
  // Forced only wins the moment of entry. Afterwards it counts as a normal reaction, otherwise a command
  // from an app would make the character ignore being shaken for the whole hold time.
  EmotionPriority held = p == EmotionPriority::Forced ? EmotionPriority::Reaction : p;
  final_ = e;
  pendingPrio_ = held;
  pendingHold_ = holdMs;
  Emotion hop = nextHop(cur_, e);
  prio_ = held;
  hold_ = hop == e ? holdMs : 0;
  // Forced requests from the test panel may break dwell, never the routing.
  if (dwellOver || p >= EmotionPriority::Strong) enter(hop);
  route_ = hop;
  return true;
}

void EmotionEngine::update(float dtMs) {
  age_ += dtMs;
  if (cur_ != final_) {
    // Walking through intermediate states: each must be seen for its dwell time.
    if (age_ >= minDwellMs(cur_)) {
      Emotion hop = nextHop(cur_, final_);
      enter(hop);
      if (hop == final_) { hold_ = pendingHold_; prio_ = pendingPrio_; }
    }
    return;
  }
  if (hold_ > 0) {
    hold_ -= dtMs;
    if (hold_ <= 0) { hold_ = 0; prio_ = EmotionPriority::Mood; }
    return;
  }
  prio_ = EmotionPriority::Mood;  // arrived with no hold time: nothing protects this emotion any more
  if (prio_ <= EmotionPriority::Idle && mood_ != cur_ && age_ >= minDwellMs(cur_)) {
    final_ = mood_;
    pendingPrio_ = EmotionPriority::Mood;
    pendingHold_ = 0;
    enter(nextHop(cur_, mood_));
  }
}

}  // namespace cc
