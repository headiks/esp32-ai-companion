#include "components.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace cc {
namespace ui {
using namespace theme;

void drawArc(Canvas& c, float cx, float cy, float r, float width, float startDeg, float sweepDeg, uint16_t color, bool roundCaps) {
  if (sweepDeg < 0) { startDeg += sweepDeg; sweepDeg = -sweepDeg; }
  if (sweepDeg <= 0) return;
  if (sweepDeg > 360) sweepDeg = 360;
  float a0 = startDeg + 90;  // the canvas counts from the top
  c.arc(cx, cy, r, width, a0, a0 + sweepDeg, color, roundCaps);
}

void progressRing(Canvas& c, float r, float width, float fraction, uint16_t fill, uint16_t track) {
  fraction = clampf(fraction, 0, 1);
  if (fraction < 1) c.ring(CX, CY, r, width, track);
  if (fraction > 0) drawArc(c, CX, CY, r, width, -90, 360 * fraction, fill, fraction < 1);
}

void scrollArc(Canvas& c, float pos, float visible) {
  constexpr float kR = 214, kW = 5, kTrack = 200, kStart = -38, kDegPerPx = 57.29578f / kR;
  drawArc(c, CX, CY, kR, kW, kStart, kTrack * kDegPerPx, TRACK);
  float thumb = clampf(visible * kTrack, 20, 60);
  drawArc(c, CX, CY, kR, kW, kStart + clampf(pos, 0, 1) * (kTrack - thumb) * kDegPerPx, thumb * kDegPerPx, ACCENT);
}

void textCentered(Canvas& c, float cx, float cy, const char* s, FontSize f, uint16_t color) {
  c.text((int)lroundf(cx), (int)lroundf(cy) - Canvas::capMiddle(f), s, f, color, Align::Center);
}

// ---- buttons --------------------------------------------------------------------------------------

bool Button::hit(int x, int y) const {
  float hw = (w > TOUCH_MIN ? w : TOUCH_MIN) / 2, hh = (h > TOUCH_MIN ? h : TOUCH_MIN) / 2;
  float dx = x - cx, dy = y - cy;
  if (kind == ButtonKind::Primary || kind == ButtonKind::Secondary) return dx * dx + dy * dy <= hw * hw;
  return fabsf(dx) <= hw && fabsf(dy) <= hh;
}

void Button::draw(Canvas& c, const char* label, bool enabled) const {
  switch (kind) {
    case ButtonKind::Primary:
      c.fillCircle(cx, cy, w / 2, enabled ? ACCENT : INK_3);
      textCentered(c, cx, cy, label, TXT_BTN, BG);
      break;
    case ButtonKind::Secondary:
      c.ring(cx, cy, w / 2 - 1, 2, INK_3);
      textCentered(c, cx, cy, label, TXT_BTN, enabled ? INK : INK_3);
      break;
    case ButtonKind::Pill:
    case ButtonKind::PillOutline:
      if (kind == ButtonKind::Pill) c.roundRect(cx - w / 2, cy - h / 2, w, h, h / 2, SURFACE);
      c.roundRectOutline(cx - w / 2, cy - h / 2, w, h, h / 2, 2, INK_3);
      textCentered(c, cx, cy, label, TXT_BTN, enabled ? INK : INK_3);
      break;
  }
}

// ---- cards ----------------------------------------------------------------------------------------

uint16_t suitColor(char suit) { return suit == 'h' || suit == 'd' ? ACCENT_2 : INK; }

void heart(Canvas& c, float x, float y, float r, uint16_t color) {
  c.fillCircle(x - 0.354f * r, y - 0.2f * r, 0.5f * r, color);
  c.fillCircle(x + 0.354f * r, y - 0.2f * r, 0.5f * r, color);
  c.diamond(x, y + 0.15f * r, 0.7071f * r, 0.7071f * r, color);
}

// Suits fit a box `size` high around (cx, cy). Neither font has them, so they are shapes.
void drawSuit(Canvas& c, char suit, float cx, float cy, float size, uint16_t color) {
  switch (suit) {
    case 'h': {
      float r = size / 1.557f;  // the heart spans 0.7 r above its centre and 0.857 r below
      heart(c, cx, cy - 0.078f * r, r, color);
      break;
    }
    case 'd':
      c.diamond(cx, cy, size * 0.36f, size * 0.5f, color);
      break;
    case 's': {
      float r = size / 1.86f, y = cy - 0.07f * r;
      c.diamond(cx, y - 0.15f * r, 0.7071f * r, 0.7071f * r, color);
      c.fillCircle(cx - 0.354f * r, y + 0.2f * r, 0.5f * r, color);
      c.fillCircle(cx + 0.354f * r, y + 0.2f * r, 0.5f * r, color);
      c.line(cx, y + 0.3f * r, cx, y + 0.9f * r, 0.16f * r, color);
      c.roundRect(cx - 0.3f * r, y + 0.82f * r, 0.6f * r, 0.18f * r, 0.09f * r, color);
      break;
    }
    default: {  // clubs
      float r = size * 0.24f;
      c.fillCircle(cx, cy - 0.26f * size, r, color);
      c.fillCircle(cx - 0.25f * size, cy + 0.04f * size, r, color);
      c.fillCircle(cx + 0.25f * size, cy + 0.04f * size, r, color);
      c.fillCircle(cx, cy - 0.04f * size, r * 0.7f, color);
      c.line(cx, cy, cx, cy + 0.42f * size, 0.09f * size, color);
      c.roundRect(cx - 0.17f * size, cy + 0.4f * size, 0.34f * size, 0.1f * size, 0.05f * size, color);
      break;
    }
  }
}

void card(Canvas& c, float x, float y, float w, float h, const char* label, bool faceUp, bool active, float stroke) {
  float r = clampf(w * 0.14f, 8, 16);
  size_t n = label ? strlen(label) : 0;
  if (!faceUp || n < 2) {
    c.roundRect(x, y, w, h, r, TRACK);  // the back: one flat, dark colour
    return;
  }
  char suit = label[n - 1], rank[4] = {};
  memcpy(rank, label, n - 1 < 3 ? n - 1 : 3);
  uint16_t ink = active ? suitColor(suit) : INK_3;
  c.roundRect(x, y, w, h, r, SURFACE);
  c.roundRectOutline(x, y, w, h, r, stroke, ink);
  FontSize f = w >= 80 ? TXT_UNB34 : w >= 56 ? TXT_UNB28 : TXT_BTN;
  textCentered(c, x + w / 2, y + h * 0.32f, rank, f, ink);
  drawSuit(c, suit, x + w / 2, y + h * 0.7f, w * 0.3f, ink);
}

// ---- bars -----------------------------------------------------------------------------------------

void statBar(Canvas& c, float x, float y, float w, const char* label, int value, uint16_t fill) {
  char v[8];
  snprintf(v, sizeof v, "%d", value);
  c.text((int)x, (int)y, label, TXT_MIN, fill == INK ? INK_2 : fill);
  c.text((int)(x + w), (int)y, v, TXT_MIN, fill, Align::Right);
  float by = y + Canvas::lineHeight(TXT_MIN) + 4;
  c.roundRect(x, by, w, 6, 3, TRACK);
  float fw = w * clampf(value / 100.f, 0, 1);
  if (fw > 0) c.roundRect(x, by, fw < 6 ? 6 : fw, 6, 3, fill);
}

// ---- list -----------------------------------------------------------------------------------------

static bool numeric(const char* s) {
  for (; *s; s++) {
    if (strchr("0123456789 :,.+-%", *s)) continue;
    if ((uint8_t)s[0] == 0xE2 && (uint8_t)s[1] == 0x88 && (uint8_t)s[2] == 0x92) { s += 2; continue; }  // minus sign
    if ((uint8_t)s[0] == 0xC2 && (uint8_t)s[1] == 0xB0) { s += 1; continue; }                          // degree
    return false;
  }
  return true;
}

void List::reset(int selected) {
  selected_ = selected;
  pos_ = (float)selected;
  drag_ = 0;
  dragging_ = false;
}

void List::tick(float dtMs) {
  if (dragging_) return;
  pos_ += (selected_ - pos_) * (1 - expf(-dtMs / 70.f));  // ease-out to the row
  if (fabsf(selected_ - pos_) < 0.002f) pos_ = (float)selected_;
}

void List::draw(Canvas& c, App& a, int count, ListSource src, void* ctx) const {
  bool values = count > 0 && src(a, 0, ctx).value;
  float step = values ? LIST_STEP + 14 : LIST_STEP;  // a selected setting takes two lines
  for (int i = (int)floorf(pos_) - 3; i <= (int)ceilf(pos_) + 3; i++) {
    if (i < 0 || i >= count) continue;
    float d = i - pos_, ad = fabsf(d), y = CY + d * step;
    if (ad > 2.5f) continue;
    ListItem it = src(a, i, ctx);
    if (ad < 0.5f) {
      if (it.value) {
        textCentered(c, CX, y - 26, it.title, TXT_CAP, INK_2);
        textCentered(c, CX, y + 16, it.value, numeric(it.value) ? NUM_M : TXT_MENU_SEL, INK);
      } else {
        textCentered(c, CX, y, it.title, TXT_MENU_SEL, INK);
        if (it.section) c.fillCircle(CX - Canvas::textWidth(it.title, TXT_MENU_SEL) / 2.f - 20, y, 6, ACCENT);
      }
      continue;
    }
    char line[96];
    snprintf(line, sizeof line, it.value ? "%s %s" : "%s", it.title, it.value);
    bool near = ad < 1.5f;
    textCentered(c, CX, y, line, near ? TXT_28 : TXT_CAP, near ? INK_2 : INK_3);
  }
  if (count > 5) scrollArc(c, count > 1 ? pos_ / (count - 1) : 0, 5.f / count);
}

int List::gesture(const GestureEvent& e, int count) {
  if (count <= 0) return -1;
  float step = LIST_STEP;
  switch (e.g) {
    case Gesture::Down:
      dragging_ = true;
      drag_ = 0;
      break;
    case Gesture::Move:
      if (dragging_ && abs(e.dy) >= abs(e.dx)) {
        drag_ += e.dy;
        pos_ = clampf(selected_ - drag_ / step, -0.4f, count - 0.6f);
      }
      break;
    case Gesture::Up:
      if (dragging_ && fabsf(drag_) > 8) selected_ = clampi((int)lroundf(pos_), 0, count - 1);
      dragging_ = false;
      break;
    case Gesture::Tap: {
      dragging_ = false;
      int row = clampi(selected_ + (int)lroundf((e.y - CY) / step), 0, count - 1);
      if (row == selected_) return row;
      selected_ = row;  // a neighbour comes to the centre first
      break;
    }
    default:
      break;
  }
  return -1;
}

}  // namespace ui
}  // namespace cc
