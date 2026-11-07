// AnimationEngine: eye parameters of the 1a design, eased between emotions, plus the life on top of them:
// blinks, gaze, breathing lids, head roll, little jumps.
// FaceEngine: draws them; all geometry is computed, nothing is pre-rendered.
#pragma once
#include "canvas.h"

namespace cc {

// One eye in the parametric model of design 1a (documentation/design/1a, section 5 of the spec).
// The white is a rounded rectangle 132 x 116 * s; the upper lid cuts it with a line, the cheek with a parabola.
struct EyePose {
  float t;   // upper lid: 0 open .. 1 closed, share of the eye height
  float a;   // lid angle in degrees: > 0 lowers the inner corners (anger), < 0 raises them (sadness)
  float b;   // cheek: 0 none .. 1, rises from below and turns the eye into a smile
  float px;  // pupil offset, screen direction: -1 .. 1 of 27 % of the eye width
  float py;  //               and of 27 % of the eye height
  float s;   // scale of the whole eye
  float pr;  // pupil radius as a share of the eye height; 0 hides it
};
constexpr int kPoseFields = sizeof(EyePose) / sizeof(float);

enum class Overlay : uint8_t { None, Sparkle, Hearts, Question, Dots, Zzz, Tear, Streams, Blush, Frost, Sweat, Drool, Joy };
enum class PupilShape : uint8_t { Round, Heart, Spiral, Star };

// Everything that makes one emotion look and behave like itself.
struct EmotionDef {
  EyePose left, right;
  PupilShape pupil;
  Overlay overlay;
  float transitionMs;    // ease-out from the previous pose, 150..250
  float blinksPerMin;
  float saccadeAmp;      // random gaze travel, px of the old scale (see AnimationEngine::composeLayers)
  float saccadeEveryMs;
  float bobAmp, bobHz;   // whole-face motion
  float tremble;         // fast jitter, px
  float gazeX, gazeY;    // preferred gaze direction
  uint16_t soundDelayMs; // sound fires at this moment of the transition, see SoundEmotionEngine
  uint8_t doubleBlinkPct;
  bool wanderLook;       // slowly looks left and right (thinking, confused)
};
const EmotionDef& emotionDef(Emotion e);

class AnimationEngine {
 public:
  AnimationEngine();
  void setEmotion(Emotion e);
  Emotion emotion() const { return emotion_; }
  void update(float dtMs);

  void setSpeed(float s) { speed_ = clampf(s, 0.25f, 3.0f); }
  void setBlinkScale(float s) { blinkScale_ = clampf(s, 0.0f, 3.0f); }
  void reseed(uint32_t s) { rng_.seed(s); }
  // Continuous inner state, 0..1 except valence -1..1; tilt in g. It colours every animation layer.
  void setDrive(float arousal, float valence, float light, float tiltX, float tiltY);
  // A second emotion mixed into the pose, so faces melt into each other instead of being fixed presets.
  void setBlend(Emotion e, float weight) { blendE_ = e; blendTarget_ = clampf(weight, 0, 0.6f); }
  void startPursuit(float ms) { pursuit_ = ms; }   // follows something invisible, as pets watch a fly

  // Direct control used by touch, games and the test panel.
  void lookAt(float x, float y, float holdMs);   // screen coordinates
  void lookOffset(float dx, float dy, float holdMs);
  void clearLook() { lookHold_ = 0; }
  void blink(float durationMs = 170, bool twice = false);
  void slowBlink() { blink(620); }
  void closeEyes(bool closed) { forceClosed_ = closed; }  // slow close / gradual open
  void kick(float vy);                                    // little jump, reaction to touch
  void independentLook(float lx, float ly, float rx, float ry, float holdMs);

  void retriggerCue() { soundPending_ = true; age_ = 0; }  // the same emotion asked for again must still answer
  bool soundCueReady();   // true once per transition, at the frame chosen by soundDelayMs

  // The pose as drawn: emotion, blend and every life layer composed.
  const EyePose& leftPose() const { return out_[0]; }
  const EyePose& rightPose() const { return out_[1]; }
  float eyeOffsetX(int eye) const { return offX_[eye]; }  // px at full size: body sway, head roll, jumps
  float eyeOffsetY(int eye) const { return offY_[eye]; }
  PupilShape pupil() const;  // switches halfway through a transition
  float blinkAmount() const { return blinkAmt_; }
  float gazeX() const { return gazeX_; }
  float gazeY() const { return gazeY_; }
  float bodyX() const { return bodyX_; }
  float bodyY() const { return bodyY_; }
  float timeMs() const { return t_; }
  float emotionAge() const { return age_; }
  bool isSettled() const { return progress_ >= 1; }

 private:
  void updateBlink(float dt);
  void updateGaze(float dt, const EmotionDef& d);
  void composeLayers();
  Emotion emotion_ = Emotion::Neutral, previous_ = Emotion::Neutral, blendE_ = Emotion::Neutral;
  EyePose from_[2], cur_[2], out_[2];
  float offX_[2] = {0, 0}, offY_[2] = {0, 0};
  float progress_ = 1;  // 0..1 of the current transition
  float blendTarget_ = 0, blend_ = 0, arousal_ = 0.6f, valence_ = 0, light_ = 0.7f, tiltX_ = 0, tiltY_ = 0, roll_ = 0;
  float pursuit_ = 0, microX_ = 0, microY_ = 0, nextMicro_ = 400;
  float t_ = 0, age_ = 0, speed_ = 1, blinkScale_ = 1;
  // blink
  float blinkAmt_ = 0, blinkT_ = -1, blinkDur_ = 170, blinkPause_ = 0, nextBlink_ = 2500, closedAmt_ = 0;
  bool blinkTwice_ = false, forceClosed_ = false;
  // gaze
  float gazeX_ = 0, gazeY_ = 0, gazeTX_ = 0, gazeTY_ = 0, nextSaccade_ = 1200, lookHold_ = 0;
  float lookX_ = 0, lookY_ = 0;
  float indepX_[2] = {0, 0}, indepY_[2] = {0, 0}, indepHold_ = 0;
  // body
  float bodyX_ = 0, bodyY_ = 0, kickY_ = 0, kickV_ = 0;
  bool soundPending_ = false;
  Rng rng_;
};

class FaceEngine {
 public:
  // Full-screen face with its overlays (tears, sparkles, zzz). `color` lets low-power mode dim the eyes.
  void draw(Canvas& c, const AnimationEngine& a, uint16_t color) const;
  // The same character fitted to `width`, top-left at (left, top), without overlays: MiniFace of the spec.
  void drawMini(Canvas& c, const AnimationEngine& a, float left, float top, float width, uint16_t color) const;
  // A fixed face for screens that show one emotion of their own (about, time not set, pairing).
  void drawMini(Canvas& c, Emotion e, float left, float top, float width, uint16_t color) const;
  // Pixel bounds the face can touch this frame. Firmware pushes only this region to save power.
  Rect bounds(const AnimationEngine& a) const;
  // Weather behind the eyes: rain, snow, drifting clouds, night stars. Pure functions of time.
  void drawWeather(Canvas& c, Weather w, float tMs, bool night) const;
};

}  // namespace cc
