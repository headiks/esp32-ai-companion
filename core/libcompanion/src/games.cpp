#include "companion/games.h"
#include <string.h>

namespace cc {

void MazeGame::generate(int size, Rng& rng, float branchy) {
  n_ = clampi(size, 3, kMax);
  memset(walls_, N | E | S | W, sizeof(walls_));
  // Growing tree: carving on from the newest open cell gives depth-first corridors, from a random one
  // gives Prim-like branching. Either way a perfect maze: exactly one route between any two cells.
  static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
  static const uint8_t bit[4] = {N, E, S, W}, opp[4] = {S, W, N, E};
  bool seen[kMax * kMax] = {};
  uint8_t open[kMax * kMax];
  int count = 0, start = rng.range(0, n_ * n_ - 1);
  open[count++] = (uint8_t)start;
  seen[start] = true;
  while (count) {
    int i = rng.unit() < branchy ? rng.range(0, count - 1) : count - 1;
    int cur = open[i], cx = cur % n_, cy = cur / n_;
    int opts[4], k = 0;
    for (int d = 0; d < 4; d++) {
      int nx = cx + dx[d], ny = cy + dy[d];
      if (nx >= 0 && ny >= 0 && nx < n_ && ny < n_ && !seen[ny * n_ + nx]) opts[k++] = d;
    }
    if (!k) { memmove(open + i, open + i + 1, (size_t)(count - i - 1)); count--; continue; }
    int d = opts[rng.range(0, k - 1)];
    int nxt = (cy + dy[d]) * n_ + cx + dx[d];
    walls_[cur] &= (uint8_t)~bit[d];
    walls_[nxt] &= (uint8_t)~opp[d];
    seen[nxt] = true;
    open[count++] = (uint8_t)nxt;
  }
  int dist[kMax * kMax];
  goal_ = distances(start, dist);
  pathLen_ = dist[goal_];
  x_ = start % n_ + 0.5f;
  y_ = start / n_ + 0.5f;
  vx_ = vy_ = 0;
  won_ = false;
}

int MazeGame::distances(int from, int* dist) const {
  static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
  static const int bit[4] = {N, E, S, W};
  for (int i = 0; i < n_ * n_; i++) dist[i] = -1;
  int q[kMax * kMax], head = 0, tail = 0;
  dist[from] = 0;
  q[tail++] = from;
  while (head < tail) {
    int c = q[head++], cx = c % n_, cy = c / n_;
    for (int d = 0; d < 4; d++) {
      if (walls_[c] & bit[d]) continue;
      int nb = (cy + dy[d]) * n_ + cx + dx[d];
      if (dist[nb] < 0) { dist[nb] = dist[c] + 1; q[tail++] = nb; }
    }
  }
  return q[tail - 1];  // breadth-first: the last one reached is the farthest
}

void MazeGame::step(float ax, float ay, float dtMs) {
  if (won_ || !n_) return;
  constexpr float kDrag = 1.6f, kBounce = 0.35f;
  const float accel = 15.f * pace_, maxSpeed = 9.f * pace_;
  int steps = (int)(dtMs / 5) + 1;
  float h = dtMs * 0.001f / steps;
  for (int i = 0; i < steps; i++) {
    vx_ += (ax * accel - vx_ * kDrag) * h;
    vy_ += (ay * accel - vy_ * kDrag) * h;
    vx_ = clampf(vx_, -maxSpeed, maxSpeed);
    vy_ = clampf(vy_, -maxSpeed, maxSpeed);
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
  float ex = x_ - (goalX() + 0.5f), ey = y_ - (goalY() + 0.5f);
  if (ex * ex + ey * ey < 0.16f) won_ = true;
}

void MazeGame::nextCellToExit(int* outX, int* outY) const {
  // Distances from the exit lead the way downhill from any cell.
  static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
  static const int bit[4] = {N, E, S, W};
  int dist[kMax * kMax];
  distances(goal_, dist);
  int cx = clampi((int)x_, 0, n_ - 1), cy = clampi((int)y_, 0, n_ - 1), c = cy * n_ + cx;
  *outX = cx; *outY = cy;
  for (int d = 0; d < 4; d++) {
    if (walls_[c] & bit[d]) continue;
    int nb = (cy + dy[d]) * n_ + cx + dx[d];
    if (dist[nb] >= 0 && dist[nb] < dist[c]) { *outX = cx + dx[d]; *outY = cy + dy[d]; }
  }
}

int BlackjackGame::value(const Card* h, int n) {
  int total = 0, aces = 0;
  for (int i = 0; i < n; i++) {
    int r = h[i].rank;
    if (r == 14) { total += 11; aces++; }
    else total += r > 10 ? 10 : r;
  }
  while (total > 21 && aces > 0) { total -= 10; aces--; }
  return total;
}

void BlackjackGame::deal(Rng& rng) {
  int n = 0;
  for (uint8_t s = 0; s < 4; s++)
    for (uint8_t r = 2; r <= 14; r++) deck_[n++] = Card{r, s};
  for (int i = 51; i > 0; i--) {
    int j = rng.range(0, i);
    Card t = deck_[i]; deck_[i] = deck_[j]; deck_[j] = t;
  }
  top_ = 0;
  chips_ -= kBet;
  playerN_ = dealerN_ = 0;
  natural_ = false;
  player_[playerN_++] = deck_[top_++];
  dealer_[dealerN_++] = deck_[top_++];
  player_[playerN_++] = deck_[top_++];
  dealer_[dealerN_++] = deck_[top_++];
  phase_ = Phase::Player;
  if (playerValue() == 21) { natural_ = true; stand(); }
}

void BlackjackGame::hit() {
  if (phase_ != Phase::Player || playerN_ >= kMaxHand) return;
  player_[playerN_++] = deck_[top_++];
  if (playerValue() > 21) { outcome_ = -1; phase_ = Phase::Done; }
  else if (playerValue() == 21) stand();
}

void BlackjackGame::stand() {
  if (phase_ == Phase::Player) phase_ = Phase::Dealer;
}

bool BlackjackGame::dealerStep() {
  if (phase_ != Phase::Dealer) return false;
  if (dealerValue() < 17 && dealerN_ < kMaxHand) {
    dealer_[dealerN_++] = deck_[top_++];
    return true;
  }
  settle();
  return false;
}

void BlackjackGame::settle() {
  int p = playerValue(), d = dealerValue();
  phase_ = Phase::Done;
  if (d > 21 || p > d) {
    outcome_ = 1;
    chips_ += natural_ ? kBet * 5 / 2 : kBet * 2;  // a natural pays 3 to 2
  } else if (p == d) {
    outcome_ = 0;
    chips_ += kBet;
  } else outcome_ = -1;
}

void BlackjackGame::setForTest(const Card* p, int pn, const Card* d, int dn) {
  playerN_ = pn; dealerN_ = dn;
  for (int i = 0; i < pn; i++) player_[i] = p[i];
  for (int i = 0; i < dn; i++) dealer_[i] = d[i];
  // A low, predictable deck for the dealer's and player's next draws.
  for (int i = 0; i < 52; i++) deck_[i] = Card{2, 0};
  top_ = 0;
  phase_ = Phase::Player;
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
