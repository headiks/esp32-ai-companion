// Chance toys, motion games in the spirit of Switch party games, and the phone controller screen.
#include "companion/games.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace cc {
using namespace theme;

static constexpr int kCx = kScreenW / 2, kCy = kScreenH / 2;

// ---- 26. coin -------------------------------------------------------------------------------------
class CoinScreen : public Screen {
 public:
  void enter(App&) override { t_ = 1; heads_ = tails_ = 0; }
  void tick(App& a, float dt) override {
    if (t_ >= 1) return;
    t_ += dt / 1500;
    if (t_ >= 1) {
      (result_ ? heads_ : tails_)++;
      a.sfx.play("games/bounce");
      a.react(result_ ? Emotion::Happy : Emotion::Skeptical, 1200);
    }
  }
  void draw(App& a, Canvas& c) override {
    miniFace(c, a, CX, 40 + 60 * 176 / 354.f, 120);
    float p = clampf(t_, 0, 1), ease = 1 - (1 - p) * (1 - p) * (1 - p);
    // The whole number of half-turns decides which side lands up; turning is a squeeze by cos, no blur.
    float ang = p >= 1 ? fmodf(turns_, 2) * 3.14159f : ease * turns_ * 3.14159f;
    float w = fabsf(cosf(ang)), y = 200 - sinf(p * 3.14159f) * 60;
    ui::ellipse(c, CX, y, 90 * w, 90, INK);
    ui::ellipse(c, CX, y, 85 * w, 85, SURFACE);
    ui::ellipse(c, CX, y, 74 * w, 74, INK_3);
    ui::ellipse(c, CX, y, 72 * w, 72, SURFACE);
    if (w > 0.7f) ui::textCentered(c, CX, y, cosf(ang) > 0 ? "Орёл" : "Решка", TXT_UNB34, INK);
    char b[16];
    snprintf(b, sizeof b, "%d : %d", heads_, tails_);
    ui::textCentered(c, CX, 318, b, NUM_S, INK);
    if (p >= 1) c.text(CX, 360, "встряхни или нажми", TXT_MIN, INK_2, Align::Center);
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
  bool result_ = false;
  int heads_ = 0, tails_ = 0;
};

// ---- 27. dice ---------------------------------------------------------------------------------------
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
    miniFace(c, a, CX, 48 + 60 * 176 / 354.f, 120);
    float wob = rollMs_ > 0 ? clampf(rollMs_ / kRoll, 0, 1) : 0, ts = a.nowMs() * 0.02f;
    for (int d = 0; d < 2; d++) {
      float x = CX + (d ? 74.f : -74.f) + wob * 12 * sinf(ts + d * 2), y = 180 + wob * 18 * sinf(ts * 1.4f + d);
      die(c, x - 58, y - 58, v_[d]);
    }
    char b[8];
    snprintf(b, sizeof b, "%d", v_[0] + v_[1]);
    ui::textCentered(c, CX, 300, b, NUM_L, rollMs_ > 0 ? INK_3 : INK);
    c.text(CX, 352, "встряхни", TXT_MIN, INK_2, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override { if (e.g == Gesture::Tap) roll(a); }
  bool onMotion(App& a, MotionEvent m) override { if (m != MotionEvent::Shake) return false; roll(a); return true; }

 private:
  static constexpr float kRoll = 1100;
  // 116 px, radius 26: a dark face with a bright outline, pips on the 23/49/75 grid.
  static void die(Canvas& c, float x, float y, int v) {
    c.roundRect(x, y, 116, 116, 26, INK);
    c.roundRect(x + 3, y + 3, 110, 110, 23, SURFACE);
    static const uint16_t pips[7] = {0, 0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};
    for (int i = 0; i < 9; i++) if ((pips[v] >> i) & 1) c.fillCircle(x + 32 + (i % 3) * 26, y + 32 + (i / 3) * 26, 9, INK);
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

// ---- 28. magic ball ---------------------------------------------------------------------------------
static const char* const kAnswers[20] = {
    "Бесспорно", "Предрешено", "Никаких сомнений", "Определённо да", "Можешь быть уверен",
    "Мне кажется — да", "Вероятнее всего", "Хорошие перспективы", "Знаки говорят — да", "Да",
    "Пока не ясно", "Спроси позже", "Лучше не сейчас", "Не могу сказать", "Сосредоточься и спроси",
    "Даже не думай", "Мой ответ — нет", "Источники говорят нет", "Перспективы так себе", "Весьма сомнительно"};

class MagicBallScreen : public Screen {
 public:
  void enter(App& a) override { shakeMs_ = 0; answer_ = -1; a.react(Emotion::Dreamy, 1500); }
  void tick(App& a, float dt) override {
    if (shakeMs_ <= 0) return;
    shakeMs_ -= dt;
    if (shakeMs_ <= 0) {
      answer_ = a.rng.range(0, 19);
      a.sfx.play("system/notify");
      a.react(answer_ < 10 ? Emotion::Winking : answer_ < 15 ? Emotion::Thinking : Emotion::Skeptical, 1800);
    }
  }
  void draw(App& a, Canvas& c) override {
    miniFace(c, a, CX, 62 + 45 * 176 / 354.f, 90);
    float k = shakeMs_ > 0 ? 8 : 0, ts = a.nowMs() * 0.04f;
    float x = CX + k * sinf(ts), y = 248 + k * 0.6f * sinf(ts * 1.3f);
    c.fillCircle(x, y, 130, SURFACE);
    c.ring(x, y, 129, 2, INK_3);
    if (answer_ < 0 || shakeMs_ > 0) ui::textCentered(c, x, y, "?", TXT_MENU_SEL, INK_3);
    else {
      c.text((int)x, (int)y - 62, "шар говорит", TXT_MIN, INK_2, Align::Center);
      const char* s = kAnswers[answer_];
      // Two lines at most: long answers drop to the smaller size.
      FontSize f = Canvas::textWidth(s, TXT_UNB34) > 2 * 220 ? TXT_UNB28 : TXT_UNB34;
      c.textWrapped((int)x, (int)y - 26, 220, s, f, INK, Align::Center, 2, Canvas::lineHeight(f) * 6 / 5, true);
    }
    c.text(CX, 394, "спроси и встряхни", TXT_MIN, INK_2, Align::Center);
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
  float shakeMs_ = 0;
  int answer_ = -1;
};

static void secondsText(char* out, int cap, float ms) {
  int tenths = (int)(ms / 100);
  snprintf(out, cap, "%d,%d", tenths / 10, tenths % 10);
}

// ---- 29. balance: keep the ball in the circle while a growing wind pushes it ----------------------
class BalanceScreen : public Screen {
 public:
  void enter(App& a) override { phase_ = Ready; ms_ = 0; a.react(Emotion::Focused, 1000); }
  bool capturesSwipes() const override { return phase_ == Run; }
  void tick(App& a, float dt) override {
    if (phase_ != Run) return;
    ms_ += dt;
    float h = dt * 0.001f, wind = 18 + ms_ * 0.004f, t = ms_ * 0.001f;
    float wx = (sinf(t * 0.9f + seed_) + 0.6f * sinf(t * 2.3f)) * wind, wy = (sinf(t * 1.1f + seed_ * 2) + 0.6f * sinf(t * 1.9f)) * wind;
    vx_ += (a.hal.ax * 420 + wx - vx_ * 0.9f) * h;
    vy_ += (a.hal.ay * 420 + wy - vy_ * 0.9f) * h;
    x_ += vx_ * h;
    y_ += vy_ * h;
    a.anim.lookOffset(x_ * 0.2f, y_ * 0.15f, 120);
    if (sqrtf(x_ * x_ + y_ * y_) > kLimit) {
      phase_ = Over;
      int secs = (int)(ms_ / 1000);
      newRecord_ = secs > a.settings.balanceBest;
      if (newRecord_) { a.settings.balanceBest = (uint16_t)secs; a.saveSettings(); }
      a.pet.event(newRecord_ ? PetEvent::GameWin : PetEvent::GameLose);
      a.sfx.play(newRecord_ ? "games/win" : "games/wrong");
      a.react(newRecord_ ? Emotion::Proud : Emotion::Dizzy, 1800);
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[24], rec[32] = "";
    if (a.settings.balanceBest) snprintf(rec, sizeof rec, "рекорд %d с", a.settings.balanceBest);
    if (phase_ == Ready) { gameStart(c, a, "Баланс", "Держи шар в круге, ветер мешает", rec); return; }
    secondsText(b, sizeof b, ms_);
    if (phase_ == Over) { gameResult(c, a, b, newRecord_ ? "с · новый рекорд" : "с", newRecord_ ? Emotion::Proud : Emotion::Dizzy); return; }
    c.ring(CX, CY, 190, 3, INK_3);
    c.ring(CX, CY, 42, 4, ACCENT);
    c.fillCircle(CX + x_, CY + y_, 22, INK);
    int w1 = Canvas::textWidth(b, NUM_M40), w2 = Canvas::textWidth(" с", TXT_MIN), x = CX - (w1 + w2) / 2;
    int base = 72 + Canvas::capBottom(NUM_M40);
    c.text(x, 72, b, NUM_M40, INK);
    c.text(x + w1, base - Canvas::capBottom(TXT_MIN), " с", TXT_MIN, INK_2);
    if (ms_ < HINT_MS) c.text(CX, 300, "держи шар в круге", TXT_MIN, INK_2, Align::Center);
    miniFace(c, a, CX, 350 + 50 * 176 / 354.f, 100);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || phase_ == Run) return;
    if (phase_ == Over && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
    if ((phase_ == Ready && kStartButton.hit(e.x, e.y)) || (phase_ == Over && kAgainButton.hit(e.x, e.y))) {
      phase_ = Run; ms_ = 0; x_ = y_ = vx_ = vy_ = 0; seed_ = a.rng.rangef(0, 6); newRecord_ = false;
      a.sfx.play("games/go");
    }
  }

 private:
  static constexpr float kLimit = 190 - 22;  // the ball touches the border
  enum Phase { Ready, Run, Over } phase_ = Ready;
  float ms_ = 0, x_ = 0, y_ = 0, vx_ = 0, vy_ = 0, seed_ = 0;
  bool newRecord_ = false;
};

// ---- 30. duel: hold still, at the signal swing first. The pet has its own reaction time. ------------
class DuelScreen : public Screen {
 public:
  void enter(App& a) override { st_ = Idle; a.react(Emotion::Determined, 1200); }
  void tick(App& a, float dt) override {
    float dev = fabsf(a.motionMgr.lastMagnitude() - 1.f);
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
      case Idle:
        a.face.drawMini(c, Emotion::Determined, CX - 140, 70, 280, INK);
        ui::textCentered(c, CX, 240, "Дуэль", TXT_MENU_SEL, INK);
        ui::textCentered(c, CX, 288, "возьми в руку, жди сигнал", TXT_MIN, INK_2);
        kReady.draw(c, "Готов");
        return;
      case Steady:
      case Wait:
      case Draw: {
        bool lit = st_ == Draw;  // the ring lights up, the screen is never flooded white
        c.ring(CX, CY, 212, 16, lit ? ACCENT : TRACK_2);
        a.face.drawMini(c, Emotion::Determined, CX - 100, 112, 200, INK);
        ui::textCentered(c, CX, 250, lit ? "Рывок!" : st_ == Steady ? "Держи ровно" : "Жди…", TXT_MENU_SEL, lit ? ACCENT : INK);
        if (!lit) ui::textCentered(c, CX, 300, st_ == Steady ? "не двигайся" : "кольцо загорится", TXT_MIN, INK_2);
        return;
      }
      case Foul: gameResult(c, a, "Рано!", "двинулся до сигнала", Emotion::Smug); return;
      case Won:
        snprintf(b, sizeof b, "%d", last_);
        gameResult(c, a, b, "мс · ты первый", Emotion::Terrified);
        return;
      case Lost:
        if (last_) { snprintf(b, sizeof b, "%d", last_); gameResult(c, a, b, "мс · Питомец быстрее", Emotion::Proud); }
        else gameResult(c, a, "Проспал", "Питомец быстрее", Emotion::Proud);
        return;
    }
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || st_ == Steady || st_ == Wait || st_ == Draw) return;
    if (st_ != Idle && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
    if ((st_ == Idle && kReady.hit(e.x, e.y)) || (st_ != Idle && kAgainButton.hit(e.x, e.y))) {
      st_ = Steady;
      steadyMs_ = 0;
      a.sfx.play("system/click");
    }
  }
  bool capturesSwipes() const override { return st_ == Steady || st_ == Wait || st_ == Draw; }
  // The duel reads the raw movement itself: a swing here must not become a pet reaction.
  bool onMotion(App&, MotionEvent m) override { return m == MotionEvent::Shake || m == MotionEvent::Jerk; }

 private:
  const ui::Button kReady = ui::primary(CX, 330);
  enum State { Idle, Steady, Wait, Draw, Foul, Won, Lost } st_ = Idle;
  float steadyMs_ = 0, waitMs_ = 0, drawMs_ = 0, petMs_ = 0;
  int last_ = 0;
};

// ---- 31. shaker: as many shakes as possible in ten seconds ------------------------------------------
class ShakerScreen : public Screen {
 public:
  void enter(App&) override { st_ = Idle; count_ = 0; }
  void tick(App& a, float dt) override {
    if (st_ != Run) return;
    left_ -= dt;
    float mag = a.motionMgr.lastMagnitude();
    if (armed_ && mag > 1.5f) { armed_ = false; count_++; a.sfx.play("games/move"); a.anim.kick(-0.12f); if (count_ % 10 == 0) a.react(Emotion::Excited, 700); }
    if (!armed_ && mag < 1.2f) armed_ = true;
    if (left_ <= 0) {
      st_ = Done;
      newRecord_ = count_ > a.settings.shakerBest;
      if (newRecord_) { a.settings.shakerBest = (uint16_t)count_; a.saveSettings(); }
      a.pet.event(PetEvent::Play);
      a.sfx.play(newRecord_ ? "games/win" : "games/draw");
      a.react(newRecord_ ? Emotion::Starstruck : Emotion::Happy, 2000);
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[32], rec[32] = "";
    if (a.settings.shakerBest) snprintf(rec, sizeof rec, "рекорд %d", a.settings.shakerBest);
    if (st_ == Idle) { gameStart(c, a, "Тряска", "Тряси устройство десять секунд", rec); return; }
    snprintf(b, sizeof b, "%d", count_);
    if (st_ == Done) { gameResult(c, a, b, newRecord_ ? "новый рекорд" : rec, newRecord_ ? Emotion::Starstruck : Emotion::Happy); return; }
    ui::progressRing(c, 214, 10, left_ / 10000);
    miniFace(c, a, CX, 84, 150);
    ui::textCentered(c, CX, 186, b, NUM_XXL, INK);
    snprintf(b, sizeof b, "Тряси! %d с", (int)(left_ / 1000) + 1);
    ui::textCentered(c, CX, 296, b, TXT_BODY, ACCENT);
    if (rec[0]) ui::textCentered(c, CX, 334, rec, TXT_MIN, INK_2);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || st_ == Run) return;
    if (st_ == Done && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
    if ((st_ == Idle && kStartButton.hit(e.x, e.y)) || (st_ == Done && kAgainButton.hit(e.x, e.y))) {
      st_ = Run; count_ = 0; left_ = 10000; armed_ = true; newRecord_ = false;
      a.sfx.play("games/go");
    }
  }
  bool capturesSwipes() const override { return st_ == Run; }
  bool onMotion(App&, MotionEvent m) override { return st_ == Run && (m == MotionEvent::Shake || m == MotionEvent::Jerk); }

 private:
  enum State { Idle, Run, Done } st_ = Idle;
  int count_ = 0;
  float left_ = 0;
  bool armed_ = true, newRecord_ = false;
};

// ---- 32. safe: turn the device like a dial. The right spot clicks; a tap locks it. -----------------
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
      // Louder near the secret: the sound is the whole clue, as with a real safe. The click itself startles.
      if (near() < 8) a.sfx.play("games/move");
      if (near() < 3) a.react(Emotion::Surprised, 300);
    }
    if (near() < 3 && !opened()) a.anim.lookOffset(0, -8, 120);
    if (openMs_ > 0) { openMs_ -= dt; if (openMs_ <= 0) start(a); }
  }
  void draw(App& a, Canvas& c) override {
    // The whole scale turns with the device; the pointer on top stays.
    for (int i = 0; i < 60; i++) {
      float ang = (i * 6 + dial_) * 0.0174533f, s = sinf(ang), co = -cosf(ang);
      bool major = i % 5 == 0;
      float r1 = major ? 203.f : 200.f, r0 = r1 - (major ? 22 : 14);
      c.line(CX + s * r0, CY + co * r0, CX + s * r1, CY + co * r1, major ? 4 : 2, major ? INK_2 : INK_3);
    }
    c.roundRect(CX - 4, 12, 8, 34, 4, ACCENT);
    miniFace(c, a, CX, 130 + 75 * 176 / 354.f, 150);
    for (int i = 0; i < 3; i++) {
      float x = CX - 44 + i * 44;
      if (i < stage_) c.fillCircle(x, 241, 13, INK);
      else c.ring(x, 241, 12, 2, INK_3);
    }
    ui::textCentered(c, CX, 292, opened() ? "Открыто!" : "крути и жди щелчка", TXT_MIN, opened() ? ACCENT : INK_2);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap || opened()) return;
    if (near() < 5) {
      a.sfx.play("games/correct");
      if (++stage_ == 3) {
        a.sfx.play("games/win");
        a.pet.event(PetEvent::GameWin);
        a.react(Emotion::Starstruck, 1800, EmotionPriority::Strong);  // stronger than the click's short surprise
        openMs_ = 2000;
      } else { secret_ = a.rng.rangef(0, 360); a.react(Emotion::Sneaky, 900, EmotionPriority::Strong); }
    } else {
      a.sfx.play("games/wrong");
      a.react(Emotion::Suspicious, 800);
    }
  }

 private:
  bool opened() const { return stage_ >= 3; }
  float near() const { return fabsf(fmodf(dial_ - secret_ + 540.f, 360.f) - 180.f); }
  void start(App& a) { stage_ = 0; secret_ = a.rng.rangef(0, 360); openMs_ = 0; a.react(Emotion::Focused, 1200); }
  float dial_ = 0, secret_ = 0, openMs_ = 0;
  int stage_ = 0, lastTick_ = 0;
};

// ---- 33. workout: the device in hand counts repetitions from acceleration peaks ---------------------
struct Exercise { const char* name; int reps; };
static const Exercise kExercises[] = {{"Прыжки", 10}, {"Приседания", 10}, {"Махи руками", 12}, {"Шаги на месте", 20}};
static constexpr int kExerciseCount = sizeof(kExercises) / sizeof(kExercises[0]);

class WorkoutScreen : public Screen {
 public:
  void enter(App& a) override { i_ = 0; reps_ = 0; finished_ = false; a.react(Emotion::Determined, 1200); }
  void tick(App& a, float dt) override {
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
    if (finished_) {
      char b[8];
      snprintf(b, sizeof b, "%d", kExerciseCount);
      gameResult(c, a, b, "упражнения · отлично!", Emotion::Proud);
      return;
    }
    ui::progressRing(c, 214, 10, (float)reps_ / kExercises[i_].reps);
    ui::textCentered(c, CX, 99, kExercises[i_].name, TXT_TITLE, INK);
    char b[48];
    snprintf(b, sizeof b, "%d", reps_);
    ui::textCentered(c, CX, 182, b, NUM_XXL, INK);
    snprintf(b, sizeof b, "из %d · упражнение %d из %d", kExercises[i_].reps, i_ + 1, kExerciseCount);
    ui::textCentered(c, CX, 274, b, TXT_CAP, INK_2);
    skip_.draw(c, "Пропустить");
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (finished_) {
      if (kExitButton.hit(e.x, e.y)) a.goBack();
      else if (kAgainButton.hit(e.x, e.y)) enter(a);
      return;
    }
    if (skip_.hit(e.x, e.y)) next(a);
  }
  bool onMotion(App&, MotionEvent m) override { return !finished_ && (m == MotionEvent::Shake || m == MotionEvent::Jerk || m == MotionEvent::Lifted); }

 private:
  void next(App& a) {
    i_++;
    reps_ = 0;
    a.sfx.play("games/correct");
    if (i_ >= kExerciseCount) {
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
  const ui::Button skip_ = ui::pill(CX, 318, 180);
  int i_ = 0, reps_ = 0;
  float refract_ = 0;
  bool armed_ = true, finished_ = false;
};

// ---- 34. mole: the character peeks out of holes around the rim --------------------------------------
class MoleScreen : public Screen {
 public:
  void enter(App&) override { phase_ = Ready; score_ = 0; }
  bool capturesSwipes() const override { return phase_ == Run; }
  void tick(App& a, float dt) override {
    if (phase_ != Run) return;
    left_ -= dt;
    upMs_ -= dt;
    if (upMs_ <= 0) {
      if (at_ >= 0) a.sfx.play("games/move");
      int was = at_;
      do at_ = a.rng.range(0, kHoles - 1); while (at_ == was);
      lifeMs_ = upMs_ = lerpf(1100, 480, clampf(score_ / 30.f, 0, 1));
    }
    if (left_ <= 0) {
      phase_ = Over;
      newRecord_ = score_ > a.settings.moleBest;
      if (newRecord_) { a.settings.moleBest = (uint16_t)score_; a.saveSettings(); }
      a.pet.event(newRecord_ ? PetEvent::GameWin : PetEvent::Play);
      a.sfx.play(newRecord_ ? "games/win" : "games/draw");
      a.react(newRecord_ ? Emotion::Proud : Emotion::Happy, 1800);
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[24], rec[32] = "";
    if (a.settings.moleBest) snprintf(rec, sizeof rec, "рекорд %d", a.settings.moleBest);
    if (phase_ == Ready) { gameStart(c, a, "Крот", "Тапни, когда он выглянет из норки", rec); return; }
    snprintf(b, sizeof b, "%d", score_);
    if (phase_ == Over) { gameResult(c, a, b, newRecord_ ? "новый рекорд" : rec, newRecord_ ? Emotion::Proud : Emotion::Happy); return; }
    for (int i = 0; i < kHoles; i++) {
      float x, y;
      pos(i, &x, &y);
      if (i == at_) {
        float pop = smooth01((lifeMs_ - upMs_) / 120) * smooth01(upMs_ / 100);
        c.fillCircle(x, y, 38, SURFACE);
        c.ring(x, y, 36.5f, 3, ACCENT);
        if (pop > 0.05f) a.face.drawMini(c, Emotion::Mischievous, x - 29 * pop, y - 14 * pop, 58 * pop, INK);
      } else c.ring(x, y, 37, 2, INK_3);
    }
    ui::textCentered(c, CX, CY - 8, b, NUM_L56, INK);
    int s = (int)(left_ / 1000) + 1;
    snprintf(b, sizeof b, "0:%02d", s);
    ui::textCentered(c, CX, CY + 40, b, TXT_MIN, INK_2);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (phase_ != Run) {
      if (phase_ == Over && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
      if ((phase_ == Ready && kStartButton.hit(e.x, e.y)) || (phase_ == Over && kAgainButton.hit(e.x, e.y))) {
        phase_ = Run; score_ = 0; left_ = 25000; at_ = -1; upMs_ = 400; lifeMs_ = 400; newRecord_ = false;
        a.sfx.play("games/go");
      }
      return;
    }
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
  static void pos(int i, float* x, float* y) { float a = i * 0.7854f; *x = CX + sinf(a) * 150; *y = CY - cosf(a) * 150; }
  enum Phase { Ready, Run, Over } phase_ = Ready;
  int score_ = 0, at_ = -1;
  float left_ = 0, upMs_ = 0, lifeMs_ = 1;
  bool newRecord_ = false;
};

// ---- 35. twenty-one, played entirely on the device; the pet is the dealer -----------------------------
class BlackjackScreen : public Screen {
 public:
  void enter(App& a) override { if (g_.phase() == BlackjackGame::Phase::Done) start(a); }
  void tick(App& a, float dt) override {
    if (g_.phase() != BlackjackGame::Phase::Dealer) return;
    stepMs_ -= dt;
    if (stepMs_ > 0) return;
    stepMs_ = 650;  // the dealer turns cards one at a time, the pause is the tension
    if (g_.dealerStep()) { a.sfx.play("games/move"); return; }
    finish(a);
  }
  void draw(App& a, Canvas& c) override {
    bool hide = g_.phase() == BlackjackGame::Phase::Player;
    char b[40];
    miniFace(c, a, CX, 38 + 65 * 176 / 354.f, 130);
    // Dealer above, small; the second card stays face down until the player stands.
    float right = row(c, 112, 50, 68, g_.dealerCount(), false, hide ? 1 : -1);
    if (!hide) {
      snprintf(b, sizeof b, "%d", g_.dealerValue());
      c.text((int)right + 14, 112 + 34 - Canvas::capMiddle(TXT_MIN), b, TXT_MIN, INK_2);
    }
    right = row(c, 202, 82, 114, g_.playerCount(), true, -1);
    snprintf(b, sizeof b, "%d", g_.playerValue());
    c.text((int)right + 16, 202 + 57 - Canvas::capMiddle(NUM_M40), b, NUM_M40, g_.playerValue() > 21 ? ACCENT_2 : INK);
    switch (g_.phase()) {
      case BlackjackGame::Phase::Player:
        hit_.draw(c, "Ещё");
        stand_.draw(c, "Хватит");
        break;
      case BlackjackGame::Phase::Dealer:
        ui::textCentered(c, CX, 356, "дилер ходит…", TXT_MIN, INK_2);
        break;
      case BlackjackGame::Phase::Done:
        ui::textCentered(c, CX, 334, result(), TXT_CAP, g_.outcome() > 0 ? ACCENT : g_.outcome() < 0 ? ACCENT_2 : INK_2);
        snprintf(b, sizeof b, g_.broke() ? "фишки кончились · нажми" : "фишки %d · нажми", g_.chips());
        ui::textCentered(c, CX, 368, b, TXT_MIN, INK_2);
        break;
    }
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g == Gesture::SwipeUp && g_.phase() == BlackjackGame::Phase::Player) { stand(a); return; }
    if (e.g != Gesture::Tap) return;
    switch (g_.phase()) {
      case BlackjackGame::Phase::Done: start(a); break;
      case BlackjackGame::Phase::Player:
        if (hit_.hit(e.x, e.y)) {
          g_.hit();
          a.sfx.play("games/move");
          if (g_.phase() == BlackjackGame::Phase::Done) finish(a);
          else if (g_.phase() == BlackjackGame::Phase::Dealer) stepMs_ = 500;
          else if (g_.playerValue() >= 17) a.react(Emotion::Thinking, 900);
        } else if (stand_.hit(e.x, e.y)) stand(a);
        break;
      default: break;
    }
  }
  bool capturesSwipes() const override { return g_.phase() == BlackjackGame::Phase::Player; }

 private:
  static void label(const BlackjackGame::Card& k, char* out) {
    static const char suits[] = "shdc";
    const char* names[] = {"J", "Q", "K", "A"};
    if (k.rank <= 10) snprintf(out, 6, "%d%c", k.rank, suits[k.suit]);
    else snprintf(out, 6, "%s%c", names[k.rank - 11], suits[k.suit]);
  }
  // A row centred a little left of the middle, the total beside it. Cards close up as the hand grows.
  // Returns the right edge of the last card.
  float row(Canvas& c, float top, float w, float h, int n, bool player, int hidden) {
    float step = n > 1 ? fminf(w + 8, 200.f / (n - 1)) : 0;
    float x0 = CX - 24 - (step * (n - 1) + w) / 2;
    char l[6];
    for (int i = 0; i < n; i++) {
      label(player ? g_.playerCard(i) : g_.dealerCard(i), l);
      ui::card(c, x0 + i * step, top, w, h, l, i != hidden, true, player ? 3 : 2);
    }
    return x0 + step * (n - 1) + w;
  }
  const char* result() const {
    if (g_.natural() && g_.outcome() > 0) return "Двадцать одно!";
    if (g_.playerValue() > 21) return "Перебор";
    return g_.outcome() > 0 ? "Победа" : g_.outcome() < 0 ? "Дилер выиграл" : "Ничья";
  }
  void start(App& a) {
    if (g_.chips() < BlackjackGame::kBet) g_.resetChips();
    g_.deal(a.rng);
    a.sfx.play("games/go");
    a.react(Emotion::Suspicious, 900);
    stepMs_ = 700;
  }
  void stand(App& a) {
    g_.stand();
    stepMs_ = 500;
    a.sfx.play("system/click");
    a.react(Emotion::Attentive, 1000);
  }
  void finish(App& a) {
    int o = g_.outcome();
    a.pet.event(o > 0 ? PetEvent::GameLose : o < 0 ? PetEvent::GameWin : PetEvent::Play);  // the pet plays the dealer's side
    a.sfx.play(o > 0 ? "games/win" : o < 0 ? "games/lose" : "games/draw");
    a.react(o > 0 ? Emotion::Amazed : o < 0 ? Emotion::Smug : Emotion::Skeptical, 2000, EmotionPriority::Strong);
  }
  const ui::Button hit_ = ui::primary(180, 338);
  const ui::Button stand_ = ui::secondary(290, 348);
  BlackjackGame g_;
  float stepMs_ = 0;
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
  // Sizes follow the card: the rank takes the top part, the suit fits in what is left, so they never touch.
  FontSize f = h >= 80 ? FontSize::M : FontSize::S;
  c.text((int)x, (int)(y - h / 2 + h * 0.05f), rank, f, col, Align::Center);
  float r = h * 0.17f < w * 0.26f ? h * 0.17f : w * 0.26f;
  suitShape(c, suit, x, y + h * 0.22f, r, col);
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
    if (!cc.cards || cc.count <= 2) return;  // one or two cards are shown whole: there is nothing to choose between
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
    c.text(kCx, 28, cc.title, FontSize::S, kGray, Align::Center);
    if (cc.cards) drawHand(c, cc);
    else drawPad(a, c);
    if (cc.hint[0]) c.textWrapped(kCx, kCy + 130, 350, cc.hint, FontSize::S, kGray, Align::Center, 2);
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
    if (cc.info[0]) c.text(kCx, 50, cc.info, FontSize::S, kSoft, Align::Center);
    if (cc.count <= 0) { c.text(kCx, kCy - 20, "Нет карт", FontSize::M, kGray, Align::Center); return; }
    if (cc.count <= 2) {
      for (int i = 0; i < cc.count; i++) drawCard(c, kCx + (cc.count == 2 ? (i ? 58.f : -58.f) : 0.f), kCy - 24, 104, 146, cc.labels[i], true, false);
      return;
    }
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
  static MoleScreen mole; static ControllerScreen controller; static MouseScreen mouse; static BlackjackScreen blackjack;
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
    case ScreenId::Blackjack: return &blackjack;
    default: return &controller;
  }
}

}  // namespace cc
