#include "companion/games.h"
#include <string.h>

namespace cc {

void MazeGame::generate(int size, Rng& rng) {
  n_ = clampi(size, 3, kMax);
  memset(walls_, N | E | S | W, sizeof(walls_));
  // Randomized depth-first search: a perfect maze, exactly one route between any two cells.
  static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
  static const uint8_t bit[4] = {N, E, S, W}, opp[4] = {S, W, N, E};
  bool seen[kMax * kMax] = {};
  uint8_t stack[kMax * kMax];
  int top = 0;
  stack[top++] = 0;
  seen[0] = true;
  while (top) {
    int cur = stack[top - 1], cx = cur % n_, cy = cur / n_;
    int opts[4], k = 0;
    for (int d = 0; d < 4; d++) {
      int nx = cx + dx[d], ny = cy + dy[d];
      if (nx >= 0 && ny >= 0 && nx < n_ && ny < n_ && !seen[ny * n_ + nx]) opts[k++] = d;
    }
    if (!k) { top--; continue; }
    int d = opts[rng.range(0, k - 1)];
    int nxt = (cy + dy[d]) * n_ + cx + dx[d];
    walls_[cur] &= (uint8_t)~bit[d];
    walls_[nxt] &= (uint8_t)~opp[d];
    seen[nxt] = true;
    stack[top++] = (uint8_t)nxt;
  }
  x_ = y_ = 0.5f; vx_ = vy_ = 0; won_ = false;
}

void MazeGame::step(float ax, float ay, float dtMs) {
  if (won_ || !n_) return;
  constexpr float kAccel = 15.f, kDrag = 1.6f, kMaxSpeed = 9.f, kBounce = 0.35f;
  int steps = (int)(dtMs / 5) + 1;
  float h = dtMs * 0.001f / steps;
  for (int i = 0; i < steps; i++) {
    vx_ += (ax * kAccel - vx_ * kDrag) * h;
    vy_ += (ay * kAccel - vy_ * kDrag) * h;
    vx_ = clampf(vx_, -kMaxSpeed, kMaxSpeed);
    vy_ = clampf(vy_, -kMaxSpeed, kMaxSpeed);
    // Each axis resolves on its own against the walls of the cell the centre is in.
    x_ += vx_ * h;
    int cx = clampi((int)x_, 0, n_ - 1), cy = clampi((int)y_, 0, n_ - 1);
    if (x_ + kBall > cx + 1 && wall(cx, cy, E)) { x_ = cx + 1 - kBall; vx_ *= -kBounce; }
    if (x_ - kBall < cx && wall(cx, cy, W)) { x_ = cx + kBall; vx_ *= -kBounce; }
    y_ += vy_ * h;
    cx = clampi((int)x_, 0, n_ - 1);
    cy = clampi((int)y_, 0, n_ - 1);
    if (y_ + kBall > cy + 1 && wall(cx, cy, S)) { y_ = cy + 1 - kBall; vy_ *= -kBounce; }
    if (y_ - kBall < cy && wall(cx, cy, N)) { y_ = cy + kBall; vy_ *= -kBounce; }
  }
  float ex = x_ - (n_ - 0.5f), ey = y_ - (n_ - 0.5f);
  if (ex * ex + ey * ey < 0.16f) won_ = true;
}

void MazeGame::nextCellToExit(int* outX, int* outY) const {
  // Breadth-first from the exit: distances lead the way downhill from any cell.
  int dist[kMax * kMax];
  memset(dist, -1, sizeof(dist));
  int q[kMax * kMax], head = 0, tail = 0, goal = n_ * n_ - 1;
  dist[goal] = 0;
  q[tail++] = goal;
  static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
  static const int bit[4] = {N, E, S, W};
  while (head < tail) {
    int c = q[head++], cx = c % n_, cy = c / n_;
    for (int d = 0; d < 4; d++) {
      if (walls_[c] & bit[d]) continue;
      int nb = (cy + dy[d]) * n_ + cx + dx[d];
      if (dist[nb] < 0) { dist[nb] = dist[c] + 1; q[tail++] = nb; }
    }
  }
  int cx = clampi((int)x_, 0, n_ - 1), cy = clampi((int)y_, 0, n_ - 1), c = cy * n_ + cx;
  *outX = cx; *outY = cy;
  for (int d = 0; d < 4; d++) {
    if (walls_[c] & bit[d]) continue;
    int nb = (cy + dy[d]) * n_ + cx + dx[d];
    if (dist[nb] >= 0 && dist[nb] < dist[c]) { *outX = cx + dx[d]; *outY = cy + dy[d]; }
  }
}

void SnakeGame::reset(Rng& rng) {
  len_ = 3; head_ = 2; dx_ = nextDx_ = 1; dy_ = nextDy_ = 0; alive_ = true;
  for (int i = 0; i < 3; i++) { bodyX_[i] = (uint8_t)(5 + i); bodyY_[i] = kSize / 2; }
  placeFood(rng);
}

void SnakeGame::placeFood(Rng& rng) {
  for (int tries = 0; tries < 400; tries++) {
    fx_ = rng.range(0, kSize - 1);
    fy_ = rng.range(0, kSize - 1);
    bool hit = false;
    for (int i = 0; i < len_; i++) if (segX(i) == fx_ && segY(i) == fy_) hit = true;
    if (!hit) return;
  }
}

void SnakeGame::turn(int dx, int dy) {
  if (dx == -dx_ && dy == -dy_) return;
  nextDx_ = dx; nextDy_ = dy;
}

bool SnakeGame::step(Rng& rng) {
  if (!alive_) return false;
  dx_ = nextDx_; dy_ = nextDy_;
  int nx = segX(0) + dx_, ny = segY(0) + dy_;
  if (nx < 0 || ny < 0 || nx >= kSize || ny >= kSize) { alive_ = false; return false; }
  bool eat = nx == fx_ && ny == fy_;
  // The tail moves away this very step, so stepping onto it is legal unless eating.
  for (int i = 0; i < len_ - (eat ? 0 : 1); i++) if (segX(i) == nx && segY(i) == ny) { alive_ = false; return false; }
  head_ = (head_ + 1) % kMaxLen;
  bodyX_[head_] = (uint8_t)nx;
  bodyY_[head_] = (uint8_t)ny;
  if (eat) { if (len_ < kMaxLen) len_++; placeFood(rng); }
  return true;
}

}  // namespace cc
