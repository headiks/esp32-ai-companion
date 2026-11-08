// Design 1a: maze (19), orbit (20), snake (21) and Simon, "Повтори" (22).
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "companion/games.h"
#include "ui.h"

namespace cc {
using namespace theme;

static void record(char* out, int cap, int best, const char* unit = "") {
  if (best > 0) snprintf(out, cap, "рекорд %d%s", best, unit);
  else out[0] = 0;
}

// ---- 19. maze: the board is a 300 px square, the ball rolls with the device; levels never end --------
class MazeScreen : public Screen {
 public:
  void enter(App& a) override {
    level_ = a.settings.mazeLevel;
    start(a);
  }
  bool capturesSwipes() const override { return !m_.won(); }
  void tick(App& a, float dt) override {
    if (m_.won()) {
      winMs_ += dt;
      if (winMs_ > 1800) {
        level_++;
        if (level_ > a.settings.mazeLevel) { a.settings.mazeLevel = (uint16_t)level_; a.saveSettings(); }
        start(a);
      }
      return;
    }
    float was = m_.speed();
    m_.step(a.hal.ax, a.hal.ay, dt);
    // A wall hit is a sudden loss of speed: the character winces and the board clicks.
    if (was - m_.speed() > 1.6f && cooldown_ <= 0) { a.sfx.play("games/bounce"); a.anim.kick(-0.1f); cooldown_ = 180; }
    if (cooldown_ > 0) cooldown_ -= dt;
    float sc = cell();
    a.anim.lookAt(kX0 + m_.x() * sc, kY0 + m_.y() * sc, 200);
    if (m_.won()) {
      winMs_ = 0;
      a.sfx.play("games/win");
      a.pet.event(PetEvent::GameWin);
      a.react(Emotion::VeryHappy, 1700);
    }
  }
  void draw(App& a, Canvas& c) override {
    const float sc = cell();
    const int n = m_.size();
    char b[24];
    snprintf(b, sizeof b, "уровень %d", level_ + 1);
    c.text(CX, 36, b, TXT_MIN, INK_2, Align::Center);
    // Walls: 5 px with round ends, drawn only in the strips they cross.
    for (int y = 0; y <= n; y++) {
      float py = kY0 + y * sc;
      if (!c.intersects(0, (int)py - (int)sc - 4, kScreenW, (int)sc + 8)) continue;
      for (int x = 0; x <= n; x++) {
        float px = kX0 + x * sc;
        bool north = x < n && (y == n || m_.wall(x, y, MazeGame::N));
        bool west = y < n && (x == n || m_.wall(x, y, MazeGame::W));
        if (north) c.line(px, py, px + sc, py, 5, INK_2);
        if (west) c.line(px, py, px, py + sc, 5, INK_2);
      }
    }
    float goalR = fminf(16, sc * 0.4f), ballR = fminf(17, sc * MazeGame::kBall);
    c.ring(kX0 + (m_.goalX() + 0.5f) * sc, kY0 + (m_.goalY() + 0.5f) * sc, goalR, 5, ACCENT);
    c.fillCircle(kX0 + m_.x() * sc, kY0 + m_.y() * sc, ballR, INK);
    miniFace(c, a, CX, 388 + 40 * 176 / 354.f, 80);
    if (m_.won()) ui::textCentered(c, CX, CY, "Выход!", TXT_MENU_SEL, ACCENT);
  }

 private:
  static constexpr float kX0 = 83, kY0 = 83;
  // Every level is a new maze from a new start. The board grows to 13x13; past that the levels go on,
  // with more dead ends and a livelier ball each time.
  void start(App& a) {
    int beyond = 5 + level_ - MazeGame::kMax;
    float hard = beyond > 0 ? (float)beyond : 0;
    m_.generate(clampi(5 + level_, 5, MazeGame::kMax), a.rng, fminf(0.06f * hard, 0.6f));
    m_.setPace(1 + fminf(0.03f * hard, 0.45f));
    winMs_ = 0;
    a.react(Emotion::Determined, 1500);
  }
  float cell() const { return 300.f / m_.size(); }
  MazeGame m_;
  int level_ = 0;
  float winMs_ = 0, cooldown_ = 0;
};

// ---- 20. orbit: a paddle on the rim keeps the ball inside; tilt or a finger moves it -----------------
class OrbitScreen : public Screen {
 public:
  void enter(App& a) override { phase_ = Ready; steering_ = false; a.react(Emotion::Curious, 1200); }
  // The finger that steered until the miss is still a steering finger: lifting it is not a swipe away.
  bool capturesSwipes() const override { return phase_ == Run || steering_; }
  void tick(App& a, float dt) override {
    if (phase_ != Run) return;
    runMs_ += dt;
    // The device's lean points where the paddle should be, unless a finger is steering.
    float tilt = sqrtf(a.hal.ax * a.hal.ax + a.hal.ay * a.hal.ay);
    if (!a.touch.isDown() && tilt > 0.12f) {
      float target = atan2f(a.hal.ax, -a.hal.ay);
      paddle_ += angleDiff(target, paddle_) * clampf(dt / 90.f, 0, 1);
    }
    float h = dt * 0.001f;
    bx_ += vx_ * h;
    by_ += vy_ * h;
    a.anim.lookOffset(bx_ * 0.2f, by_ * 0.16f, 150);
    float d = sqrtf(bx_ * bx_ + by_ * by_);
    if (d <= kBounceR) return;
    float nx = bx_ / d, ny = by_ / d, off = angleDiff(atan2f(nx, -ny), paddle_);
    if (fabsf(off) < halfWidth()) {
      // Reflect about the rim normal; where the ball meets the paddle steers it, as in the old game.
      float dot = vx_ * nx + vy_ * ny;
      vx_ -= 2 * dot * nx;
      vy_ -= 2 * dot * ny;
      float sp = fminf(sqrtf(vx_ * vx_ + vy_ * vy_) * 1.03f, 460), dir = atan2f(vy_, vx_) + off * 0.9f;
      vx_ = cosf(dir) * sp;
      vy_ = sinf(dir) * sp;
      bx_ = nx * (kBounceR - 1);
      by_ = ny * (kBounceR - 1);
      score_++;
      a.sfx.play("games/bounce");
      a.anim.kick(-0.14f);
      if (score_ % 5 == 0) a.react(Emotion::Excited, 900);
      else if (a.rng.range(0, 3) == 0) a.react(Emotion::Happy, 700);
    } else {
      phase_ = Over;
      steering_ = a.touch.isDown();
      newRecord_ = score_ > a.settings.orbitBest;
      if (newRecord_) { a.settings.orbitBest = (uint16_t)score_; a.saveSettings(); }
      a.pet.event(newRecord_ ? PetEvent::GameWin : PetEvent::GameLose);
      a.sfx.play(newRecord_ ? "games/win" : "games/lose");
      a.react(newRecord_ ? Emotion::Proud : Emotion::Sad, 2000);
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[24], rec[32];
    record(rec, sizeof rec, a.settings.orbitBest);
    if (phase_ == Ready) { gameStart(c, a, "Орбита", "Наклоняй, чтобы ракетка отбивала шарик", rec); return; }
    if (phase_ == Over) {
      snprintf(b, sizeof b, "%d", score_);
      gameResult(c, a, b, newRecord_ ? "новый рекорд" : rec, newRecord_ ? Emotion::Starstruck : Emotion::Sad);
      return;
    }
    // Track and paddle live on the rim; the paddle narrows as the score grows.
    c.ring(CX, CY, kPaddleR, 2, TRACK);
    float deg = paddle_ * 57.29578f, hw = halfWidth() * 57.29578f;
    c.arc(CX, CY, kPaddleR, kPaddleW, deg - hw, deg + hw, ACCENT, true);
    miniFace(c, a, CX, CY - 40, 120);
    snprintf(b, sizeof b, "%d", score_);
    ui::textCentered(c, CX, CY + 46, b, NUM_L56, INK);
    if (runMs_ < HINT_MS) c.text(CX, CY + 92, "наклон или палец — ракетка", TXT_MIN, INK_2, Align::Center);
    c.fillCircle(CX + bx_, CY + by_, kBallR, INK);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (phase_ == Run) {
      if (e.g == Gesture::Move || e.g == Gesture::Down) paddle_ = atan2f((float)(e.x - CX), (float)(CY - e.y));
      return;
    }
    if (steering_) {
      if (e.g != Gesture::Down) return;
      steering_ = false;
    }
    if (e.g != Gesture::Tap) return;
    if (phase_ == Over && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
    if ((phase_ == Ready && kStartButton.hit(e.x, e.y)) || (phase_ == Over && kAgainButton.hit(e.x, e.y))) start(a);
  }

 private:
  static constexpr float kPaddleR = 208, kPaddleW = 12, kBallR = 10, kBounceR = kPaddleR - kPaddleW / 2 - kBallR;
  static float angleDiff(float a, float b) { return fmodf(a - b + 9.42478f, 6.28319f) - 3.14159f; }
  float halfWidth() const { return fmaxf(0.62f - score_ * 0.012f, 0.26f); }  // radians, never below playable
  void start(App& a) {
    phase_ = Run;
    score_ = 0;
    runMs_ = 0;
    newRecord_ = false;
    bx_ = by_ = 0;
    float dir = a.rng.rangef(0, 6.28319f);
    vx_ = cosf(dir) * 170;
    vy_ = sinf(dir) * 170;
    a.sfx.play("games/go");
    a.react(Emotion::Focused, 1500);
  }
  enum Phase { Ready, Run, Over } phase_ = Ready;
  float paddle_ = 0, bx_ = 0, by_ = 0, vx_ = 0, vy_ = 0, runMs_ = 0;
  int score_ = 0;
  bool newRecord_ = false, steering_ = false;
};

// ---- 21. snake: swipes turn it --------------------------------------------------------------------
class SnakeScreen : public Screen {
 public:
  void enter(App& a) override { phase_ = Ready; g_.reset(a.rng); stepMs_ = 0; a.react(Emotion::Mischievous, 1500); }
  bool capturesSwipes() const override { return phase_ == Run; }
  void tick(App& a, float dt) override {
    if (phase_ != Run) return;
    runMs_ += dt;
    stepMs_ += dt;
    float period = lerpf(230, 90, clampf(g_.score() / 40.f, 0, 1));
    if (stepMs_ < period) return;
    stepMs_ = 0;
    int was = g_.length();
    if (!g_.step(a.rng)) {
      phase_ = Over;
      newRecord_ = g_.score() > a.settings.snakeBest;
      if (newRecord_) { a.settings.snakeBest = (uint16_t)g_.score(); a.saveSettings(); }
      a.pet.event(newRecord_ ? PetEvent::GameWin : PetEvent::GameLose);
      a.sfx.play(newRecord_ ? "games/win" : "games/lose");
      a.react(newRecord_ ? Emotion::Proud : Emotion::Dizzy, 2200);
    } else if (g_.length() > was) {
      a.sfx.play("games/correct");
      a.pet.event(PetEvent::Feed);
      a.react(Emotion::Happy, 700);
    }
    a.anim.lookAt(kX0 + g_.foodX() * kCell, kY0 + g_.foodY() * kCell, 300);
  }
  void draw(App& a, Canvas& c) override {
    char b[24], rec[32];
    record(rec, sizeof rec, a.settings.snakeBest);
    if (phase_ == Ready) { gameStart(c, a, "Змейка", "Свайпы поворачивают змейку", rec); return; }
    if (phase_ == Over) {
      snprintf(b, sizeof b, "%d", g_.score());
      gameResult(c, a, b, newRecord_ ? "новый рекорд" : rec, newRecord_ ? Emotion::Proud : Emotion::Dizzy);
      return;
    }
    // The field: a dot in every cell and a clear frame. The snake dies on the frame, so it must read at once;
    // the spec's dots over the whole screen hid where the field ends.
    constexpr float kSide = SnakeGame::kSize * kCell;
    c.roundRectOutline(kX0 - 6, kY0 - 6, kSide + 12, kSide + 12, 12, 2, INK_2);
    for (int y = 0; y < SnakeGame::kSize; y++) {
      float py = kY0 + y * kCell + kCell / 2.f;
      if (!c.intersects(0, (int)py - 2, kScreenW, 4)) continue;
      for (int x = 0; x < SnakeGame::kSize; x++) c.fillCircle(kX0 + x * kCell + kCell / 2.f, py, 1.6f, SNAKE_GRID);
    }
    c.fillCircle(kX0 + g_.foodX() * kCell + kCell / 2.f, kY0 + g_.foodY() * kCell + kCell / 2.f, 7, ACCENT);
    int n = g_.length();
    for (int i = n - 1; i >= 0; i--) {
      // The tail fades from INK to a warm grey: the direction reads without anything else.
      float t = n > 1 ? (float)i / (n - 1) : 0;
      uint16_t col = blend565(INK, SNAKE_TAIL, (uint32_t)(255 * t));
      c.roundRect(kX0 + g_.segX(i) * kCell, kY0 + g_.segY(i) * kCell, kCell, kCell, 4, col);
    }
    // Eyes on the head, looking where it goes.
    int dx = n > 1 ? g_.segX(0) - g_.segX(1) : 1, dy = n > 1 ? g_.segY(0) - g_.segY(1) : 0;
    float hx = kX0 + g_.segX(0) * kCell + kCell / 2.f, hy = kY0 + g_.segY(0) * kCell + kCell / 2.f;
    for (int s = -1; s <= 1; s += 2) {
      float ex = hx + dx * 4 - dy * s * 4.5f, ey = hy + dy * 4 + dx * s * 4.5f;
      bool along = dx != 0;
      c.fillRect((int)(ex - (along ? 3 : 2)), (int)(ey - (along ? 2 : 3)), along ? 6 : 4, along ? 4 : 6, BG);
    }
    snprintf(b, sizeof b, "%d", g_.score());
    ui::textCentered(c, CX, 38 + 14, b, NUM_S30, INK);
    if (runMs_ < HINT_MS) c.text(CX, 394, "свайп — поворот", TXT_MIN, INK_2, Align::Center);  // under the frame
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (phase_ != Run) {
      if (e.g != Gesture::Tap) return;
      if (phase_ == Over && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
      if ((phase_ == Ready && kStartButton.hit(e.x, e.y)) || (phase_ == Over && kAgainButton.hit(e.x, e.y))) {
        if (phase_ == Over) g_.reset(a.rng);
        phase_ = Run;
        runMs_ = 0;
        newRecord_ = false;
        a.sfx.play("games/go");
      }
      return;
    }
    if (e.g == Gesture::SwipeLeft) g_.turn(-1, 0);
    if (e.g == Gesture::SwipeRight) g_.turn(1, 0);
    if (e.g == Gesture::SwipeUp) g_.turn(0, -1);
    if (e.g == Gesture::SwipeDown) g_.turn(0, 1);
  }

 private:
  static constexpr int kCell = 20;
  static constexpr float kX0 = CX - SnakeGame::kSize * kCell / 2.f, kY0 = CY - SnakeGame::kSize * kCell / 2.f;
  enum Phase { Ready, Run, Over } phase_ = Ready;
  SnakeGame g_;
  float stepMs_ = 0, runMs_ = 0;
  bool newRecord_ = false;
};

// ---- 22. Simon, "Повтори": four sectors of a ring light up in turn ---------------------------------
class SimonScreen : public Screen {
 public:
  void enter(App& a) override { state_ = Ready; len_ = 0; a.react(Emotion::Attentive, 1200); }
  void tick(App& a, float dt) override {
    if (litMs_ > 0) { litMs_ -= dt; if (litMs_ <= 0) lit_ = -1; }
    if (state_ != Show) return;
    timer_ -= dt;
    if (timer_ > 0) return;
    if (pos_ < len_) {
      light(a, seq_[pos_++], 420);
      timer_ = 560;
    } else {
      state_ = Input;
      pos_ = 0;
      a.react(Emotion::Focused, 800);
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[24], rec[32];
    record(rec, sizeof rec, a.settings.simonBest);
    if (state_ == Ready) { gameStart(c, a, "Повтори", "Запомни, как загорятся цвета, и повтори", rec); return; }
    if (state_ == Over) {
      snprintf(b, sizeof b, "%d", len_ - 1);
      gameResult(c, a, b, newRecord_ ? "раундов · новый рекорд" : "раундов", newRecord_ ? Emotion::Proud : Emotion::Confused);
      return;
    }
    static const uint16_t on[4] = {ACCENT, ACCENT_2, simon::GREEN, simon::YELLOW};
    static const uint16_t off[4] = {simon::DIM_BLUE, simon::DIM_RED, simon::DIM_GREEN, simon::DIM_YELLOW};
    for (int i = 0; i < 4; i++) ui::drawArc(c, CX, CY, 176, 72, kStart[i], 86, lit_ == i ? on[i] : off[i], false);
    miniFace(c, a, CX, CY - 40, 130);
    ui::textCentered(c, CX, CY + 22, state_ == Show ? "Запоминай" : "Повтори", TXT_CAP, INK);
    snprintf(b, sizeof b, "раунд %d", len_);
    ui::textCentered(c, CX, CY + 52, b, TXT_MIN, INK_2);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (state_ == Over && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
    if ((state_ == Ready && kStartButton.hit(e.x, e.y)) || (state_ == Over && kAgainButton.hit(e.x, e.y))) { next(a, true); return; }
    if (state_ != Input) return;
    float dist = sqrtf((float)((e.x - CX) * (e.x - CX) + (e.y - CY) * (e.y - CY)));
    if (dist < 120) return;
    float ang = atan2f((float)(e.y - CY), (float)(e.x - CX)) * 57.29578f;  // 0 to the right, clockwise
    if (ang < 0) ang += 360;
    int sector = -1;
    for (int i = 0; i < 4; i++) if (fmodf(ang - kStart[i] + 360, 360) < 90) sector = i;
    if (sector < 0) return;
    if (sector != seq_[pos_]) {
      state_ = Over;
      newRecord_ = len_ - 1 > a.settings.simonBest;
      if (newRecord_) { a.settings.simonBest = (uint16_t)(len_ - 1); a.saveSettings(); }
      a.sfx.play("games/wrong");
      a.pet.event(PetEvent::GameLose);
      a.react(Emotion::Confused, 1800);
      return;
    }
    light(a, sector, 220);
    if (++pos_ == len_) {
      a.pet.event(PetEvent::Play);
      a.react(Emotion::Happy, 900);
      next(a, false);
    }
  }

 private:
  // The spec's sectors: 86° each with 4° gaps, top left, top right, bottom right, bottom left.
  static constexpr float kStart[4] = {184, 274, 4, 94};
  enum State { Ready, Show, Input, Over };
  void light(App& a, int i, float ms) {
    lit_ = i;
    litMs_ = ms;
    static const char* const tones[4] = {"games/tone1", "games/tone2", "games/tone3", "games/tone4"};
    a.sfx.play(tones[i]);
    a.anim.lookOffset(i == 1 || i == 2 ? 26.f : -26.f, i == 0 || i == 1 ? -16.f : 16.f, ms);
  }
  void next(App& a, bool restart) {
    if (restart) { len_ = 0; newRecord_ = false; }
    if (len_ < (int)sizeof(seq_)) seq_[len_++] = (uint8_t)a.rng.range(0, 3);
    state_ = Show;
    pos_ = 0;
    timer_ = restart ? 500 : 900;
  }
  State state_ = Ready;
  uint8_t seq_[64];
  int len_ = 0, pos_ = 0, lit_ = -1;
  float timer_ = 0, litMs_ = 0;
  bool newRecord_ = false;
};
constexpr float SimonScreen::kStart[4];

Screen* gameScreen(ScreenId id) {
  static MazeScreen maze; static OrbitScreen orbit; static SnakeScreen snake; static SimonScreen simon;
  switch (id) {
    case ScreenId::Maze: return &maze;
    case ScreenId::Orbit: return &orbit;
    case ScreenId::Snake: return &snake;
    default: return &simon;
  }
}

}  // namespace cc
