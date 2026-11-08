// Game logic without drawing, so the physics and rules can be tested directly.
#pragma once
#include "types.h"

namespace cc {

// Ball in a generated maze, rolled by device tilt. Positions are in cell units.
class MazeGame {
 public:
  static constexpr int kMax = 13;
  enum { N = 1, E = 2, S = 4, W = 8 };
  // A new perfect maze from a random start; the exit is the cell farthest from it.
  // branchy 0..1: 0 grows long winding corridors, 1 many short dead ends that are harder to read.
  void generate(int size, Rng& rng, float branchy = 0);
  void setPace(float k) { pace_ = k; }        // over 1 the ball rolls faster
  void step(float ax, float ay, float dtMs);  // tilt in g, screen axes
  int size() const { return n_; }
  bool wall(int cx, int cy, int dir) const { return walls_[cy * n_ + cx] & dir; }
  float x() const { return x_; }
  float y() const { return y_; }
  bool won() const { return won_; }
  int goalX() const { return goal_ % n_; }
  int goalY() const { return goal_ / n_; }
  int pathLength() const { return pathLen_; }  // cells from the start to the exit
  float speed() const { return sqrtf(vx_ * vx_ + vy_ * vy_); }
  // Next cell on the shortest way to the exit; used by the test autopilot.
  void nextCellToExit(int* cx, int* cy) const;
  static constexpr float kBall = 0.26f;
 private:
  int distances(int from, int* dist) const;  // breadth-first; returns the farthest cell
  int n_ = 0, goal_ = 0, pathLen_ = 0;
  uint8_t walls_[kMax * kMax];
  float x_ = 0.5f, y_ = 0.5f, vx_ = 0, vy_ = 0, pace_ = 1;
  bool won_ = false;
};

// Twenty-one. The dealer draws step by step, so the screen can show each card arrive.
class BlackjackGame {
 public:
  struct Card { uint8_t rank, suit; };  // rank 2..14, suit 0..3
  enum class Phase : uint8_t { Player, Dealer, Done };
  static constexpr int kMaxHand = 12, kBet = 10;
  void deal(Rng& rng);
  void hit();
  void stand();
  bool dealerStep();          // one dealer card; false when the dealer has finished and the round is settled
  static int value(const Card* hand, int n);
  int playerValue() const { return value(player_, playerN_); }
  int dealerValue() const { return value(dealer_, dealerN_); }
  int playerCount() const { return playerN_; }
  int dealerCount() const { return dealerN_; }
  const Card& playerCard(int i) const { return player_[i]; }
  const Card& dealerCard(int i) const { return dealer_[i]; }
  Phase phase() const { return phase_; }
  int chips() const { return chips_; }
  bool broke() const { return chips_ < kBet && phase_ == Phase::Done; }
  void resetChips() { chips_ = 100; }
  int outcome() const { return outcome_; }  // 1 win, 0 push, -1 loss; valid when Done
  bool natural() const { return natural_; }
  void setForTest(const Card* p, int pn, const Card* d, int dn);
 private:
  void settle();
  Card deck_[52];
  int top_ = 0;
  Card player_[kMaxHand], dealer_[kMaxHand];
  int playerN_ = 0, dealerN_ = 0, chips_ = 100, outcome_ = 0;
  bool natural_ = false;
  Phase phase_ = Phase::Done;
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
