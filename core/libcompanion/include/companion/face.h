// AnimationEngine: physically-damped eye parameters, blinks, gaze, idle life.
// FaceEngine: draws the animated parameters; all geometry is computed, nothing is pre-rendered.
#pragma once
#include "canvas.h"

namespace cc {

// All geometry is in px at full size, relative to the eye rest position, mirrored for the right
// eye so that "+x" always points to the nose. Layout follows the reference picture: a wide ellipse
// with a straight, slanted upper lid and a round pupil pressed against that lid.
struct EyePose {
  float dx, dy;          // eye position offset
  float rx, ry;          // width / height radius
  float up, upSlope;     // upper lid: y offset at eye centre; +slope lowers the nose side (mean look)
  float lo, loSlope;     // lower lid
  float cheek;           // top of the cheek cut, relative to the centre (>ry = no cut). "^" eyes.
  float pupilR, px, py;  // pupil radius and offset
};
constexpr int kPoseFields = sizeof(EyePose) / sizeof(float);

enum class Accent : uint8_t { None, Zzz, Tear, Hearts, Bang, Question, Sparkle, Steam, Dots, Blush, Sweat, Frost, Stream, Drool };
enum class PupilShape : uint8_t { Round, Heart, Spiral, Star };

// Everything that makes one emotion look and sound like itself.
struct EmotionDef {
  EyePose left, right;
  float transitionMs;    // spring settle time when entering
  float damping;         // <1 overshoots: surprise pops, sadness sinks
  float blinksPerMin;
  float saccadeAmp;      // random gaze travel, px
  float saccadeEveryMs;
  float bobAmp, bobHz;   // whole-face motion
  float tremble;         // fast jitter, px
  float gazeX, gazeY;    // preferred gaze direction
  Accent accent;
  PupilShape pupil;
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

  const EyePose& leftPose() const { return out_[0]; }
  const EyePose& rightPose() const { return out_[1]; }
  float blinkAmount() const { return blinkAmt_; }
  float gazeX() const { return gazeX_; }
  float gazeY() const { return gazeY_; }
  float bodyX() const { return bodyX_; }
  float bodyY() const { return bodyY_; }
  float timeMs() const { return t_; }
  float emotionAge() const { return age_; }
  float eyeGazeOffset(int eye, bool y) const { return indepHold_ > 0 ? (y ? indepY_[eye] : indepX_[eye]) : 0; }
  bool isSettled() const { return settled_; }

 private:
  void updateBlink(float dt);
  void updateGaze(float dt, const EmotionDef& d);
  void composeLayers();
  Emotion emotion_ = Emotion::Neutral, blendE_ = Emotion::Neutral;
  EyePose cur_[2], vel_[2], out_[2];
  float blendTarget_ = 0, blend_ = 0, arousal_ = 0.6f, valence_ = 0, light_ = 0.7f, tiltX_ = 0, tiltY_ = 0, roll_ = 0;
  float pursuit_ = 0, microX_ = 0, microY_ = 0, nextMicro_ = 400, squash_ = 0, lastGX_ = 0, lastGY_ = 0;
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
  bool soundPending_ = false, settled_ = false;
  Rng rng_;
};

class FaceEngine {
 public:
  // Full-screen face. `tint` lets low-power mode dim the eyes without touching panel brightness.
  void draw(Canvas& c, const AnimationEngine& a, uint16_t color = kWhite) const;
  // Same character at any scale: the pet screen and games use a small live face.
  void drawScaled(Canvas& c, const AnimationEngine& a, float cx, float cy, float scale, uint16_t color = kWhite) const;
  // Pixel bounds the face can touch this frame. Firmware pushes only this region to save power.
  Rect bounds(const AnimationEngine& a) const;
  // Weather behind the eyes: rain, snow, drifting clouds, night stars. Pure functions of time.
  void drawWeather(Canvas& c, Weather w, float tMs, bool night) const;
 private:
  void drawEye(Canvas& c, const AnimationEngine& a, int eye, float cx, float cy, float scale, uint16_t color) const;
  void drawAccent(Canvas& c, const AnimationEngine& a, float cx, float cy, float scale) const;
};

}  // namespace cc
