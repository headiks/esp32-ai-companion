// Design 1a, time screens 9..13 of the spec and the weather screen: digital and analog clocks, stopwatch,
// timer, "time not set", weather.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "ui/components.h"

namespace cc {
using namespace theme;

static const char* const kWeekdays[7] = {"вс", "пн", "вт", "ср", "чт", "пт", "сб"};
static const char* const kMonths[12] = {"января", "февраля", "марта", "апреля", "мая", "июня", "июля", "августа", "сентября", "октября", "ноября", "декабря"};
static const char* const kSkyWords[7] = {"ясно", "облачно", "дождь", "снег", "гроза", "туман", "ветер"};

// "+12°", "−3°", "0°": the minus is the typographic one, as in the design.
static void temperature(char* out, int cap, float c) {
  int t = (int)lroundf(c);
  snprintf(out, cap, "%s%d°", t > 0 ? "+" : t < 0 ? "−" : "", t < 0 ? -t : t);
}

static bool night(App& a) {
  DateTime d = a.clock.now();
  return d.hour >= 22 || d.hour < 7;
}

// The emotion the spec pairs with each kind of weather.
static Emotion skyEmotion(Weather w) {
  static const Emotion e[7] = {Emotion::Happy, Emotion::Curious, Emotion::Sad, Emotion::Excited, Emotion::Scared, Emotion::Confused, Emotion::Skeptical};
  return e[(int)w];
}

static void miniFixed(App& a, Canvas& c, Emotion e, float cx, float top, float width) {
  a.face.drawMini(c, e, cx - width / 2, top, width, INK);
}

// ---- 13. time not set: shown in place of any clock until the first sync -----------------------------
static void drawTimeNotSet(App& a, Canvas& c) {
  miniFixed(a, c, Emotion::Confused, CX, 96, 180);
  ui::textCentered(c, CX, 240, "--:--", NUM_XL, INK_3);
  ui::textCentered(c, CX, 300, "Время не задано", TXT_BODY, INK);
  c.textWrapped(CX, 322, 300, "Подключите Wi-Fi, телефон или ПК", TXT_MIN, INK_2, Align::Center, 2);
}

// ---- 9. digital clock ------------------------------------------------------------------------------
class DigitalClockScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    if (!a.clock.isSet()) { drawTimeNotSet(a, c); return; }
    DateTime d = a.clock.now();
    ui::progressRing(c, 218, 4, (d.second + a.clock.secondFraction()) / 60);
    char b[48];
    snprintf(b, sizeof b, "%s, %d %s", kWeekdays[d.weekday], d.day, kMonths[d.month - 1]);
    ui::textCentered(c, CX, 136, b, TXT_CAP, INK_2);
    snprintf(b, sizeof b, "%02d:%02d", d.hour, d.minute);
    ui::textCentered(c, CX, 214, b, NUM_XL, INK);
    if (a.weather.known) {
      char t[12];
      temperature(t, sizeof t, a.weather.tempC);
      snprintf(b, sizeof b, "%s · %s", t, kSkyWords[(int)a.weather.cond]);
      ui::textCentered(c, CX, 290, b, TXT_CAP, ACCENT);
    }
    miniFixed(a, c, night(a) ? Emotion::Sleepy : Emotion::Neutral, CX, 342, 130);
  }
};

// ---- 10. analog clock ------------------------------------------------------------------------------
class AnalogClockScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    if (!a.clock.isSet()) { drawTimeNotSet(a, c); return; }
    DateTime d = a.clock.now();
    auto ray = [&](float deg, float r0, float r1, float w, uint16_t col) {
      float rad = deg * 0.0174533f, s = sinf(rad), co = -cosf(rad);
      c.line(CX + s * r0, CY + co * r0, CX + s * r1, CY + co * r1, w, col);
    };
    for (int i = 0; i < 60; i++) if (i % 5) ray(i * 6.f, 211, 217, 3, TRACK);  // minutes
    for (int i = 0; i < 12; i++) {
      bool main = i % 3 == 0;
      ray(i * 30.f, main ? 187 : 193, 215, main ? 6 : 4, main ? INK : INK_2);
    }
    char b[16];
    snprintf(b, sizeof b, "%s %d", kWeekdays[d.weekday], d.day);
    c.text(CX, 300, b, TXT_CAP, INK_2, Align::Center);
    float secF = d.second + a.clock.secondFraction(), mF = d.minute + secF / 60, hF = (d.hour % 12) + mF / 60;
    auto hand = [&](float deg, float len, float tail, float w, uint16_t col) {
      float rad = deg * 0.0174533f, s = sinf(rad), co = -cosf(rad);
      c.line(CX - s * tail, CY - co * tail, CX + s * len, CY + co * len, w, col);
    };
    hand(hF * 30, 105, 0, 12, INK);
    hand(mF * 6, 169, 0, 7, INK);
    hand(secF * 6, 186, 29, 3, ACCENT);
    c.fillCircle(CX, CY, 10, BG);
    c.ring(CX, CY, 10, 4, ACCENT);
  }
};

// ---- 11. stopwatch ---------------------------------------------------------------------------------
static void mmss(char* out, int cap, float ms) {
  uint32_t t = (uint32_t)(ms < 0 ? 0 : ms);
  snprintf(out, cap, "%02u:%02u", (unsigned)(t / 60000), (unsigned)((t / 1000) % 60));
}

class StopwatchScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    ui::progressRing(c, 218, 4, fmodf(a.swMs / 1000.f, 60.f) / 60);
    ui::textCentered(c, CX, 115, "Секундомер", TXT_CAP, INK_2);
    // Whole seconds big, tenths small in the accent, sitting on the same baseline.
    char main[16], tenth[8];
    mmss(main, sizeof main, a.swMs);
    snprintf(tenth, sizeof tenth, ",%d", ((int)a.swMs / 100) % 10);
    int w1 = Canvas::textWidth(main, NUM_92), w2 = Canvas::textWidth(tenth, NUM_M40), x = CX - (w1 + w2) / 2;
    int top = 185 - Canvas::capMiddle(NUM_92), base = top + Canvas::capBottom(NUM_92);
    c.text(x, top, main, NUM_92, INK);
    c.text(x + w1, base - Canvas::capBottom(NUM_M40), tenth, NUM_M40, ACCENT);
    if (a.swLaps > 0) {
      char lap[16], line[40];
      mmss(lap, sizeof lap, a.swMs - a.swLap);
      snprintf(line, sizeof line, "круг %d · %s,%d", a.swLaps + 1, lap, ((int)(a.swMs - a.swLap) / 100) % 10);
      ui::textCentered(c, CX, 254, line, TXT_MIN, INK_2);
    }
    lap_.draw(c, a.swRunning || a.swMs <= 0 ? "Круг" : "Сброс", a.swRunning || a.swMs > 0);
    go_.draw(c, a.swRunning ? "Стоп" : "Старт");
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g != Gesture::Tap) return;
    if (go_.hit(e.x, e.y)) {
      a.swRunning = !a.swRunning;
      a.sfx.play("system/click");
    } else if (lap_.hit(e.x, e.y)) {
      if (a.swRunning) { a.swLap = a.swMs; a.swLaps++; }
      else { a.swMs = 0; a.swLap = 0; a.swLaps = 0; }
      a.sfx.play("system/click");
    }
  }

 private:
  ui::Button lap_ = ui::secondary(180, 358 - 36);
  ui::Button go_ = ui::primary(284, 358 - 46);
};

// ---- 12. timer -------------------------------------------------------------------------------------
class TimerScreen : public Screen {
 public:
  void enter(App&) override { field_ = 0; dragging_ = false; }
  void draw(App& a, Canvas& c) override {
    bool counting = a.timerRunning || a.timerLeftMs > 0;
    float ms = counting ? a.timerLeftMs : a.timerSetMs;
    ui::progressRing(c, 214, 10, a.timerSetMs > 0 ? clampf(ms / a.timerSetMs, 0, 1) : 0);
    char b[32];
    int setS = setSeconds(a);
    if (setS % 60) snprintf(b, sizeof b, "Таймер · %d мин %d с", setS / 60, setS % 60);
    else snprintf(b, sizeof b, "Таймер · %d мин", setS / 60);
    ui::textCentered(c, CX, 128, b, TXT_CAP, INK_2);
    mmss(b, sizeof b, ms + (counting ? 999 : 0));
    // Minutes and seconds are set apart: the one the buttons change is bright and underlined.
    Digits d = digits(b);
    int top = kDigitsY - Canvas::capMiddle(NUM_96);
    char part[4] = {b[0], b[1], 0};
    c.text(d.x0, top, part, NUM_96, counting || field_ == 0 ? INK : INK_2);
    c.text(d.x0 + d.wm, top, ":", NUM_96, counting ? INK : INK_2);
    part[0] = b[3]; part[1] = b[4];
    c.text(d.x0 + d.wm + d.wc, top, part, NUM_96, counting || field_ == 1 ? INK : INK_2);
    if (!counting) {
      float x = field_ ? d.x0 + d.wm + d.wc : d.x0, w = field_ ? d.ws : d.wm;
      c.roundRect(x + 4, top + Canvas::capBottom(NUM_96) + 10, w - 8, 6, 3, ACCENT);
    }
    minus_.draw(c, "−1", !a.timerRunning);
    go_.draw(c, a.timerRunning ? "Пауза" : "Старт");
    plus_.draw(c, "+1", !a.timerRunning);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    bool editable = !a.timerRunning && a.timerLeftMs <= 0;
    bool onDigits = abs(e.y - kDigitsY) < 60 && abs(e.x - CX) < 170;
    switch (e.g) {
      case Gesture::Down: dragging_ = editable && onDigits; drag_ = 0; return;
      case Gesture::Move:
        // Dragging the digits rolls the chosen part like a wheel: up adds, down takes away.
        if (!dragging_) return;
        drag_ -= e.dy;
        while (drag_ >= kDragStep) { drag_ -= kDragStep; change(a, 1); }
        while (drag_ <= -kDragStep) { drag_ += kDragStep; change(a, -1); }
        return;
      case Gesture::Up: dragging_ = false; return;
      case Gesture::Tap: break;
      default: return;
    }
    if (editable && onDigits) {
      char b[8];
      mmss(b, sizeof b, a.timerSetMs);
      Digits d = digits(b);
      field_ = e.x >= d.x0 + d.wm + d.wc / 2 ? 1 : 0;
      a.sfx.play("system/click");
    } else if (minus_.hit(e.x, e.y) && !a.timerRunning) {
      change(a, -1);
    } else if (plus_.hit(e.x, e.y) && !a.timerRunning) {
      change(a, 1);
    } else if (go_.hit(e.x, e.y)) {
      if (!a.timerRunning && a.timerLeftMs <= 0) a.timerLeftMs = a.timerSetMs;
      a.timerRunning = !a.timerRunning;
      a.sfx.play("system/click");
    }
  }

 private:
  struct Digits { int x0, wm, wc, ws; };
  static constexpr int kDigitsY = 198, kDragStep = 22;
  static Digits digits(const char* mmssText) {
    char part[4] = {mmssText[0], mmssText[1], 0};
    int wm = Canvas::textWidth(part, NUM_96), wc = Canvas::textWidth(":", NUM_96);
    part[0] = mmssText[3]; part[1] = mmssText[4];
    int ws = Canvas::textWidth(part, NUM_96);
    return Digits{CX - (wm + wc + ws) / 2, wm, wc, ws};
  }
  static int setSeconds(App& a) { return (int)(a.timerSetMs / 1000 + 0.5f); }
  // Minutes 0..99; seconds go round 0..59 without touching the minutes. Never down to zero in total.
  void change(App& a, int d) {
    int t = setSeconds(a), m = t / 60, s = t % 60;
    if (field_ == 0) m = clampi(m + d, 0, 99);
    else s = (s + d + 60) % 60;
    if (m * 60 + s == 0 || m * 60 + s == t) return;
    a.timerSetMs = (m * 60 + s) * 1000.f;
    a.timerLeftMs = 0;
    a.sfx.play("system/click");
  }
  ui::Button minus_ = ui::secondary(132, 340 - 36);
  ui::Button go_ = ui::primary(CX, 340 - 48, 96);
  ui::Button plus_ = ui::secondary(334, 340 - 36);
  int field_ = 0;  // 0 minutes, 1 seconds
  float drag_ = 0;
  bool dragging_ = false;
};

// ---- weather ---------------------------------------------------------------------------------------
static void weatherIcon(Canvas& c, Weather w, float x, float y) {
  switch (w) {
    case Weather::Clear:
      c.fillCircle(x, y, 30, INK);
      for (int i = 0; i < 8; i++) {
        float r = i * 0.785398f, s = sinf(r), co = cosf(r);
        c.line(x + co * 42, y + s * 42, x + co * 54, y + s * 54, 4, ACCENT);
      }
      break;
    case Weather::Clouds: ui::cloud(c, x, y - 4, 1, INK); break;
    case Weather::Rain:
      ui::cloud(c, x, y - 16, 0.9f, INK);
      for (int i = 0; i < 4; i++) c.line(x - 30 + i * 20, y + 22, x - 34 + i * 20, y + 40, 4, ACCENT);
      break;
    case Weather::Snow:
      ui::cloud(c, x, y - 16, 0.9f, INK);
      for (int i = 0; i < 5; i++) c.fillCircle(x - 36 + i * 18, y + 30 + (i % 2) * 10, 4, INK);
      break;
    case Weather::Storm:
      ui::cloud(c, x, y - 16, 0.9f, INK_2);
      ui::bolt(c, x + 2, y + 14, 0.5f, 5, ACCENT_2);
      break;
    case Weather::Fog:
      for (int i = 0; i < 4; i++) c.line(x - 48 + (i % 2) * 12, y - 24 + i * 16, x + 48 - (i % 2) * 12, y - 24 + i * 16, 5, INK_2);
      break;
    case Weather::Wind:
      for (int i = 0; i < 3; i++) c.line(x - 50 + i * 10, y - 18 + i * 18, x + 50 - (i == 1) * 16, y - 18 + i * 18, 5, INK);
      break;
    default: break;
  }
}

class WeatherScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    static const char* const kTitles[7] = {"Ясно", "Облачно", "Дождь", "Снег", "Гроза", "Туман", "Ветер"};
    if (!a.weather.known) {
      weatherIcon(c, Weather::Clouds, CX, 108);
      ui::textCentered(c, CX, 218, "--°", NUM_96, INK_3);
      ui::textCentered(c, CX, 290, "Нет данных", TXT_TITLE, INK);
      c.textWrapped(CX, 314, 300, "Нужен Wi-Fi с координатами или приложение", TXT_MIN, INK_2, Align::Center, 2);
      return;
    }
    Weather w = a.weather.cond;
    weatherIcon(c, w, CX, 108);
    char b[32];
    temperature(b, sizeof b, a.weather.tempC);
    ui::textCentered(c, CX, 218, b, NUM_96, INK);
    ui::textCentered(c, CX, 290, kTitles[(int)w], TXT_TITLE, INK);
    if (a.weather.at && a.clock.isSet()) {
      DateTime d = civilFromEpoch(a.weather.at + a.clock.tzMinutes() * 60);
      snprintf(b, sizeof b, "обновлено в %02d:%02d", d.hour, d.minute);
      ui::textCentered(c, CX, 326, b, TXT_MIN, INK_2);
    }
    miniFixed(a, c, skyEmotion(w), CX, 368, 120);
  }
};

Screen* timeScreen(ScreenId id) {
  static DigitalClockScreen digital;
  static AnalogClockScreen analog;
  static StopwatchScreen stopwatch;
  static TimerScreen timer;
  static WeatherScreen weather;
  switch (id) {
    case ScreenId::DigitalClock: return &digital;
    case ScreenId::AnalogClock: return &analog;
    case ScreenId::Stopwatch: return &stopwatch;
    case ScreenId::Timer: return &timer;
    case ScreenId::Weather: return &weather;
    default: return nullptr;
  }
}

}  // namespace cc
