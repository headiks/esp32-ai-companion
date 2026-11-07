#include "companion/face.h"
#include <string.h>
#include "ui/components.h"

namespace cc {
using namespace theme;

// Eye parameters of design 1a, from the table in section 5 of documentation/design/1a/claude-code-prompt.md:
//   P(t, a, b, px, py, s, pr) for the left eye and the right one, pupil shape, overlay, then the behaviour of
//   the emotion: transition ms, blinks per minute, saccade amplitude and period, bob amplitude and frequency,
//   tremble, preferred gaze x and y (only for a wandering look: px, py already aim the eyes), sound delay,
//   double blink %, wandering look.
#define P(...) EyePose{__VA_ARGS__}
static const EmotionDef kDefs[kEmotionCount] = {
    // neutral: Спокоен
    {P(.14, 0, 0, 0, 0, 1, .17), P(.14, 0, 0, 0, 0, 1, .17), PupilShape::Round, Overlay::None, 250, 13, 22, 1700, 2.2f, .28f, 0, 0, 0, 40, 15, false},
    // happy: Рад
    {P(0, 0, .42, 0, -.2, 1, .17), P(0, 0, .42, 0, -.2, 1, .17), PupilShape::Round, Overlay::None, 220, 16, 16, 1400, 4.f, .9f, 0, 0, 0, 60, 25, false},
    // very_happy: Очень рад
    {P(0, 0, .78, 0, 0, 1, 0), P(0, 0, .78, 0, 0, 1, 0), PupilShape::Round, Overlay::Sparkle, 200, 0, 0, 3000, 9.f, 2.1f, 0, 0, 0, 50, 0, false},
    // sad: Грустит
    {P(.36, -16, 0, 0, .35, 1, .17), P(.36, -16, 0, 0, .35, 1, .17), PupilShape::Round, Overlay::Tear, 250, 7, 10, 3200, 2.f, .18f, 0, 0, 0, 220, 10, false},
    // angry: Злится
    {P(.42, 24, .06, 0, .25, 1, .17), P(.42, 24, .06, 0, .25, 1, .17), PupilShape::Round, Overlay::None, 150, 6, 6, 2200, 0, 0, 1.2f, 0, 0, 20, 0, false},
    // annoyed: Раздражён
    {P(.5, 6, 0, 0, .3, 1, .17), P(.5, 6, 0, 0, .3, 1, .17), PupilShape::Round, Overlay::None, 250, 9, 6, 2600, 1.f, .2f, 0, 0, 0, 90, 0, false},
    // surprised: Удивлён
    {P(0, 0, 0, 0, 0, 1.14, .11), P(0, 0, 0, 0, 0, 1.14, .11), PupilShape::Round, Overlay::None, 150, 3, 4, 2500, 0, 0, 0, 0, 0, 15, 0, false},
    // curious: Любопытно
    {P(0, 0, 0, .3, -.2, 1.06, .17), P(.4, -6, 0, .3, -.2, 1, .17), PupilShape::Round, Overlay::Question, 250, 12, 30, 1100, 1.5f, .5f, 0, 0, 0, 80, 10, false},
    // thinking: Думает
    {P(.2, -4, 0, .45, -.5, 1, .17), P(.42, -4, 0, .45, -.5, 1, .17), PupilShape::Round, Overlay::Dots, 250, 7, 14, 1900, 1.f, .25f, 0, -34, 0, 130, 0, true},
    // sleepy: Сонный
    {P(.62, -3, 0, 0, .45, 1, .17), P(.62, -3, 0, 0, .45, 1, .17), PupilShape::Round, Overlay::None, 250, 16, 8, 3800, 3.f, .16f, 0, 0, 0, 260, 0, false},
    // sleeping: Спит
    {P(.86, 0, 0, 0, 0, 1, 0), P(.86, 0, 0, 0, 0, 1, 0), PupilShape::Round, Overlay::Zzz, 250, 0, 0, 9000, 4.f, .13f, 0, 0, 0, 500, 0, false},
    // waking_up: Просыпается
    {P(.5, 0, .12, 0, .25, 1, .17), P(.5, 0, .12, 0, .25, 1, .17), PupilShape::Round, Overlay::None, 250, 40, 18, 900, 1.5f, .25f, 0, 0, 0, 180, 90, false},
    // scared: Испуган
    {P(.06, -10, 0, 0, 0, 1.1, .09), P(.06, -10, 0, 0, 0, 1.1, .09), PupilShape::Round, Overlay::None, 150, 12, 18, 420, 0, 0, 2.6f, 0, 0, 10, 30, false},
    // affection: Нежность
    {P(0, 0, .3, 0, 0, 1, .2), P(0, 0, .3, 0, 0, 1, .2), PupilShape::Heart, Overlay::Hearts, 250, 11, 6, 2800, 3.f, .55f, 0, 0, 0, 120, 30, false},
    // bored: Скучает
    {P(.55, -4, 0, .3, .4, 1, .17), P(.55, -4, 0, .3, .4, 1, .17), PupilShape::Round, Overlay::None, 250, 8, 18, 3400, 1.5f, .15f, 0, -26, 0, 200, 0, true},
    // attentive: Внимателен
    {P(.06, 0, 0, 0, -.1, 1.05, .17), P(.06, 0, 0, 0, -.1, 1.05, .17), PupilShape::Round, Overlay::None, 180, 9, 4, 2600, .8f, .4f, 0, 0, 0, 40, 0, false},
    // confused: Растерян
    {P(.05, 6, 0, 0, 0, 1.05, .17), P(.4, -14, 0, 0, 0, 1, .17), PupilShape::Round, Overlay::Question, 250, 15, 24, 800, 1.f, .35f, 0, 0, 0, 100, 40, true},
    // excited: Воодушевлён
    {P(0, 0, .25, 0, 0, 1.08, .24), P(0, 0, .25, 0, 0, 1.08, .24), PupilShape::Round, Overlay::Sparkle, 160, 18, 14, 700, 7.f, 2.8f, 0, 0, 0, 30, 20, false},
    // mischievous: Хитрый
    {P(.34, 22, .22, -.2, 0, 1, .17), P(.34, 22, .22, -.2, 0, 1, .17), PupilShape::Round, Overlay::None, 250, 12, 18, 1500, 1.2f, .6f, 0, 0, 0, 80, 10, false},
    // shy: Стесняется
    {P(.2, 0, .28, .25, .35, 1, .17), P(.2, 0, .28, .25, .35, 1, .17), PupilShape::Round, Overlay::Blush, 250, 22, 12, 1300, 1.f, .4f, 0, -24, 0, 120, 40, true},
    // proud: Гордится
    {P(.4, -8, .25, 0, -.1, 1, .17), P(.4, -8, .25, 0, -.1, 1, .17), PupilShape::Round, Overlay::Sparkle, 250, 8, 6, 2600, 1.2f, .3f, 0, 0, 0, 90, 0, false},
    // dizzy: Кружится голова
    {P(0, 0, 0, 0, 0, 1.08, .16), P(0, 0, 0, 0, 0, 1.08, .16), PupilShape::Spiral, Overlay::None, 250, 4, 0, 5000, 2.f, .9f, 0, 0, 0, 40, 0, false},
    // yawning: Зевает
    {P(.8, 0, .06, 0, 0, 1, 0), P(.8, 0, .06, 0, 0, 1, 0), PupilShape::Round, Overlay::None, 250, 0, 4, 3000, 2.f, .15f, 0, 0, 0, 200, 0, false},
    // cold: Мёрзнет
    {P(.3, 0, .3, 0, -.1, 1, .17), P(.3, 0, .3, 0, -.1, 1, .17), PupilShape::Round, Overlay::Frost, 250, 20, 8, 1400, 0, 0, 1.7f, 0, 0, 80, 20, false},
    // hot: Жарко
    {P(.5, 0, 0, 0, .3, 1, .17), P(.5, 0, 0, 0, .3, 1, .17), PupilShape::Round, Overlay::Sweat, 250, 14, 10, 2600, 2.5f, .2f, 0, 0, 0, 150, 0, false},
    // dreamy: Мечтает
    {P(.4, -10, .3, .2, -.6, 1, .17), P(.4, -10, .3, .2, -.6, 1, .17), PupilShape::Round, Overlay::None, 250, 9, 26, 2400, 2.5f, .17f, 0, 12, 0, 240, 0, true},
    // focused: Сосредоточен
    {P(.42, 10, .2, 0, 0, 1, .15), P(.42, 10, .2, 0, 0, 1, .15), PupilShape::Round, Overlay::None, 220, 5, 3, 3000, .4f, .3f, 0, 0, 0, 50, 0, false},
    // winking: Подмигивает
    {P(.1, 0, .2, 0, 0, 1, .17), P(.86, 0, 0, 0, 0, 1, 0), PupilShape::Round, Overlay::None, 170, 0, 8, 2000, 2.f, .8f, 0, 0, 0, 40, 0, false},
    // smug: Доволен собой
    {P(.52, -6, .12, .2, 0, 1, .17), P(.52, -6, .12, .2, 0, 1, .17), PupilShape::Round, Overlay::None, 250, 9, 8, 2400, 1.f, .3f, 0, 0, 0, 90, 0, false},
    // skeptical: Сомневается
    {P(.1, 4, 0, 0, 0, 1.05, .17), P(.5, -6, 0, 0, 0, 1, .17), PupilShape::Round, Overlay::None, 250, 8, 8, 2000, .6f, .3f, 0, 0, 0, 80, 0, false},
    // guilty: Виноват
    {P(.2, -18, 0, -.3, .5, 1, .17), P(.2, -18, 0, -.3, .5, 1, .17), PupilShape::Round, Overlay::None, 250, 14, 12, 1500, 1.f, .2f, 0, -18, 0, 150, 20, true},
    // embarrassed: Смущён
    {P(.14, 0, .35, 0, .1, 1, .17), P(.14, 0, .35, 0, .1, 1, .17), PupilShape::Round, Overlay::Blush, 250, 34, 14, 800, 1.f, .8f, .5f, 0, 0, 70, 50, true},
    // hopeful: Надеется
    {P(0, -12, .1, 0, -.6, 1, .2), P(0, -12, .1, 0, -.6, 1, .2), PupilShape::Round, Overlay::None, 250, 10, 6, 2400, 1.5f, .4f, 0, 0, 0, 110, 10, false},
    // lonely: Одиноко
    {P(.34, -14, 0, -.4, .4, 1, .17), P(.34, -14, 0, -.4, .4, 1, .17), PupilShape::Round, Overlay::Tear, 250, 7, 12, 3600, 1.5f, .13f, 0, -30, 0, 280, 10, true},
    // grateful: Благодарен
    {P(0, 0, .75, 0, 0, 1, 0), P(0, 0, .75, 0, 0, 1, 0), PupilShape::Round, Overlay::Hearts, 250, 0, 0, 3000, 2.5f, .5f, 0, 0, 0, 110, 0, false},
    // jealous: Ревнует
    {P(.55, 8, 0, .6, .1, 1, .17), P(.55, 8, 0, .6, .1, 1, .17), PupilShape::Round, Overlay::None, 250, 6, 5, 2600, .5f, .2f, 0, 0, 0, 90, 0, false},
    // determined: Решителен
    {P(.38, 26, 0, 0, .1, 1, .2), P(.38, 26, 0, 0, .1, 1, .2), PupilShape::Round, Overlay::None, 200, 5, 3, 3000, .6f, .5f, 0, 0, 0, 40, 0, false},
    // terrified: В ужасе
    {P(0, -8, 0, 0, 0, 1.14, .06), P(0, -8, 0, 0, 0, 1.14, .06), PupilShape::Round, Overlay::Sweat, 150, 6, 26, 300, 0, 0, 4.f, 0, 0, 10, 0, false},
    // disgusted: Брезгует
    {P(.55, 10, .32, 0, 0, 1, .17), P(.62, 10, .32, 0, 0, 1, .17), PupilShape::Round, Overlay::None, 240, 12, 6, 1800, .8f, .4f, 0, 0, 0, 60, 0, false},
    // hungry: Голоден
    {P(.1, 0, 0, 0, .5, 1, .17), P(.1, 0, 0, 0, .5, 1, .17), PupilShape::Round, Overlay::Drool, 250, 10, 8, 1500, 1.2f, .3f, 0, 0, 0, 120, 0, false},
    // starstruck: Восхищён
    {P(0, 0, 0, 0, 0, 1.08, .22), P(0, 0, 0, 0, 0, 1.08, .22), PupilShape::Star, Overlay::Sparkle, 200, 8, 4, 2000, 4.f, 1.2f, 0, 0, 0, 40, 0, false},
    // laughing: Смеётся
    {P(0, 0, .82, 0, 0, 1, 0), P(0, 0, .82, 0, 0, 1, 0), PupilShape::Round, Overlay::Joy, 180, 0, 0, 3000, 6.f, 4.f, 2.4f, 0, 0, 30, 0, false},
    // crying: Плачет
    {P(.78, -14, 0, 0, 0, 1, 0), P(.78, -14, 0, 0, 0, 1, 0), PupilShape::Round, Overlay::Streams, 250, 0, 0, 4000, 3.f, 1.5f, .8f, 0, 0, 180, 0, false},
    // relieved: Облегчение
    {P(.45, 0, .18, 0, 0, 1, .15), P(.45, 0, .18, 0, 0, 1, .15), PupilShape::Round, Overlay::None, 250, 8, 4, 3200, 3.f, .15f, 0, 0, 0, 200, 0, false},
    // sneaky: Крадётся
    {P(.55, 4, .1, .7, 0, 1, .17), P(.55, 4, .1, .7, 0, 1, .17), PupilShape::Round, Overlay::None, 240, 5, 40, 700, .4f, .3f, 0, 40, 0, 60, 0, true},
    // nostalgic: Ностальгия
    {P(.3, -8, .3, -.3, -.5, 1, .17), P(.3, -8, .3, -.3, -.5, 1, .17), PupilShape::Round, Overlay::None, 250, 8, 10, 4200, 2.f, .14f, 0, -28, 0, 260, 0, true},
    // playful: Игривый
    {P(.2, 0, .25, 0, 0, 1, .17), P(.45, -10, .2, 0, 0, 1, .17), PupilShape::Round, Overlay::None, 200, 15, 20, 800, 5.f, 1.6f, 0, 0, 0, 50, 20, false},
    // amazed: Изумлён
    {P(0, 0, 0, 0, 0, 1.12, .24), P(0, 0, 0, 0, 0, 1.12, .24), PupilShape::Round, Overlay::None, 200, 4, 4, 2600, .8f, .3f, 0, 0, 0, 40, 0, false},
    // exhausted: Вымотан
    {P(.66, -8, 0, 0, .4, 1, .17), P(.72, -8, 0, 0, .4, 1, .17), PupilShape::Round, Overlay::Sweat, 250, 12, 4, 4000, 3.f, .1f, 0, 0, 0, 280, 0, false},
    // suspicious: Подозревает
    {P(.44, -4, .26, .6, 0, 1, .17), P(.44, -4, .26, .6, 0, 1, .17), PupilShape::Round, Overlay::None, 250, 5, 10, 1100, .5f, .2f, 0, 30, 0, 90, 0, true},
};
#undef P
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) == kEmotionCount, "one row per emotion");

const EmotionDef& emotionDef(Emotion e) { return kDefs[(int)e]; }

static EyePose lerpPose(const EyePose& a, const EyePose& b, float k) {
  EyePose r;
  const float* pa = (const float*)&a;
  const float* pb = (const float*)&b;
  float* pr = (float*)&r;
  for (int i = 0; i < kPoseFields; i++) pr[i] = pa[i] + (pb[i] - pa[i]) * k;
  return r;
}

AnimationEngine::AnimationEngine() {
  from_[0] = cur_[0] = out_[0] = kDefs[0].left;
  from_[1] = cur_[1] = out_[1] = kDefs[0].right;
}

void AnimationEngine::setEmotion(Emotion e) {
  if (e == emotion_) return;
  previous_ = emotion_;
  emotion_ = e;
  from_[0] = cur_[0];  // a new emotion mid-way starts from where the face is, never jumps
  from_[1] = cur_[1];
  progress_ = 0;
  age_ = 0;
  soundPending_ = true;
  nextSaccade_ = 300;
}

PupilShape AnimationEngine::pupil() const { return emotionDef(progress_ < 0.5f ? previous_ : emotion_).pupil; }

bool AnimationEngine::soundCueReady() {
  if (!soundPending_ || age_ < emotionDef(emotion_).soundDelayMs) return false;
  soundPending_ = false;
  return true;
}

void AnimationEngine::lookAt(float x, float y, float holdMs) {
  lookOffset((x - kScreenW / 2.f) * 0.3f, (y - kScreenH / 2.f) * 0.25f, holdMs);
}

void AnimationEngine::lookOffset(float dx, float dy, float holdMs) {
  lookX_ = dx; lookY_ = dy; lookHold_ = holdMs;
}

void AnimationEngine::independentLook(float lx, float ly, float rx, float ry, float holdMs) {
  indepX_[0] = lx; indepY_[0] = ly; indepX_[1] = rx; indepY_[1] = ry; indepHold_ = holdMs;
}

void AnimationEngine::blink(float ms, bool twice) {
  if (blinkT_ >= 0) return;
  blinkDur_ = ms; blinkT_ = 0; blinkTwice_ = twice;
}

void AnimationEngine::kick(float vy) { kickV_ = vy; }

void AnimationEngine::updateBlink(float dt) {
  const EmotionDef& d = emotionDef(emotion_);
  float rate = d.blinksPerMin * blinkScale_ * lerpf(0.75f, 1.25f, arousal_);
  if (blinkT_ < 0 && blinkPause_ <= 0 && rate > 0.01f) {
    nextBlink_ -= dt;
    if (nextBlink_ <= 0) {
      bool twice = rng_.range(0, 99) < d.doubleBlinkPct;
      float dur = emotion_ == Emotion::Sleepy ? 520.f : lerpf(260.f, 150.f, arousal_);
      blink(dur, twice);
      // Humans blink irregularly: Poisson-like spacing around the configured rate.
      float mean = 60000.f / rate;
      nextBlink_ = mean * rng_.rangef(0.45f, 1.55f);
    }
  }
  float amt = 0;
  if (blinkT_ >= 0) {
    blinkT_ += dt;
    float p = blinkT_ / blinkDur_;
    // Fast close, slower open, as real eyelids do.
    amt = p < 0.38f ? smooth01(p / 0.38f) : 1 - smooth01((p - 0.38f) / 0.62f);
    if (p >= 1) {
      amt = 0;
      blinkT_ = -1;
      if (blinkTwice_) { blinkTwice_ = false; blinkPause_ = blinkDur_ * 0.5f; }
    }
  } else if (blinkPause_ > 0) {
    blinkPause_ -= dt;
    if (blinkPause_ <= 0) blinkT_ = 0;
  }
  float tgt = forceClosed_ ? 1.f : 0.f;
  closedAmt_ += (tgt - closedAmt_) * clampf(dt / (forceClosed_ ? 420.f : 700.f), 0, 1);
  blinkAmt_ = amt > closedAmt_ ? amt : closedAmt_;
}

void AnimationEngine::updateGaze(float dt, const EmotionDef& d) {
  if (lookHold_ > 0) {
    lookHold_ -= dt;
    gazeTX_ = lookX_; gazeTY_ = lookY_;
    nextSaccade_ = 600;
  } else if (emotion_ == Emotion::Dizzy) {
    gazeTX_ = 26 * cosf(t_ * 0.007f);
    gazeTY_ = 22 * sinf(t_ * 0.007f);
  } else if (pursuit_ > 0) {
    pursuit_ -= dt;
    gazeTX_ = 42 * sinf(t_ * 0.0013f) + 14 * sinf(t_ * 0.0041f);
    gazeTY_ = 22 * sinf(t_ * 0.0019f + 1.f);
    nextSaccade_ = 500;
  } else {
    nextSaccade_ -= dt;
    if (nextSaccade_ <= 0) {
      if (d.wanderLook) {
        // Slow wandering stays on one side of the preferred direction, then switches.
        gazeTX_ = d.gazeX * (rng_.range(0, 2) == 0 ? -1.f : 1.f) + rng_.rangef(-d.saccadeAmp, d.saccadeAmp) * 0.5f;
        gazeTY_ = d.gazeY + rng_.rangef(-d.saccadeAmp, d.saccadeAmp) * 0.25f;
      } else {
        gazeTX_ = d.gazeX + rng_.rangef(-d.saccadeAmp, d.saccadeAmp);
        gazeTY_ = d.gazeY + rng_.rangef(-d.saccadeAmp, d.saccadeAmp) * 0.5f;
        if (rng_.range(0, 3) == 0) { gazeTX_ = d.gazeX; gazeTY_ = d.gazeY; }  // often returns to the viewer
      }
      nextSaccade_ = d.saccadeEveryMs * rng_.rangef(0.5f, 1.5f) * lerpf(1.4f, 0.7f, arousal_);
      // Big gaze shifts often come with a blink.
      if (fabsf(gazeTX_ - gazeX_) > 18 && rng_.range(0, 3) == 0) blink(150);
    }
  }
  // Fixation is never perfectly still: microsaccades.
  nextMicro_ -= dt;
  if (nextMicro_ <= 0) {
    microX_ = rng_.rangef(-2.6f, 2.6f);
    microY_ = rng_.rangef(-1.6f, 1.6f);
    nextMicro_ = rng_.rangef(280, 1200);
  }
  if (indepHold_ > 0) indepHold_ -= dt;
  // Saccades are fast: about 40 ms time constant.
  float k = clampf(dt / 42.f, 0, 1);
  // Eyes counter-rotate against a device tilt, so the creature keeps looking at the world.
  gazeX_ += (gazeTX_ + microX_ - tiltX_ * 16 - gazeX_) * k;
  gazeY_ += (gazeTY_ + microY_ - tiltY_ * 12 - gazeY_) * k;
}

void AnimationEngine::setDrive(float arousal, float valence, float light, float tiltX, float tiltY) {
  arousal_ = clampf(arousal, 0, 1);
  valence_ = clampf(valence, -1, 1);
  light_ = clampf(light, 0, 1);
  tiltX_ += (clampf(tiltX, -1, 1) - tiltX_) * 0.12f;
  tiltY_ += (clampf(tiltY, -1, 1) - tiltY_) * 0.12f;
}

// Smooth pseudo-noise from incommensurate sines: never repeats visibly and costs three sinf calls.
static inline float wander(float t, float seed) {
  return (sinf(t + seed) + 0.6f * sinf(t * 2.33f + seed * 1.7f) + 0.35f * sinf(t * 5.1f + seed * 2.9f)) / 1.95f;
}

// A head tilt in degrees that belongs to the emotion.
static float emotionRoll(Emotion e) {
  switch (e) {
    case Emotion::Curious: return 7;
    case Emotion::Confused: return -6;
    case Emotion::Shy: return 5;
    case Emotion::Dreamy: return 3;
    case Emotion::Winking: return -4;
    case Emotion::Sleepy: return 2.5f;
    case Emotion::Skeptical: return 5;
    case Emotion::Playful: return -6;
    case Emotion::Nostalgic: return -3;
    case Emotion::Lonely: return 4;
    case Emotion::Exhausted: return 6;
    case Emotion::Hopeful: return -3;
    default: return 0;
  }
}


// Pupil travel per px of gaze: the gaze is kept in px of the old face, where it moved the pupil by 0.75 px;
// one unit of px and py is 27 % of the 132 x 116 eye.
static constexpr float kGazeX = 0.75f / (132 * 0.27f), kGazeY = 0.75f / (116 * 0.27f);

void AnimationEngine::update(float dtMs) {
  float dt = clampf(dtMs, 0, 100) * speed_;
  t_ += dt;
  age_ += dt;
  const EmotionDef& d = emotionDef(emotion_);
  const EmotionDef& d2 = emotionDef(blendE_);
  blend_ += (blendTarget_ - blend_) * clampf(dt / 1800.f, 0, 1);
  updateBlink(dt);
  updateGaze(dt, d);

  // Ease-out from the pose at the moment of the change: fast start, soft landing, 150..250 ms.
  progress_ = clampf(progress_ + dt / d.transitionMs, 0, 1);
  float q = 1 - progress_, k = 1 - q * q * q;
  const EyePose* ta[2] = {&d.left, &d.right};
  const EyePose* tb[2] = {&d2.left, &d2.right};
  for (int e = 0; e < 2; e++) {
    // A closed eye or a "^" arc has no pupil to mix: blending would open it.
    float bl = ta[e]->pr == 0 ? 0 : blend_;
    cur_[e] = lerpPose(from_[e], lerpPose(*ta[e], *tb[e], bl), k);
  }

  // Body: breathing, sway and jitter, all scaled by arousal.
  float ts = t_ * 0.001f;
  const float tau = 6.2831853f;
  float bob = d.bobAmp * sinf(ts * tau * d.bobHz);
  float breathHz = lerpf(0.14f, 0.34f, arousal_);
  float breath = lerpf(2.0f, 1.0f, arousal_) * sinf(ts * tau * breathHz);
  float sway = 2.2f * wander(ts * 0.31f, 1.3f);
  float tx = 0, ty = 0;
  if (d.tremble > 0) {
    tx = d.tremble * sinf(ts * 91.f) * sinf(ts * 13.f);
    ty = d.tremble * 0.6f * sinf(ts * 77.f + 1.f);
  }
  kickV_ += (-0.00012f * kickY_ - 0.012f * kickV_) * dt;
  kickY_ += kickV_ * dt;
  bodyX_ = sway + tx + tiltX_ * 7;
  bodyY_ = bob + breath + ty + kickY_ + tiltY_ * 6;
  float rollTarget = (emotionRoll(emotion_) + wander(ts * 0.17f, 9.f) * 2.4f + tiltX_ * 9) * 0.0174533f;
  roll_ += (rollTarget - roll_) * clampf(dt / 320.f, 0, 1);
  composeLayers();
}

void AnimationEngine::composeLayers() {
  float ts = t_ * 0.001f;
  float amp = lerpf(0.6f, 1.2f, arousal_);
  float rollDeg = roll_ * 57.29578f;
  for (int e = 0; e < 2; e++) {
    EyePose p = cur_[e];
    float side = e ? 1.f : -1.f;  // screen side
    float dir = -side;            // the spec's dir: +1 for the left eye
    // Lids breathe on their own, droop with drowsiness and follow the gaze down, as real lids do.
    p.t = clampf(p.t + wander(ts * 0.55f, 1.3f + e * 2.1f) * 0.018f * amp + (1 - arousal_) * 0.05f + gazeY_ * 0.0022f, 0, 0.96f);
    p.a += wander(ts * 0.31f, 4.f + e) * 1.2f - valence_ * 2.5f;
    if (p.b > 0) p.b = clampf(p.b + (valence_ > 0 ? valence_ : 0) * 0.04f, 0, 0.95f);
    // Pupil: hippus noise, darkness and interest dilate it.
    if (p.pr > 0) p.pr *= (1 + 0.07f * wander(ts * 0.9f, 7.f)) * lerpf(1.12f, 0.94f, light_) * (1 + 0.1f * (arousal_ - 0.5f));
    float gx = gazeX_ + (indepHold_ > 0 ? indepX_[e] : 0), gy = gazeY_ + (indepHold_ > 0 ? indepY_[e] : 0);
    p.px = clampf(p.px + gx * kGazeX, -1.1f, 1.1f);
    p.py = clampf(p.py + gy * kGazeY, -0.85f, 0.85f);  // the pupil never touches the rim, where it would read as a lid
    // The head rolls: one eye up, the other down, and both lids turn with it.
    p.a += dir * rollDeg;
    // A blink brings the lid to 0.9 and back.
    if (p.t < 0.9f) p.t += (0.9f - p.t) * blinkAmt_;
    out_[e] = p;
    offX_[e] = bodyX_;
    offY_[e] = bodyY_ + side * 85 * roll_;
  }
}

// ---- drawing ---------------------------------------------------------------------------------
// Geometry of the spec at full size: eyes 85 px either side of the centre, centre line at y 224. A mini face
// maps the region (56, 136)..(410, 312) onto its own rectangle; everything below is computed in screen pixels.

struct EyeGeom {
  float cx, x0, x1, y0, y1, rx;
  float lidY, lidK;                  // the white stays below y = lidY + lidK (x - cx)
  float cheekTop, cheekRise, cheekSpan;  // the white stays above the cheek parabola
  float pcx, pcy, pr;                // round pupil
  bool pupilPass;                    // second pass: only the pupil, clipped to the white
};

// Rows of the eye: rounded white, minus what is above the lid and below the cheek. In the pupil pass, the
// part of those rows covered by the pupil.
static int eyeSpans(float sy, float* xs, int max, const void* ctx) {
  const EyeGeom& g = *(const EyeGeom*)ctx;
  if (sy < g.y0 || sy > g.y1) return 0;
  float hw = (g.x1 - g.x0) / 2, e = 0;
  if (sy < g.y0 + g.rx) e = g.y0 + g.rx - sy;
  else if (sy > g.y1 - g.rx) e = sy - (g.y1 - g.rx);
  if (e > 0) {
    float q = g.rx * g.rx - e * e;
    if (q <= 0) return 0;
    hw += sqrtf(q) - g.rx;
  }
  float l = g.cx - hw, r = g.cx + hw;
  float d = sy - g.lidY;
  if (fabsf(g.lidK) < 1e-4f) {
    if (d < 0) return 0;
  } else if (g.lidK > 0) {
    float b = g.cx + d / g.lidK;
    if (b < r) r = b;
  } else {
    float b = g.cx + d / g.lidK;
    if (b > l) l = b;
  }
  if (r <= l) return 0;
  float part[4] = {l, r, 0, 0};
  int parts = 1;
  if (g.cheekRise > 0 && sy > g.cheekTop) {
    float cut = g.cheekSpan * sqrtf((sy - g.cheekTop) / g.cheekRise);
    part[1] = r < g.cx - cut ? r : g.cx - cut;
    part[2] = l > g.cx + cut ? l : g.cx + cut;
    part[3] = r;
    parts = 2;
  }
  float pl = 0, pr = 0;
  if (g.pupilPass) {
    float dy = sy - g.pcy, q = g.pr * g.pr - dy * dy;
    if (q <= 0) return 0;
    float h = sqrtf(q);
    pl = g.pcx - h;
    pr = g.pcx + h;
  }
  int n = 0;
  for (int i = 0; i < parts && n < max; i++) {
    float a = part[2 * i], b = part[2 * i + 1];
    if (g.pupilPass) {
      if (a < pl) a = pl;
      if (b > pr) b = pr;
    }
    if (b > a) { xs[2 * n] = a; xs[2 * n + 1] = b; n++; }
  }
  return n;
}

static void drawEyes(Canvas& c, const EyePose* pose, const float* offX, const float* offY, PupilShape shape,
                     float left, float top, float k, uint16_t color) {
  for (int e = 0; e < 2; e++) {
    const EyePose& p = pose[e];
    float dir = e ? -1.f : 1.f;
    float s = p.s > 0.5f ? p.s : 0.5f;
    float w = 132 * s * k, h = 116 * s * k;
    float cx = left + (233 - dir * 85 + offX[e] - 56) * k, cy = top + (224 + offY[e] - 136) * k;
    EyeGeom g;
    g.cx = cx; g.x0 = cx - w / 2; g.x1 = cx + w / 2; g.y0 = cy - h / 2; g.y1 = cy + h / 2; g.rx = 46 * s * k;
    g.lidY = g.y0 + clampf(p.t, 0, 1) * h;
    g.lidK = tanf(clampf(p.a, -60, 60) * 0.0174533f) * dir;
    float b = clampf(p.b, 0, 1);
    g.cheekTop = g.y1 - b * h;
    g.cheekRise = 0.6f * b * h;
    g.cheekSpan = w / 2 + 10 * k;
    g.pr = h * p.pr;
    g.pcx = cx + p.px * w * 0.27f;
    g.pcy = cy + p.py * h * 0.27f;
    g.pupilPass = false;
    c.spans(g.y0, g.y1, g.x0, g.x1, color, eyeSpans, &g);
    if (p.pr <= 0.01f) continue;
    float r = g.pr;
    switch (shape) {
      case PupilShape::Round:
        g.pupilPass = true;
        c.spans(g.pcy - r, g.pcy + r, g.pcx - r, g.pcx + r, BG, eyeSpans, &g);
        break;
      // Shaped pupils are not clipped by the lid: these emotions keep their eyes open, so hide them on a blink.
      case PupilShape::Heart:
        if (p.t < 0.5f) ui::heart(c, g.pcx, g.pcy, 1.5f * r, BG);
        break;
      case PupilShape::Spiral:
        if (p.t < 0.5f) {
          c.ring(g.pcx, g.pcy, 1.5f * r, 5 * k, BG);
          c.ring(g.pcx, g.pcy, 0.8f * r, 5 * k, BG);
          c.fillCircle(g.pcx, g.pcy, 0.28f * r, BG);
        }
        break;
      case PupilShape::Star:
        // Four points: outer radius 1.5 R, inner 0.42 R, as two crossed diamonds.
        if (p.t < 0.5f) {
          c.diamond(g.pcx, g.pcy, 1.5f * r, 0.37f * r, BG);
          c.diamond(g.pcx, g.pcy, 0.37f * r, 1.5f * r, BG);
        }
        break;
    }
  }
}

// A plus made of two bars with rounded ends.
static void plus(Canvas& c, float x, float y, float half, float thick, uint16_t col) {
  c.line(x - half, y, x + half, y, thick, col);
  c.line(x, y - half, x, y + half, thick, col);
}

// A drop: a circle with a triangle above it, 2 r high.
static void drop(Canvas& c, float x, float y, float r, uint16_t col) {
  c.fillCircle(x, y, r, col);
  c.diamond(x, y - r * 0.95f, r * 0.62f, r * 1.05f, col);
}

// Overlays live only on the full face. Positions are the spec's; they move with the body.
static void drawOverlay(Canvas& c, const AnimationEngine& a, float ox, float oy) {
  const EmotionDef& d = emotionDef(a.emotion());
  if (a.emotionAge() < 120) return;  // let the eyes change first
  float ts = a.timeMs() * 0.001f;
  auto X = [&](float x) { return x + ox; };
  auto Y = [&](float y) { return y + oy; };
  switch (d.overlay) {
    case Overlay::None: break;
    case Overlay::Sparkle: {
      const float px[3] = {112, 233, 356}, py[3] = {96, 72, 96}, half[3] = {9, 12, 8};
      for (int i = 0; i < 3; i++) plus(c, X(px[i]), Y(py[i]), half[i] * (0.8f + 0.2f * sinf(ts * 3.1f + i * 2.1f)), 3.5f, INK);
      break;
    }
    case Overlay::Hearts:
      ui::heart(c, X(116), Y(108 - 4 * sinf(ts * 2.f)), 22, ACCENT);
      ui::heart(c, X(352), Y(92 - 4 * sinf(ts * 2.f + 1.7f)), 15, ACCENT);
      break;
    case Overlay::Question:
      ui::textCentered(c, X(310), Y(110), "?", TXT_MENU_SEL, ACCENT);
      break;
    case Overlay::Dots:
      for (int i = 0; i < 3; i++) {
        float ph = fmodf(ts * 1.2f - i * 0.22f, 1.f);
        c.fillCircle(X(212.f + 21 * i), Y(128 - 4 * sinf(ph * 6.283f)), 5, INK_2);
      }
      break;
    case Overlay::Zzz: {
      // The letters come one after another and drift up, then start again.
      float ph = fmodf(ts * 0.4f, 1.f), lift = 10 * ph;
      if (ph > 0.05f) ui::textCentered(c, X(316), Y(170 - lift), "z", TXT_UNB28, INK_2);
      if (ph > 0.3f) ui::textCentered(c, X(344), Y(134 - lift), "z", TXT_UNB34, ACCENT);
      if (ph > 0.55f) ui::textCentered(c, X(378), Y(90 - lift), "Z", TXT_MENU_SEL, ACCENT);
      break;
    }
    case Overlay::Tear: {
      float ph = fmodf(ts * 0.3f, 1.f);
      if (ph < 0.8f) drop(c, X(118), Y(326 + 30 * ph), 8, ACCENT);
      break;
    }
    case Overlay::Streams:
      for (int side = 0; side < 2; side++) {
        float x = side ? 332.f : 134.f;
        float run = fmodf(ts * 1.1f, 1.f) * 32;  // the dots run down
        for (int k = 0; k < 4; k++) c.fillCircle(X(x), Y(262 + 32 * k + run), 7.f - k - run / 32, ACCENT);
      }
      break;
    case Overlay::Blush:
      for (int side = 0; side < 2; side++) {
        float cx = side ? 318.f : 148.f;
        for (int i = 0; i < 3; i++) c.line(X(cx - 30 + 18 * i), Y(320), X(cx - 22 + 18 * i), Y(302), 5, ACCENT_2);
      }
      break;
    case Overlay::Frost: {
      const float px[5] = {96, 160, 233, 306, 370}, py[5] = {108, 84, 72, 84, 108};
      for (int i = 0; i < 5; i++) plus(c, X(px[i]), Y(py[i]), 7 * (0.75f + 0.25f * sinf(ts * 2.3f + i)), 3, ACCENT);
      break;
    }
    case Overlay::Sweat: {
      float ph = fmodf(ts * 0.35f, 1.f);
      drop(c, X(394), Y(158 + 26 * ph), 9, ACCENT);
      break;
    }
    case Overlay::Drool: {
      float stretch = 8 * (0.5f + 0.5f * sinf(ts * 1.6f));
      c.line(X(150), Y(276), X(150), Y(330 + stretch), 3, INK_2);
      c.fillCircle(X(150), Y(334 + stretch), 6, INK_2);
      break;
    }
    case Overlay::Joy: {
      float bounce = 4 * sinf(ts * 6.f);
      drop(c, X(70), Y(262 + bounce), 8, ACCENT);
      drop(c, X(396), Y(262 - bounce), 8, ACCENT);
      break;
    }
  }
}

void FaceEngine::draw(Canvas& c, const AnimationEngine& a, uint16_t color) const {
  EyePose pose[2] = {a.leftPose(), a.rightPose()};
  float ox[2] = {a.eyeOffsetX(0), a.eyeOffsetX(1)}, oy[2] = {a.eyeOffsetY(0), a.eyeOffsetY(1)};
  drawEyes(c, pose, ox, oy, a.pupil(), 56, 136, 1, color);
  drawOverlay(c, a, a.bodyX(), a.bodyY());
}

void FaceEngine::drawMini(Canvas& c, const AnimationEngine& a, float left, float top, float width, uint16_t color) const {
  EyePose pose[2] = {a.leftPose(), a.rightPose()};
  float ox[2] = {a.eyeOffsetX(0), a.eyeOffsetX(1)}, oy[2] = {a.eyeOffsetY(0), a.eyeOffsetY(1)};
  drawEyes(c, pose, ox, oy, a.pupil(), left, top, width / 354.f, color);
}

void FaceEngine::drawMini(Canvas& c, Emotion e, float left, float top, float width, uint16_t color) const {
  const EmotionDef& d = emotionDef(e);
  EyePose pose[2] = {d.left, d.right};
  float zero[2] = {0, 0};
  drawEyes(c, pose, zero, zero, d.pupil, left, top, width / 354.f, color);
}

void FaceEngine::drawWeather(Canvas& c, Weather w, float tMs, bool night) const {
  const float t = tMs;
  auto hash = [](int i, float k) { return fmodf(sinf(i * 12.9898f + k) * 43758.5f, 1.f); };
  auto frac01 = [](float v) { v = fmodf(v, 1.f); return v < 0 ? v + 1 : v; };
  if (w == Weather::Clear && night) {
    for (int i = 0; i < 26; i++) {
      float x = frac01(hash(i, 1.f)) * kScreenW, y = frac01(hash(i, 2.f)) * kScreenH;
      float tw = 0.5f + 0.5f * sinf(t * 0.0016f * (0.5f + frac01(hash(i, 3.f))) + i);
      c.fillRect((int)x, (int)y, tw > 0.8f ? 3 : 2, tw > 0.8f ? 3 : 2, tw > 0.5f ? kGray : kDim);
    }
  }
  if (w == Weather::Clouds || w == Weather::Storm || w == Weather::Rain) {
    for (int i = 0; i < 4; i++) {
      float x = frac01(i * 0.27f + t * 0.000012f * (1 + i)) * 620 - 80, y = 54.f + i * 15;
      c.fillCircle(x, y, 34.f + i * 5, kDark);
      c.fillCircle(x + 42, y + 6, 26.f + i * 4, kDark);
      c.fillCircle(x - 38, y + 8, 22, kDark);
    }
  }
  if (w == Weather::Rain || w == Weather::Storm) {
    int n = w == Weather::Storm ? 34 : 22;
    float slant = w == Weather::Storm ? 8.f : 3.f;
    for (int i = 0; i < n; i++) {
      float sp = 0.42f + 0.25f * frac01(hash(i, 5.f));
      float y = frac01(t * sp / 520.f + frac01(hash(i, 6.f))) * 520 - 30;
      float x = frac01(hash(i, 7.f)) * kScreenW - slant * y / 40;
      c.line(x, y, x + slant, y + 15, 1.6f, kDim);
    }
  }
  if (w == Weather::Snow) {
    for (int i = 0; i < 26; i++) {
      float sp = 0.03f + 0.05f * frac01(hash(i, 8.f));
      float y = frac01(t * sp / 500.f + frac01(hash(i, 9.f))) * 500 - 16;
      float x = frac01(hash(i, 10.f)) * kScreenW + 16 * sinf(t * 0.001f + i);
      c.fillCircle(x, y, 1.6f + 1.8f * frac01(hash(i, 11.f)), kGray);
    }
  }
  if (w == Weather::Fog) {
    for (int i = 0; i < 5; i++) {
      float y = 90.f + i * 72 + 8 * sinf(t * 0.0006f + i);
      c.line(30 + 40 * sinf(t * 0.0004f + i), y, 436 - 40 * sinf(t * 0.0005f + i * 2), y, 9, kDark);
    }
  }
  if (w == Weather::Wind) {
    for (int i = 0; i < 8; i++) {
      float x = frac01(t * 0.0004f * (1 + 0.3f * i) + i * 0.23f) * 640 - 90;
      float y = 60.f + i * 48 + 10 * sinf(t * 0.002f + i);
      c.line(x, y, x + 60, y - 3, 2, kDim);
    }
  }
}

Rect FaceEngine::bounds(const AnimationEngine&) const {
  // Eyes at their widest, overlays, the clock line on top and room for jumps. The CO5300 needs even sizes.
  return Rect{40, 36, 386, 344};
}

}  // namespace cc
