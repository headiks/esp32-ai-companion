// All interface screens. Unified look: black, white, greys and muted pastels, content fits the round panel.
#include "ui.h"
#include <stdio.h>
#include <string.h>

namespace cc {

static constexpr int kCx = kScreenW / 2, kCy = kScreenH / 2;

static const char* const kScreenNames[(int)ScreenId::Count] = {
    "face", "menu", "pet", "analog_clock", "digital_clock", "timer", "stopwatch", "notifications",
    "settings", "diagnostics", "games", "info", "tictactoe", "reaction", "guess", "pet_game",
    "maze", "orbit", "snake", "simon", "coin", "dice", "magic_ball", "balance", "duel", "shaker", "safe", "workout", "mole", "blackjack", "weather", "controller", "mouse",
    "talk", "setting_value", "clocks", "remotes", "ble_pairing"};

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

void miniFace(Canvas& c, App& a, float cx, float cy, float width) {
  float k = width / 354;  // the face block is 354 x 176 at full size, its centre 88 px below its top
  a.face.drawMini(c, a.anim, cx - width / 2, cy - 88 * k, width, a.eyeColor());
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

Screen* screenInstance(ScreenId id) {
  if (Screen* s = systemScreen(id)) return s;
  if (Screen* s = timeScreen(id)) return s;
  if (Screen* s = playScreen(id)) return s;
  switch (id) {
    case ScreenId::Maze: case ScreenId::Orbit: case ScreenId::Snake: case ScreenId::Simon: return gameScreen(id);
    case ScreenId::BlePairing: return partyScreen(ScreenId::Mouse);  // shared until the pairing screen of stage 8
    default: return partyScreen(id);
  }
}


}  // namespace cc
