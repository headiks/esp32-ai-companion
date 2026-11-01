#include "companion/audio.h"
#include <string.h>

namespace cc {

const char* soundCategoryName(SoundCategory c) {
  static const char* const n[] = {"emotions", "interactions", "system", "games"};
  return n[(int)c];
}

struct PackEntry { char name[32]; uint32_t offset; uint32_t samples; };

bool SoundPack::load(const uint8_t* data, uint32_t size) {
  count_ = 0;
  if (!data || size < 8 || memcmp(data, "CSP1", 4)) return false;
  uint32_t n;
  memcpy(&n, data + 4, 4);
  if (n > 512 || 8 + n * sizeof(PackEntry) > size) return false;
  data_ = data; size_ = size; count_ = (int)n;
  return true;
}

static PackEntry entryAt(const uint8_t* data, int i) {
  PackEntry e;
  memcpy(&e, data + 8 + (size_t)i * sizeof(PackEntry), sizeof(PackEntry));
  return e;
}

int SoundPack::find(const char* name) const {
  for (int i = 0; i < count_; i++)
    if (!strncmp((const char*)data_ + 8 + (size_t)i * sizeof(PackEntry), name, 32)) return i;
  return -1;
}

const char* SoundPack::name(int i) const {
  return (i >= 0 && i < count_) ? (const char*)data_ + 8 + (size_t)i * sizeof(PackEntry) : "";
}

const int16_t* SoundPack::pcm(int i, uint32_t* samples) const {
  *samples = 0;
  if (i < 0 || i >= count_) return nullptr;
  PackEntry e = entryAt(data_, i);
  if ((uint64_t)e.offset + (uint64_t)e.samples * 2 > size_) return nullptr;
  *samples = e.samples;
  return (const int16_t*)(data_ + e.offset);
}

float SoundPack::durationMs(int i) const {
  uint32_t n;
  pcm(i, &n);
  return n * 1000.f / kSampleRate;
}

void AudioManager::push(const Cmd& c) {
  uint32_t h = head_.load(std::memory_order_relaxed);
  if (h - tail_.load(std::memory_order_acquire) >= kRing) return;  // full: dropping a sound beats blocking the UI
  ring_[h % kRing] = c;
  head_.store(h + 1, std::memory_order_release);
}

void AudioManager::play(int sound, SoundCategory cat, float gain, int priority) {
  if (sound < 0 || !pack_ || muted_ || !catOn_[(int)cat]) return;
  push(Cmd{(int16_t)sound, (int16_t)priority, (uint8_t)cat, 0, gain, nullptr, 0});
}

void AudioManager::playPcm(const int16_t* pcm, uint32_t n) {
  if (!muted_) push(Cmd{-1, 9, 0, 1, 1.0f, pcm, n});
}

void AudioManager::stopAll() { push(Cmd{-1, 0, 0, 2, 0, nullptr, 0}); }

void AudioManager::start(Voice& v, Pending& p, const int16_t* pcm, uint32_t n, float gain, int prio) {
  bool busy = v.pcm && v.fadeOut == 0;
  if (busy) {
    // Conflict rule: an equal or stronger sound interrupts with a short fade; a weaker one is
    // dropped, unless the current sound is about to finish, then it waits as the single queue slot.
    if (prio < v.prio) {
      if (n && !p.valid && v.n - v.pos < kSampleRate / 5) p = Pending{pcm, n, gain, prio, true};
      return;
    }
    v.fadeOut = kSampleRate / 100;  // 10 ms, avoids clicks
    p = Pending{pcm, n, gain, prio, true};
    return;
  }
  if (v.pcm) { p = Pending{pcm, n, gain, prio, true}; return; }  // already fading: replace what comes next
  v = Voice{};
  v.pcm = pcm; v.n = n; v.gain = gain; v.prio = prio; v.fadeIn = kSampleRate / 500;
}

void AudioManager::mix(int16_t* out, int samples) {
  uint32_t t = tail_.load(std::memory_order_relaxed);
  while (t != head_.load(std::memory_order_acquire)) {
    const Cmd& c = ring_[t % kRing];
    if (c.kind == 2) {
      for (int i = 0; i < kVoices; i++) {
        if (voices_[i].pcm && !voices_[i].fadeOut) voices_[i].fadeOut = kSampleRate / 100;
        pending_[i].valid = false;
      }
    } else if (c.kind == 1) {
      start(voices_[0], pending_[0], c.pcm, c.n, c.gain, c.priority);
    } else if (pack_) {
      uint32_t n;
      const int16_t* pcm = pack_->pcm(c.sound, &n);
      if (pcm) {
        int vi = c.cat == (uint8_t)SoundCategory::Emotion ? 0 : 1;
        // Game results may speak over the emotional voice; other effects stay on their own voice.
        start(voices_[vi], pending_[vi], pcm, n, c.gain, c.priority);
        last_.store(c.sound);
        playCount_++;
      }
    }
    t++;
    tail_.store(t, std::memory_order_release);
  }

  float vt = volumeTarget_.load();
  // Volume is ramped in about 100 ms per unit, so slider moves do not click.
  float vstep = 1.0f / (kSampleRate * 0.1f);
  int active = 0;
  for (int i = 0; i < samples; i++) {
    int32_t acc = 0;
    if (volume_ < vt) { volume_ += vstep; if (volume_ > vt) volume_ = vt; }
    else if (volume_ > vt) { volume_ -= vstep; if (volume_ < vt) volume_ = vt; }
    for (int k = 0; k < kVoices; k++) {
      Voice& v = voices_[k];
      if (!v.pcm) continue;
      float g = v.gain;
      if (v.fadeIn > 0) { g *= 1.f - (float)v.fadeIn / (kSampleRate / 500); v.fadeIn--; }
      if (v.fadeOut > 0) {
        g *= (float)v.fadeOut / (kSampleRate / 100);
        v.fadeOut--;
        if (v.fadeOut == 0) v.pos = v.n;
      }
      if (v.pos >= v.n) {
        v = Voice{};
        Pending& p = pending_[k];
        if (p.valid) {
          v.pcm = p.pcm; v.n = p.n; v.gain = p.gain; v.prio = p.prio; v.fadeIn = kSampleRate / 500;
          p.valid = false;
        }
        continue;
      }
      acc += (int32_t)((float)v.pcm[v.pos++] * g);
    }
    acc = (int32_t)((float)acc * volume_ * volume_);  // perceptual curve
    if (acc > 32767) acc = 32767;
    if (acc < -32768) acc = -32768;
    out[i] = (int16_t)acc;
  }
  for (int k = 0; k < kVoices; k++) if (voices_[k].pcm) active++;
  active_.store(active);
}

// Gains are per emotion: quiet moods stay quiet even at high volume.
static const float kGain[kEmotionCount] = {
    /*Neutral*/ .45f, /*Happy*/ .85f, /*VeryHappy*/ .95f, /*Sad*/ .6f, /*Angry*/ .8f, /*Annoyed*/ .6f, /*Surprised*/ .9f,
    /*Curious*/ .75f, /*Thinking*/ .55f, /*Sleepy*/ .45f, /*Sleeping*/ .4f, /*WakingUp*/ .65f, /*Scared*/ .85f,
    /*Affection*/ .7f, /*Bored*/ .45f, /*Attentive*/ .5f, /*Confused*/ .7f, /*Excited*/ .95f,
    /*Mischievous*/ .7f, /*Shy*/ .5f, /*Proud*/ .7f, /*Dizzy*/ .7f, /*Yawning*/ .5f, /*Cold*/ .6f, /*Hot*/ .5f, /*Dreamy*/ .45f, /*Focused*/ .45f, /*Winking*/ .6f,
    /*Smug*/ .6f, /*Skeptical*/ .55f, /*Guilty*/ .5f, /*Embarrassed*/ .5f, /*Hopeful*/ .6f, /*Lonely*/ .45f, /*Grateful*/ .7f, /*Jealous*/ .55f, /*Determined*/ .65f,
    /*Terrified*/ .9f, /*Disgusted*/ .6f, /*Hungry*/ .6f, /*Starstruck*/ .8f, /*Laughing*/ .85f, /*Crying*/ .65f, /*Relieved*/ .5f, /*Sneaky*/ .45f, /*Nostalgic*/ .45f,
    /*Playful*/ .75f, /*Amazed*/ .8f, /*Exhausted*/ .45f, /*Suspicious*/ .5f};
// Minimum silence before the same emotion may speak again.
static const uint16_t kCooldownMs[kEmotionCount] = {
    /*Neutral*/ 15000, /*Happy*/ 4000, /*VeryHappy*/ 4000, /*Sad*/ 8000, /*Angry*/ 4000, /*Annoyed*/ 5000, /*Surprised*/ 2500,
    /*Curious*/ 6000, /*Thinking*/ 8000, /*Sleepy*/ 15000, /*Sleeping*/ 30000, /*WakingUp*/ 8000, /*Scared*/ 2500,
    /*Affection*/ 5000, /*Bored*/ 20000, /*Attentive*/ 5000, /*Confused*/ 5000, /*Excited*/ 3000,
    /*Mischievous*/ 8000, /*Shy*/ 8000, /*Proud*/ 6000, /*Dizzy*/ 4000, /*Yawning*/ 15000, /*Cold*/ 12000, /*Hot*/ 12000, /*Dreamy*/ 20000, /*Focused*/ 12000, /*Winking*/ 6000,
    /*Smug*/ 8000, /*Skeptical*/ 8000, /*Guilty*/ 8000, /*Embarrassed*/ 8000, /*Hopeful*/ 8000, /*Lonely*/ 20000, /*Grateful*/ 5000, /*Jealous*/ 10000, /*Determined*/ 8000,
    /*Terrified*/ 2500, /*Disgusted*/ 6000, /*Hungry*/ 15000, /*Starstruck*/ 5000, /*Laughing*/ 4000, /*Crying*/ 10000, /*Relieved*/ 10000, /*Sneaky*/ 12000, /*Nostalgic*/ 20000,
    /*Playful*/ 5000, /*Amazed*/ 4000, /*Exhausted*/ 20000, /*Suspicious*/ 10000};
// Strong reactions may interrupt gentle ones, never the reverse.
static const uint8_t kPrio[kEmotionCount] = {
    /*Neutral*/ 1, /*Happy*/ 3, /*VeryHappy*/ 4, /*Sad*/ 2, /*Angry*/ 4, /*Annoyed*/ 3, /*Surprised*/ 5, /*Curious*/ 2, /*Thinking*/ 1,
    /*Sleepy*/ 1, /*Sleeping*/ 1, /*WakingUp*/ 3, /*Scared*/ 5, /*Affection*/ 3, /*Bored*/ 1, /*Attentive*/ 2, /*Confused*/ 2, /*Excited*/ 4,
    /*Mischievous*/ 2, /*Shy*/ 2, /*Proud*/ 3, /*Dizzy*/ 4, /*Yawning*/ 1, /*Cold*/ 2, /*Hot*/ 1, /*Dreamy*/ 1, /*Focused*/ 1, /*Winking*/ 2,
    /*Smug*/ 2, /*Skeptical*/ 2, /*Guilty*/ 2, /*Embarrassed*/ 2, /*Hopeful*/ 2, /*Lonely*/ 1, /*Grateful*/ 3, /*Jealous*/ 2, /*Determined*/ 2,
    /*Terrified*/ 5, /*Disgusted*/ 3, /*Hungry*/ 2, /*Starstruck*/ 4, /*Laughing*/ 4, /*Crying*/ 3, /*Relieved*/ 2, /*Sneaky*/ 1, /*Nostalgic*/ 1,
    /*Playful*/ 3, /*Amazed*/ 4, /*Exhausted*/ 1, /*Suspicious*/ 2};

float SoundEmotionEngine::gainOf(Emotion e) const { return kGain[(int)e]; }
float SoundEmotionEngine::cooldownOf(Emotion e) const { return kCooldownMs[(int)e]; }

void SoundEmotionEngine::attach(AudioManager* a, const SoundPack* p) {
  audio_ = a; pack_ = p;
  char name[48];
  for (int i = 0; i < kEmotionCount; i++) {
    const char* en = emotionName((Emotion)i);
    int n = 0;
    const char* pre = "emotions/";
    while (*pre) name[n++] = *pre++;
    while (*en) name[n++] = *en++;
    name[n] = 0;
    ids_[i][0] = p ? p->find(name) : -1;
    name[n++] = '_'; name[n++] = 'b'; name[n] = 0;
    ids_[i][1] = p ? p->find(name) : -1;
    lastAt_[i] = -1e9f;
    next_[i] = 0;
  }
}

void SoundEmotionEngine::resetCooldowns() { for (float& t : lastAt_) t = -1e9f; }

void SoundEmotionEngine::onEmotionCue(Emotion e, float nowMs) {
  int i = (int)e;
  if (!enabled || !audio_ || nowMs - lastAt_[i] < kCooldownMs[i]) return;
  lastAt_[i] = nowMs;
  int id = ids_[i][next_[i]];
  if (ids_[i][1] >= 0) next_[i] ^= 1;  // alternating variants keep repeats from feeling mechanical
  audio_->play(id, SoundCategory::Emotion, kGain[i], kPrio[i]);
}

void SoundEmotionEngine::play(const char* name, float) {
  if (!audio_ || !pack_) return;
  int id = pack_->find(name);
  SoundCategory cat = SoundCategory::System;
  if (!strncmp(name, "emotions/", 9)) cat = SoundCategory::Emotion;
  else if (!strncmp(name, "interactions/", 13)) cat = SoundCategory::Interaction;
  else if (!strncmp(name, "games/", 6)) cat = SoundCategory::Game;
  audio_->play(id, cat, cat == SoundCategory::System ? 0.8f : 0.7f, 2);
}

}  // namespace cc
