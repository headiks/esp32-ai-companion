// Design 1a: the pet (14), the firefly (15), the games list (16), the shared start and result templates
// (17, 18), tic-tac-toe (23) and guess the number (25).
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

namespace cc {
using namespace theme;

// ---- 17, 18. templates -------------------------------------------------------------------------------
const ui::Button kStartButton = ui::primary(CX, 326);
const ui::Button kExitButton = ui::secondary(176, 320);
const ui::Button kAgainButton = ui::primary(280, 310);

void gameStart(Canvas& c, App& a, const char* name, const char* rule, const char* record) {
  c.text(CX, 50, name, TXT_MIN, INK_2, Align::Center);
  a.face.drawMini(c, Emotion::Focused, CX - 85, 92, 170, INK);
  c.textWrapped(CX, 186, 300, rule, TXT_28, INK, Align::Center, 3, 36, true);
  if (record && record[0]) c.text(CX, 276, record, TXT_MIN, INK_2, Align::Center);
  kStartButton.draw(c, "Старт");
}

static bool numericText(const char* s) {
  for (; *s; s++) if (!strchr("0123456789 :,.+-", *s)) return false;
  return true;
}

void gameResult(Canvas& c, App& a, const char* value, const char* caption, Emotion face) {
  c.ring(CX, CY, 218, 4, ACCENT);
  a.face.drawMini(c, face, CX - 85, 62, 170, INK);
  ui::textCentered(c, CX, 192, value, numericText(value) ? NUM_84 : TXT_MENU_SEL, INK);
  if (caption && caption[0]) ui::textCentered(c, CX, 258, caption, TXT_CAP, ACCENT);
  kExitButton.draw(c, "Выход");
  kAgainButton.draw(c, "Ещё");
}

// ---- 14. pet ---------------------------------------------------------------------------------------
static const char* petStatusTitle(PetStatus s) {
  static const char* const t[] = {"Доволен", "Голоден", "Устал", "Скучает", "Радуется", "Спит", "Раздражён"};
  return t[(int)s];
}

class PetScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    const PetStats& s = a.pet.stats;
    miniFace(c, a, CX, 44 + 80 * 176 / 354.f, 160);
    ui::textCentered(c, CX, 140, petStatusTitle(a.pet.status()), TXT_CAP, ACCENT);
    struct { const char* n; float v; uint16_t col; } st[6] = {
        {"Еда", 100 - s.hunger, INK}, {"Силы", s.energy, INK}, {"Игра", s.fun, INK},
        {"Друзья", s.attention, INK}, {"Любовь", s.affection, ACCENT_2}, {"Здоровье", s.health, INK}};
    for (int i = 0; i < 6; i++) ui::statBar(c, i % 2 ? 248.f : 60.f, 166.f + (i / 2) * 48, 158, st[i].n, (int)(st[i].v + 0.5f), st[i].col);
    int main = mostNeeded(a);
    for (int i = 0; i < 4; i++) {
      ui::Button b = button(i);
      if (i == main) b.kind = ui::ButtonKind::Primary;
      b.draw(c, i == 3 && a.pet.asleep() ? "Будить" : kLabels[i]);
    }
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    static const PetEvent ev[4] = {PetEvent::Feed, PetEvent::Pet, PetEvent::Play, PetEvent::GoSleep};
    for (int i = 0; i < 4; i++) {
      if (!button(i).hit(e.x, e.y)) continue;
      a.petAction(i == 3 && a.pet.asleep() ? PetEvent::WakeUp : ev[i]);
      return;
    }
  }

 private:
  static constexpr const char* kLabels[4] = {"Еда", "Ласка", "Игра", "Сон"};
  static ui::Button button(int i) { return ui::secondary(CX - 120 + i * 80.f, 318); }  // inside r210
  // The button of the need that is lowest right now becomes the main one.
  static int mostNeeded(App& a) {
    const PetStats& s = a.pet.stats;
    float need[4] = {100 - s.hunger, s.affection, s.fun, a.pet.asleep() ? 0.f : s.energy};
    int best = 2;
    for (int i = 0; i < 4; i++) if (need[i] < need[best] - 5) best = i;
    return best;
  }
};
constexpr const char* PetScreen::kLabels[4];

// ---- 15. firefly ---------------------------------------------------------------------------------
class PetGameScreen : public Screen {
 public:
  void enter(App& a) override { phase_ = Start; score_ = 0; a.react(Emotion::Curious, 1500); }
  void tick(App& a, float dt) override {
    if (phase_ != Play) return;
    left_ -= dt;
    float ts = a.nowMs() * 0.001f;
    float speed = 1.0f + score_ * 0.06f;
    // The firefly stays inside the safe circle, around the face.
    fx_ = CX + 150 * sinf(ts * 1.3f * speed + seed_) * cosf(ts * 0.37f);
    fy_ = CY + 140 * sinf(ts * 0.9f * speed + seed_ * 1.7f);
    a.anim.lookAt(fx_, fy_, 200);
    if (left_ <= 0) {
      phase_ = Done;
      a.pet.event(PetEvent::Play);
      a.pet.stats.fun = clampf(a.pet.stats.fun + score_ * 1.5f, 0, 100);
      if (score_ >= 8) { a.pet.event(PetEvent::GameWin); a.sfx.play("games/win"); a.react(Emotion::VeryHappy, 2500); }
      else { a.sfx.play("games/draw"); a.react(Emotion::Happy, 2000); }
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[24];
    if (phase_ == Start) { gameStart(c, a, "Светлячок", "Лови огонёк, пока горит кольцо", nullptr); return; }
    if (phase_ == Done) {
      snprintf(b, sizeof b, "%d", score_);
      gameResult(c, a, b, "поймано", score_ >= 8 ? Emotion::VeryHappy : Emotion::Happy);
      return;
    }
    ui::progressRing(c, 218, 4, left_ / kGameMs);
    snprintf(b, sizeof b, "%d", score_);
    ui::textCentered(c, CX, 46 + 16, b, NUM_M40, INK);
    miniFace(c, a, CX, 190 + 110 * 176 / 354.f, 220);
    c.fillCircle(fx_, fy_, 26, FIREFLY_HALO);
    c.fillCircle(fx_, fy_, 11, ACCENT);
    if (kGameMs - left_ < HINT_MS) c.text(CX, 346, "тапни по огоньку", TXT_MIN, INK_2, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (phase_ != Play) {
      if (phase_ == Done && kExitButton.hit(e.x, e.y)) { a.goBack(); return; }
      if ((phase_ == Start && kStartButton.hit(e.x, e.y)) || (phase_ == Done && kAgainButton.hit(e.x, e.y))) {
        phase_ = Play; score_ = 0; left_ = kGameMs; seed_ = a.rng.rangef(0, 6);
        a.sfx.play("games/go");
        a.react(Emotion::Excited, 2000);
      }
      return;
    }
    float dx = e.x - fx_, dy = e.y - fy_;
    if (dx * dx + dy * dy < 46 * 46) {
      score_++;
      seed_ += a.rng.rangef(1, 3);
      a.sfx.play("games/correct");
      a.anim.kick(-0.25f);
      a.react(Emotion::Happy, 800);
    } else a.sfx.play("games/move");
  }
  bool capturesSwipes() const override { return phase_ == Play; }

 private:
  static constexpr float kGameMs = 20000;
  enum Phase { Start, Play, Done } phase_ = Start;
  int score_ = 0;
  float left_ = 0, fx_ = CX, fy_ = CY, seed_ = 0;
};

// ---- 16. games list --------------------------------------------------------------------------------
struct GameItem { const char* title; ScreenId id; };
static const GameItem kGames[] = {{"Лабиринт", ScreenId::Maze}, {"Орбита", ScreenId::Orbit}, {"Двадцать одно", ScreenId::Blackjack}, {"Монетка", ScreenId::Coin},
                                  {"Кубики", ScreenId::Dice}, {"Шар судьбы", ScreenId::MagicBall}, {"Дуэль", ScreenId::Duel}, {"Тряска", ScreenId::Shaker},
                                  {"Сейф", ScreenId::Safe}, {"Баланс", ScreenId::Balance}, {"Тренировка", ScreenId::Workout}, {"Крот", ScreenId::Mole},
                                  {"Змейка", ScreenId::Snake}, {"Повтори", ScreenId::Simon}, {"Крестики-нолики", ScreenId::TicTacToe},
                                  {"Угадай число", ScreenId::Guess}, {"Светлячок", ScreenId::PetGame}};
static constexpr int kGameCount = sizeof(kGames) / sizeof(kGames[0]);

class GamesScreen : public Screen {
 public:
  void enter(App& a) override {
    // Back from a game, the list stays on it.
    for (int i = 0; i < kGameCount; i++) if (kGames[i].id == a.previous()) return;
    list_.reset(0);
  }
  void tick(App&, float dt) override { list_.tick(dt); }
  void draw(App& a, Canvas& c) override {
    char b[16];
    snprintf(b, sizeof b, "%d из %d", list_.selected() + 1, kGameCount);
    c.text(CX, 50, b, TXT_MIN, INK_2, Align::Center);
    list_.draw(c, a, kGameCount, [](App&, int i, void*) { return ui::ListItem{kGames[i].title, nullptr, false}; }, nullptr);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    int row = list_.gesture(e, kGameCount);
    if (row >= 0) { a.sfx.play("system/click"); a.showScreen(kGames[row].id); }
  }

 private:
  ui::List list_;
};

// ---- 23. tic-tac-toe -------------------------------------------------------------------------------
class TicTacToeScreen : public Screen {
 public:
  void enter(App&) override { mode_ = 0; restart(); }
  void tick(App& a, float dt) override {
    if (mode_ == 1 && !result_ && turn_ == 2) {
      think_ -= dt;
      if (think_ <= 0) { aiMove(a); place(a, aiCell_, 2); }
    }
    if (result_ && resetIn_ > 0) resetIn_ -= dt;
  }
  void draw(App& a, Canvas& c) override {
    if (mode_ == 0) {  // who plays: the start of the game, with two choices
      c.text(CX, 50, "Крестики-нолики", TXT_MIN, INK_2, Align::Center);
      a.face.drawMini(c, Emotion::Playful, CX - 85, 92, 170, INK);
      kVsPet.draw(c, "С Питомцем");
      kVsHuman.draw(c, "Вдвоём");
      return;
    }
    for (int i = 1; i < 3; i++) {
      c.line(kX0 + i * kCell, kY0 + 4, kX0 + i * kCell, kY0 + 3 * kCell - 4, 4, INK_3);
      c.line(kX0 + 4, kY0 + i * kCell, kX0 + 3 * kCell - 4, kY0 + i * kCell, 4, INK_3);
    }
    for (int i = 0; i < 9; i++) {
      float cx = kX0 + (i % 3) * kCell + kCell / 2, cy = kY0 + (i / 3) * kCell + kCell / 2;
      bool lit = (result_ == 1 || result_ == 2) && ((winMask_ >> i) & 1);
      if (b_[i] == 1) {
        uint16_t col = lit ? ACCENT : INK;
        c.line(cx - 18, cy - 18, cx + 18, cy + 18, 8, col);
        c.line(cx + 18, cy - 18, cx - 18, cy + 18, 8, col);
      } else if (b_[i] == 2) c.ring(cx, cy, 19, 8, lit ? INK : ACCENT);
    }
    miniFace(c, a, CX, 40 + 55 * 176 / 354.f, 110);
    char st[64];
    if (result_) snprintf(st, sizeof st, "%s", result_ == 3 ? "ничья" : mode_ == 1 ? (result_ == 1 ? "ты победил" : "Питомец победил") : (result_ == 1 ? "победил X" : "победил O"));
    else if (mode_ == 2) snprintf(st, sizeof st, "ходит %s", turn_ == 1 ? "X" : "O");
    else snprintf(st, sizeof st, "%s", turn_ == 1 ? "твой ход" : "ходит Питомец");
    c.text(CX, 98, st, TXT_MIN, result_ ? ACCENT : INK_2, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (mode_ == 0) {
      if (kVsPet.hit(e.x, e.y)) { mode_ = 1; restart(); a.sfx.play("games/go"); }
      else if (kVsHuman.hit(e.x, e.y)) { mode_ = 2; restart(); a.sfx.play("games/go"); }
      return;
    }
    if (result_) { if (resetIn_ <= 0) { restart(); a.sfx.play("games/go"); } return; }
    if (mode_ == 1 && turn_ != 1) return;
    int col = (int)((e.x - kX0) / kCell), row = (int)((e.y - kY0) / kCell);
    if (e.x < kX0 || e.y < kY0 || col > 2 || row > 2) return;
    int cell = row * 3 + col;
    if (b_[cell]) { a.sfx.play("system/error"); return; }
    place(a, cell, turn_);
  }
  int checkWinner(const uint8_t* b, uint16_t* mask = nullptr) const {
    static const uint8_t L[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
    for (auto& l : L) {
      if (b[l[0]] && b[l[0]] == b[l[1]] && b[l[1]] == b[l[2]]) {
        if (mask) *mask = (uint16_t)((1 << l[0]) | (1 << l[1]) | (1 << l[2]));
        return b[l[0]];
      }
    }
    for (int i = 0; i < 9; i++) if (!b[i]) return 0;
    return 3;
  }

 private:
  static constexpr float kX0 = 117, kY0 = 148, kCell = 77.33f;  // the spec's 117..349 x 148..380
  void restart() { memset(b_, 0, sizeof(b_)); turn_ = 1; result_ = 0; winMask_ = 0; resetIn_ = 0; }
  void place(App& a, int cell, int who) {
    b_[cell] = (uint8_t)who;
    a.sfx.play("games/move");
    result_ = checkWinner(b_, &winMask_);
    if (!result_) {
      turn_ = who == 1 ? 2 : 1;
      if (mode_ == 1 && turn_ == 2) { think_ = 600; a.react(Emotion::Thinking, 700); }
      return;
    }
    resetIn_ = 600;
    if (mode_ == 2) { a.sfx.play(result_ == 3 ? "games/draw" : "games/win"); a.react(result_ == 3 ? Emotion::Confused : Emotion::Excited, 2000); return; }
    if (result_ == 1) {
      a.settings.tttWins++; a.pet.event(PetEvent::GameLose);
      a.sfx.play("games/lose"); a.react(a.rng.range(0, 2) == 0 ? Emotion::Crying : Emotion::Sad, 2400);
    } else if (result_ == 2) {
      a.settings.tttLosses++; a.pet.event(PetEvent::GameWin);
      a.sfx.play("games/win"); a.react(Emotion::VeryHappy, 2400);
    } else {
      a.settings.tttDraws++; a.pet.event(PetEvent::Play);
      a.sfx.play("games/draw"); a.react(Emotion::Confused, 2000);
    }
    a.saveSettings();
  }
  int score(uint8_t* b, int who, int depth) {
    int w = checkWinner(b);
    if (w == 2) return 10 - depth;
    if (w == 1) return depth - 10;
    if (w == 3) return 0;
    int best = who == 2 ? -100 : 100;
    for (int i = 0; i < 9; i++) {
      if (b[i]) continue;
      b[i] = (uint8_t)who;
      int v = score(b, who == 2 ? 1 : 2, depth + 1);
      b[i] = 0;
      if ((who == 2 && v > best) || (who == 1 && v < best)) best = v;
    }
    return best;
  }
  void aiMove(App& a) {
    // A friend, not a machine: strength depends on mood, so a sleepy pet is beatable.
    int blunderPct = 15 + (int)(100 - a.pet.stats.energy) / 3;
    int bestCell = -1, bestV = -100;
    int free_[9], nf = 0;
    for (int i = 0; i < 9; i++) if (!b_[i]) free_[nf++] = i;
    if (a.rng.range(0, 99) < blunderPct) { aiCell_ = free_[a.rng.range(0, nf - 1)]; return; }
    for (int k = 0; k < nf; k++) {
      int i = free_[k];
      b_[i] = 2;
      int v = score(b_, 1, 0);
      b_[i] = 0;
      if (v > bestV) { bestV = v; bestCell = i; }
    }
    aiCell_ = bestCell;
  }
  const ui::Button kVsPet = ui::pill(CX, 236, 240);
  const ui::Button kVsHuman = ui::pill(CX, 322, 240, theme::PILL_H, false);
  uint8_t b_[9];
  int turn_ = 1, result_ = 0, mode_ = 0, aiCell_ = 0;
  uint16_t winMask_ = 0;
  float think_ = 0, resetIn_ = 0;
};

// ---- 25. guess the number --------------------------------------------------------------------------
class GuessScreen : public Screen {
 public:
  void enter(App& a) override { restart(a); }
  void draw(App& a, Canvas& c) override {
    // The top half of the rim is the scale 1..100: what is still possible is lit, the dot is the guess.
    ui::drawArc(c, CX, CY, kArcR, 8, 180, 180, TRACK);
    ui::drawArc(c, CX, CY, kArcR, 8, angleOf(lo_), angleOf(hi_) - angleOf(lo_), won_ ? INK : ACCENT);
    float g = angleOf(guess_) * 0.0174533f;
    c.fillCircle(CX + cosf(g) * kArcR, CY + sinf(g) * kArcR, 9, INK);
    miniFace(c, a, CX, 58 + 50 * 176 / 354.f, 100);
    char b[48];
    snprintf(b, sizeof b, "%d", guess_);
    ui::textCentered(c, CX, 176, b, NUM_96, won_ ? ACCENT : INK);
    ui::textCentered(c, CX, 246, hint_, TXT_BODY, ACCENT);
    snprintf(b, sizeof b, "от %d до %d · попытка %d", lo_, hi_, tries_ + (won_ ? 0 : 1));
    ui::textCentered(c, CX, 284, b, TXT_MIN, INK_2);
    m10_.draw(c, "−10", !won_);
    p10_.draw(c, "+10", !won_);
    m1_.draw(c, "−1", !won_);
    p1_.draw(c, "+1", !won_);
    ok_.draw(c, won_ ? "Ещё" : "Ответ");
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    int d = m10_.hit(e.x, e.y) ? -10 : m1_.hit(e.x, e.y) ? -1 : p1_.hit(e.x, e.y) ? 1 : p10_.hit(e.x, e.y) ? 10 : 0;
    if (d && !won_) { guess_ = clampi(guess_ + d, 1, 100); a.sfx.play("games/move"); return; }
    if (!ok_.hit(e.x, e.y)) return;
    if (won_) { restart(a); return; }
    tries_++;
    if (guess_ == secret_) {
      won_ = true;
      snprintf(hint_, sizeof(hint_), "Угадал!");
      a.sfx.play("games/win");
      a.pet.event(PetEvent::GameWin);
      a.react(Emotion::VeryHappy, 2500);
    } else {
      // Every answer narrows what is left; the arc shows it.
      if (guess_ < secret_) lo_ = guess_ + 1 > lo_ ? guess_ + 1 : lo_;
      else hi_ = guess_ - 1 < hi_ ? guess_ - 1 : hi_;
      snprintf(hint_, sizeof(hint_), "%s", guess_ < secret_ ? "Больше!" : "Меньше!");
      a.sfx.play("games/wrong");
      a.react(abs(guess_ - secret_) <= 5 ? Emotion::Excited : Emotion::Thinking, 1500);
    }
  }

 private:
  void restart(App& a) {
    secret_ = a.rng.range(1, 100); guess_ = 50; tries_ = 0; won_ = false; lo_ = 1; hi_ = 100;
    hint_[0] = 0;
    a.react(Emotion::Skeptical, 1500);
    a.sfx.play("games/go");
  }
  const ui::Button m10_ = ui::secondary(84, 140), p10_ = ui::secondary(382, 140);
  const ui::Button m1_ = ui::secondary(146, 320), ok_ = ui::primary(CX, 320), p1_ = ui::secondary(320, 320);
  static constexpr float kArcR = 214;
  static float angleOf(int v) { return 180 + (v - 1) * 180 / 99.f; }  // 1 on the left, 100 on the right
  int secret_ = 1, guess_ = 50, tries_ = 0, lo_ = 1, hi_ = 100;
  bool won_ = false;
  char hint_[48] = "";
};

Screen* playScreen(ScreenId id) {
  static PetScreen pet;
  static PetGameScreen firefly;
  static GamesScreen games;
  static TicTacToeScreen ttt;
  static GuessScreen guess;
  switch (id) {
    case ScreenId::Pet: return &pet;
    case ScreenId::PetGame: return &firefly;
    case ScreenId::Games: return &games;
    case ScreenId::TicTacToe: return &ttt;
    case ScreenId::Guess: return &guess;
    default: return nullptr;
  }
}

}  // namespace cc
