// Design 1a building blocks. Everything is drawn from theme.h; a screen keeps the geometry (buttons, lists)
// as members, draws it once per strip and asks the same objects for hits.
#pragma once
#include "companion/input.h"
#include "theme.h"

namespace cc {
class App;
namespace ui {

// Arcs follow the spec: 0° points right, angles grow clockwise, the top is -90°.
void drawArc(Canvas& c, float cx, float cy, float r, float width, float startDeg, float sweepDeg, uint16_t color,
             bool roundCaps = true);

// Track and fill from the top, clockwise. Widths: 4 seconds, 10 timer and workout, 16 reaction.
void progressRing(Canvas& c, float r, float width, float fraction, uint16_t fill = theme::ACCENT, uint16_t track = theme::TRACK_2);

// Right rim, r 214: where the list is. pos 0..1 through the list, visible = share of it on screen.
void scrollArc(Canvas& c, float pos, float visible);

// Text centred on the middle of its capitals and digits, which is where the eye puts its centre.
void textCentered(Canvas& c, float cx, float cy, const char* s, FontSize f, uint16_t color);

enum class ButtonKind : uint8_t { Primary, Secondary, Pill, PillOutline };

struct Button {
  float cx, cy, w, h;  // round kinds use w as the diameter
  ButtonKind kind;
  bool hit(int x, int y) const;  // the touch area never shrinks below TOUCH_MIN
  void draw(Canvas& c, const char* label, bool enabled = true) const;
};
// Positions are given by the top of the button, as in the spec.
constexpr Button primary(float cx, float top, float d = theme::BTN_PRIMARY) { return {cx, top + d / 2, d, d, ButtonKind::Primary}; }
constexpr Button secondary(float cx, float top, float d = theme::BTN_SECONDARY) { return {cx, top + d / 2, d, d, ButtonKind::Secondary}; }
constexpr Button pill(float cx, float top, float w, float h = theme::PILL_H, bool filled = true) {
  return {cx, top + h / 2, w, h, filled ? ButtonKind::Pill : ButtonKind::PillOutline};
}

// Card suits by their letter in labels such as "10h": s spades, h hearts, d diamonds, c clubs.
uint16_t suitColor(char suit);
void drawSuit(Canvas& c, char suit, float cx, float cy, float size, uint16_t color);
// The heart of the spec: two circles r/2 and a square r x r turned by 45°.
void heart(Canvas& c, float x, float y, float r, uint16_t color);
// label "As", "10h"; empty or faceDown draws the back. Inactive cards keep a dim outline.
void card(Canvas& c, float x, float y, float w, float h, const char* label, bool faceUp = true, bool active = true, float stroke = 2);

// Weather pieces shared by the face and the weather screen.
void cloud(Canvas& c, float x, float y, float s, uint16_t color);   // three circles on a rounded base
void bolt(Canvas& c, float x, float y, float s, float thick, uint16_t color);  // zigzag lightning, top at (x, y)
void sparkle(Canvas& c, float x, float y, float half, float thick, uint16_t color);  // a plus with round ends

// Caption and value on one line, a 6 px bar under them.
void statBar(Canvas& c, float x, float y, float w, const char* label, int value, uint16_t fill = theme::INK);

// Five visible rows, 64 px apart; the selected one sits on the centre line in the big font.
struct ListItem {
  const char* title;
  const char* value;  // settings show it under the caption of the selected row
  bool section;       // a dot marks rows that open more rows
};
using ListSource = ListItem (*)(App&, int index, void* ctx);

class List {
 public:
  void reset(int selected = 0);
  void tick(float dtMs);
  void draw(Canvas& c, App& a, int count, ListSource src, void* ctx) const;
  // Returns the row a tap activates, or -1. Drags scroll; a tap on another row brings it to the centre.
  int gesture(const GestureEvent& e, int count);
  int selected() const { return selected_; }

 private:
  float pos_ = 0;        // row shown at the centre, animated
  float drag_ = 0;       // pixels dragged since the finger went down
  int selected_ = 0;
  bool dragging_ = false;
};

}  // namespace ui
}  // namespace cc
