// Game logic without drawing, so the physics and rules can be tested directly.
#pragma once
#include "types.h"

namespace cc {

// Ball in a generated maze, rolled by device tilt. Positions are in cell units.
class MazeGame {
 public:
  static constexpr int kMax = 13;
  enum { N = 1, E = 2, S = 4, W = 8 };
  void generate(int size, Rng& rng);
  void step(float ax, float ay, float dtMs);  // tilt in g, screen axes
  int size() const { return n_; }
  bool wall(int cx, int cy, int dir) const { return walls_[cy * n_ + cx] & dir; }
  float x() const { return x_; }
  float y() const { return y_; }
  bool won() const { return won_; }
  float speed() const { return sqrtf(vx_ * vx_ + vy_ * vy_); }
  // Next cell on the shortest way to the exit; used by the test autopilot.
  void nextCellToExit(int* cx, int* cy) const;
  static constexpr float kBall = 0.26f;
 private:
  int n_ = 0;
  uint8_t walls_[kMax * kMax];
  float x_ = 0.5f, y_ = 0.5f, vx_ = 0, vy_ = 0;
  bool won_ = false;
};

class SnakeGame {
 public:
  static constexpr int kSize = 15, kMaxLen = kSize * kSize;
  void reset(Rng& rng);
  void turn(int dx, int dy);   // ignored when it would reverse into the neck
  bool step(Rng& rng);         // one move; false when the snake died
  int length() const { return len_; }
  int segX(int i) const { return bodyX_[(head_ - i + kMaxLen) % kMaxLen]; }  // 0 is the head
  int segY(int i) const { return bodyY_[(head_ - i + kMaxLen) % kMaxLen]; }
  int foodX() const { return fx_; }
  int foodY() const { return fy_; }
  bool alive() const { return alive_; }
  int score() const { return len_ - 3; }
  void placeFoodForTest(int x, int y) { fx_ = x; fy_ = y; }
 private:
  void placeFood(Rng& rng);
  uint8_t bodyX_[kMaxLen], bodyY_[kMaxLen];
  int head_ = 0, len_ = 3, dx_ = 1, dy_ = 0, nextDx_ = 1, nextDy_ = 0, fx_ = 0, fy_ = 0;
  bool alive_ = true;
};

}  // namespace cc
