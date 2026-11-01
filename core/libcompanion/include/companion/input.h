// Input analysis that does not depend on hardware: touch gestures, motion events, ambient sound events.
#pragma once
#include "types.h"

namespace cc {

enum class Gesture : uint8_t { None, Down, Move, Up, Tap, DoubleTap, LongPress, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

struct GestureEvent { Gesture g = Gesture::None; int x = 0, y = 0, dx = 0, dy = 0; };

class TouchManager {
 public:
  void setDoubleTapEnabled(bool on) { dblEnabled_ = on; }
  void setSensitivity(int percent) { sens_ = clampi(percent, 10, 100); }
  // Raw state, once per frame. Events are read with poll() until it returns false.
  void feed(bool down, int x, int y, float dtMs);
  bool poll(GestureEvent* e);
  bool isDown() const { return down_; }
  int x() const { return x_; }
  int y() const { return y_; }
 private:
  void emit(Gesture g, int x, int y, int dx = 0, int dy = 0);
  static constexpr int kQ = 8;
  GestureEvent q_[kQ];
  int qh_ = 0, qt_ = 0;
  bool down_ = false, longFired_ = false, moved_ = false, dblEnabled_ = true, pendingTap_ = false;
  int x_ = 0, y_ = 0, sx_ = 0, sy_ = 0, tapX_ = 0, tapY_ = 0, sens_ = 60;
  float heldMs_ = 0, sinceTap_ = 0, cooldown_ = 0, upGlitch_ = 0;
};

enum class MotionEvent : uint8_t { None, FaceDown, FaceUp, Lifted, Shake, Jerk, Still };

class GestureManager {
 public:
  // Accelerometer in g, gyro in deg/s. Face-up on a table reads about (0, 0, +1).
  void feed(float ax, float ay, float az, float gx, float gy, float gz, float dtMs);
  bool poll(MotionEvent* e);
  bool faceDown() const { return faceDown_; }
  bool still() const { return stillMs_ > 4000; }
  float lastMagnitude() const { return mag_; }
 private:
  void emit(MotionEvent e);
  static constexpr int kQ = 6;
  MotionEvent q_[kQ];
  int qh_ = 0, qt_ = 0;
  bool faceDown_ = false, stillReported_ = false;
  float downMs_ = 0, upMs_ = 0, stillMs_ = 0, mag_ = 1, shakeWindow_ = 0, jerkCooldown_ = 0, shakeCooldown_ = 0, lastHp_ = 0;
  int shakePeaks_ = 0;
  float gravZ_ = 1;
};

enum class SoundEvent : uint8_t { None, Loud, Clap, VoiceStart, VoiceEnd };

// All processing is local. PCM frames are analysed and discarded; nothing is stored.
class MicrophoneManager {
 public:
  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }
  void setSensitivity(int percent) { sens_ = clampi(percent, 10, 100); }
  void feed(const int16_t* pcm, int samples);  // 16 kHz mono
  bool poll(SoundEvent* e);
  float levelDb() const { return levelDb_; }   // dBFS, slow meter
  bool voiceActive() const { return voice_; }
 private:
  void emit(SoundEvent e);
  void frame(float rms, float peak, float zcr);
  static constexpr int kQ = 6, kFrame = 320;  // 20 ms
  SoundEvent q_[kQ];
  int qh_ = 0, qt_ = 0;
  bool enabled_ = true, voice_ = false;
  int sens_ = 60, fill_ = 0, zc_ = 0, voiceFrames_ = 0, silenceFrames_ = 0, sinceLoud_ = 1000, sinceClap_ = 1000, loudFrames_ = 0;
  float acc_ = 0, peak_ = 0, levelDb_ = -90, floorDb_ = -55;
  int16_t last_ = 0;
};

}  // namespace cc
