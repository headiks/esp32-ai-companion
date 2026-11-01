// Chance toys, motion games in the spirit of Switch party games, and the phone controller screen.
#include "ui.h"
#include <stdio.h>
#include <string.h>

namespace cc {

static constexpr int kCx = kScreenW / 2, kCy = kScreenH / 2;

// A solid ellipse, using the eye rasterizer with lids out of the way.
static void ellipse(Canvas& c, float x, float y, float rx, float ry, uint16_t col) {
  if (rx < 1) rx = 1;
  EyeShape s{x, y, rx, ry, y - ry - 2, 0, y + ry + 2, 0, y + ry + 50, 1, 1};
  c.eye(s, col);
}

// ---- Coin ---------------------------------------------------------------------------------------
class CoinScreen : public Screen {
 public:
  void enter(App&) override { t_ = 1; heads_ = tails_ = 0; shown_ = false; }
  void tick(App& a, float dt) override {
    if (t_ >= 1) return;
    t_ += dt / 1500;
    if (t_ >= 1) {
      shown_ = true;
      (result_ ? heads_ : tails_)++;
      a.sfx.play("games/bounce");
      a.react(result_ ? Emotion::Happy : Emotion::Skeptical, 1200);
    }
  }
  void draw(App& a, Canvas& c) override {
    float p = clampf(t_, 0, 1), ease = 1 - (1 - p) * (1 - p) * (1 - p);
    // The whole number of half-turns decides which side lands up.
    float ang = ease * turns_ * 3.14159f;
    float y = kCy - 16 - sinf(p * 3.14159f) * 90;
    float w = fabsf(cosf(ang));
    bool front = cosf(ang) > 0;
    ellipse(c, kCx, y, 92 * w, 92, front ? kWhite : kGray);
    if (w > 0.45f) c.text(kCx, (int)y - 20, front ? "ОРЁЛ" : "РЕШКА", FontSize::M, kBlack, Align::Center);
    char b[32];
    snprintf(b, sizeof(b), "%d : %d", heads_, tails_);
    c.text(kCx, kCy + 112, b, FontSize::M, kGray, Align::Center);
    if (!shown_ && p >= 1) c.text(kCx, kCy + 150, "Встряхни или нажми", FontSize::S, kDim, Align::Center);
    a.face.drawScaled(c, a.anim, kCx, 66, 0.2f, a.eyeColor());
  }
  void onGesture(App& a, const GestureEvent& e) override { if (e.g == Gesture::Tap) flip(a); }
  bool onMotion(App& a, MotionEvent m) override { if (m != MotionEvent::Shake) return false; flip(a); return true; }
 private:
  void flip(App& a) {
    if (t_ < 1) return;
    result_ = a.rng.range(0, 1) == 1;
    // Heads is an even number of half-turns, so the coin finishes on the chosen side.
    turns_ = (float)(a.rng.range(4, 6) * 2 + (result_ ? 0 : 1));
    t_ = 0;
    a.sfx.play("interactions/shake");
    a.react(Emotion::Attentive, 1500);
  }
  float t_ = 1, turns_ = 0;
  bool result_ = false, shown_ = false;
  int heads_ = 0, tails_ = 0;
};

// ---- Dice ---------------------------------------------------------------------------------------
class DiceScreen : public Screen {
 public:
  void enter(App&) override { rollMs_ = 0; }
  void tick(App& a, float dt) override {
    if (rollMs_ <= 0) return;
    rollMs_ -= dt;
    stepMs_ -= dt;
    if (stepMs_ <= 0) {
      // Faces change slower and slower as the dice lose speed.
      stepMs_ = lerpf(170, 45, clampf(rollMs_ / kRoll, 0, 1));
      v_[0] = a.rng.range(1, 6);
      v_[1] = a.rng.range(1, 6);
      a.sfx.play("games/move");
    }
    if (rollMs_ <= 0) {
      int sum = v_[0] + v_[1];
      a.sfx.play("games/bounce");
      a.react(sum == 12 ? Emotion::Starstruck : sum == 2 ? Emotion::Crying : v_[0] == v_[1] ? Emotion::Amazed : sum >= 8 ? Emotion::Happy : Emotion::Neutral, 1500);
    }
  }
  void draw(App& a, Canvas& c) override {
    float wob = rollMs_ > 0 ? clampf(rollMs_ / kRoll, 0, 1) : 0, ts = a.nowMs() * 0.02f;
    for (int d = 0; d < 2; d++) {
      float x = kCx + (d ? 84.f : -84.f) + wob * 16 * sinf(ts + d * 2), y = kCy - 14 + wob * 26 * sinf(ts * 1.4f + d);
      float s = 124 * (1 + wob * 0.06f * sinf(ts * 2.1f + d));
      die(c, x, y, s, v_[d]);
    }
    char b[8];
    snprintf(b, sizeof(b), "%d", v_[0] + v_[1]);
    c.text(kCx, kCy + 72, b, FontSize::L, rollMs_ > 0 ? kDim : kWhite, Align::Center);
    a.face.drawScaled(c, a.anim, kCx, 96, 0.22f, a.eyeColor());
  }
  void onGesture(App& a, const GestureEvent& e) override { if (e.g == Gesture::Tap) roll(a); }
  bool onMotion(App& a, MotionEvent m) override { if (m != MotionEvent::Shake) return false; roll(a); return true; }
 private:
  static constexpr float kRoll = 1100;
  static void die(Canvas& c, float x, float y, float s, int v) {
    c.roundRect(x - s / 2, y - s / 2, s, s, s * 0.17f, kWhite);
    float q = s * 0.26f, r = s * 0.09f;
    // Pip map for a 3x3 grid, read as bit flags.
    static const uint16_t pips[7] = {0, 0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};
    for (int i = 0; i < 9; i++) if ((pips[v] >> i) & 1) c.fillCircle(x + (i % 3 - 1) * q, y + (i / 3 - 1) * q, r, kBlack);
  }
  void roll(App& a) {
    if (rollMs_ > 0) return;
    rollMs_ = kRoll;
    stepMs_ = 0;
    a.sfx.play("interactions/shake");
    a.react(Emotion::Excited, 900);
  }
  float rollMs_ = 0, stepMs_ = 0;
  int v_[2] = {6, 6};
};

// ---- Magic ball -----------------------------------------------------------------------------------
static const char* const kAnswers[20] = {
    "Бесспорно", "Предрешено", "Никаких сомнений", "Определённо да", "Можешь быть уверен",
    "Мне кажется — да", "Вероятнее всего", "Хорошие перспективы", "Знаки говорят — да", "Да",
    "Пока не ясно", "Спроси позже", "Лучше не сейчас", "Не могу сказать", "Сосредоточься и спроси",
    "Даже не думай", "Мой ответ — нет", "Мои источники говорят нет", "Перспективы так себе", "Весьма сомнительно"};

class MagicBallScreen : public Screen {
 public:
  void enter(App&) override { fade_ = 0; shakeMs_ = 0; answer_ = -1; }
  void tick(App& a, float dt) override {
    if (shakeMs_ > 0) {
      shakeMs_ -= dt;
      if (shakeMs_ <= 0) {
        answer_ = a.rng.range(0, 19);
        fade_ = 0;
        a.sfx.play("system/notify");
        a.react(answer_ < 10 ? Emotion::Winking : answer_ < 15 ? Emotion::Thinking : Emotion::Skeptical, 1800);
      }
    } else if (fade_ < 1) fade_ += dt / 700;
  }
  void draw(App& a, Canvas& c) override {
    float k = shakeMs_ > 0 ? 8 : 0, ts = a.nowMs() * 0.04f;
    float x = kCx + k * sinf(ts), y = kCy + k * 0.6f * sinf(ts * 1.3f);
    c.ring(x, y, 208, 2, kDark);
    if (shakeMs_ > 0 || answer_ < 0) {
      c.text((int)x, (int)y - 40, "8", FontSize::L, kDim, Align::Center);
      if (answer_ < 0 && shakeMs_ <= 0) c.textWrapped(kCx, kCy + 12, 280, "Задай вопрос и встряхни", FontSize::S, kGray, Align::Center, 2);
      return;
    }
    // The answer floats up out of the dark.
    uint16_t col = blend565(kBlack, kSky, (uint32_t)(clampf(fade_, 0, 1) * 255));
    c.line(x - 150, y - 86, x + 150, y - 86, 2, blend565(kBlack, kDim, (uint32_t)(clampf(fade_, 0, 1) * 255)));
    c.line(x - 150, y - 86, x, y + 150, 2, blend565(kBlack, kDim, (uint32_t)(clampf(fade_, 0, 1) * 255)));
    c.line(x + 150, y - 86, x, y + 150, 2, blend565(kBlack, kDim, (uint32_t)(clampf(fade_, 0, 1) * 255)));
    c.textWrapped((int)x, (int)y - 56, 190, kAnswers[answer_], FontSize::M, col, Align::Center, 4);
  }
  void onGesture(App& a, const GestureEvent& e) override { if (e.g == Gesture::Tap) ask(a); }
  bool onMotion(App& a, MotionEvent m) override { if (m != MotionEvent::Shake) return false; ask(a); return true; }
 private:
  void ask(App& a) {
    if (shakeMs_ > 0) return;
    shakeMs_ = 900;
    a.sfx.play("interactions/shake");
    a.react(Emotion::Curious, 1000);
  }
  float fade_ = 0, shakeMs_ = 0;
  int answer_ = -1;
};

// ---- Balance: keep the marble in the middle while a growing wind pushes it ------------------------
class BalanceScreen : public Screen {
 public:
  void enter(App& a) override { running_ = false; over_ = false; ms_ = 0; a.react(Emotion::Focused, 1000); }
  bool capturesSwipes() const override { return running_; }
  void tick(App& a, float dt) override {
    char b[8];
    snprintf(b, sizeof(b), "%d", (int)(ms_ / 1000));
    num_.set(b);
    num_.tick(dt);
    if (!running_) return;
    ms_ += dt;
    float h = dt * 0.001f, wind = 18 + ms_ * 0.004f, t = ms_ * 0.001f;
    float wx = (sinf(t * 0.9f + seed_) + 0.6f * sinf(t * 2.3f)) * wind, wy = (sinf(t * 1.1f + seed_ * 2) + 0.6f * sinf(t * 1.9f)) * wind;
    vx_ += (a.hal.ax * 420 + wx - vx_ * 0.9f) * h;
    vy_ += (a.hal.ay * 420 + wy - vy_ * 0.9f) * h;
    x_ += vx_ * h;
    y_ += vy_ * h;
    float d = sqrtf(x_ * x_ + y_ * y_);
    a.anim.lookOffset(x_ * 0.2f, y_ * 0.15f, 120);
    if (d > kLimit) {
      running_ = false;
      over_ = true;
      int secs = (int)(ms_ / 1000);
      bool record = secs > a.settings.balanceBest;
      if (record) { a.settings.balanceBest = (uint16_t)secs; a.saveSettings(); }
      a.pet.event(record ? PetEvent::GameWin : PetEvent::GameLose);
      a.sfx.play(record ? "games/win" : "games/wrong");
      a.react(record ? Emotion::Proud : Emotion::Dizzy, 1800);
    }
  }
  void draw(App& a, Canvas& c) override {
    c.ring(kCx, kCy, kLimit + 11, 2, kDim);
    c.ring(kCx, kCy, 34, 1.5f, kDark);
    float d = sqrtf(x_ * x_ + y_ * y_);
    c.fillCircle(kCx + x_, kCy + y_, 11, d > kLimit * 0.7f ? kRose : kWhite);
    num_.draw(c, kCx, 72, 5, 1, kGray);
    if (!running_) c.text(kCx, kCy + 50, over_ ? "Ещё раз?" : "Нажми и лови шар", FontSize::S, kGray, Align::Center);
    a.face.drawScaled(c, a.anim, kCx, kScreenH - 50, 0.16f, a.eyeColor());
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || running_) return;
    running_ = true; over_ = false; ms_ = 0; x_ = y_ = vx_ = vy_ = 0; seed_ = a.rng.rangef(0, 6);
    a.sfx.play("games/go");
  }
 private:
  static constexpr float kLimit = 150;
  DotText num_;
  bool running_ = false, over_ = false;
  float ms_ = 0, x_ = 0, y_ = 0, vx_ = 0, vy_ = 0, seed_ = 0;
};

// ---- Duel: hold still, at the signal swing first. The pet has its own reaction time. ----------------
class DuelScreen : public Screen {
 public:
  void enter(App& a) override { st_ = Idle; a.react(Emotion::Determined, 1200); }
  void tick(App& a, float dt) override {
    float mag = a.motionMgr.lastMagnitude();
    float dev = fabsf(mag - 1.f);
    switch (st_) {
      case Steady:
        steadyMs_ = dev < 0.12f ? steadyMs_ + dt : 0;
        if (steadyMs_ > 900) { st_ = Wait; waitMs_ = a.rng.rangef(1800, 5000); }
        break;
      case Wait:
        waitMs_ -= dt;
        if (dev > 0.55f) {
          st_ = Foul;
          a.sfx.play("games/wrong");
          a.pet.event(PetEvent::GameWin);
          a.react(Emotion::Smug, 1800);
        } else if (waitMs_ <= 0) {
          st_ = Draw;
          drawMs_ = 0;
          petMs_ = a.rng.rangef(240, 520);
          a.sfx.play("games/go");
          a.react(Emotion::Determined, 900);
        }
        break;
      case Draw:
        drawMs_ += dt;
        if (dev > 0.55f || drawMs_ > 1500) {
          last_ = drawMs_ > 1500 ? 0 : (int)drawMs_;
          bool win = last_ > 0 && last_ < petMs_;
          if (win && (!a.settings.duelBest || last_ < a.settings.duelBest)) { a.settings.duelBest = (uint16_t)last_; a.saveSettings(); }
          st_ = win ? Won : Lost;
          a.pet.event(win ? PetEvent::GameLose : PetEvent::GameWin);
          a.sfx.play(win ? "games/win" : "games/lose");
          a.react(win ? Emotion::Terrified : Emotion::Proud, 1800, EmotionPriority::Strong);
        }
        break;
      default: break;
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[40];
    switch (st_) {
      case Idle: label(a, c, "Дуэль", "Возьми в руку и нажми"); break;
      case Steady: label(a, c, "Держи ровно", "Не двигайся"); break;
      case Wait: label(a, c, "Жди сигнала…", ""); break;
      case Draw:
        c.fillCircle(kCx, kCy, 205, kWhite);
        c.text(kCx, kCy - 26, "РЫВОК!", FontSize::L, kBlack, Align::Center);
        break;
      case Foul: label(a, c, "Фальстарт", "Нажми, чтобы повторить"); break;
      case Won: snprintf(b, sizeof(b), "%d мс", last_); label(a, c, "Попал первым", b); break;
      case Lost: snprintf(b, sizeof(b), "%d мс против %d", last_, (int)petMs_); label(a, c, "Питомец быстрее", b); break;
    }
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || st_ == Steady || st_ == Wait || st_ == Draw) return;
    st_ = Steady;
    steadyMs_ = 0;
    a.sfx.play("system/click");
  }
  bool capturesSwipes() const override { return st_ == Steady || st_ == Wait || st_ == Draw; }
  // The duel reads the raw movement itself: a swing here must not become a pet reaction.
  bool onMotion(App&, MotionEvent m) override { return m == MotionEvent::Shake || m == MotionEvent::Jerk; }
 private:
  void label(App& a, Canvas& c, const char* t, const char* s) {
    a.face.drawScaled(c, a.anim, kCx, 136, 0.5f, a.eyeColor());
    c.text(kCx, 238, t, FontSize::M, kWhite, Align::Center);
    c.text(kCx, 276, s, FontSize::S, kGray, Align::Center);
  }
  enum State { Idle, Steady, Wait, Draw, Foul, Won, Lost } st_ = Idle;
  float steadyMs_ = 0, waitMs_ = 0, drawMs_ = 0, petMs_ = 0;
  int last_ = 0;
};

// ---- Shaker: as many strokes as possible in ten seconds -----------------------------------------------
class ShakerScreen : public Screen {
 public:
  void enter(App&) override { st_ = Idle; count_ = 0; }
  void tick(App& a, float dt) override {
    char b[8];
    snprintf(b, sizeof(b), "%d", count_);
    num_.set(b);
    num_.tick(dt * 3);
    if (st_ != Run) return;
    left_ -= dt;
    float mag = a.motionMgr.lastMagnitude();
    if (armed_ && mag > 1.5f) { armed_ = false; count_++; a.sfx.play("games/move"); a.anim.kick(-0.12f); if (count_ % 10 == 0) a.react(Emotion::Excited, 700); }
    if (!armed_ && mag < 1.2f) armed_ = true;
    if (left_ <= 0) {
      st_ = Done;
      bool record = count_ > a.settings.shakerBest;
      if (record) { a.settings.shakerBest = (uint16_t)count_; a.saveSettings(); }
      a.pet.event(PetEvent::Play);
      a.sfx.play(record ? "games/win" : "games/draw");
      a.react(record ? Emotion::Starstruck : Emotion::Happy, 2000);
    }
  }
  void draw(App& a, Canvas& c) override {
    drawRimSquares(c, st_ == Run ? left_ / 10000 : 0, kSky);
    num_.draw(c, kCx, kCy - 40, 10, 2, kWhite);
    a.face.drawScaled(c, a.anim, kCx, 118, 0.28f, a.eyeColor());
    if (st_ != Run) {
      char b[32];
      snprintf(b, sizeof(b), "Рекорд %d", a.settings.shakerBest);
      c.text(kCx, kCy + 66, b, FontSize::S, kGray, Align::Center);
      c.text(kCx, kCy + 94, "Нажми и тряси 10 секунд", FontSize::S, kGray, Align::Center);
    }
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || st_ == Run) return;
    st_ = Run; count_ = 0; left_ = 10000; armed_ = true;
    a.sfx.play("games/go");
  }
  bool capturesSwipes() const override { return st_ == Run; }
  bool onMotion(App&, MotionEvent m) override { return st_ == Run && (m == MotionEvent::Shake || m == MotionEvent::Jerk); }
 private:
  enum State { Idle, Run, Done } st_ = Idle;
  DotText num_;
  int count_ = 0;
  float left_ = 0;
  bool armed_ = true;
};

// ---- Safe: turn the device like a dial. The right spot clicks; a tap locks it. ---------------------------
class SafeScreen : public Screen {
 public:
  void enter(App& a) override { start(a); }
  void tick(App& a, float dt) override {
    float tilt = sqrtf(a.hal.ax * a.hal.ax + a.hal.ay * a.hal.ay);
    if (tilt > 0.14f) {
      float target = atan2f(a.hal.ax, -a.hal.ay) * 57.29578f;
      float diff = fmodf(target - dial_ + 540.f, 360.f) - 180.f;
      dial_ += diff * clampf(dt / 110.f, 0, 1);
    }
    int tick = (int)floorf(dial_ / 6);
    if (tick != lastTick_ && !opened()) {
      lastTick_ = tick;
      // Louder near the secret: the sound is the whole clue, as with a real safe.
      if (near() < 8) a.sfx.play("games/move");
    }
    if (near() < 3 && !opened()) a.anim.lookOffset(0, -8, 120);
    if (openMs_ > 0) { openMs_ -= dt; if (openMs_ <= 0) start(a); }
  }
  void draw(App& a, Canvas& c) override {
    for (int i = 0; i < 60; i++) {
      float ang = (i * 6 + dial_) * 0.0174533f, s = sinf(ang), co = -cosf(ang);
      float r0 = i % 5 == 0 ? 186.f : 196.f;
      c.line(kCx + s * r0, kCy + co * r0, kCx + s * 208, kCy + co * 208, i % 5 == 0 ? 3 : 1.5f, i % 5 == 0 ? kGray : kDim);
    }
    c.fillCircle(kCx, 14, 5, near() < 3 ? kMint : kDim);
    for (int i = 0; i < 3; i++) {
      float x = kCx - 40 + i * 40;
      if (i < stage_) c.fillCircle(x, kCy + 70, 9, kMint); else c.ring(x, kCy + 70, 9, 2, kDim);
    }
    a.face.drawScaled(c, a.anim, kCx, kCy - 26, 0.42f, a.eyeColor());
    c.text(kCx, kCy + 100, opened() ? "Открыто!" : "Нажми на щелчке", FontSize::S, opened() ? kMint : kGray, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || opened()) return;
    if (near() < 5) {
      a.sfx.play("games/correct");
      if (++stage_ == 3) {
        a.sfx.play("games/win");
        a.pet.event(PetEvent::GameWin);
        a.react(Emotion::Starstruck, 1800);
        openMs_ = 2000;
      } else { secret_ = a.rng.rangef(0, 360); a.react(Emotion::Sneaky, 900); }
    } else {
      a.sfx.play("games/wrong");
      a.react(Emotion::Suspicious, 800);
    }
  }
 private:
  bool opened() const { return stage_ >= 3; }
  float near() const { return fabsf(fmodf(dial_ - secret_ + 540.f, 360.f) - 180.f); }
  void start(App& a) { stage_ = 0; secret_ = a.rng.rangef(0, 360); openMs_ = 0; a.react(Emotion::Sneaky, 1200); }
  float dial_ = 0, secret_ = 0, openMs_ = 0;
  int stage_ = 0, lastTick_ = 0;
};

// ---- Workout: the device in hand counts repetitions from acceleration peaks ---------------------------------
struct Exercise { const char* name; int reps; };
static const Exercise kExercises[] = {{"Прыжки", 10}, {"Приседания", 10}, {"Махи руками", 12}, {"Шаги на месте", 20}};

class WorkoutScreen : public Screen {
 public:
  void enter(App& a) override { i_ = 0; reps_ = 0; finished_ = false; a.react(Emotion::Determined, 1200); }
  void tick(App& a, float dt) override {
    char b[8];
    snprintf(b, sizeof(b), "%d", reps_);
    num_.set(b);
    num_.tick(dt * 3);
    if (finished_) return;
    refract_ -= dt;
    float mag = a.motionMgr.lastMagnitude();
    // One rep is a clear peak with a pause after it, so shaking cannot score a dozen at once.
    if (armed_ && mag > 1.35f && refract_ <= 0) {
      armed_ = false;
      refract_ = 320;
      reps_++;
      a.sfx.play("games/move");
      a.anim.kick(-0.18f);
      if (reps_ >= kExercises[i_].reps) next(a);
    }
    if (!armed_ && mag < 1.12f) armed_ = true;
  }
  void draw(App& a, Canvas& c) override {
    const int n = sizeof(kExercises) / sizeof(kExercises[0]);
    if (finished_) {
      a.face.draw(c, a.anim, a.eyeColor());
      c.text(kCx, kCy + 90, "Отлично потренировались!", FontSize::S, kMint, Align::Center);
      return;
    }
    drawRimSquares(c, (float)reps_ / kExercises[i_].reps, kMint);
    a.face.drawScaled(c, a.anim, kCx, 112, 0.34f, a.eyeColor());
    c.text(kCx, 164, kExercises[i_].name, FontSize::M, kWhite, Align::Center);
    num_.draw(c, kCx, 212, 9, 2, kWhite);
    char b[32];
    snprintf(b, sizeof(b), "из %d  ·  %d/%d", kExercises[i_].reps, i_ + 1, n);
    c.text(kCx, 300, b, FontSize::S, kGray, Align::Center);
    c.text(kCx, 340, "Нажми, чтобы пропустить", FontSize::S, kDim, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (finished_) { enter(a); return; }
    next(a);
  }
  bool onMotion(App&, MotionEvent m) override { return !finished_ && (m == MotionEvent::Shake || m == MotionEvent::Jerk || m == MotionEvent::Lifted); }
 private:
  void next(App& a) {
    i_++;
    reps_ = 0;
    a.sfx.play("games/correct");
    if (i_ >= (int)(sizeof(kExercises) / sizeof(kExercises[0]))) {
      finished_ = true;
      a.pet.stats.energy = clampf(a.pet.stats.energy - 6, 0, 100);
      a.pet.stats.happiness = clampf(a.pet.stats.happiness + 12, 0, 100);
      a.pet.stats.stress = clampf(a.pet.stats.stress - 15, 0, 100);
      a.settings.workouts++;
      a.saveSettings();
      a.sfx.play("games/win");
      a.react(Emotion::Proud, 2500);
    } else a.react(Emotion::Happy, 800);
  }
  DotText num_;
  int i_ = 0, reps_ = 0;
  float refract_ = 0;
  bool armed_ = true, finished_ = false;
};

// ---- Mole: little eyes peek out of holes around the rim -------------------------------------------------------
class MoleScreen : public Screen {
 public:
  void enter(App&) override { running_ = false; score_ = 0; }
  bool capturesSwipes() const override { return running_; }
  void tick(App& a, float dt) override {
    char b[8];
    snprintf(b, sizeof(b), "%d", score_);
    num_.set(b);
    num_.tick(dt * 2);
    if (!running_) return;
    left_ -= dt;
    upMs_ -= dt;
    if (upMs_ <= 0) {
      if (at_ >= 0) a.sfx.play("games/move");
      int was = at_;
      do at_ = a.rng.range(0, kHoles - 1); while (at_ == was);
      lifeMs_ = upMs_ = lerpf(1100, 480, clampf(score_ / 30.f, 0, 1));
    }
    if (left_ <= 0) {
      running_ = false;
      bool record = score_ > a.settings.moleBest;
      if (record) { a.settings.moleBest = (uint16_t)score_; a.saveSettings(); }
      a.pet.event(record ? PetEvent::GameWin : PetEvent::Play);
      a.sfx.play(record ? "games/win" : "games/draw");
      a.react(record ? Emotion::Proud : Emotion::Happy, 1800);
    }
  }
  void draw(App& a, Canvas& c) override {
    for (int i = 0; i < kHoles; i++) {
      float x, y;
      pos(i, &x, &y);
      c.ring(x, y, 34, 2, kDark);
      if (running_ && i == at_) {
        float pop = smooth01((lifeMs_ - upMs_) / 120) * smooth01(upMs_ / 100);
        c.fillCircle(x - 13, y, 11 * pop, kRose);
        c.fillCircle(x + 13, y, 11 * pop, kRose);
        c.fillCircle(x - 13, y + 2, 4 * pop, kBlack);
        c.fillCircle(x + 13, y + 2, 4 * pop, kBlack);
      }
    }
    num_.draw(c, kCx, kCy - 20, 6, 1, kWhite);
    if (!running_) c.text(kCx, kCy + 34, "Нажми", FontSize::S, kGray, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (!running_) { running_ = true; score_ = 0; left_ = 25000; at_ = -1; upMs_ = 400; lifeMs_ = 400; a.sfx.play("games/go"); return; }
    if (at_ < 0) return;
    float x, y;
    pos(at_, &x, &y);
    if ((e.x - x) * (e.x - x) + (e.y - y) * (e.y - y) < 46 * 46) {
      score_++;
      at_ = -1;
      upMs_ = 200;
      a.sfx.play("games/correct");
      a.react(Emotion::Mischievous, 500);
    }
  }
 private:
  static constexpr int kHoles = 8;
  static void pos(int i, float* x, float* y) { float a = i * 0.7854f; *x = kCx + sinf(a) * 150; *y = kCy - cosf(a) * 150; }
  DotText num_;
  bool running_ = false;
  int score_ = 0, at_ = -1;
  float left_ = 0, upMs_ = 0, lifeMs_ = 1;
};

// ---- Controller: the phone hosts the game, this screen is the gamepad ----------------------------------------------
static void suitShape(Canvas& c, char s, float x, float y, float r, uint16_t col) {
  switch (s) {
    case 'h': c.heart(x, y - r * 0.35f, r * 0.72f, col); break;
    case 'd':
      for (int i = -(int)r; i <= (int)r; i++) { int w = (int)(r * 0.8f * (1 - fabsf((float)i) / r)); c.fillRect((int)x - w, (int)y + i, w * 2 + 1, 1, col); }
      break;
    case 'c':
      c.fillCircle(x, y - r * 0.45f, r * 0.42f, col);
      c.fillCircle(x - r * 0.5f, y + r * 0.15f, r * 0.42f, col);
      c.fillCircle(x + r * 0.5f, y + r * 0.15f, r * 0.42f, col);
      c.line(x, y, x, y + r, r * 0.25f, col);
      break;
    default:
      for (int i = 0; i <= (int)r; i++) { int w = (int)(r * 0.95f * i / r); c.fillRect((int)x - w, (int)(y - r) + i, w * 2 + 1, 1, col); }
      c.fillCircle(x - r * 0.46f, y + r * 0.05f, r * 0.46f, col);
      c.fillCircle(x + r * 0.46f, y + r * 0.05f, r * 0.46f, col);
      c.line(x, y, x, y + r, r * 0.22f, col);
      break;
  }
}

static void drawCard(Canvas& c, float x, float y, float w, float h, const char* label, bool focus, bool marked) {
  c.roundRect(x - w / 2, y - h / 2, w, h, 8, focus ? kWhite : (marked ? kDim : kDark));
  size_t n = strlen(label);
  if (!n) return;
  char rank[6];
  snprintf(rank, sizeof(rank), "%.*s", (int)(n - 1), label);
  char suit = label[n - 1];
  bool red = suit == 'h' || suit == 'd';
  uint16_t col = focus ? (red ? rgb565(170, 96, 110) : kBlack) : (red ? kRose : kGray);
  FontSize f = focus ? FontSize::M : FontSize::S;
  c.text((int)x, (int)(y - h * 0.40f), rank, f, col, Align::Center);
  suitShape(c, suit, x, y + h * 0.16f, focus ? 15 : 9, col);
  if (marked) c.fillCircle(x, y - h / 2 - 11, 5, kMint);
}

class ControllerScreen : public Screen {
 public:
  void enter(App&) override { sel_ = 0; pos_ = 0; held_ = false; }
  bool capturesSwipes() const override { return true; }
  void tick(App& a, float dt) override {
    ControllerCfg& cc = a.controller;
    cc.keepaliveMs -= dt;
    if (cc.keepaliveMs <= 0) { cc.active = false; a.showScreen(ScreenId::Face); a.react(Emotion::Confused, 1500); return; }
    a.power.activity();
    a.anim.lookOffset(a.hal.ax * 40, a.hal.ay * 26, 200);
    if (!cc.cards || cc.count <= 0) return;
    if (sel_ >= cc.count) { sel_ = cc.count - 1; pos_ = sel_ + 0.5f; cc.selected = sel_; }
    int was = sel_;
    if (cc.count <= kDirectLimit) {
      // A small hand maps directly: each card has its own lean angle. Hysteresis keeps a trembling hand from flicking.
      float pos = clampf((a.hal.ax + 0.42f) / 0.84f, 0, 0.999f) * cc.count;
      if (pos < sel_ - 0.2f || pos > sel_ + 1.2f) sel_ = (int)pos;
      pos_ = sel_ + 0.5f;
    } else {
      // A big hand scrolls instead: the lean sets the speed, level holds still, so any card stays reachable.
      float t = a.hal.ax, mag = fabsf(t);
      if (mag > kDeadZone) {
        float speed = (mag - kDeadZone) * 13;   // cards per second
        pos_ += (t > 0 ? speed : -speed) * dt * 0.001f;
        pos_ = clampf(pos_, 0, cc.count - 0.001f);
        sel_ = (int)pos_;
      } else pos_ = sel_ + 0.5f;  // settles on the card, the next lean starts from its middle
    }
    if (sel_ != was) {
      cc.selected = sel_;
      a.sfx.play("games/move");
      a.pushInput("select", sel_);
    }
  }
  void draw(App& a, Canvas& c) override {
    const ControllerCfg& cc = a.controller;
    c.text(kCx, 22, cc.title, FontSize::S, kGray, Align::Center);
    if (cc.cards) drawHand(c, cc);
    else drawPad(a, c);
    if (cc.hint[0]) c.textWrapped(kCx, kCy + 136, 280, cc.hint, FontSize::S, kGray, Align::Center, 2);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    switch (e.g) {
      case Gesture::Down: held_ = true; a.pushInput("down", sel_); break;
      case Gesture::Up: held_ = false; a.pushInput("up", sel_); break;
      case Gesture::Tap: a.sfx.play("system/click"); a.pushInput("tap", sel_); break;
      case Gesture::LongPress: a.pushInput("long_press", sel_); break;
      case Gesture::SwipeLeft: a.pushInput("swipe_left", sel_); break;
      case Gesture::SwipeRight: a.pushInput("swipe_right", sel_); break;
      case Gesture::SwipeUp: a.sfx.play("interactions/swipe"); a.pushInput("swipe_up", sel_); break;
      case Gesture::SwipeDown: a.sfx.play("interactions/swipe"); a.pushInput("swipe_down", sel_); break;
      default: break;
    }
  }
  bool onMotion(App& a, MotionEvent m) override {
    if (m == MotionEvent::Shake) a.pushInput("shake", sel_);
    return m == MotionEvent::Shake || m == MotionEvent::Jerk || m == MotionEvent::Lifted || m == MotionEvent::FaceDown;
  }
 private:
  static constexpr int kDirectLimit = 6, kSide = 3;
  static constexpr float kDeadZone = 0.12f;
  void drawHand(Canvas& c, const ControllerCfg& cc) const {
    if (cc.info[0]) c.text(kCx, 44, cc.info, FontSize::S, kSoft, Align::Center);
    if (cc.count <= 0) { c.text(kCx, kCy - 20, "Нет карт", FontSize::M, kGray, Align::Center); return; }
    int focus = sel_ < cc.count ? sel_ : cc.count - 1;
    // Neighbours sit below the focused card in fixed slots. Only a window of the hand fits; "+N" tells what is beyond.
    for (int k = -kSide; k <= kSide; k++) {
      int i = focus + k;
      if (!k || i < 0 || i >= cc.count) continue;
      bool mark = cc.marks[i] == '1';
      drawCard(c, kCx + k * 52.f, kCy + 82 - (mark ? 8 : 0), 46, 64, cc.labels[i], false, mark);
    }
    int leftHidden = focus - kSide, rightHidden = cc.count - 1 - focus - kSide;
    char more[8];
    if (leftHidden > 0) { snprintf(more, sizeof(more), "+%d", leftHidden); c.text(kCx - 196, kCy + 72, more, FontSize::S, kDim, Align::Center); }
    if (rightHidden > 0) { snprintf(more, sizeof(more), "+%d", rightHidden); c.text(kCx + 196, kCy + 72, more, FontSize::S, kDim, Align::Center); }
    drawCard(c, kCx, kCy - 54, 96, 132, cc.labels[focus], true, cc.marks[focus] == '1');
    if (cc.target[0]) {
      c.text(kCx - 118, kCy - 112, "бей", FontSize::S, kGray, Align::Center);
      drawCard(c, kCx - 118, kCy - 46, 58, 82, cc.target, false, false);
    }
    // Position in the hand and everything chosen, at a glance on the rim.
    int chosen = 0;
    for (int i = 0; i < cc.count; i++) {
      float ang = (cc.count > 1 ? 140 + 80.f * i / (cc.count - 1) : 180.f) * 0.0174533f;
      float x = kCx + sinf(ang) * (kCx - 9), y = kCy - cosf(ang) * (kCy - 9);
      bool mark = cc.marks[i] == '1';
      chosen += mark;
      c.fillCircle(x, y, i == focus ? 4.5f : 2.5f, i == focus ? kWhite : (mark ? kMint : kDim));
    }
    char st[24];
    snprintf(st, sizeof(st), "%d/%d", focus + 1, cc.count);
    c.text(kCx + 112, kCy - 86, st, FontSize::S, kGray, Align::Center);
    if (chosen) {
      snprintf(st, sizeof(st), "выбрано %d", chosen);
      c.text(kCx + 118, kCy - 60, st, FontSize::S, kMint, Align::Center);
    }
  }
  void drawPad(App& a, Canvas& c) const {
    // A ring with a dot that follows the tilt; the disc fills while the screen is held.
    float x = kCx + a.hal.ax * 130, y = kCy - 20 + a.hal.ay * 130;
    c.ring(kCx, kCy - 20, 140, 2, kDark);
    c.line(kCx - 140, kCy - 20, kCx + 140, kCy - 20, 1, kDark);
    c.line(kCx, kCy - 160, kCx, kCy + 120, 1, kDark);
    if (held_) c.fillCircle(kCx, kCy - 20, 56, kDim);
    c.fillCircle(x, y, 18, kSky);
  }
  int sel_ = 0;
  float pos_ = 0;
  bool held_ = false;
};

// ---- Mouse: a Bluetooth mouse. Tilt or finger moves the pointer, the rim scrolls, as on a laptop touchpad ------------
class MouseScreen : public Screen {
 public:
  void enter(App& a) override {
    zeroX_ = avgX_ = a.hal.ax; zeroY_ = avgY_ = a.hal.ay;   // however it is held now becomes "still"
    tiltOn_ = true; rim_ = false; dragging_ = false; lastDy_ = 0;
    a.mouseState = App::MouseState{};
    a.react(Emotion::Focused, 1000);
  }
  bool wantsDoubleTap() const override { return true; }
  bool capturesSwipes() const override { return true; }
  void tick(App& a, float dt) override {
    a.power.activity();
    App::MouseState& m = a.mouseState;
    if (m.leftReleaseMs > 0) { m.leftReleaseMs -= dt; if (m.leftReleaseMs <= 0) { m.buttons &= ~1; m.buttonsChanged = true; } }
    if (m.rightReleaseMs > 0) { m.rightReleaseMs -= dt; if (m.rightReleaseMs <= 0) { m.buttons &= ~2; m.buttonsChanged = true; } }
    // Slow average of the lean: a new zero point must not be taken from the peak of a shake.
    avgX_ += (a.hal.ax - avgX_) * clampf(dt / 400.f, 0, 1);
    avgY_ += (a.hal.ay - avgY_) * clampf(dt / 400.f, 0, 1);
    // The finger is a clutch: while it moves the pointer, tilt does not interfere. A violent movement is not a lean at all.
    if (tiltOn_ && !a.touch.isDown() && fabsf(a.motionMgr.lastMagnitude() - 1.f) < 0.45f) {
      m.x += curve(a.hal.ax - zeroX_) * dt;
      m.y += curve(a.hal.ay - zeroY_) * dt;
    }
    a.anim.lookOffset(clampf(a.hal.ax - zeroX_, -1, 1) * 36, clampf(a.hal.ay - zeroY_, -1, 1) * 24, 150);
  }
  void draw(App& a, Canvas& c) override {
    uint8_t b = a.mouseState.buttons;
    c.arc(kCx, kCy, kCx - 9, 10, 15, 165, kDark);   // the right rim is the scroll strip
    for (int i = 0; i < 5; i++) { float ang = (60 + i * 15) * 0.0174533f; c.fillCircle(kCx + sinf(ang) * (kCx - 9), kCy - cosf(ang) * (kCx - 9), 2, kDim); }
    float px = kCx + clampf(a.hal.ax - zeroX_, -1, 1) * 70, py = kCy - 44 + clampf(a.hal.ay - zeroY_, -1, 1) * 70;
    c.ring(kCx, kCy - 44, 78, 2, kDark);
    if (tiltOn_) c.fillCircle(px, py, 12, kSky); else c.ring(kCx, kCy - 44, 12, 2, kDim);
    c.roundRect(kCx - 72, kCy + 50, 68, 34, 10, (b & 1) ? kWhite : kDark);
    c.roundRect(kCx + 4, kCy + 50, 68, 34, 10, (b & 2) ? kWhite : kDark);
    c.text(kCx - 38, kCy + 54, "Л", FontSize::S, (b & 1) ? kBlack : kGray, Align::Center);
    c.text(kCx + 38, kCy + 54, "П", FontSize::S, (b & 2) ? kBlack : kGray, Align::Center);
    c.text(kCx, 48, a.hal.bleConnected ? "Подключена" : "Найди в Bluetooth", FontSize::S, a.hal.bleConnected ? kMint : kGray, Align::Center);
    c.textWrapped(kCx, kCy + 96, 300, tiltOn_ ? "тап левая · два тапа правая\nкрай прокрутка · тряска стоп\n↓ от верхнего края выход"
                                              : "наклон выключен, встряхни", FontSize::S, kDim, Align::Center, 3);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    App::MouseState& m = a.mouseState;
    switch (e.g) {
      case Gesture::Down:
        rim_ = onRim(e.x, e.y);
        downY_ = e.y;
        lastDy_ = 0;
        break;
      case Gesture::Move:
        if (rim_) m.wheel -= e.dy * 0.12f;
        else { m.x += e.dx * 1.6f; m.y += e.dy * 1.6f; }
        break;
      case Gesture::Up:
        if (dragging_) { dragging_ = false; m.buttons &= ~1; m.buttonsChanged = true; }
        break;
      case Gesture::Tap: click(m, 1, &m.leftReleaseMs); a.sfx.play("system/click"); break;
      case Gesture::DoubleTap: click(m, 2, &m.rightReleaseMs); a.sfx.play("system/click"); break;
      case Gesture::LongPress:
        // Press and hold, then move: dragging and selecting, as with tap-and-drag on a touchpad.
        dragging_ = true;
        m.buttons |= 1;
        m.buttonsChanged = true;
        a.sfx.play("interactions/hold");
        break;
      case Gesture::SwipeDown:
        if (downY_ < 60) { m.buttons = 0; m.buttonsChanged = true; a.showScreen(ScreenId::Face); }
        break;
      default: break;
    }
  }
  bool onMotion(App& a, MotionEvent ev) override {
    if (ev == MotionEvent::Shake) {
      tiltOn_ = !tiltOn_;
      zeroX_ = avgX_; zeroY_ = avgY_;
      a.mouseState.x = a.mouseState.y = 0;
      a.sfx.play(tiltOn_ ? "games/go" : "system/click");
      return true;
    }
    return ev == MotionEvent::Jerk || ev == MotionEvent::Lifted || ev == MotionEvent::FaceDown;
  }
 private:
  // Pixels per millisecond. A dead zone for a steady hand, then speed grows with the square for fine control near centre.
  static float curve(float t) {
    float mag = fabsf(t);
    if (mag < 0.06f) return 0;
    float v = (mag - 0.06f) * (mag - 0.06f) * 5.5f;
    if (v > 1.6f) v = 1.6f;
    return t > 0 ? v : -v;
  }
  static bool onRim(int x, int y) {
    float dx = (float)(x - kCx), dy = (float)(y - kCy);
    return dx > 40 && dx * dx + dy * dy > 175 * 175;
  }
  static void click(App::MouseState& m, uint8_t bit, float* release) { m.buttons |= bit; m.buttonsChanged = true; *release = 45; }
  float zeroX_ = 0, zeroY_ = 0, avgX_ = 0, avgY_ = 0;
  int downY_ = 0, lastDy_ = 0;
  bool tiltOn_ = true, rim_ = false, dragging_ = false;
};

Screen* partyScreen(ScreenId id) {
  static CoinScreen coin; static DiceScreen dice; static MagicBallScreen ball; static BalanceScreen balance;
  static DuelScreen duel; static ShakerScreen shaker; static SafeScreen safe; static WorkoutScreen workout;
  static MoleScreen mole; static ControllerScreen controller; static MouseScreen mouse;
  switch (id) {
    case ScreenId::Coin: return &coin;
    case ScreenId::Dice: return &dice;
    case ScreenId::MagicBall: return &ball;
    case ScreenId::Balance: return &balance;
    case ScreenId::Duel: return &duel;
    case ScreenId::Shaker: return &shaker;
    case ScreenId::Safe: return &safe;
    case ScreenId::Workout: return &workout;
    case ScreenId::Mole: return &mole;
    case ScreenId::Mouse: return &mouse;
    default: return &controller;
  }
}

}  // namespace cc
