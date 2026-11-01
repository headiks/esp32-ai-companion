#include "companion/face.h"
#include <string.h>

namespace cc {

static constexpr float kEyeX = 122;   // distance of each eye from the screen centre
static constexpr float kEyeY = -42;   // ellipse centre sits above screen centre: lids cut its top

// Shorthand: dx dy  rx ry  up slope  lo slope  cheek  pupilR px py
#define POSE(...) EyePose{__VA_ARGS__}
#define NOCHEEK 200.f

static const EmotionDef kDefs[kEmotionCount] = {
    // Neutral: the reference look. Slanted flat lid, pupil pressed against it, slightly sly.
    {POSE(0, 0, 90, 72, 2, .19f, 72, 0, NOCHEEK, 19, -8, 23), POSE(0, 0, 90, 72, 2, .19f, 72, 0, NOCHEEK, 19, -8, 23),
     260, .86f, 13, 22, 1700, 2.2f, .28f, 0, 0, 0, Accent::None, PupilShape::Round, 40, 15, false},
    // Happy: lids lift, cheeks push up into a smile.
    {POSE(0, -4, 90, 74, -74, 0, 74, 0, 28, 21, 0, -12), POSE(0, -4, 90, 74, -74, 0, 74, 0, 28, 21, 0, -12),
     220, .62f, 16, 16, 1400, 4.f, .9f, 0, 0, 0, Accent::None, PupilShape::Round, 60, 25, false},
    // VeryHappy: full "^ ^" arcs, bouncing.
    {POSE(0, -10, 90, 74, -74, 0, 74, 0, -14, 0, 0, 0), POSE(0, -10, 90, 74, -74, 0, 74, 0, -14, 0, 0, 0),
     200, .55f, 0, 0, 3000, 9.f, 2.1f, 0, 0, 0, Accent::Sparkle, PupilShape::Round, 50, 0, false},
    // Sad: nose side of the lid rises, pupil large and low, whole face sinks.
    {POSE(4, 10, 84, 66, -2, -.34f, 66, 0, NOCHEEK, 23, 0, 24), POSE(4, 10, 84, 66, -2, -.34f, 66, 0, NOCHEEK, 23, 0, 24),
     760, 1.f, 7, 10, 3200, 2.f, .18f, 0, 0, 10, Accent::Tear, PupilShape::Round, 220, 10, false},
    // Angry: strong slant, small pupils, trembling with tension.
    {POSE(8, 4, 92, 64, 10, .46f, 64, 0, NOCHEEK, 15, 6, 30), POSE(8, 4, 92, 64, 10, .46f, 64, 0, NOCHEEK, 15, 6, 30),
     150, .68f, 6, 6, 2200, 0, 0, 1.2f, 0, 0, Accent::Steam, PupilShape::Round, 20, 0, false},
    // Annoyed: flat half lids and a side glance.
    {POSE(0, 2, 90, 62, 12, .05f, 54, -.02f, NOCHEEK, 16, 0, 26), POSE(0, 2, 90, 62, 12, .05f, 54, -.02f, NOCHEEK, 16, 0, 26),
     300, .9f, 9, 6, 2600, 1.f, .2f, 0, 34, 0, Accent::None, PupilShape::Round, 90, 0, false},
    // Surprised: eyes pop wide, tiny pupils.
    {POSE(-4, -10, 88, 86, -86, 0, 86, 0, NOCHEEK, 15, 0, 0), POSE(-4, -10, 88, 86, -86, 0, 86, 0, NOCHEEK, 15, 0, 0),
     120, .46f, 3, 4, 2500, 0, 0, 0, 0, 0, Accent::Bang, PupilShape::Round, 15, 0, false},
    // Curious: one eye opens more, tilted head.
    {POSE(0, -8, 92, 84, -44, 0, 84, 0, NOCHEEK, 22, 4, -6), POSE(0, 8, 88, 68, -4, .14f, 68, 0, NOCHEEK, 20, 0, 14),
     280, .7f, 12, 30, 1100, 1.5f, .5f, 0, 10, -10, Accent::Question, PupilShape::Round, 80, 10, false},
    // Thinking: pupils up and away, one lid lower.
    {POSE(0, -2, 88, 72, -24, -.08f, 72, 0, NOCHEEK, 16, 0, -4), POSE(0, 2, 88, 72, 6, .1f, 72, 0, NOCHEEK, 16, 0, 2),
     340, .9f, 7, 14, 1900, 1.f, .25f, 0, -34, -22, Accent::Dots, PupilShape::Round, 130, 0, true},
    // Sleepy: heavy lids, slow breathing.
    {POSE(0, 8, 90, 68, 30, .02f, 68, 0, NOCHEEK, 20, 0, 34), POSE(0, 8, 90, 68, 30, .02f, 68, 0, NOCHEEK, 20, 0, 34),
     900, 1.f, 16, 8, 3800, 3.f, .16f, 0, 0, 8, Accent::None, PupilShape::Round, 260, 0, false},
    // Sleeping: only the lower arc of the eye stays visible.
    {POSE(0, 14, 90, 66, 54, 0, 66, 0, NOCHEEK, 0, 0, 40), POSE(0, 14, 90, 66, 54, 0, 66, 0, NOCHEEK, 0, 0, 40),
     1400, 1.f, 0, 0, 9000, 4.f, .13f, 0, 0, 0, Accent::Zzz, PupilShape::Round, 500, 0, false},
    // WakingUp: lids drag open, double blinks handled by doubleBlinkPct.
    {POSE(0, 6, 90, 70, 18, .08f, 70, 0, NOCHEEK, 18, 0, 26), POSE(0, 6, 90, 70, 18, .08f, 70, 0, NOCHEEK, 18, 0, 26),
     1100, .95f, 40, 18, 900, 1.5f, .25f, 0, 0, 0, Accent::None, PupilShape::Round, 180, 90, false},
    // Scared: wide, pupils shrink to dots, shaking, eyes pull together.
    {POSE(14, -6, 78, 84, -70, -.2f, 84, 0, NOCHEEK, 10, 0, 0), POSE(14, -6, 78, 84, -70, -.2f, 84, 0, NOCHEEK, 10, 0, 0),
     110, .55f, 12, 18, 420, 0, 0, 2.6f, 0, 0, Accent::None, PupilShape::Round, 10, 30, false},
    // Affection: soft raised cheeks, heart pupils.
    {POSE(0, -2, 90, 76, -76, 0, 76, 0, 36, 28, 2, -8), POSE(0, -2, 90, 76, -76, 0, 76, 0, 36, 28, 2, -8),
     420, .8f, 11, 6, 2800, 3.f, .55f, 0, 0, 0, Accent::Hearts, PupilShape::Heart, 120, 30, false},
    // Bored: half closed, looking away and down.
    {POSE(0, 8, 90, 64, 20, -.05f, 64, 0, NOCHEEK, 18, 0, 26), POSE(0, 8, 90, 64, 20, -.05f, 64, 0, NOCHEEK, 18, 0, 26),
     700, 1.f, 8, 18, 3400, 1.5f, .15f, 0, -26, 12, Accent::None, PupilShape::Round, 200, 0, true},
    // Attentive: open, centred, pupils big and still.
    {POSE(0, -4, 90, 78, -32, .05f, 78, 0, NOCHEEK, 23, 0, 0), POSE(0, -4, 90, 78, -32, .05f, 78, 0, NOCHEEK, 23, 0, 0),
     180, .75f, 9, 4, 2600, .8f, .4f, 0, 0, 0, Accent::None, PupilShape::Round, 40, 0, false},
    // Confused: mismatched eyes and pupil sizes.
    {POSE(0, -6, 86, 80, -48, -.05f, 80, 0, NOCHEEK, 14, 0, -4), POSE(0, 6, 90, 62, 14, .24f, 62, 0, NOCHEEK, 22, 0, 22),
     320, .7f, 15, 24, 800, 1.f, .35f, 0, 0, 0, Accent::Question, PupilShape::Round, 100, 40, true},
    // Excited: wide and smiling, big pupils, quick bounce.
    {POSE(0, -6, 92, 84, -84, 0, 84, 0, 44, 27, 0, -14), POSE(0, -6, 92, 84, -84, 0, 84, 0, 44, 27, 0, -14),
     160, .5f, 18, 14, 700, 7.f, 2.8f, 0, 0, 0, Accent::Sparkle, PupilShape::Round, 30, 20, false},
};

const EmotionDef& emotionDef(Emotion e) { return kDefs[(int)e]; }

AnimationEngine::AnimationEngine() {
  cur_[0] = kDefs[0].left;
  cur_[1] = kDefs[0].right;
  memset(vel_, 0, sizeof(vel_));
}

void AnimationEngine::setEmotion(Emotion e) {
  if (e == emotion_) return;
  emotion_ = e;
  age_ = 0;
  soundPending_ = true;
  settled_ = false;
  nextSaccade_ = 300;
}

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
  float rate = d.blinksPerMin * blinkScale_;
  if (blinkT_ < 0 && blinkPause_ <= 0 && rate > 0.01f) {
    nextBlink_ -= dt;
    if (nextBlink_ <= 0) {
      bool twice = rng_.range(0, 99) < d.doubleBlinkPct;
      float dur = emotion_ == Emotion::Sleepy ? 520.f : 170.f;
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
      nextSaccade_ = d.saccadeEveryMs * rng_.rangef(0.5f, 1.5f);
    }
  }
  if (indepHold_ > 0) indepHold_ -= dt;
  // Saccades are fast: about 40 ms time constant.
  float k = clampf(dt / 42.f, 0, 1);
  gazeX_ += (gazeTX_ - gazeX_) * k;
  gazeY_ += (gazeTY_ - gazeY_) * k;
}

void AnimationEngine::update(float dtMs) {
  float dt = clampf(dtMs, 0, 100) * speed_;
  t_ += dt;
  age_ += dt;
  const EmotionDef& d = emotionDef(emotion_);
  updateBlink(dt);
  updateGaze(dt, d);

  // Spring toward the pose. A settle time T maps to omega = 4.6 / T for a critically damped spring.
  float omega = 4.6f / d.transitionMs;
  float k = omega * omega, c = 2 * d.damping * omega;
  const float* tgt[2] = {(const float*)&d.left, (const float*)&d.right};
  int steps = (int)(dt / 8.0f) + 1;
  float h = dt / steps;
  float err = 0;
  for (int s = 0; s < steps; s++) {
    for (int e = 0; e < 2; e++) {
      float* x = (float*)&cur_[e];
      float* v = (float*)&vel_[e];
      for (int i = 0; i < kPoseFields; i++) {
        v[i] += (k * (tgt[e][i] - x[i]) - c * v[i]) * h;
        x[i] += v[i] * h;
        if (s == steps - 1) err += fabsf(tgt[e][i] - x[i]);
      }
    }
  }
  settled_ = err < 1.5f;

  // Idle life: breathing bob, slow sway; never fully still.
  float ts = t_ * 0.001f;
  const float tau = 6.2831853f;
  float bob = d.bobAmp * sinf(ts * tau * d.bobHz);
  float breath = 1.2f * sinf(ts * tau * 0.21f);
  float sway = 1.4f * sinf(ts * tau * 0.087f + 1.3f);
  float tx = 0, ty = 0;
  if (d.tremble > 0) {
    tx = d.tremble * sinf(ts * 91.f) * sinf(ts * 13.f);
    ty = d.tremble * 0.6f * sinf(ts * 77.f + 1.f);
  }
  kickV_ += (-0.00012f * kickY_ - 0.012f * kickV_) * dt;
  kickY_ += kickV_ * dt;
  bodyX_ = sway + tx;
  bodyY_ = bob + breath + ty + kickY_;
}

// ---- drawing ---------------------------------------------------------------------------------

void FaceEngine::drawEye(Canvas& c, const AnimationEngine& a, int eye, float ox, float oy, float sc, uint16_t color) const {
  const EyePose& p = eye == 0 ? a.leftPose() : a.rightPose();
  float mirror = eye == 0 ? 1.f : -1.f;  // +x in pose space is toward the nose
  // Nearest-neighbour blending of the emotion's pose is done by springs, so values may overshoot.
  float ry = p.ry < 8 ? 8 : p.ry, rx = p.rx < 8 ? 8 : p.rx;
  float cx = ox + (-mirror * kEyeX + mirror * p.dx + a.bodyX()) * sc;
  float cy = oy + (kEyeY + p.dy + a.bodyY()) * sc;

  float up = p.up, lo = p.lo;
  // Blink lowers the upper lid to a resting arc on the lower edge of the eye.
  float closed = ry - 11.f;
  float b = a.blinkAmount();
  up = lerpf(up, closed, b);
  float slope = p.upSlope * (1 - b);
  float gx = a.gazeX() + a.eyeGazeOffset(eye, false);
  float gy = a.gazeY() + a.eyeGazeOffset(eye, true);

  EyeShape s;
  s.cx = cx; s.cy = cy; s.rx = rx * sc; s.ry = ry * sc;
  s.upY = cy + up * sc; s.upSlope = slope * mirror;
  s.loY = cy + lo * sc; s.loSlope = p.loSlope * mirror;
  float cheek = p.cheek;
  if (cheek < 160) {
    s.cheekY = cy + cheek * sc;
    s.cheekRx = rx * 1.18f * sc;
    s.cheekRy = ry * 1.2f * sc;
  } else {
    s.cheekY = cy + (ry + 40) * sc; s.cheekRx = 1; s.cheekRy = 1;
  }
  c.eye(s, color);

  if (p.pupilR < 1 || b > 0.72f) return;
  float px = cx + (mirror * p.px + gx * 0.75f) * sc;
  float py = cy + (p.py + gy * 0.75f) * sc;
  // Keep the pupil on the eye even at extreme gaze.
  float nx = (px - cx) / (s.rx - 6 * sc), ny = (py - cy) / (s.ry - 6 * sc);
  float len = sqrtf(nx * nx + ny * ny);
  if (len > 1) { px = cx + (px - cx) / len; py = cy + (py - cy) / len; }
  float r = p.pupilR * sc * (1 - b * 0.25f);
  if (emotionDef(a.emotion()).pupil == PupilShape::Heart && a.emotionAge() > 40) {
    float beat = 1 + 0.08f * sinf(a.timeMs() * 0.0075f);
    c.heart(px, py - r * 0.2f, r * 0.9f * beat, kBlack);
  } else {
    c.fillCircle(px, py, r, kBlack);
  }
}

void FaceEngine::drawAccent(Canvas& c, const AnimationEngine& a, float ox, float oy, float sc) const {
  const EmotionDef& d = emotionDef(a.emotion());
  float t = a.emotionAge();
  float ts = a.timeMs() * 0.001f;
  if (d.accent == Accent::None || sc < 0.8f) return;
  switch (d.accent) {
    case Accent::Zzz: {
      for (int i = 0; i < 3; i++) {
        float ph = fmodf(ts * 0.22f + i * 0.33f, 1.f);
        if (t < 1200) break;
        int x = (int)(ox + 96 + ph * 70 + 8 * sinf(ph * 9));
        int y = (int)(oy - 70 - ph * 90);
        c.text(x, y, "z", i == 1 ? FontSize::L : FontSize::M, ph > 0.8f ? kDim : kOrange);
      }
      break;
    }
    case Accent::Tear: {
      float ph = fmodf(ts * 0.28f, 1.f);
      if (t < 900 || ph > 0.75f) break;
      float y = oy + 46 + ph * 120, x = ox - 150;
      c.fillCircle(x, y, 7, kOrange);
      c.line(x, y - 11, x, y - 3, 4, kOrange);
      break;
    }
    case Accent::Hearts: {
      for (int i = 0; i < 2; i++) {
        float ph = fmodf(ts * 0.3f + i * 0.5f, 1.f);
        if (ph > 0.85f) continue;
        float x = ox + (i ? 110.f : -118.f) + 10 * sinf(ph * 7);
        c.heart(x, oy - 150 - ph * 44, 11 + 5 * sinf(ph * 3.1f), kOrange);
      }
      break;
    }
    case Accent::Bang:
      if (t < 800) {
        float grow = smooth01(t / 90);
        c.roundRect(ox - 5, oy - 186 * grow, 10, 42 * grow, 5, kYellow);
        c.fillCircle(ox, oy - 132 * grow, 6 * grow, kYellow);
      }
      break;
    case Accent::Question:
      if (fmodf(ts * 0.35f, 1.f) < 0.55f)
        c.text((int)ox + 6, (int)oy - 170, "?", FontSize::L, kYellow, Align::Center);
      break;
    case Accent::Sparkle:
      for (int i = 0; i < 3; i++) {
        float ph = fmodf(ts * 0.9f + i * 0.37f, 1.f);
        float sz = 9 * sinf(ph * 3.14159f);
        float x = ox + (i == 0 ? -128.f : i == 1 ? 132.f : 4.f), y = oy + (i == 2 ? -176.f : -158.f);
        c.line(x - sz, y, x + sz, y, 3, kYellow);
        c.line(x, y - sz, x, y + sz, 3, kYellow);
      }
      break;
    case Accent::Steam:
      for (int i = 0; i < 2; i++) {
        float ph = fmodf(ts * 1.1f + i * 0.5f, 1.f);
        float x = ox + (i ? 40.f : -40.f), y = oy - 104 - ph * 44;
        float r = 3 + ph * 7;
        c.ring(x, y, r, 3, ph > 0.7f ? kDim : kOrange);
      }
      break;
    case Accent::Dots:
      for (int i = 0; i < 3; i++) {
        float ph = fmodf(ts * 0.8f - i * 0.18f, 1.f);
        float s = 3.5f + 2.5f * sinf(ph * 6.283f);
        c.fillCircle(ox - 16 + i * 16, oy - 130, s, kYellow);
      }
      break;
    default: break;
  }
}

void FaceEngine::draw(Canvas& c, const AnimationEngine& a, uint16_t color) const {
  drawScaled(c, a, kScreenW / 2.f, kScreenH / 2.f, 1.f, color);
}

void FaceEngine::drawScaled(Canvas& c, const AnimationEngine& a, float cx, float cy, float sc, uint16_t color) const {
  drawEye(c, a, 0, cx, cy, sc, color);
  drawEye(c, a, 1, cx, cy, sc, color);
  drawAccent(c, a, cx, cy, sc);
}

Rect FaceEngine::bounds(const AnimationEngine&) const {
  // Includes room for accents and overshoot. The CO5300 needs even start and width.
  return Rect{0, 20, kScreenW, kScreenH - 60};
}

}  // namespace cc
