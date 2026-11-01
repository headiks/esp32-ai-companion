// Internal: screen base class and widgets. One static instance of every screen, no heap use.
#pragma once
#include "companion/app.h"

namespace cc {

class Screen {
 public:
  virtual ~Screen() {}
  virtual void enter(App&) {}
  virtual void tick(App&, float) {}
  virtual void draw(App&, Canvas&) = 0;
  virtual void onGesture(App&, const GestureEvent&) {}
  virtual bool wantsDoubleTap() const { return false; }
  virtual bool capturesSwipes() const { return false; }  // games use the whole screen
  virtual bool onMotion(App&, MotionEvent) { return false; }  // true: the game used it, no pet reaction
};

enum class Icon : uint8_t { Food, Heart, Ball, Moon, Sun, Clock, Bell, Gear, Chip, Game, Info, Timer, Stopwatch, Face, Message, Mail, Phone, Calendar, Warning };
void drawIcon(Canvas& c, Icon i, float cx, float cy, float r, uint16_t color);

// Scrolling list used by menus and settings.
struct ListRow {
  const char* title;
  const char* value;       // nullptr for plain rows
  Icon icon;
  bool hasIcon;
  bool adjustable;         // shows < > and reports left/right zones
};

class ListView {
 public:
  void reset() { scroll_ = 0; vel_ = 0; dragging_ = false; }
  void draw(Canvas& c, const char* title, int count, ListRow (*row)(App&, int, void*), App& a, void* ctx);
  // Returns the tapped row, or -1. zone: -1 left, 0 centre, +1 right.
  int gesture(const GestureEvent& e, int count, int* zone);
  void tick(float dt, int count);
 private:
  float scroll_ = 0, vel_ = 0;
  bool dragging_ = false;
};

struct Button {
  float x, y, r;
  bool hit(int px, int py) const { float dx = px - x, dy = py - y; return dx * dx + dy * dy <= (r + 8) * (r + 8); }
};
// Numbers built from small squares. Changing a digit ripples through its cells instead of switching at once.
class DotText {
 public:
  void set(const char* s);      // call from tick(): draw() runs once per strip
  void tick(float dtMs) { if (t_ < 1) t_ += dtMs / 340.f; }
  void draw(Canvas& c, int cx, int y, int cell, int gap, uint16_t on) const;
  static int width(const char* s, int cell, int gap);
 private:
  char cur_[12] = "", prev_[12] = "";
  float t_ = 1;
};

// Sixty squares along the rim: seconds, progress, score.
void drawRimSquares(Canvas& c, float fraction, uint16_t on);
void drawWeatherIcon(Canvas& c, Weather w, float cx, float cy, float size, float tMs);
const char* weatherTitle(Weather w);

Screen* gameScreen(ScreenId id);
Screen* partyScreen(ScreenId id);

void drawButton(Canvas& c, const Button& b, const char* label, bool accent = false);
void drawTitle(Canvas& c, const char* title);
void formatTime(char* out, int cap, float ms, bool tenths);

Screen* screenInstance(ScreenId id);

}  // namespace cc
