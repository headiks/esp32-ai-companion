#include "companion/input.h"

namespace cc {

// ---- touch -----------------------------------------------------------------------------------
static constexpr float kLongMs = 650, kDoubleMs = 280, kRepeatGuardMs = 60;

void TouchManager::emit(Gesture g, int x, int y, int dx, int dy) {
  int n = (qh_ + 1) % kQ;
  if (n == qt_) return;
  q_[qh_] = GestureEvent{g, x, y, dx, dy};
  qh_ = n;
}

bool TouchManager::poll(GestureEvent* e) {
  if (qt_ == qh_) return false;
  *e = q_[qt_];
  qt_ = (qt_ + 1) % kQ;
  return true;
}

void TouchManager::feed(bool down, int x, int y, float dt) {
  if (cooldown_ > 0) cooldown_ -= dt;
  // Capacitive panels drop out for a frame during fast moves: bridge such gaps instead of reporting a release.
  if (!down && down_ && upGlitch_ < 40) { upGlitch_ += dt; return; }
  upGlitch_ = 0;

  int moveThreshold = 34 - sens_ / 5;           // 14..32 px
  if (down) {
    if (!down_) {
      down_ = true; moved_ = false; longFired_ = false;
      sx_ = x_ = x; sy_ = y_ = y; heldMs_ = 0;
      emit(Gesture::Down, x, y);
    } else {
      int dx = x - x_, dy = y - y_;
      heldMs_ += dt;
      if (dx || dy) {
        x_ = x; y_ = y;
        int tx = x - sx_, ty = y - sy_;
        if (tx * tx + ty * ty > moveThreshold * moveThreshold) moved_ = true;
        emit(Gesture::Move, x, y, dx, dy);
      }
      if (!moved_ && !longFired_ && heldMs_ >= kLongMs) {
        longFired_ = true;
        pendingTap_ = false;
        emit(Gesture::LongPress, x, y);
      }
    }
  } else if (down_) {
    down_ = false;
    emit(Gesture::Up, x_, y_);
    int tx = x_ - sx_, ty = y_ - sy_;
    int ax = tx < 0 ? -tx : tx, ay = ty < 0 ? -ty : ty;
    int swipeMin = 74 - sens_ / 3;              // 41..71 px
    if (longFired_) {
      // the long press has already been reported
    } else if (moved_ && (ax >= swipeMin || ay >= swipeMin)) {
      if (ax > ay) emit(tx > 0 ? Gesture::SwipeRight : Gesture::SwipeLeft, sx_, sy_, tx, ty);
      else emit(ty > 0 ? Gesture::SwipeDown : Gesture::SwipeUp, sx_, sy_, tx, ty);
      pendingTap_ = false;
      cooldown_ = kRepeatGuardMs * 2;
    } else if (!moved_ && cooldown_ <= 0) {
      if (!dblEnabled_) {
        emit(Gesture::Tap, x_, y_);
        cooldown_ = kRepeatGuardMs;
      } else if (pendingTap_) {
        pendingTap_ = false;
        emit(Gesture::DoubleTap, x_, y_);
        cooldown_ = kRepeatGuardMs;
      } else {
        pendingTap_ = true; sinceTap_ = 0; tapX_ = x_; tapY_ = y_;
      }
    }
  }
  if (pendingTap_ && !down_) {
    sinceTap_ += dt;
    if (sinceTap_ > kDoubleMs) {
      pendingTap_ = false;
      emit(Gesture::Tap, tapX_, tapY_);
      cooldown_ = kRepeatGuardMs;
    }
  }
}

// ---- motion ----------------------------------------------------------------------------------
void GestureManager::emit(MotionEvent e) {
  int n = (qh_ + 1) % kQ;
  if (n == qt_) return;
  q_[qh_] = e;
  qh_ = n;
}

bool GestureManager::poll(MotionEvent* e) {
  if (qt_ == qh_) return false;
  *e = q_[qt_];
  qt_ = (qt_ + 1) % kQ;
  return true;
}

void GestureManager::feed(float ax, float ay, float az, float gx, float gy, float gz, float dt) {
  mag_ = sqrtf(ax * ax + ay * ay + az * az);
  float gyro = sqrtf(gx * gx + gy * gy + gz * gz);
  float dev = fabsf(mag_ - 1.f);

  // Orientation: low-pass z so a shake does not count as a flip.
  gravZ_ += (az - gravZ_) * clampf(dt / 250.f, 0, 1);
  if (gravZ_ < -0.72f) { downMs_ += dt; upMs_ = 0; }
  else if (gravZ_ > -0.2f) { upMs_ += dt; downMs_ = 0; }
  if (!faceDown_ && downMs_ > 1500) { faceDown_ = true; emit(MotionEvent::FaceDown); }
  if (faceDown_ && upMs_ > 150) {
    faceDown_ = false;
    emit(MotionEvent::FaceUp);
    emit(MotionEvent::Lifted);
  }

  // Stillness
  if (dev < 0.05f && gyro < 6.f) {
    stillMs_ += dt;
    if (stillMs_ > 30000 && !stillReported_) { stillReported_ = true; emit(MotionEvent::Still); }
  } else {
    // Picking the device up after it rested for a while counts as a lift.
    if (stillReported_ && !faceDown_ && (dev > 0.12f || gyro > 25.f)) emit(MotionEvent::Lifted);
    if (dev > 0.05f || gyro > 12.f) { stillMs_ = 0; stillReported_ = false; }
  }

  if (shakeCooldown_ > 0) shakeCooldown_ -= dt;
  if (jerkCooldown_ > 0) jerkCooldown_ -= dt;
  // Shake: several high peaks within 900 ms. Jerk: one isolated strong peak.
  bool peak = mag_ > 1.8f && lastHp_ <= 1.8f;
  lastHp_ = mag_;
  if (shakeWindow_ > 0) {
    shakeWindow_ -= dt;
    if (shakeWindow_ <= 0) {
      if (shakePeaks_ == 1 && jerkCooldown_ <= 0) { emit(MotionEvent::Jerk); jerkCooldown_ = 1200; }
      shakePeaks_ = 0;
    }
  }
  if (peak) {
    shakePeaks_++;
    if (shakeWindow_ <= 0) shakeWindow_ = 450;
    else shakeWindow_ += 180;
    if (shakePeaks_ >= 3 && shakeCooldown_ <= 0) {
      emit(MotionEvent::Shake);
      shakeCooldown_ = 2000;
      shakePeaks_ = 0;
      shakeWindow_ = 0;
    }
  }
}

// ---- microphone ------------------------------------------------------------------------------
void MicrophoneManager::emit(SoundEvent e) {
  int n = (qh_ + 1) % kQ;
  if (n == qt_) return;
  q_[qh_] = e;
  qh_ = n;
}

bool MicrophoneManager::poll(SoundEvent* e) {
  if (qt_ == qh_) return false;
  *e = q_[qt_];
  qt_ = (qt_ + 1) % kQ;
  return true;
}

void MicrophoneManager::feed(const int16_t* pcm, int n) {
  if (!enabled_) { levelDb_ = -90; return; }
  for (int i = 0; i < n; i++) {
    int16_t s = pcm[i];
    acc_ += (float)s * (float)s;
    float a = fabsf((float)s);
    if (a > peak_) peak_ = a;
    if ((s >= 0) != (last_ >= 0)) zc_++;
    last_ = s;
    if (++fill_ == kFrame) {
      frame(sqrtf(acc_ / kFrame), peak_, (float)zc_ / kFrame);
      fill_ = 0; acc_ = 0; peak_ = 0; zc_ = 0;
    }
  }
}

void MicrophoneManager::frame(float rms, float peak, float zcr) {
  float db = 20 * log10f(rms / 32768.f + 1e-6f);
  levelDb_ += (db - levelDb_) * 0.25f;
  // Noise floor follows quickly downward and slowly upward, so speech does not raise it.
  if (db < floorDb_) floorDb_ += (db - floorDb_) * 0.2f;
  else floorDb_ += (db - floorDb_) * 0.002f;
  floorDb_ = clampf(floorDb_, -80, -25);

  float sensOffset = (sens_ - 60) * 0.25f;   // more sensitive = lower thresholds
  float loudTh = -16 - sensOffset;
  sinceLoud_++; sinceClap_++;

  // Clap: a sharp peak 26+ dB above the floor that dies within ~100 ms.
  bool impulse = peak / 32768.f > 0.3f - sensOffset * 0.006f && db > floorDb_ + 22;
  if (db > loudTh) loudFrames_++;
  else {
    if (loudFrames_ >= 1 && loudFrames_ <= 5 && sinceClap_ > 15) { emit(SoundEvent::Clap); sinceClap_ = 0; }
    else if (loudFrames_ > 5 && sinceLoud_ > 50) { emit(SoundEvent::Loud); sinceLoud_ = 0; }
    loudFrames_ = 0;
  }
  if (loudFrames_ > 15 && sinceLoud_ > 50) { emit(SoundEvent::Loud); sinceLoud_ = 0; }
  (void)impulse;

  // Voice activity: sustained energy above the floor with a speech-like zero crossing rate.
  bool speechy = db > floorDb_ + 10 - sensOffset * 0.3f && db < loudTh && zcr > 0.015f && zcr < 0.35f;
  if (speechy) {
    voiceFrames_++;
    silenceFrames_ = 0;
    if (!voice_ && voiceFrames_ >= 12) { voice_ = true; emit(SoundEvent::VoiceStart); }
  } else {
    silenceFrames_++;
    if (silenceFrames_ > 4) voiceFrames_ = 0;
    if (voice_ && silenceFrames_ >= 35) { voice_ = false; emit(SoundEvent::VoiceEnd); }
  }
}

}  // namespace cc
