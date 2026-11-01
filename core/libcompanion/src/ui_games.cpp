// Games made for a round, tiltable, touch screen: maze, orbit pong, snake from squares, Simon on the rim.
#include "companion/games.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

namespace cc {

static constexpr int kCx = kScreenW / 2, kCy = kScreenH / 2;

// Maze: the board is the inscribed square of the circle, the ball rolls with the device.
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
    a.anim.lookAt(ox() + m_.x() * sc, oy() + m_.y() * sc, 200);
    if (m_.won()) {
      winMs_ = 0;
      a.sfx.play("games/win");
      a.pet.event(PetEvent::GameWin);
      a.react(Emotion::VeryHappy, 1700);
    }
  }
  void draw(App& a, Canvas& c) override {
    const float sc = cell();
    const int x0 = (int)ox(), y0 = (int)oy(), n = m_.size(), s = (int)sc;
    // Plain rectangles: walls are axis-aligned and the whole board redraws every frame.
    for (int y = 0; y < n; y++) {
      if (!c.intersects(0, y0 + y * s - 2, kScreenW, s + 4)) continue;
      for (int x = 0; x < n; x++) {
        int px = x0 + x * s, py = y0 + y * s;
        if (m_.wall(x, y, MazeGame::N)) c.fillRect(px - 1, py - 1, s + 3, 3, kGray);
        if (m_.wall(x, y, MazeGame::W)) c.fillRect(px - 1, py - 1, 3, s + 3, kGray);
        if (x == n - 1) c.fillRect(px + s - 1, py - 1, 3, s + 3, kGray);
        if (y == n - 1) c.fillRect(px - 1, py + s - 1, s + 3, 3, kGray);
      }
    }
    float pulse = 1 + 0.2f * sinf(a.nowMs() * 0.006f);
    c.fillCircle(x0 + (n - 0.5f) * sc, y0 + (n - 0.5f) * sc, sc * 0.22f * pulse, kAccent);
    c.fillCircle(x0 + m_.x() * sc, y0 + m_.y() * sc, sc * MazeGame::kBall, kWhite);
    char lv[8];
    snprintf(lv, sizeof(lv), "%d", level_ + 1);
    c.text(kCx, 14, lv, FontSize::S, kGray, Align::Center);
    a.face.drawScaled(c, a.anim, kCx, 428, 0.14f, a.eyeColor());
    if (m_.won()) c.text(kCx, kCy - 24, "Выход!", FontSize::L, kSoft, Align::Center);
  }
 private:
  void start(App& a) {
    m_.generate(clampi(5 + level_, 5, MazeGame::kMax), a.rng);
    winMs_ = 0;
    a.react(Emotion::Determined, 1500);
  }
  float cell() const { return 300.f / m_.size(); }
  float ox() const { return kCx - 150.f; }
  float oy() const { return kCy - 150.f; }
  MazeGame m_;
  int level_ = 0;
  float winMs_ = 0, cooldown_ = 0;
};

// Orbit: a paddle runs around the rim. Tilt or a finger moves it; the ball must stay in the circle.
class OrbitScreen : public Screen {
 public:
  void enter(App& a) override { running_ = false; over_ = false; score_ = 0; a.react(Emotion::Curious, 1200); }
  bool capturesSwipes() const override { return running_; }
  void tick(App& a, float dt) override {
    score_ = clampi(score_, 0, 9999);
    char b[8];
    snprintf(b, sizeof(b), "%d", score_);
    num_.set(b);
    num_.tick(dt);
    if (!running_) return;
    // The device's tilt points where the paddle should be, unless a finger is steering.
    float tilt = sqrtf(a.hal.ax * a.hal.ax + a.hal.ay * a.hal.ay);
    if (!a.touch.isDown() && tilt > 0.12f) {
      float target = atan2f(a.hal.ax, -a.hal.ay);
      float diff = fmodf(target - paddle_ + 9.42478f, 6.28319f) - 3.14159f;
      paddle_ += diff * clampf(dt / 90.f, 0, 1);
    }
    float h = dt * 0.001f;
    bx_ += vx_ * h;
    by_ += vy_ * h;
    a.anim.lookOffset(bx_ * 0.2f, by_ * 0.16f, 150);
    float d = sqrtf(bx_ * bx_ + by_ * by_);
    if (d <= kRim - kBallR) return;
    float nx = bx_ / d, ny = by_ / d, ang = atan2f(nx, -ny);
    float diff = fmodf(ang - paddle_ + 9.42478f, 6.28319f) - 3.14159f;
    if (fabsf(diff) < halfWidth()) {
      // Reflect about the rim normal; the offset from the paddle centre steers the ball.
      float dot = vx_ * nx + vy_ * ny;
      vx_ -= 2 * dot * nx;
      vy_ -= 2 * dot * ny;
      float sp = sqrtf(vx_ * vx_ + vy_ * vy_) * 1.03f;
      if (sp > 460) sp = 460;
      float dir = atan2f(vy_, vx_) + diff * 0.9f;
      vx_ = cosf(dir) * sp;
      vy_ = sinf(dir) * sp;
      bx_ = nx * (kRim - kBallR - 1);
      by_ = ny * (kRim - kBallR - 1);
      score_++;
      a.sfx.play("games/bounce");
      a.anim.kick(-0.14f);
      if (score_ % 5 == 0) a.react(Emotion::Excited, 900);
      else if (a.rng.range(0, 3) == 0) a.react(Emotion::Happy, 700);
    } else {
      running_ = false;
      over_ = true;
      bool record = score_ > a.settings.orbitBest;
      if (record) { a.settings.orbitBest = (uint16_t)score_; a.saveSettings(); }
      a.pet.event(record ? PetEvent::GameWin : PetEvent::GameLose);
      a.sfx.play(record ? "games/win" : "games/lose");
      a.react(record ? Emotion::Proud : Emotion::Sad, 2000);
    }
  }
  void draw(App& a, Canvas& c) override {
    c.ring(kCx, kCy, kRim + 7, 1, kDark);
    float deg = paddle_ * 57.29578f, hw = halfWidth() * 57.29578f;
    c.arc(kCx, kCy, kRim + 7, 8, deg - hw, deg + hw, kAccent);
    a.face.drawScaled(c, a.anim, kCx, kCy - 8, 0.42f, a.eyeColor());
    if (running_) c.fillCircle(kCx + bx_, kCy + by_, kBallR, kWhite);
    num_.draw(c, kCx, kCy + 42, 5, 1, running_ ? kGray : kWhite);
    if (!running_) c.text(kCx, kCy + 92, over_ ? "Ещё раз" : "Нажми", FontSize::S, kGray, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if ((e.g == Gesture::Move || e.g == Gesture::Down) && running_) paddle_ = atan2f((float)(e.x - kCx), (float)(kCy - e.y));
    if (e.g != Gesture::Tap || running_) return;
    running_ = true; over_ = false; score_ = 0;
    bx_ = by_ = 0;
    float dir = a.rng.rangef(0, 6.28319f);
    vx_ = cosf(dir) * 170; vy_ = sinf(dir) * 170;
    a.sfx.play("games/go");
    a.react(Emotion::Focused, 1500);
  }
 private:
  // The paddle narrows as the score grows, down to a limit that stays playable.
  float halfWidth() const { float w = 0.62f - score_ * 0.012f; return w < 0.26f ? 0.26f : w; }
  static constexpr float kRim = 214, kBallR = 9;
  DotText num_;
  bool running_ = false, over_ = false;
  int score_ = 0;
  float paddle_ = 0, bx_ = 0, by_ = 0, vx_ = 0, vy_ = 0;
};

// Snake from squares, matching the dot-matrix look of the clock. Swipes turn it.
class SnakeScreen : public Screen {
 public:
  void enter(App& a) override { started_ = false; g_.reset(a.rng); stepMs_ = 0; a.react(Emotion::Mischievous, 1500); }
  bool capturesSwipes() const override { return started_ && g_.alive(); }
  void tick(App& a, float dt) override {
    if (!started_ || !g_.alive()) return;
    stepMs_ += dt;
    float period = lerpf(230, 90, clampf(g_.score() / 40.f, 0, 1));
    if (stepMs_ < period) return;
    stepMs_ = 0;
    int was = g_.length();
    if (!g_.step(a.rng)) {
      bool record = g_.score() > a.settings.snakeBest;
      if (record) { a.settings.snakeBest = (uint16_t)g_.score(); a.saveSettings(); }
      a.pet.event(record ? PetEvent::GameWin : PetEvent::GameLose);
      a.sfx.play(record ? "games/win" : "games/lose");
      a.react(record ? Emotion::Proud : Emotion::Dizzy, 2200);
    } else if (g_.length() > was) {
      a.sfx.play("games/correct");
      a.pet.event(PetEvent::Feed);
      a.react(Emotion::Happy, 700);
    }
    a.anim.lookAt(ox() + g_.foodX() * kCell, oy() + g_.foodY() * kCell, 300);
  }
  void draw(App& a, Canvas& c) override {
    for (int y = 0; y < SnakeGame::kSize; y++)
      for (int x = 0; x < SnakeGame::kSize; x++) {
        if (x % 2 == 0 && y % 2 == 0) c.fillRect(ox() + x * kCell + 8, oy() + y * kCell + 8, 2, 2, kDark);
      }
    c.fillRect(ox() + g_.foodX() * kCell + 2, oy() + g_.foodY() * kCell + 2, kCell - 4, kCell - 4, kAccent);
    for (int i = g_.length() - 1; i >= 0; i--) {
      // The tail fades: readable direction without any extra elements.
      uint16_t col = blend565(kDim, kWhite, (uint32_t)(255 - 180 * i / (g_.length() > 1 ? g_.length() - 1 : 1)));
      c.fillRect(ox() + g_.segX(i) * kCell + 1, oy() + g_.segY(i) * kCell + 1, kCell - 2, kCell - 2, col);
    }
    char b[16];
    snprintf(b, sizeof(b), "%d", g_.score());
    c.text(kCx, 18, b, FontSize::S, kGray, Align::Center);
    if (!started_) c.text(kCx, kCy + 60, "Свайпы — повороты", FontSize::S, kGray, Align::Center);
    if (!g_.alive()) c.text(kCx, kCy - 24, "Конец", FontSize::L, kSoft, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g == Gesture::Tap) {
      if (!started_) { started_ = true; a.sfx.play("games/go"); }
      else if (!g_.alive()) { g_.reset(a.rng); started_ = true; a.sfx.play("games/go"); }
      return;
    }
    if (e.g == Gesture::SwipeLeft) g_.turn(-1, 0);
    if (e.g == Gesture::SwipeRight) g_.turn(1, 0);
    if (e.g == Gesture::SwipeUp) g_.turn(0, -1);
    if (e.g == Gesture::SwipeDown) g_.turn(0, 1);
  }
 private:
  static constexpr int kCell = 20;
  static int ox() { return kCx - SnakeGame::kSize * kCell / 2; }
  static int oy() { return kCy - SnakeGame::kSize * kCell / 2; }
  SnakeGame g_;
  bool started_ = false;
  float stepMs_ = 0;
};

// Simon on four rim sectors: repeat the growing sequence of light and tone.
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
    static const uint16_t col[4] = {kRose, kSoft, kMint, kSky};
    for (int i = 0; i < 4; i++) {
      float a0 = i * 90.f + 5, a1 = a0 + 80;
      bool on = lit_ == i;
      c.arc(kCx, kCy, 190, on ? 62 : 44, a0, a1, on ? col[i] : blend565(kBlack, col[i], 96));
    }
    a.face.drawScaled(c, a.anim, kCx, kCy - 14, 0.42f, a.eyeColor());
    char b[12];
    snprintf(b, sizeof(b), "%d", len_);
    if (state_ == Ready) c.text(kCx, kCy + 36, "Нажми", FontSize::S, kGray, Align::Center);
    else c.text(kCx, kCy + 30, b, FontSize::M, kWhite, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (state_ == Ready || state_ == Over) { next(a, true); return; }
    if (state_ != Input) return;
    float ang = atan2f((float)(e.x - kCx), (float)(kCy - e.y)) * 57.29578f;
    if (ang < 0) ang += 360;
    float dist = sqrtf((float)((e.x - kCx) * (e.x - kCx) + (e.y - kCy) * (e.y - kCy)));
    if (dist < 110) return;
    int sector = (int)(ang / 90) % 4;
    if (sector != seq_[pos_]) {
      state_ = Over;
      bool record = len_ - 1 > a.settings.simonBest;
      if (record) { a.settings.simonBest = (uint16_t)(len_ - 1); a.saveSettings(); }
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
  enum State { Ready, Show, Input, Over };
  void light(App& a, int i, float ms) {
    lit_ = i;
    litMs_ = ms;
    static const char* const tones[4] = {"games/tone1", "games/tone2", "games/tone3", "games/tone4"};
    a.sfx.play(tones[i]);
    a.anim.lookOffset(i == 0 || i == 1 ? 26.f : -26.f, i == 1 || i == 2 ? 16.f : -16.f, ms);
  }
  void next(App& a, bool restart) {
    if (restart) len_ = 0;
    if (len_ < (int)sizeof(seq_)) seq_[len_++] = (uint8_t)a.rng.range(0, 3);
    state_ = Show;
    pos_ = 0;
    timer_ = restart ? 500 : 900;
  }
  State state_ = Ready;
  uint8_t seq_[64];
  int len_ = 0, pos_ = 0, lit_ = -1;
  float timer_ = 0, litMs_ = 0;
};

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
