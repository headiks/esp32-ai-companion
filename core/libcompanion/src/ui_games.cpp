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

// ---- 19. maze: the board is a 300 px square, the ball rolls with the device ------------------------
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
    c.ring(kX0 + (n - 0.5f) * sc, kY0 + (n - 0.5f) * sc, goalR, 5, ACCENT);
    c.fillCircle(kX0 + m_.x() * sc, kY0 + m_.y() * sc, ballR, INK);
    miniFace(c, a, CX, 388 + 40 * 176 / 354.f, 80);
    if (m_.won()) ui::textCentered(c, CX, CY, "Выход!", TXT_MENU_SEL, ACCENT);
  }

 private:
  static constexpr float kX0 = 83, kY0 = 83;
  void start(App& a) {
    m_.generate(clampi(5 + level_, 5, MazeGame::kMax), a.rng);
    winMs_ = 0;
    a.react(Emotion::Determined, 1500);
  }
  float cell() const { return 300.f / m_.size(); }
  MazeGame m_;
  int level_ = 0;
  float winMs_ = 0, cooldown_ = 0;
};

// ---- 20. orbit: run around the orbit, a tap jumps over the blocks ---------------------------------
// Design 1a turned the old rim pong into this runner: the screen it draws has a player on an orbit and
// blocks to jump over, nothing to bat.
class OrbitScreen : public Screen {
 public:
  void enter(App& a) override { phase_ = Ready; a.react(Emotion::Curious, 1200); }
  bool capturesSwipes() const override { return phase_ == Run; }
  void tick(App& a, float dt) override {
    if (phase_ != Run) return;
    runMs_ += dt;
    float h = dt * 0.001f;
    float speed = 70 + score_ * 3.5f;  // degrees a second
    if (speed > 170) speed = 170;
    angle_ += speed * h;
    if (jumpMs_ >= 0) { jumpMs_ += dt; if (jumpMs_ > kJumpMs) jumpMs_ = -1; }
    a.anim.lookOffset(cosf(angle_ * 0.0174533f) * 30, sinf(angle_ * 0.0174533f) * 20, 150);
    // Angles are not wrapped: the player only moves forward, a block is passed once the player is beyond it.
    const float reach = (11 + 16) / kR * 57.29578f;  // half a block plus the player, as an angle
    for (int i = 0; i < kBlocks; i++) {
      float d = blocks_[i] - angle_;
      if (fabsf(d) < reach && lift() < 16) { crash(a); return; }
      if (d < -reach) {
        score_++;
        a.sfx.play("games/correct");
        if (score_ % 5 == 0) a.react(Emotion::Excited, 900);
        // The passed block goes ahead of the farthest one.
        float far = angle_;
        for (int k = 0; k < kBlocks; k++) far = fmaxf(far, blocks_[k]);
        blocks_[i] = far + a.rng.rangef(80, 150);
      }
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[24], rec[32];
    record(rec, sizeof rec, a.settings.orbitBest);
    if (phase_ == Ready) { gameStart(c, a, "Орбита", "Прыгай через блоки на орбите", rec); return; }
    if (phase_ == Over) {
      snprintf(b, sizeof b, "%d", score_);
      gameResult(c, a, b, newRecord_ ? "новый рекорд" : rec, newRecord_ ? Emotion::Starstruck : Emotion::Sad);
      return;
    }
    c.ring(CX, CY, kR, 4, TRACK);
    for (int i = 0; i < kBlocks; i++) {
      float half = 11 / kR * 57.29578f;
      ui::drawArc(c, CX, CY, kR, 16, blocks_[i] - half, 2 * half, INK, false);
    }
    float r = kR + lift(), rad = angle_ * 0.0174533f;
    c.fillCircle(CX + cosf(rad) * r, CY + sinf(rad) * r, 16, ACCENT);
    miniFace(c, a, CX, 146 + 60 * 176 / 354.f, 120);
    snprintf(b, sizeof b, "%d", score_);
    ui::textCentered(c, CX, 232, b, NUM_L56, INK);
    if (runMs_ < HINT_MS) c.text(CX, 292, "тап — прыжок", TXT_MIN, INK_2, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (phase_ == Run) {
      if (jumpMs_ < 0) { jumpMs_ = 0; a.sfx.play("games/move"); }
      return;
    }
    if (phase_ == Over && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
    if ((phase_ == Ready && kStartButton.hit(e.x, e.y)) || (phase_ == Over && kAgainButton.hit(e.x, e.y))) start(a);
  }

 private:
  static constexpr int kBlocks = 3;
  static constexpr float kR = 160, kJumpMs = 620;
  float lift() const { return jumpMs_ < 0 ? 0 : 40 * sinf(3.14159f * jumpMs_ / kJumpMs); }
  void start(App& a) {
    phase_ = Run;
    score_ = 0;
    angle_ = -90;
    jumpMs_ = -1;
    runMs_ = 0;
    newRecord_ = false;
    for (int i = 0; i < kBlocks; i++) blocks_[i] = angle_ + 100 + i * 110 + a.rng.rangef(0, 30);
    a.sfx.play("games/go");
    a.react(Emotion::Focused, 1500);
  }
  void crash(App& a) {
    phase_ = Over;
    newRecord_ = score_ > a.settings.orbitBest;
    if (newRecord_) { a.settings.orbitBest = (uint16_t)score_; a.saveSettings(); }
    a.pet.event(newRecord_ ? PetEvent::GameWin : PetEvent::GameLose);
    a.sfx.play(newRecord_ ? "games/win" : "games/lose");
    a.react(newRecord_ ? Emotion::Proud : Emotion::Sad, 2000);
  }
  enum Phase { Ready, Run, Over } phase_ = Ready;
  float angle_ = -90, jumpMs_ = -1, runMs_ = 0;
  float blocks_[kBlocks] = {};  // absolute angles, ahead of the player
  int score_ = 0;
  bool newRecord_ = false;
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
    for (int y = 13; y < kScreenH; y += 22) {  // the dotted field, inside r200
      if (!c.intersects(0, y - 2, kScreenW, 4)) continue;
      for (int x = 13; x < kScreenW; x += 22)
        if ((x - CX) * (x - CX) + (y - CY) * (y - CY) < 200 * 200) c.fillCircle((float)x, (float)y, 1.6f, SNAKE_GRID);
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
    if (runMs_ < HINT_MS) c.text(CX, 360, "свайп — поворот", TXT_MIN, INK_2, Align::Center);
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
