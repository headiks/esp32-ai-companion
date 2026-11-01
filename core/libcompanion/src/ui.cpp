// All interface screens. Unified look: black, white, greys and muted pastels, content fits the round panel.
#include "ui.h"
#include <stdio.h>
#include <string.h>

namespace cc {

static constexpr int kCx = kScreenW / 2, kCy = kScreenH / 2;

static const char* const kScreenNames[(int)ScreenId::Count] = {
    "face", "menu", "pet", "analog_clock", "digital_clock", "timer", "stopwatch", "notifications",
    "settings", "diagnostics", "games", "info", "tictactoe", "reaction", "guess", "pet_game",
    "maze", "orbit", "snake", "simon", "coin", "dice", "magic_ball", "balance", "duel", "shaker", "safe", "workout", "mole", "weather", "controller", "mouse"};

const char* screenName(ScreenId s) { return kScreenNames[(int)s]; }
bool screenFromName(const char* n, ScreenId* out) {
  for (int i = 0; i < (int)ScreenId::Count; i++)
    if (!strcmp(n, kScreenNames[i])) { *out = (ScreenId)i; return true; }
  return false;
}

// ---- widgets ---------------------------------------------------------------------------------
void drawIcon(Canvas& c, Icon i, float x, float y, float r, uint16_t col) {
  const float t = r * 0.2f < 2 ? 2 : r * 0.2f;
  switch (i) {
    case Icon::Food:
      c.fillCircle(x - r * .28f, y + r * .1f, r * .55f, col);
      c.fillCircle(x + r * .28f, y + r * .1f, r * .55f, col);
      c.line(x, y - r * .4f, x + r * .22f, y - r * .85f, t, col);
      break;
    case Icon::Heart: c.heart(x, y - r * .1f, r * .68f, col); break;
    case Icon::Ball:
      c.ring(x, y, r * .72f, t, col);
      c.line(x - r * .7f, y, x + r * .7f, y, t * .7f, col);
      c.line(x, y - r * .7f, x, y + r * .7f, t * .7f, col);
      break;
    case Icon::Moon:
      c.fillCircle(x, y, r * .75f, col);
      c.fillCircle(x + r * .36f, y - r * .22f, r * .62f, kBlack);
      break;
    case Icon::Sun:
      c.fillCircle(x, y, r * .42f, col);
      for (int k = 0; k < 8; k++) {
        float a = k * 0.7854f, s = sinf(a), co = cosf(a);
        c.line(x + s * r * .62f, y + co * r * .62f, x + s * r * .92f, y + co * r * .92f, t * .8f, col);
      }
      break;
    case Icon::Clock:
    case Icon::Timer:
    case Icon::Stopwatch:
      c.ring(x, y, r * .75f, t, col);
      c.line(x, y, x, y - r * .5f, t, col);
      c.line(x, y, x + r * .35f, y, t, col);
      if (i == Icon::Stopwatch) c.line(x, y - r * .75f, x, y - r, t * 1.2f, col);
      if (i == Icon::Timer) { c.line(x - r * .3f, y - r, x + r * .3f, y - r, t, col); }
      break;
    case Icon::Bell:
      c.fillCircle(x, y - r * .1f, r * .52f, col);
      c.fillRect((int)(x - r * .52f), (int)(y - r * .1f), (int)(r * 1.04f), (int)(r * .5f), col);
      c.line(x - r * .72f, y + r * .42f, x + r * .72f, y + r * .42f, t, col);
      c.fillCircle(x, y + r * .68f, r * .16f, col);
      break;
    case Icon::Gear:
      c.ring(x, y, r * .48f, t * 1.3f, col);
      for (int k = 0; k < 8; k++) {
        float a = k * 0.7854f, s = sinf(a), co = cosf(a);
        c.line(x + s * r * .6f, y + co * r * .6f, x + s * r * .92f, y + co * r * .92f, t * 1.2f, col);
      }
      break;
    case Icon::Chip:
      c.roundRect(x - r * .55f, y - r * .55f, r * 1.1f, r * 1.1f, r * .12f, col);
      c.roundRect(x - r * .3f, y - r * .3f, r * .6f, r * .6f, r * .05f, kBlack);
      for (int k = -1; k <= 1; k++) {
        c.line(x + k * r * .3f, y - r * .85f, x + k * r * .3f, y + r * .85f, t * .6f, col);
        c.line(x - r * .85f, y + k * r * .3f, x + r * .85f, y + k * r * .3f, t * .6f, col);
      }
      c.roundRect(x - r * .3f, y - r * .3f, r * .6f, r * .6f, r * .05f, kBlack);
      break;
    case Icon::Game:
      c.roundRect(x - r * .85f, y - r * .42f, r * 1.7f, r * .84f, r * .35f, col);
      c.line(x - r * .55f, y, x - r * .2f, y, t * .7f, kBlack);
      c.line(x - r * .37f, y - r * .17f, x - r * .37f, y + r * .17f, t * .7f, kBlack);
      c.fillCircle(x + r * .42f, y, r * .14f, kBlack);
      break;
    case Icon::Info:
      c.ring(x, y, r * .75f, t, col);
      c.fillCircle(x, y - r * .32f, t * .7f, col);
      c.line(x, y - r * .05f, x, y + r * .4f, t, col);
      break;
    case Icon::Face:
      c.fillCircle(x - r * .4f, y, r * .32f, col);
      c.fillCircle(x + r * .4f, y, r * .32f, col);
      break;
    case Icon::Message:
      c.roundRect(x - r * .8f, y - r * .6f, r * 1.6f, r * 1.05f, r * .25f, col);
      c.line(x - r * .3f, y + r * .35f, x - r * .5f, y + r * .78f, t * 1.2f, col);
      break;
    case Icon::Mail:
      c.roundRect(x - r * .8f, y - r * .55f, r * 1.6f, r * 1.1f, r * .1f, col);
      c.line(x - r * .72f, y - r * .42f, x, y + r * .1f, t * .8f, kBlack);
      c.line(x + r * .72f, y - r * .42f, x, y + r * .1f, t * .8f, kBlack);
      break;
    case Icon::Phone:
      c.arc(x + r * .1f, y - r * .1f, r * .75f, t * 1.4f, 130, 230, col);
      c.fillCircle(x - r * .1f, y - r * .7f, r * .22f, col);
      c.fillCircle(x + r * .65f, y + r * .1f, r * .22f, col);
      break;
    case Icon::Calendar:
      c.roundRect(x - r * .72f, y - r * .6f, r * 1.44f, r * 1.3f, r * .12f, col);
      c.fillRect((int)(x - r * .55f), (int)(y - r * .22f), (int)(r * 1.1f), (int)(r * .75f), kBlack);
      c.fillCircle(x - r * .25f, y + r * .1f, r * .12f, col);
      c.fillCircle(x + r * .25f, y + r * .1f, r * .12f, col);
      break;
    case Icon::Warning:
      c.line(x, y - r * .7f, x - r * .75f, y + r * .6f, t * 1.2f, col);
      c.line(x, y - r * .7f, x + r * .75f, y + r * .6f, t * 1.2f, col);
      c.line(x - r * .75f, y + r * .6f, x + r * .75f, y + r * .6f, t * 1.2f, col);
      c.line(x, y - r * .2f, x, y + r * .15f, t, col);
      c.fillCircle(x, y + r * .4f, t * .6f, col);
      break;
  }
}

void drawTitle(Canvas& c, const char* title) { c.text(kCx, 20, title, FontSize::S, kGray, Align::Center); }

void drawButton(Canvas& c, const Button& b, const char* label, bool accent) {
  // Outlines, not filled discs: one accent key per screen is the only heavy element.
  if (accent) c.fillCircle(b.x, b.y, b.r, kAccent);
  else c.ring(b.x, b.y, b.r - 1, 2, kDim);
  FontSize f = Canvas::textWidth(label, FontSize::M) < (int)(b.r * 1.7f) ? FontSize::M : FontSize::S;
  c.text((int)b.x, (int)b.y - Canvas::lineHeight(f) / 2, label, f, accent ? kBlack : kWhite, Align::Center);
}

// 5x7 cells, one byte per row, low 5 bits, leftmost cell is bit 4.
static const uint8_t* dotGlyph(char ch) {
  static const uint8_t k[][7] = {
      {14, 17, 17, 17, 17, 17, 14}, {4, 12, 4, 4, 4, 4, 14},   {14, 17, 1, 2, 4, 8, 31},   {31, 2, 4, 2, 1, 17, 14},
      {2, 6, 10, 18, 31, 2, 2},     {31, 16, 30, 1, 1, 17, 14}, {6, 8, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
      {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 2, 12}};
  static const uint8_t colon[7] = {0, 4, 4, 0, 4, 4, 0}, minus[7] = {0, 0, 0, 14, 0, 0, 0}, dot[7] = {0, 0, 0, 0, 0, 0, 4}, blank[7] = {0};
  if (ch >= '0' && ch <= '9') return k[ch - '0'];
  return ch == ':' ? colon : ch == '-' ? minus : ch == '.' ? dot : blank;
}

static int dotCols(char ch) { return ch == ':' || ch == '.' ? 3 : 5; }

void DotText::set(const char* s) {
  if (!strcmp(s, cur_)) return;
  snprintf(prev_, sizeof(prev_), "%s", cur_);
  snprintf(cur_, sizeof(cur_), "%s", s);
  t_ = 0;
}

int DotText::width(const char* s, int cell, int gap) {
  int w = 0;
  for (; *s; s++) w += dotCols(*s) * (cell + gap) + gap * 2;
  return w - gap * 2;
}

void DotText::draw(Canvas& c, int cx, int y, int cell, int gap, uint16_t on) const {
  int x = cx - width(cur_, cell, gap) / 2;
  size_t plen = strlen(prev_);
  for (int i = 0; cur_[i]; i++) {
    char ch = cur_[i];
    int cols = dotCols(ch);
    const uint8_t* g = dotGlyph(ch);
    const uint8_t* p = dotGlyph((size_t)i < plen ? prev_[i] : ' ');
    int shift = cols == 3 ? 1 : 0;   // narrow glyphs keep their centre column
    for (int ry = 0; ry < 7; ry++) {
      for (int cl = 0; cl < cols; cl++) {
        int bit = 4 - (cl + shift);
        float a = ((g[ry] >> bit) & 1) ? 1.f : 0.f, b = ((p[ry] >> bit) & 1) ? 1.f : 0.f;
        // A diagonal wave sweeps over the digit: each cell flips at its own moment.
        float v = lerpf(b, a, smooth01((t_ - (cl + ry) * 0.05f) * 2.2f));
        int px = x + cl * (cell + gap), py = y + ry * (cell + gap);
        if (v < 0.04f) continue;
        int sz = (int)(cell * (0.45f + 0.55f * v) + 0.5f), off = (cell - sz) / 2;
        c.fillRect(px + off, py + off, sz, sz, blend565(kBlack, on, (uint32_t)(v * 255)));
      }
    }
    x += cols * (cell + gap) + gap * 2;
  }
}

void drawRimSquares(Canvas& c, float fraction, uint16_t on) {
  int lit = (int)(fraction * 60 + 0.001f);
  for (int i = 0; i < 60; i++) {
    float ang = i * 0.10472f, sx = sinf(ang), co = -cosf(ang);
    int x = (int)(kCx + sx * (kCx - 11)), y = (int)(kCy + co * (kCy - 11));
    if (i < lit) c.fillRect(x - 2, y - 2, 5, 5, on);
    else c.fillRect(x - 1, y - 1, 3, 3, i % 5 == 0 ? kDim : kDark);
  }
}

const char* weatherTitle(Weather w) {
  static const char* const t[] = {"Ясно", "Облачно", "Дождь", "Снег", "Гроза", "Туман", "Ветер"};
  return t[(int)w];
}

void drawWeatherIcon(Canvas& c, Weather w, float x, float y, float r, float t) {
  float ts = t * 0.001f;
  if (w == Weather::Clear) {
    c.fillCircle(x, y, r * 0.42f, kSoft);
    for (int k = 0; k < 12; k++) {
      float a = k * 0.5236f + ts * 0.25f, l = r * (0.62f + 0.06f * sinf(ts * 2 + k));
      c.line(x + sinf(a) * l, y + cosf(a) * l, x + sinf(a) * (l + r * 0.2f), y + cosf(a) * (l + r * 0.2f), 3, kAccent);
    }
    return;
  }
  if (w == Weather::Wind) {
    for (int k = 0; k < 3; k++) {
      float yy = y - r * 0.4f + k * r * 0.4f, off = r * 0.14f * sinf(ts * 1.6f + k);
      c.line(x - r * 0.8f + off, yy, x + r * 0.7f + off, yy, 4, kWhite);
    }
    return;
  }
  if (w == Weather::Fog) {
    for (int k = 0; k < 4; k++) {
      float yy = y - r * 0.45f + k * r * 0.3f, off = r * 0.12f * sinf(ts + k * 1.3f);
      c.line(x - r * 0.7f + off, yy, x + r * 0.7f - off, yy, 4, kGray);
    }
    return;
  }
  // A cloud from three discs, breathing slightly.
  float b = 1 + 0.03f * sinf(ts * 1.2f);
  float cy = y - r * 0.12f;
  uint16_t col = w == Weather::Storm ? kGray : kWhite;
  c.fillCircle(x - r * 0.38f, cy + r * 0.08f, r * 0.3f * b, col);
  c.fillCircle(x + r * 0.06f, cy - r * 0.1f, r * 0.42f * b, col);
  c.fillCircle(x + r * 0.46f, cy + r * 0.1f, r * 0.26f * b, col);
  c.fillRect((int)(x - r * 0.4f), (int)(cy + r * 0.1f), (int)(r * 0.88f), (int)(r * 0.26f), col);
  if (w == Weather::Rain || w == Weather::Storm) {
    for (int k = 0; k < 5; k++) {
      float ph = fmodf(ts * 1.1f + k * 0.37f, 1.f);
      float xx = x - r * 0.5f + k * r * 0.25f, yy = cy + r * 0.45f + ph * r * 0.5f;
      c.line(xx, yy, xx - 2, yy + r * 0.1f, 2.5f, kAccent);
    }
  }
  if (w == Weather::Storm && fmodf(ts * 0.5f, 1.f) < 0.2f) {
    c.line(x + 4, cy + r * 0.32f, x - r * 0.12f, cy + r * 0.65f, 4, kSoft);
    c.line(x - r * 0.12f, cy + r * 0.65f, x + r * 0.1f, cy + r * 0.65f, 4, kSoft);
    c.line(x + r * 0.1f, cy + r * 0.65f, x - r * 0.05f, cy + r, 4, kSoft);
  }
  if (w == Weather::Snow) {
    for (int k = 0; k < 5; k++) {
      float ph = fmodf(ts * 0.42f + k * 0.29f, 1.f);
      c.fillCircle(x - r * 0.5f + k * r * 0.25f + 4 * sinf(ts * 2 + k), cy + r * 0.45f + ph * r * 0.55f, 3, kWhite);
    }
  }
}

void formatTime(char* out, int cap, float ms, bool tenths) {
  uint32_t t = (uint32_t)(ms < 0 ? 0 : ms);
  uint32_t m = t / 60000, s = (t / 1000) % 60;
  if (tenths) snprintf(out, (size_t)cap, "%02u:%02u.%u", (unsigned)m, (unsigned)s, (unsigned)((t / 100) % 10));
  else snprintf(out, (size_t)cap, "%02u:%02u", (unsigned)m, (unsigned)s);
}

static constexpr int kRowH = 70;

// A wheel: rows pass through the centre of the screen, where the focused one is bright. Any row can be tapped.
void ListView::draw(Canvas& c, const char* title, int count, ListRow (*rowFn)(App&, int, void*), App& a, void* ctx) {
  for (int i = 0; i < count; i++) {
    float mid = kCy + i * kRowH - scroll_;
    if (mid < 30 || mid > kScreenH - 20) continue;
    if (!c.intersects(0, (int)mid - kRowH / 2, kScreenW, kRowH)) continue;
    ListRow r = rowFn(a, i, ctx);
    float dist = fabsf(mid - kCy) / kRowH;
    uint16_t col = dist < 0.5f ? kWhite : (dist < 1.5f ? kGray : kDim);
    if (r.value) {
      c.text(kCx, (int)mid - 25, r.title, FontSize::S, dist < 0.5f ? kGray : kDim, Align::Center);
      c.text(kCx, (int)mid - 2, r.value, FontSize::M, dist < 0.5f ? kSoft : col, Align::Center);
      if (r.adjustable && dist < 0.5f) {
        c.line(kCx - 176, mid + 10, kCx - 186, mid, 3, kAccent);
        c.line(kCx - 186, mid, kCx - 176, mid - 10, 3, kAccent);
        c.line(kCx + 176, mid + 10, kCx + 186, mid, 3, kAccent);
        c.line(kCx + 186, mid, kCx + 176, mid - 10, 3, kAccent);
      }
    } else {
      int tw = Canvas::textWidth(r.title, FontSize::M);
      if (r.hasIcon) drawIcon(c, r.icon, (float)(kCx - tw / 2 - 22), mid, 13, dist < 0.5f ? kAccent : kDim);
      c.text(kCx + (r.hasIcon ? 4 : 0), (int)mid - Canvas::lineHeight(FontSize::M) / 2, r.title, FontSize::M, col, Align::Center);
    }
  }
  drawTitle(c, title);
  float maxScroll = (count - 1) * kRowH;
  if (maxScroll > 0) {
    float at = 70 + 40 * clampf(scroll_ / maxScroll, 0, 1);
    c.arc(kCx, kCy, kCx - 8, 3, 70, 110, kDark);
    c.arc(kCx, kCy, kCx - 8, 3, at - 3, at + 3, kAccent);
  }
}

void ListView::tick(float dt, int count) {
  float maxScroll = (count - 1) * kRowH;
  if (maxScroll < 0) maxScroll = 0;
  if (!dragging_) {
    scroll_ += vel_ * dt;
    vel_ *= powf(0.993f, dt);
    if (fabsf(vel_) < 0.05f) {
      // Glide to the nearest row: the wheel always rests on an item.
      vel_ = 0;
      float snap = roundf(scroll_ / kRowH) * kRowH;
      scroll_ += (snap - scroll_) * clampf(dt / 70.f, 0, 1);
    }
  }
  if (scroll_ < 0) { scroll_ = 0; vel_ = 0; }
  if (scroll_ > maxScroll) { scroll_ = maxScroll; vel_ = 0; }
}

int ListView::gesture(const GestureEvent& e, int count, int* zone) {
  switch (e.g) {
    case Gesture::Down: dragging_ = true; vel_ = 0; break;
    case Gesture::Move:
      if (abs(e.dy) >= abs(e.dx)) { scroll_ -= e.dy; vel_ = -e.dy * 0.06f; }
      break;
    case Gesture::Up: dragging_ = false; break;
    case Gesture::Tap: {
      int row = (int)floorf((e.y - kCy + kRowH / 2.f + scroll_) / kRowH);
      if (row < 0 || row >= count || e.y < 40) return -1;
      if (zone) *zone = e.x < kCx - 80 ? -1 : (e.x > kCx + 80 ? 1 : 0);
      return row;
    }
    default: break;
  }
  return -1;
}

// ---- face ------------------------------------------------------------------------------------
class FaceScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    if (a.thunderFlashMs > 0) c.clear(kDim);
    if (a.weather.known) a.face.drawWeather(c, a.weather.cond, a.nowMs(), a.alertness() < 0.3f);
    a.face.draw(c, a.anim, a.eyeColor());
  }
  bool wantsDoubleTap() const override { return true; }
  void onGesture(App& a, const GestureEvent& e) override {
    switch (e.g) {
      case Gesture::Down:
        path_ = 0;
        a.anim.lookAt((float)e.x, (float)e.y, 900);
        if (a.pet.asleep()) a.wakeNow();
        break;
      case Gesture::Move:
        a.anim.lookAt((float)e.x, (float)e.y, 900);
        path_ += abs(e.dx) + abs(e.dy);
        if (path_ > 260) {
          // Rubbing back and forth without travelling far reads as petting.
          path_ = 0;
          a.pet.event(PetEvent::Stroke);
          a.sfx.play("interactions/pet");
          a.react(Emotion::Affection, 2200);
        }
        break;
      case Gesture::Tap:
        a.pet.event(PetEvent::Tap);
        a.anim.kick(-0.16f);
        a.sfx.play("interactions/tap");
        if (a.pet.irritatedByRepeats()) a.react(a.pet.stats.stress > 60 ? Emotion::Angry : Emotion::Annoyed, 2200);
        else a.react(a.pet.stats.curiosity > 50 ? Emotion::Curious : Emotion::Attentive, 1400);
        break;
      case Gesture::DoubleTap:
        a.pet.event(PetEvent::DoubleTap);
        a.anim.kick(-0.3f);
        a.sfx.play("interactions/double_tap");
        if (a.pet.irritatedByRepeats()) a.react(Emotion::Annoyed, 2000);
        else a.react(a.pet.stats.fun > 75 ? Emotion::Laughing : (a.pet.stats.happiness > 55 ? Emotion::Happy : Emotion::Surprised), 1800);
        break;
      case Gesture::LongPress: a.sfx.play("interactions/hold"); a.showScreen(ScreenId::Menu); break;
      case Gesture::SwipeUp: a.sfx.play("interactions/swipe"); a.showScreen(ScreenId::Settings); break;
      case Gesture::SwipeDown: a.sfx.play("interactions/swipe"); a.showScreen(ScreenId::Notifications); break;
      default: break;
    }
  }
 private:
  int path_ = 0;
};

// ---- main menu -------------------------------------------------------------------------------
struct MenuItem { const char* title; Icon icon; ScreenId target; };
static const MenuItem kMenu[] = {
    {"Лицо", Icon::Face, ScreenId::Face},
    {"Поговорить", Icon::Message, ScreenId::Count},  // not a screen: starts a voice dialog
    {"Питомец", Icon::Heart, ScreenId::Pet},
    {"Игры", Icon::Game, ScreenId::Games},
    {"Мышь", Icon::Stopwatch, ScreenId::Mouse},
    {"Погода", Icon::Sun, ScreenId::Weather},
    {"Часы", Icon::Clock, ScreenId::DigitalClock},
    {"Циферблат", Icon::Clock, ScreenId::AnalogClock},
    {"Таймер", Icon::Timer, ScreenId::Timer},
    {"Секундомер", Icon::Stopwatch, ScreenId::Stopwatch},
    {"Уведомления", Icon::Bell, ScreenId::Notifications},
    {"Настройки", Icon::Gear, ScreenId::Settings},
    {"Диагностика", Icon::Chip, ScreenId::Diagnostics},
    {"Об устройстве", Icon::Info, ScreenId::Info},
};
static constexpr int kMenuCount = sizeof(kMenu) / sizeof(kMenu[0]);

class MenuScreen : public Screen {
 public:
  void enter(App&) override { list_.reset(); }
  void tick(App&, float dt) override { list_.tick(dt, kMenuCount); }
  void draw(App& a, Canvas& c) override {
    list_.draw(c, "", kMenuCount, [](App&, int i, void*) { return ListRow{kMenu[i].title, nullptr, kMenu[i].icon, true, false}; }, a, nullptr);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    int row = list_.gesture(e, kMenuCount, nullptr);
    if (row < 0) return;
    a.sfx.play("system/click");
    if (kMenu[row].target == ScreenId::Count) a.listen();
    else a.showScreen(kMenu[row].target);
  }
 private:
  ListView list_;
};

// ---- virtual pet -----------------------------------------------------------------------------
static const char* petStatusTitle(PetStatus s) {
  static const char* const t[] = {"Доволен", "Голоден", "Устал", "Скучает", "Радуется", "Спит", "Раздражён"};
  return t[(int)s];
}

class PetScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    const PetStats& s = a.pet.stats;
    struct { Icon i; float v; } st[6] = {{Icon::Food, 100 - s.hunger}, {Icon::Sun, s.energy}, {Icon::Ball, s.fun},
                                         {Icon::Face, s.attention}, {Icon::Heart, s.affection}, {Icon::Warning, s.health}};
    // Six gauges form the rim. Each spans 54 degrees, the gaps separate them.
    for (int i = 0; i < 6; i++) {
      float a0 = i * 60.f + 3, a1 = a0 + 54;
      c.arc(kCx, kCy, kCx - 10, 4, a0, a1, kDark);
      float lvl = clampf(st[i].v, 0, 100) / 100;
      if (lvl > 0.02f) c.arc(kCx, kCy, kCx - 10, 4, a0, a0 + 54 * lvl, st[i].v < 25 ? kAccent : kSoft);
    }
    a.face.drawScaled(c, a.anim, kCx, 118, 0.5f, a.eyeColor());
    c.text(kCx, 164, petStatusTitle(a.pet.status()), FontSize::S, kGray, Align::Center);
    for (int i = 0; i < 6; i++) {
      float x = kCx - 112 + (i % 3) * 112, y = 216 + (i / 3) * 48;
      drawIcon(c, st[i].i, x - 28, y, 11, st[i].v < 25 ? kAccent : kDim);
      char pct[8];
      snprintf(pct, sizeof(pct), "%d", (int)(st[i].v + 0.5f));
      c.text((int)x - 10, (int)y - Canvas::lineHeight(FontSize::M) / 2, pct, FontSize::M, st[i].v < 25 ? kAccent : kWhite);
      c.text((int)x + Canvas::textWidth(pct, FontSize::M) - 8, (int)y - 4, "%", FontSize::S, kDim);
    }
    static const Icon icons[5] = {Icon::Food, Icon::Heart, Icon::Ball, Icon::Moon, Icon::Sun};
    for (int i = 0; i < 5; i++) {
      Button b = action(i);
      if (flash_ == i) c.fillCircle(b.x, b.y, b.r, kAccent);
      else c.ring(b.x, b.y, b.r - 1, 2, kDim);
      drawIcon(c, icons[i], b.x, b.y, 16, flash_ == i ? kBlack : kWhite);
    }
  }
  void tick(App&, float dt) override { if (flashMs_ > 0) { flashMs_ -= dt; if (flashMs_ <= 0) flash_ = -1; } }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    for (int i = 0; i < 5; i++) {
      if (!action(i).hit(e.x, e.y)) continue;
      flash_ = i; flashMs_ = 220;
      static const PetEvent ev[5] = {PetEvent::Feed, PetEvent::Pet, PetEvent::Play, PetEvent::GoSleep, PetEvent::WakeUp};
      a.petAction(ev[i]);
      return;
    }
  }
 private:
  // Five buttons along the lower arc of the circle.
  static Button action(int i) {
    static const float xs[5] = {-136, -68, 0, 68, 136}, ys[5] = {-12, 8, 16, 8, -12};
    return Button{kCx + xs[i], 346 + ys[i] + 22, 27};
  }
  int flash_ = -1;
  float flashMs_ = 0;
};

// ---- clocks ----------------------------------------------------------------------------------
static const char* const kWeekdays[7] = {"ВС", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ"};
static const char* const kMonths[12] = {"января", "февраля", "марта", "апреля", "мая", "июня", "июля", "августа", "сентября", "октября", "ноября", "декабря"};

class AnalogClockScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    DateTime d = a.clock.now();
    float secF = d.second + a.clock.secondFraction();
    drawRimSquares(c, secF / 60, kAccent);
    for (int i = 0; i < 12; i++) {
      float ang = i * 30 * 0.0174533f, s = sinf(ang), co = -cosf(ang);
      float r0 = kCx - 44, r1 = kCx - 26;
      c.line(kCx + s * r0, kCy + co * r0, kCx + s * r1, kCy + co * r1, i % 3 == 0 ? 5 : 2.5f, i % 3 == 0 ? kWhite : kGray);
    }
    char date[32];
    snprintf(date, sizeof(date), "%s %d", kWeekdays[d.weekday], d.day);
    c.text(kCx, kCy + 84, date, FontSize::S, kGray, Align::Center);
    float mF = d.minute + secF / 60, hF = (d.hour % 12) + mF / 60;
    auto hand = [&](float deg, float len, float tail, float w, uint16_t col) {
      float rad = deg * 0.0174533f, s = sinf(rad), co = -cosf(rad);
      c.line(kCx - s * tail, kCy - co * tail, kCx + s * len, kCy + co * len, w, col);
    };
    hand(hF * 30, 104, 0, 7, kWhite);
    hand(mF * 6, 164, 0, 4, kWhite);
    c.fillCircle(kCx, kCy, 7, kWhite);
    c.fillCircle(kCx, kCy, 3, kAccent);
  }
};

class DigitalClockScreen : public Screen {
 public:
  void tick(App& a, float dt) override {
    DateTime d = a.clock.now();
    char t[12];
    snprintf(t, sizeof(t), "%02d:%02d", d.hour, d.minute);
    time_.set(t);
    time_.tick(dt);
  }
  void draw(App& a, Canvas& c) override {
    DateTime d = a.clock.now();
    drawRimSquares(c, (d.second + 1) / 60.f, kAccent);
    time_.draw(c, kCx, kCy - 48, 12, 2, kWhite);
    char date[40];
    snprintf(date, sizeof(date), "%s  %d %s", kWeekdays[d.weekday], d.day, kMonths[d.month - 1]);
    c.text(kCx, kCy + 66, date, FontSize::S, kGray, Align::Center);
    if (a.weather.known) {
      char w[16];
      snprintf(w, sizeof(w), "%d°", (int)roundf(a.weather.tempC));
      drawWeatherIcon(c, a.weather.cond, kCx - 24, kCy - 96, 17, a.nowMs());
      c.text(kCx + 6, kCy - 110, w, FontSize::M, kWhite);
    } else if (!a.clock.isSet()) {
      c.text(kCx, kCy + 94, "время не задано", FontSize::S, kDim, Align::Center);
    }
  }
 private:
  DotText time_;
};

class WeatherScreen : public Screen {
 public:
  void tick(App& a, float dt) override {
    char t[12];
    snprintf(t, sizeof(t), a.weather.known ? "%d" : "--", (int)roundf(a.weather.tempC));
    temp_.set(t);
    temp_.tick(dt);
  }
  void draw(App& a, Canvas& c) override {
    Weather w = a.weather.known ? a.weather.cond : Weather::Clouds;
    drawWeatherIcon(c, w, kCx, 130, 66, a.nowMs());
    temp_.draw(c, kCx, 214, 8, 2, kWhite);
    if (a.weather.known) c.text(kCx, 300, weatherTitle(w), FontSize::M, kSoft, Align::Center);
    else c.textWrapped(kCx, 296, 300, "Нет данных: нужен Wi-Fi с координатами или приложение", FontSize::S, kGray, Align::Center, 3);
    a.face.drawScaled(c, a.anim, kCx, 392, 0.28f, a.eyeColor());
  }
 private:
  DotText temp_;
};

// ---- timer / stopwatch -----------------------------------------------------------------------
class TimerScreen : public Screen {
 public:
  void tick(App& a, float dt) override {
    char t[16];
    formatTime(t, sizeof(t), (a.timerRunning || a.timerLeftMs > 0 ? a.timerLeftMs : a.timerSetMs) + 999, false);
    time_.set(t);
    time_.tick(dt);
  }
  void draw(App& a, Canvas& c) override {
    float ms = a.timerRunning || a.timerLeftMs > 0 ? a.timerLeftMs : a.timerSetMs;
    drawRimSquares(c, a.timerSetMs > 0 ? clampf(ms / a.timerSetMs, 0, 1) : 0, kAccent);
    drawTitle(c, "");
    time_.draw(c, kCx, kCy - 104, 11, 2, kWhite);
    drawButton(c, kMinus, "-1");
    drawButton(c, kPlus, "+1");
    drawButton(c, kGo, a.timerRunning ? "Пауза" : "Старт", true);
    drawButton(c, kReset, "Сброс");
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (kMinus.hit(e.x, e.y) && !a.timerRunning) {
      a.timerSetMs = a.timerSetMs > 60000 ? a.timerSetMs - 60000 : 60000;
      a.timerLeftMs = 0; a.sfx.play("system/click");
    } else if (kPlus.hit(e.x, e.y) && !a.timerRunning) {
      a.timerSetMs = a.timerSetMs < 99 * 60000 ? a.timerSetMs + 60000 : a.timerSetMs;
      a.timerLeftMs = 0; a.sfx.play("system/click");
    } else if (kGo.hit(e.x, e.y)) {
      if (!a.timerRunning && a.timerLeftMs <= 0) a.timerLeftMs = a.timerSetMs;
      a.timerRunning = !a.timerRunning;
      a.sfx.play("system/click");
    } else if (kReset.hit(e.x, e.y)) {
      a.timerRunning = false; a.timerLeftMs = 0; a.sfx.play("system/click");
    }
  }
 private:
  DotText time_;
  static constexpr Button kMinus{kCx - 112, kCy + 40, 36}, kPlus{kCx + 112, kCy + 40, 36};
  static constexpr Button kGo{kCx - 52, kCy + 128, 46}, kReset{kCx + 52, kCy + 128, 46};
};
constexpr Button TimerScreen::kMinus, TimerScreen::kPlus, TimerScreen::kGo, TimerScreen::kReset;

class StopwatchScreen : public Screen {
 public:
  void tick(App& a, float dt) override {
    char t[16];
    formatTime(t, sizeof(t), a.swMs, false);
    time_.set(t);
    time_.tick(dt);
    char l[8];
    snprintf(l, sizeof(l), ".%d", ((int)a.swMs / 100) % 10);
    tenths_.set(l);
    tenths_.tick(dt * 4);
  }
  void draw(App& a, Canvas& c) override {
    drawRimSquares(c, fmodf(a.swMs / 1000.f, 60.f) / 60, kAccent);
    time_.draw(c, kCx, kCy - 106, 11, 2, kWhite);
    tenths_.draw(c, kCx, kCy - 8, 5, 1, kAccent);
    if (a.swLap > 0) {
      char t[16], lap[24];
      formatTime(t, sizeof(t), a.swLap, true);
      snprintf(lap, sizeof(lap), "Круг %s", t);
      c.text(kCx, kCy + 44, lap, FontSize::S, kGray, Align::Center);
    }
    drawButton(c, kGo, a.swRunning ? "Стоп" : "Старт", true);
    drawButton(c, kLap, a.swRunning ? "Круг" : "Сброс");
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (kGo.hit(e.x, e.y)) { a.swRunning = !a.swRunning; a.sfx.play("system/click"); }
    else if (kLap.hit(e.x, e.y)) {
      if (a.swRunning) a.swLap = a.swMs;
      else { a.swMs = 0; a.swLap = 0; }
      a.sfx.play("system/click");
    }
  }
 private:
  DotText time_, tenths_;
  static constexpr Button kGo{kCx - 56, kCy + 124, 48}, kLap{kCx + 56, kCy + 124, 48};
};
constexpr Button StopwatchScreen::kGo, StopwatchScreen::kLap;

// ---- notifications ---------------------------------------------------------------------------
static Icon iconOf(uint8_t i) {
  static const Icon m[] = {Icon::Bell, Icon::Message, Icon::Mail, Icon::Phone, Icon::Calendar, Icon::Warning};
  return i < 6 ? m[i] : Icon::Bell;
}

class NotificationsScreen : public Screen {
 public:
  void enter(App&) override { open_ = -1; list_.reset(); }
  void tick(App& a, float dt) override { list_.tick(dt, rows(a)); }
  void draw(App& a, Canvas& c) override {
    int n = a.notifications.count();
    if (open_ >= 0 && open_ < n) {
      const Notification& m = a.notifications.at(open_);
      drawTitle(c, "Уведомление");
      drawIcon(c, iconOf(m.icon), kCx, 76, 22, kAccent);
      int y = c.textWrapped(kCx, 108, 340, m.title, FontSize::M, kWhite, Align::Center, 2);
      c.textWrapped(kCx, y + 8, 370, m.text, FontSize::S, kWhite, Align::Center, 7);
      DateTime d = civilFromEpoch(m.time + a.clock.tzMinutes() * 60);
      char t[40];
      snprintf(t, sizeof(t), "%02d:%02d  %s", d.hour, d.minute, m.source);
      c.text(kCx, kScreenH - 64, t, FontSize::S, kGray, Align::Center);
      return;
    }
    if (n == 0) {
      drawTitle(c, "Уведомления");
      drawIcon(c, Icon::Bell, kCx, kCy - 30, 36, kDim);
      c.text(kCx, kCy + 20, "Пока ничего нет", FontSize::M, kGray, Align::Center);
      return;
    }
    list_.draw(c, "", n + 1, [](App& ap, int i, void*) {
      if (i >= ap.notifications.count()) return ListRow{"Очистить всё", nullptr, Icon::Warning, true, false};
      const Notification& m = ap.notifications.at(i);
      static char buf[60];
      snprintf(buf, sizeof(buf), "%s%s", m.read ? "" : "• ", m.title);
      return ListRow{buf, nullptr, iconOf(m.icon), true, false};
    }, a, nullptr);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (open_ >= 0) {
      if (e.g == Gesture::Tap) { open_ = -1; a.sfx.play("system/click"); }
      return;
    }
    int row = list_.gesture(e, rows(a), nullptr);
    if (row < 0) return;
    a.sfx.play("system/click");
    if (row >= a.notifications.count()) { a.notifications.clear(); list_.reset(); }
    else { open_ = row; a.notifications.markRead(row); }
  }
 private:
  static int rows(App& a) { return a.notifications.count() ? a.notifications.count() + 1 : 0; }
  ListView list_;
  int open_ = -1;
};

// ---- settings --------------------------------------------------------------------------------
enum SettingRow {
  kBright, kVolume, kSound, kCatEmo, kCatInt, kCatSys, kCatGame, kMic, kMicSens, kTouchSens, kAnim, kBlink,
  kSleep, kSave, kPersona, kWifi, kBle, kAiConsent, kReset, kSettingRows
};

class SettingsScreen : public Screen {
 public:
  void enter(App&) override { list_.reset(); confirm_ = false; }
  void tick(App&, float dt) override { list_.tick(dt, kSettingRows); }
  void draw(App& a, Canvas& c) override {
    list_.draw(c, "", kSettingRows, [](App& ap, int i, void* ctx) {
      static char v[28];
      const Settings& s = ap.settings;
      auto onoff = [](bool b) { return b ? "Вкл" : "Выкл"; };
      switch (i) {
        case kBright: snprintf(v, sizeof(v), "%d%%", s.brightness); return ListRow{"Яркость", v, Icon::Sun, false, true};
        case kVolume: snprintf(v, sizeof(v), "%d%%", s.volume); return ListRow{"Громкость", v, Icon::Bell, false, true};
        case kSound: return ListRow{"Звук", onoff(s.soundOn), Icon::Bell, false, true};
        case kCatEmo: return ListRow{"Звуки эмоций", onoff(s.catEmotion), Icon::Bell, false, true};
        case kCatInt: return ListRow{"Звуки касаний", onoff(s.catInteraction), Icon::Bell, false, true};
        case kCatSys: return ListRow{"Системные звуки", onoff(s.catSystem), Icon::Bell, false, true};
        case kCatGame: return ListRow{"Звуки игр", onoff(s.catGame), Icon::Bell, false, true};
        case kMic: return ListRow{"Микрофоны", onoff(s.micOn), Icon::Bell, false, true};
        case kMicSens: snprintf(v, sizeof(v), "%d%%", s.micSensitivity); return ListRow{"Чувств. микрофонов", v, Icon::Bell, false, true};
        case kTouchSens: snprintf(v, sizeof(v), "%d%%", s.touchSensitivity); return ListRow{"Чувств. касаний", v, Icon::Bell, false, true};
        case kAnim: snprintf(v, sizeof(v), "%d%%", s.animSpeed); return ListRow{"Скорость анимации", v, Icon::Bell, false, true};
        case kBlink: snprintf(v, sizeof(v), "%d%%", s.blinkRate); return ListRow{"Частота моргания", v, Icon::Bell, false, true};
        case kSleep: snprintf(v, sizeof(v), "%d мин", s.sleepAfterS / 60); return ListRow{"Переход в сон", v, Icon::Moon, false, true};
        case kSave: return ListRow{"Энергосбережение", onoff(s.powerSave), Icon::Bell, false, true};
        case kPersona: return ListRow{"Характер", personalityTitle((Personality)s.personality), Icon::Heart, false, true};
        case kWifi: snprintf(v, sizeof(v), "%s", s.wifiOn ? (s.wifiSsid[0] ? s.wifiSsid : "Вкл, нет сети") : "Выкл"); return ListRow{"Wi-Fi", v, Icon::Bell, false, true};
        case kBle: return ListRow{"Bluetooth", onoff(s.bleOn), Icon::Bell, false, true};
        case kAiConsent: return ListRow{"Голос на AI-сервер", s.aiVoiceConsent ? "Разрешён" : "Запрещён", Icon::Bell, false, true};
        default: return ListRow{"Сбросить всё", *(bool*)ctx ? "Точно? Нажми ещё раз" : "Настройки и питомец", Icon::Warning, false, false};
      }
    }, a, &confirm_);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    int zone = 0;
    int row = list_.gesture(e, kSettingRows, &zone);
    if (row < 0) return;
    int dir = zone < 0 ? -1 : 1;
    Settings& s = a.settings;
    auto step = [&](uint8_t& v, int d, int lo, int hi) { v = (uint8_t)clampi(v + d * dir, lo, hi); };
    switch (row) {
      case kBright: step(s.brightness, 10, 10, 100); break;
      case kVolume: step(s.volume, 10, 0, 100); break;
      case kSound: s.soundOn = !s.soundOn; break;
      case kCatEmo: s.catEmotion = !s.catEmotion; break;
      case kCatInt: s.catInteraction = !s.catInteraction; break;
      case kCatSys: s.catSystem = !s.catSystem; break;
      case kCatGame: s.catGame = !s.catGame; break;
      case kMic: s.micOn = !s.micOn; break;
      case kMicSens: step(s.micSensitivity, 10, 10, 100); break;
      case kTouchSens: step(s.touchSensitivity, 10, 10, 100); break;
      case kAnim: step(s.animSpeed, 10, 50, 200); break;
      case kBlink: step(s.blinkRate, 20, 0, 200); break;
      case kSleep: s.sleepAfterS = (uint16_t)clampi(s.sleepAfterS + dir * 60, 60, 3600); break;
      case kSave: s.powerSave = !s.powerSave; break;
      case kPersona: s.personality = (uint8_t)((s.personality + (int)Personality::Count + dir) % (int)Personality::Count); break;
      case kWifi: s.wifiOn = !s.wifiOn; a.netDirty = true; break;
      case kBle: s.bleOn = !s.bleOn; a.netDirty = true; break;
      case kAiConsent: s.aiVoiceConsent = !s.aiVoiceConsent; break;
      case kReset:
        if (confirm_) { confirm_ = false; a.factoryReset(); a.showScreen(ScreenId::Face); return; }
        confirm_ = true;
        break;
    }
    if (row != kReset) confirm_ = false;
    a.applySettings();
    a.saveSettings();
    a.sfx.play(row == kVolume ? "system/volume" : "system/click");
  }
 private:
  ListView list_;
  bool confirm_ = false;
};

// ---- diagnostics / info ----------------------------------------------------------------------
class DiagnosticsScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    drawTitle(c, "Диагностика");
    const HalInfo& h = a.hal;
    char b[64];
    int y = 52;
    auto row = [&](const char* s, uint16_t col = kWhite) { c.text(kCx, y, s, FontSize::S, col, Align::Center); y += 25; };
    snprintf(b, sizeof(b), "%.0f FPS   %s", (double)h.fps, h.platform); row(b, kSoft);
    snprintf(b, sizeof(b), "RAM %u  PSRAM %u KB", (unsigned)(h.freeHeap / 1024), (unsigned)(h.freePsram / 1024)); row(b);
    snprintf(b, sizeof(b), "Сенсор:%s  IMU:%s  PMIC:%s", h.touchOk ? "OK" : "НЕТ", h.imuOk ? "OK" : "НЕТ", h.pmicOk ? "OK" : "НЕТ"); row(b);
    snprintf(b, sizeof(b), "Кодек:%s  Микрофон:%s", h.codecOk ? "OK" : "НЕТ", h.micOk ? "OK" : "НЕТ"); row(b);
    snprintf(b, sizeof(b), "Акб: %d%% %d мВ%s", a.power.batteryPercent(), a.power.batteryMv(), a.power.charging() ? " заряд" : ""); row(b);
    snprintf(b, sizeof(b), "Уск: %.2f %.2f %.2f", (double)h.ax, (double)h.ay, (double)h.az); row(b);
    snprintf(b, sizeof(b), "Звук: %.0f дБ%s", (double)a.mic.levelDb(), a.mic.voiceActive() ? "  голос" : ""); row(b);
    float m = clampf((a.mic.levelDb() + 70) / 70, 0, 1);
    c.roundRect(kCx - 110, y + 2, 220, 8, 4, kDark);
    if (m > 0.04f) c.roundRect(kCx - 110, y + 2, 220 * m, 8, 4, kAccent);
    y += 20;
    snprintf(b, sizeof(b), "BLE:%s  Wi-Fi:%s", h.bleConnected ? "связь" : "нет", h.wifiConnected ? h.ip : "нет"); row(b);
    snprintf(b, sizeof(b), "Эмоция: %s", emotionName(a.emotion.current())); row(b, kSoft);
    snprintf(b, sizeof(b), "Касание: %d, %d", a.touch.x(), a.touch.y()); row(b, kGray);
    if (a.touch.isDown()) { c.ring((float)a.touch.x(), (float)a.touch.y(), 22, 3, kAccent); }
  }
};

class InfoScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    drawTitle(c, "Об устройстве");
    drawIcon(c, Icon::Face, kCx, 76, 30, kWhite);
    const HalInfo& h = a.hal;
    char b[64];
    int y = 104;
    auto row = [&](const char* s, uint16_t col = kWhite) { c.text(kCx, y, s, FontSize::S, col, Align::Center); y += 27; };
    row("AI-компаньон", kSoft);
    row("ESP32-S3-Touch-AMOLED-1.75C");
    snprintf(b, sizeof(b), "Прошивка %s, протокол %d", kFirmwareVersion, kProtocolVersion); row(b);
    snprintf(b, sizeof(b), "Flash %u МБ  PSRAM %u МБ", (unsigned)h.flashMb, (unsigned)h.psramMb); row(b);
    snprintf(b, sizeof(b), "MAC %s", h.mac); row(b, kGray);
    snprintf(b, sizeof(b), "IP %s", h.ip); row(b, kGray);
    snprintf(b, sizeof(b), "BLE: Companion-%s", h.mac[0] && strlen(h.mac) >= 17 ? h.mac + 12 : "?"); row(b);
    snprintf(b, sizeof(b), "PIN для приложений: %s", a.settings.pin); row(b, kAccent);
    snprintf(b, sizeof(b), "AI: %s", a.aiClient().name()); row(b, kGray);
  }
};

// ---- games -----------------------------------------------------------------------------------
struct GameItem { const char* title; ScreenId id; };
static const GameItem kGames[] = {{"Лабиринт", ScreenId::Maze}, {"Орбита", ScreenId::Orbit}, {"Монетка", ScreenId::Coin}, {"Кубики", ScreenId::Dice},
                                  {"Шар предсказаний", ScreenId::MagicBall}, {"Дуэль", ScreenId::Duel}, {"Шейкер", ScreenId::Shaker}, {"Сейф", ScreenId::Safe},
                                  {"Баланс", ScreenId::Balance}, {"Зарядка", ScreenId::Workout}, {"Кротики", ScreenId::Mole}, {"Змейка", ScreenId::Snake},
                                  {"Память", ScreenId::Simon}, {"Крестики-нолики", ScreenId::TicTacToe}, {"Реакция", ScreenId::Reaction},
                                  {"Угадай число", ScreenId::Guess}, {"Светлячок", ScreenId::PetGame}};
static constexpr int kGameCount = sizeof(kGames) / sizeof(kGames[0]);

class GamesScreen : public Screen {
 public:
  void enter(App&) override { list_.reset(); }
  void tick(App&, float dt) override { list_.tick(dt, kGameCount); }
  void draw(App& a, Canvas& c) override {
    list_.draw(c, "", kGameCount, [](App&, int i, void*) { return ListRow{kGames[i].title, nullptr, Icon::Game, true, false}; }, a, nullptr);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    int row = list_.gesture(e, kGameCount, nullptr);
    if (row >= 0) { a.sfx.play("system/click"); a.showScreen(kGames[row].id); }
  }
 private:
  ListView list_;
};

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
    drawTitle(c, "Крестики-нолики");
    if (mode_ == 0) {
      a.face.drawScaled(c, a.anim, kCx, 120, 0.42f);
      drawButton(c, kVsPet, "С Питомцем", true);
      drawButton(c, kVsHuman, "Вдвоём");
      return;
    }
    const int s = 86, x0 = kCx - s * 3 / 2, y0 = 118;
    for (int i = 1; i < 3; i++) {
      c.line((float)x0 + i * s, (float)y0 + 6, (float)x0 + i * s, (float)y0 + 3 * s - 6, 4, kDim);
      c.line((float)x0 + 6, (float)y0 + i * s, (float)x0 + 3 * s - 6, (float)y0 + i * s, 4, kDim);
    }
    for (int i = 0; i < 9; i++) {
      float cx = x0 + (i % 3) * s + s / 2.f, cy = y0 + (i / 3) * s + s / 2.f;
      bool hl = result_ == 1 || result_ == 2 ? ((winMask_ >> i) & 1) : false;
      if (b_[i] == 1) {
        uint16_t col = hl ? kAccent : kWhite;
        c.line(cx - 22, cy - 22, cx + 22, cy + 22, 9, col);
        c.line(cx + 22, cy - 22, cx - 22, cy + 22, 9, col);
      } else if (b_[i] == 2) c.ring(cx, cy, 24, 9, hl ? kAccent : kSoft);
    }
    a.face.drawScaled(c, a.anim, kCx, 80, 0.2f);
    char st[64];
    if (result_) {
      const char* r = result_ == 3 ? "Ничья" : (mode_ == 1 ? (result_ == 1 ? "Ты победил!" : "Я выиграл!") : (result_ == 1 ? "Победил X" : "Победил O"));
      snprintf(st, sizeof(st), "%s", r);
    } else if (mode_ == 2) snprintf(st, sizeof(st), "Ходит %s", turn_ == 1 ? "X" : "O");
    else snprintf(st, sizeof(st), "%d : %d : %d", a.settings.tttWins, a.settings.tttDraws, a.settings.tttLosses);
    c.text(kCx, y0 + 3 * s + 4, st, FontSize::M, kSoft, Align::Center);
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
    const int s = 86, x0 = kCx - s * 3 / 2, y0 = 118;
    int col = (e.x - x0) / s, row = (e.y - y0) / s;
    if (e.x < x0 || e.y < y0 || col > 2 || row > 2) return;
    int cell = row * 3 + col;
    if (b_[cell]) { a.sfx.play("system/error"); return; }
    place(a, cell, turn_);
  }
  // Test access: plays a move through the same rules as touch.
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
  static constexpr Button kVsPet{kCx - 84, kCy + 76, 72}, kVsHuman{kCx + 84, kCy + 76, 72};
  uint8_t b_[9];
  int turn_ = 1, result_ = 0, mode_ = 0, aiCell_ = 0;
  uint16_t winMask_ = 0;
  float think_ = 0, resetIn_ = 0;
};
constexpr Button TicTacToeScreen::kVsPet, TicTacToeScreen::kVsHuman;

class ReactionScreen : public Screen {
 public:
  void enter(App& a) override { state_ = Idle; a.react(Emotion::Attentive, 1000); }
  void tick(App& a, float dt) override {
    if (state_ == Waiting) {
      waitMs_ -= dt;
      if (waitMs_ <= 0) { state_ = Signal; elapsed_ = 0; a.sfx.play("games/go"); a.react(Emotion::Surprised, 500); }
    } else if (state_ == Signal) {
      elapsed_ += dt;
      if (elapsed_ > 3000) { state_ = Missed; a.sfx.play("games/wrong"); a.react(Emotion::Bored, 1500); }
    }
  }
  void draw(App& a, Canvas& c) override {
    char b[48];
    switch (state_) {
      case Signal:
        c.fillCircle(kCx, kCy, 200, kAccent);
        c.text(kCx, kCy - 24, "ЖМИ!", FontSize::L, kBlack, Align::Center);
        return;
      case Idle:
        drawTitle(c, "Реакция");
        a.face.drawScaled(c, a.anim, kCx, 130, 0.42f);
        c.textWrapped(kCx, 200, 320, "Нажми, когда экран загорится", FontSize::M, kWhite, Align::Center);
        break;
      case Waiting:
        a.face.drawScaled(c, a.anim, kCx, 180, 0.6f);
        c.text(kCx, kCy + 60, "Жди…", FontSize::L, kWhite, Align::Center);
        return;
      case Early:
        a.face.drawScaled(c, a.anim, kCx, 130, 0.42f);
        c.text(kCx, 200, "Рано!", FontSize::L, kAccent, Align::Center);
        break;
      case Missed:
        a.face.drawScaled(c, a.anim, kCx, 130, 0.42f);
        c.text(kCx, 200, "Проспал", FontSize::L, kAccent, Align::Center);
        break;
      case Result:
        a.face.drawScaled(c, a.anim, kCx, 120, 0.36f);
        snprintf(b, sizeof(b), "%d мс", last_);
        c.text(kCx, 180, b, FontSize::L, kSoft, Align::Center);
        break;
    }
    if (a.settings.reactionBest) { snprintf(b, sizeof(b), "Рекорд %d мс", a.settings.reactionBest); c.text(kCx, 284, b, FontSize::S, kGray, Align::Center); }
    c.text(kCx, 340, "Нажми, чтобы начать", FontSize::S, kGray, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap && e.g != Gesture::Down) return;
    if (e.g == Gesture::Down) {
      // Measure on the press itself: waiting for release would add finger-lift time.
      if (state_ == Signal) {
        last_ = (int)elapsed_;
        state_ = Result;
        ignoreTap_ = true;
        a.sfx.play("games/correct");
        bool record = !a.settings.reactionBest || last_ < a.settings.reactionBest;
        if (record) { a.settings.reactionBest = (uint16_t)last_; a.saveSettings(); }
        a.pet.event(PetEvent::Play);
        a.react(record ? Emotion::VeryHappy : (last_ < 350 ? Emotion::Happy : Emotion::Neutral), 1800);
      } else if (state_ == Waiting) {
        state_ = Early;
        ignoreTap_ = true;
        a.sfx.play("games/wrong");
        a.react(Emotion::Annoyed, 1500);
      }
      return;
    }
    if (ignoreTap_) { ignoreTap_ = false; return; }
    if (state_ != Waiting && state_ != Signal) {
      state_ = Waiting;
      waitMs_ = a.rng.rangef(1500, 4500);
      a.react(Emotion::Attentive, 2000);
    }
  }
  bool capturesSwipes() const override { return state_ == Waiting || state_ == Signal; }
 private:
  enum State { Idle, Waiting, Signal, Result, Early, Missed } state_ = Idle;
  float waitMs_ = 0, elapsed_ = 0;
  int last_ = 0;
  bool ignoreTap_ = false;
};

class GuessScreen : public Screen {
 public:
  void enter(App& a) override { restart(a); }
  void tick(App&, float dt) override {
    char b[8];
    snprintf(b, sizeof(b), "%d", guess_);
    num_.set(b);
    num_.tick(dt);
  }
  void draw(App& a, Canvas& c) override {
    a.face.drawScaled(c, a.anim, kCx, 86, 0.34f);
    char b[32];
    num_.draw(c, kCx, 142, 11, 2, won_ ? kAccent : kWhite);
    c.text(kCx, 256, hint_, FontSize::M, kSoft, Align::Center);
    snprintf(b, sizeof(b), "Попыток: %d", tries_);
    c.text(kCx, 290, b, FontSize::S, kGray, Align::Center);
    drawButton(c, kM10, "-10"); drawButton(c, kM1, "-1"); drawButton(c, kP1, "+1"); drawButton(c, kP10, "+10");
    drawButton(c, kOk, won_ ? "Ещё" : "Ответ", true);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    int d = kM10.hit(e.x, e.y) ? -10 : kM1.hit(e.x, e.y) ? -1 : kP1.hit(e.x, e.y) ? 1 : kP10.hit(e.x, e.y) ? 10 : 0;
    if (d && !won_) { guess_ = clampi(guess_ + d, 1, 100); a.sfx.play("games/move"); return; }
    if (!kOk.hit(e.x, e.y)) return;
    if (won_) { restart(a); return; }
    tries_++;
    if (guess_ == secret_) {
      won_ = true;
      snprintf(hint_, sizeof(hint_), "Угадал!");
      a.sfx.play("games/win");
      a.pet.event(PetEvent::GameWin);
      a.react(Emotion::VeryHappy, 2500);
    } else {
      snprintf(hint_, sizeof(hint_), "%s", guess_ < secret_ ? "Моё больше" : "Моё меньше");
      a.sfx.play("games/wrong");
      a.react(abs(guess_ - secret_) <= 5 ? Emotion::Excited : Emotion::Thinking, 1500);
    }
  }
 private:
  void restart(App& a) {
    secret_ = a.rng.range(1, 100); guess_ = 50; tries_ = 0; won_ = false;
    snprintf(hint_, sizeof(hint_), "От 1 до 100");
    a.react(Emotion::Curious, 1500);
    a.sfx.play("games/go");
  }
  static constexpr Button kM10{kCx - 150, 292, 31}, kM1{kCx - 84, 342, 31}, kP1{kCx + 84, 342, 31}, kP10{kCx + 150, 292, 31}, kOk{kCx, 382, 42};
  DotText num_;
  int secret_ = 1, guess_ = 50, tries_ = 0;
  bool won_ = false;
  char hint_[48] = "";
};
constexpr Button GuessScreen::kM10, GuessScreen::kM1, GuessScreen::kP1, GuessScreen::kP10, GuessScreen::kOk;

// Pet mini game: the character chases a firefly with its eyes; the player catches it.
class PetGameScreen : public Screen {
 public:
  void enter(App& a) override { running_ = false; score_ = 0; finished_ = false; a.react(Emotion::Curious, 1500); }
  void tick(App& a, float dt) override {
    if (!running_) return;
    left_ -= dt;
    float ts = a.nowMs() * 0.001f;
    float speed = 1.0f + score_ * 0.06f;
    fx_ = kCx + 150 * sinf(ts * 1.3f * speed + phase_) * cosf(ts * 0.37f);
    fy_ = kCy + 140 * sinf(ts * 0.9f * speed + phase_ * 1.7f);
    a.anim.lookAt(fx_, fy_, 200);
    if (left_ <= 0) {
      running_ = false; finished_ = true;
      a.pet.event(PetEvent::Play);
      a.pet.stats.fun = clampf(a.pet.stats.fun + score_ * 1.5f, 0, 100);
      if (score_ >= 8) { a.pet.event(PetEvent::GameWin); a.sfx.play("games/win"); a.react(Emotion::VeryHappy, 2500); }
      else { a.sfx.play("games/draw"); a.react(Emotion::Happy, 2000); }
    }
  }
  void draw(App& a, Canvas& c) override {
    if (running_) {
      a.face.draw(c, a.anim);
      c.fillCircle(fx_, fy_, 22, kAccent);
      c.fillCircle(fx_, fy_, 11, kSoft);
      char b[24];
      snprintf(b, sizeof(b), "%d   %d с", score_, (int)(left_ / 1000) + 1);
      c.text(kCx, 28, b, FontSize::M, kSoft, Align::Center);
      return;
    }
    drawTitle(c, "Поймай светлячка");
    a.face.drawScaled(c, a.anim, kCx, 130, 0.5f);
    char b[32];
    if (finished_) { snprintf(b, sizeof(b), "Поймано: %d", score_); c.text(kCx, 210, b, FontSize::L, kSoft, Align::Center); }
    c.text(kCx, 300, "Нажми, чтобы начать", FontSize::M, kWhite, Align::Center);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (!running_) {
      running_ = true; finished_ = false; score_ = 0; left_ = 20000; phase_ = a.rng.rangef(0, 6);
      a.sfx.play("games/go");
      a.react(Emotion::Excited, 2000);
      return;
    }
    float dx = e.x - fx_, dy = e.y - fy_;
    if (dx * dx + dy * dy < 46 * 46) {
      score_++;
      phase_ += a.rng.rangef(1, 3);
      a.sfx.play("games/correct");
      a.anim.kick(-0.25f);
      a.react(Emotion::Happy, 800);
    } else a.sfx.play("games/move");
  }
  bool capturesSwipes() const override { return running_; }
 private:
  bool running_ = false, finished_ = false;
  int score_ = 0;
  float left_ = 0, fx_ = kCx, fy_ = kCy, phase_ = 0;
};

Screen* screenInstance(ScreenId id) {
  static FaceScreen face; static MenuScreen menu; static PetScreen pet; static AnalogClockScreen analog;
  static DigitalClockScreen digital; static TimerScreen timer; static StopwatchScreen sw;
  static NotificationsScreen notes; static SettingsScreen settings; static DiagnosticsScreen diag;
  static GamesScreen games; static InfoScreen info; static TicTacToeScreen ttt; static ReactionScreen react;
  static GuessScreen guess; static PetGameScreen pg; static WeatherScreen weather;
  Screen* all[(int)ScreenId::Count] = {&face, &menu, &pet, &analog, &digital, &timer, &sw, &notes, &settings, &diag, &games, &info, &ttt, &react, &guess, &pg,
                                       gameScreen(ScreenId::Maze), gameScreen(ScreenId::Orbit), gameScreen(ScreenId::Snake), gameScreen(ScreenId::Simon),
                                       partyScreen(ScreenId::Coin), partyScreen(ScreenId::Dice), partyScreen(ScreenId::MagicBall), partyScreen(ScreenId::Balance),
                                       partyScreen(ScreenId::Duel), partyScreen(ScreenId::Shaker), partyScreen(ScreenId::Safe), partyScreen(ScreenId::Workout),
                                       partyScreen(ScreenId::Mole), &weather, partyScreen(ScreenId::Controller), partyScreen(ScreenId::Mouse)};
  return all[(int)id];
}

}  // namespace cc
