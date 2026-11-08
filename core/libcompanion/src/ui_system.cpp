// Design 1a, system screens 1..8 of the spec: face, menu (and its clock and remote sub-lists), talk,
// notifications, settings, one setting's value, diagnostics, about.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "ui/components.h"

namespace cc {
using namespace theme;

// Text of one line, centred, cut with an ellipsis if wider than maxW.
static void line(Canvas& c, float cx, float cy, const char* s, FontSize f, uint16_t col, int maxW = 380) {
  c.textWrapped((int)cx, (int)cy - Canvas::capMiddle(f), maxW, s, f, col, Align::Center, 1, 0, true);
}

static void clockText(App& a, char* out, int cap) {
  DateTime d = a.clock.now();
  if (a.clock.isSet()) snprintf(out, cap, "%02d:%02d", d.hour, d.minute);
  else snprintf(out, cap, "--:--");
}

// ---- 1. face ---------------------------------------------------------------------------------------
class FaceScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    // Thunder thickens the bolt instead of flashing the whole screen: big bright fields are out.
    bool sky = a.weather.known, flash = a.thunderFlashMs > 0;
    if (sky) a.face.drawWeather(c, a.weather.cond, a.nowMs(), false, flash);
    a.face.draw(c, a.anim, a.eyeColor());
    if (sky) a.face.drawWeather(c, a.weather.cond, a.nowMs(), true, flash);
    if (a.clock.isSet()) {
      char t[8];
      clockText(a, t, sizeof t);
      c.text(CX, 44, t, TXT_MIN, INK_2, Align::Center);
    }
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
      case Gesture::Tap:  // the spec: a tap opens the menu; the face still notices it
        a.pet.event(PetEvent::Tap);
        a.sfx.play("interactions/tap");
        // Poking it again and again is not asking for the menu: it gets annoyed and stays.
        if (a.pet.irritatedByRepeats()) { a.anim.kick(-0.16f); a.react(a.pet.stats.stress > 60 ? Emotion::Angry : Emotion::Annoyed, 2200); break; }
        a.react(a.pet.stats.curiosity > 50 ? Emotion::Curious : Emotion::Attentive, 1400);
        a.showScreen(ScreenId::Menu);
        break;
      case Gesture::DoubleTap:
        a.pet.event(PetEvent::DoubleTap);
        a.anim.kick(-0.3f);
        a.sfx.play("interactions/double_tap");
        if (a.pet.irritatedByRepeats()) a.react(Emotion::Annoyed, 2000);
        else a.react(a.pet.stats.fun > 75 ? Emotion::Laughing : (a.pet.stats.happiness > 55 ? Emotion::Happy : Emotion::Surprised), 1800);
        break;
      case Gesture::LongPress: a.sfx.play("interactions/hold"); a.listen(); break;  // talk
      case Gesture::SwipeUp: a.sfx.play("interactions/swipe"); a.showScreen(ScreenId::Settings); break;
      case Gesture::SwipeDown: a.sfx.play("interactions/swipe"); a.showScreen(ScreenId::Notifications); break;
      default: break;
    }
  }

 private:
  int path_ = 0;
};

// ---- 2. menu and its sub-lists -------------------------------------------------------------------
struct MenuEntry { const char* title; ScreenId target; };
static constexpr ScreenId kTalk = ScreenId::Count;  // not a screen to open: starts the voice dialog

static const MenuEntry kMain[] = {
    {"Уведомления", ScreenId::Notifications}, {"Часы", ScreenId::ClockMenu}, {"Лицо", ScreenId::Face}, {"Поговорить", kTalk},
    {"Питомец", ScreenId::Pet}, {"Игры", ScreenId::Games}, {"Пульты", ScreenId::Remotes}, {"Настройки", ScreenId::Settings},
    {"Об устройстве", ScreenId::Info}};
static const MenuEntry kClocks[] = {{"Часы", ScreenId::DigitalClock}, {"Циферблат", ScreenId::AnalogClock}, {"Таймер", ScreenId::Timer},
                                    {"Секундомер", ScreenId::Stopwatch}, {"Погода", ScreenId::Weather}};
static const MenuEntry kRemotes[] = {{"Мышь", ScreenId::Mouse}, {"Поиск по Bluetooth", ScreenId::BlePairing}};

class MenuScreen : public Screen {
 public:
  MenuScreen(const MenuEntry* items, int count, int start) : items_(items), count_(count), start_(start) {}
  void enter(App&) override { list_.reset(start_); }  // opens on its first choice, "Лицо" in the main menu
  void tick(App&, float dt) override { list_.tick(dt); }
  void draw(App& a, Canvas& c) override {
    list_.draw(c, a, count_, [](App&, int i, void* ctx) {
      const MenuScreen* m = (const MenuScreen*)ctx;
      return ui::ListItem{m->items_[i].title, nullptr, true};
    }, this);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    int row = list_.gesture(e, count_);
    if (row < 0) return;
    a.sfx.play("system/click");
    if (items_[row].target == kTalk) a.listen();
    else a.showScreen(items_[row].target);
  }

 private:
  const MenuEntry* items_;
  int count_, start_;
  ui::List list_;
};

// ---- 3. talk ---------------------------------------------------------------------------------------
class TalkScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    // The ring hears: two segments grow with the microphone level.
    c.ring(CX, CY, 214, 6, TRACK);
    float level = a.talk == App::Talk::Listening ? clampf((a.mic.levelDb() + 60) / 40, 0, 1) : 0.15f;
    float sweep = (40 + 120 * level) / 214 * 57.29578f, spin = a.nowMs() * 0.02f;
    ui::drawArc(c, CX, CY, 214, 6, -40 + spin - sweep / 2, sweep, ACCENT);
    ui::drawArc(c, CX, CY, 214, 6, 140 + spin - sweep / 2, sweep, ACCENT);
    miniFace(c, a, CX, 96 + 120 * 176 / 354.f, 240);
    const char* title = "Слушаю…";
    switch (a.talk) {
      case App::Talk::Thinking: title = "Думаю…"; break;
      case App::Talk::Answer: title = "Отвечаю"; break;
      case App::Talk::Unavailable: title = "Голос недоступен"; break;
      default: break;
    }
    line(c, CX, 236 + 15, title, TXT_TITLE, INK);
    char quote[220];
    if (a.talk == App::Talk::Answer) snprintf(quote, sizeof quote, "«%s»", a.talkText);
    else snprintf(quote, sizeof quote, "%s", a.talk == App::Talk::Unavailable ? a.talkProblem : "");
    c.textWrapped(CX, 282, 300, quote, TXT_CAP, INK_2, Align::Center, 2, 0, true);
    stop_.draw(c, a.talk == App::Talk::Unavailable ? "Назад" : "Стоп");
  }
  void onGesture(App& a, const GestureEvent& e) override {
    if (e.g == Gesture::Tap && stop_.hit(e.x, e.y)) {
      a.sfx.play("system/click");
      a.talk = App::Talk::Off;
      a.showScreen(ScreenId::Face);
    }
  }

 private:
  ui::Button stop_ = ui::secondary(CX, 356);
};

// ---- 4. notifications ------------------------------------------------------------------------------
class NotificationsScreen : public Screen {
 public:
  void enter(App& a) override {
    index_ = 0;
    if (a.notifications.count()) a.notifications.markRead(0);
  }
  void draw(App& a, Canvas& c) override {
    int n = a.notifications.count();
    if (n == 0) {
      a.face.drawMini(c, Emotion::Neutral, CX - 60, 150, 120, INK);
      line(c, CX, 260, "Уведомлений нет", TXT_BODY, INK_2);
      return;
    }
    if (index_ >= n) index_ = n - 1;
    const Notification& m = a.notifications.at(index_);
    char b[48];
    snprintf(b, sizeof b, "%d из %d", index_ + 1, n);
    c.text(CX, 50, b, TXT_MIN, INK_2, Align::Center);
    const float x = 58, y = 100, w = 350, h = 210;
    c.roundRect(x, y, w, h, 32, SURFACE);
    c.fillCircle(x + 26 + 5, y + 24 + 13, 5, ACCENT);
    char src[48];
    snprintf(src, sizeof src, "%s", m.title);
    c.textWrapped((int)(x + 26 + 18), (int)(y + 24), 200, src, TXT_MIN, ACCENT, Align::Left, 1, 0, true);
    if (m.time) {
      DateTime d = civilFromEpoch(m.time + a.clock.tzMinutes() * 60);
      snprintf(b, sizeof b, "%02d:%02d", d.hour, d.minute);
      c.text((int)(x + w - 26), (int)(y + 24), b, TXT_MIN, INK_2, Align::Right);
    }
    c.textWrapped((int)(x + 26), (int)(y + 24 + 36), (int)(w - 52), m.text, TXT_BODY, INK, Align::Left, 4, 34, true);
    clear_.draw(c, "Очистить всё");
  }
  bool capturesSwipes() const override { return true; }  // horizontal swipes turn the cards
  void onGesture(App& a, const GestureEvent& e) override {
    int n = a.notifications.count();
    // The screen came down from the face: any vertical swipe sends it back, and so does a swipe right
    // past the first card. The cards only use left and right.
    if (e.g == Gesture::SwipeUp || e.g == Gesture::SwipeDown || (e.g == Gesture::SwipeRight && index_ == 0)) {
      a.sfx.play("interactions/swipe");
      a.goBack();
      return;
    }
    if (e.g == Gesture::SwipeLeft || e.g == Gesture::SwipeRight) {
      int next = clampi(index_ + (e.g == Gesture::SwipeLeft ? 1 : -1), 0, n ? n - 1 : 0);
      if (next != index_) { index_ = next; a.notifications.markRead(index_); a.sfx.play("interactions/swipe"); }
    } else if (e.g == Gesture::Tap && n && clear_.hit(e.x, e.y)) {
      a.sfx.play("system/click");
      a.notifications.clear();
      index_ = 0;
    }
  }

 private:
  int index_ = 0;
  ui::Button clear_ = ui::pill(CX, 330, 180);
};

// ---- 5. settings, 6. one value -----------------------------------------------------------------------
enum SettingRow {
  kBright, kVolume, kSound, kCatEmo, kMic, kWifi, kTime,  // the spec's seven, then the rest of the old menu
  kCatInt, kCatSys, kCatGame, kMicSens, kTouchSens, kAnim, kBlink, kSleep, kSave, kPersona, kBle, kAiConsent, kDiag, kReset, kSettingRows
};

// A value edited on its own screen: name, range, unit and the field behind it.
struct ValueSpec { const char* row; const char* title; const char* note; int lo, hi, step; const char* unit; };
static const ValueSpec& valueSpec(int row) {
  static const ValueSpec kBrightSpec{"Яркость", "Яркость", "гаснет через 30 с", 10, 100, 10, "%"};
  static const ValueSpec kVolumeSpec{"Громкость", "Громкость", "", 0, 100, 10, "%"};
  static const ValueSpec kMicSpec{"Чувствительность микрофона", "Микрофон", "чувствительность", 10, 100, 10, "%"};
  static const ValueSpec kTouchSpec{"Чувствительность касаний", "Касания", "чувствительность", 10, 100, 10, "%"};
  static const ValueSpec kAnimSpec{"Скорость анимации", "Анимация", "скорость", 50, 200, 10, "%"};
  static const ValueSpec kBlinkSpec{"Частота моргания", "Моргание", "частота", 0, 200, 20, "%"};
  static const ValueSpec kSleepSpec{"Переход в сон", "Сон", "после бездействия, мин", 1, 60, 1, ""};
  switch (row) {
    case kVolume: return kVolumeSpec;
    case kMicSens: return kMicSpec;
    case kTouchSens: return kTouchSpec;
    case kAnim: return kAnimSpec;
    case kBlink: return kBlinkSpec;
    case kSleep: return kSleepSpec;
    default: return kBrightSpec;
  }
}

static int getValue(App& a, int row) {
  const Settings& s = a.settings;
  switch (row) {
    case kVolume: return s.volume;
    case kMicSens: return s.micSensitivity;
    case kTouchSens: return s.touchSensitivity;
    case kAnim: return s.animSpeed;
    case kBlink: return s.blinkRate;
    case kSleep: return s.sleepAfterS / 60;
    default: return s.brightness;
  }
}

static void setValue(App& a, int row, int v) {
  const ValueSpec& sp = valueSpec(row);
  v = clampi(v, sp.lo, sp.hi);
  Settings& s = a.settings;
  switch (row) {
    case kVolume: s.volume = (uint8_t)v; break;
    case kMicSens: s.micSensitivity = (uint8_t)v; break;
    case kTouchSens: s.touchSensitivity = (uint8_t)v; break;
    case kAnim: s.animSpeed = (uint8_t)v; break;
    case kBlink: s.blinkRate = (uint8_t)v; break;
    case kSleep: s.sleepAfterS = (uint16_t)(v * 60); break;
    default: s.brightness = (uint8_t)v; break;
  }
  a.applySettings();  // brightness reaches the panel at once
}

static bool isValueRow(int row) {
  return row == kBright || row == kVolume || row == kMicSens || row == kTouchSens || row == kAnim || row == kBlink || row == kSleep;
}

class SettingsScreen : public Screen {
 public:
  void enter(App& a) override {
    confirm_ = false;
    // Coming back from a value or the diagnostics keeps the row in place.
    if (a.previous() != ScreenId::SettingValue && a.previous() != ScreenId::Diagnostics) list_.reset(0);
  }
  void tick(App&, float dt) override { list_.tick(dt); }
  void draw(App& a, Canvas& c) override {
    list_.draw(c, a, kSettingRows, [](App& ap, int i, void* ctx) {
      static char v[40];
      const Settings& s = ap.settings;
      auto onoff = [](bool b) { return b ? "Вкл" : "Выкл"; };
      switch (i) {
        case kSound: return ui::ListItem{"Звук", onoff(s.soundOn), false};
        case kCatEmo: return ui::ListItem{"Звуки эмоций", onoff(s.catEmotion), false};
        case kMic: return ui::ListItem{"Микрофон", onoff(s.micOn), false};
        case kWifi:
          snprintf(v, sizeof v, "%s", s.wifiOn ? (s.wifiSsid[0] ? s.wifiSsid : "Вкл") : "Выкл");
          return ui::ListItem{"Wi-Fi", v, false};
        case kTime: clockText(ap, v, sizeof v); return ui::ListItem{"Время", v, false};
        case kCatInt: return ui::ListItem{"Звуки касаний", onoff(s.catInteraction), false};
        case kCatSys: return ui::ListItem{"Системные звуки", onoff(s.catSystem), false};
        case kCatGame: return ui::ListItem{"Звуки игр", onoff(s.catGame), false};
        case kSave: return ui::ListItem{"Энергосбережение", onoff(s.powerSave), false};
        case kPersona: return ui::ListItem{"Характер", personalityTitle((Personality)s.personality), false};
        case kBle: return ui::ListItem{"Bluetooth", onoff(s.bleOn), false};
        case kAiConsent: return ui::ListItem{"Голос на AI-сервер", s.aiVoiceConsent ? "Разрешён" : "Запрещён", false};
        case kDiag: return ui::ListItem{"Диагностика", "›", false};
        case kReset: return ui::ListItem{"Сбросить всё", *(bool*)ctx ? "Нажмите ещё раз" : "Нет", false};
        default: {
          const ValueSpec& sp = valueSpec(i);
          if (i == kSleep) snprintf(v, sizeof v, "%d мин", getValue(ap, i));
          else snprintf(v, sizeof v, "%d%s", getValue(ap, i), sp.unit);
          return ui::ListItem{sp.row, v, false};
        }
      }
    }, &confirm_);
  }
  void onGesture(App& a, const GestureEvent& e) override {
    int row = list_.gesture(e, kSettingRows);
    if (row < 0) return;
    a.sfx.play("system/click");
    if (isValueRow(row)) { a.editSetting = (uint8_t)row; a.showScreen(ScreenId::SettingValue); return; }
    if (row == kDiag) { a.showScreen(ScreenId::Diagnostics); return; }
    if (row == kReset) {
      if (confirm_) { confirm_ = false; a.factoryReset(); a.showScreen(ScreenId::Face); }
      else confirm_ = true;
      return;
    }
    confirm_ = false;
    Settings& s = a.settings;
    switch (row) {
      case kSound: s.soundOn = !s.soundOn; break;
      case kCatEmo: s.catEmotion = !s.catEmotion; break;
      case kMic: s.micOn = !s.micOn; break;
      case kWifi: s.wifiOn = !s.wifiOn; a.netDirty = true; break;
      case kCatInt: s.catInteraction = !s.catInteraction; break;
      case kCatSys: s.catSystem = !s.catSystem; break;
      case kCatGame: s.catGame = !s.catGame; break;
      case kSave: s.powerSave = !s.powerSave; break;
      case kPersona: s.personality = (uint8_t)((s.personality + 1) % (int)Personality::Count); break;
      case kBle: s.bleOn = !s.bleOn; a.netDirty = true; break;
      case kAiConsent: s.aiVoiceConsent = !s.aiVoiceConsent; break;
      default: return;  // time: set by the network, the phone or the PC
    }
    a.applySettings();
    a.saveSettings();
  }

 private:
  ui::List list_;
  bool confirm_ = false;
};

// The arc of a value: from 150° to 270°, lower left up to the top.
static constexpr float kArcStart = 150, kArcSweep = 120, kArcR = 196;

class SettingValueScreen : public Screen {
 public:
  void enter(App&) override { dragging_ = false; }
  void draw(App& a, Canvas& c) override {
    int row = a.editSetting;
    const ValueSpec& sp = valueSpec(row);
    int v = getValue(a, row);
    float f = sp.hi > sp.lo ? (float)(v - sp.lo) / (sp.hi - sp.lo) : 0;
    ui::drawArc(c, CX, CY, kArcR, 12, kArcStart, kArcSweep, TRACK);
    if (f > 0) ui::drawArc(c, CX, CY, kArcR, 12, kArcStart, kArcSweep * f, INK);
    line(c, CX, 140, sp.title, TXT_28, INK_2);
    char b[16];
    snprintf(b, sizeof b, "%d%s", v, sp.unit);
    line(c, CX, 214, b, NUM_92, INK, 400);
    if (sp.note[0]) line(c, CX, 282, sp.note, TXT_MIN, INK_2);
    c.text(82, 330, "−", TXT_TITLE, INK_2, Align::Center);  // the spec's x58/x408, Y356 is past r210
    c.text(384, 330, "+", TXT_TITLE, INK_2, Align::Center);
  }
  bool capturesSwipes() const override { return true; }
  void onGesture(App& a, const GestureEvent& e) override {
    int row = a.editSetting;
    const ValueSpec& sp = valueSpec(row);
    switch (e.g) {
      case Gesture::Down: dragging_ = onArc(e.x, e.y) && !onButton(e.x, e.y); break;  // the value follows a drag, not a touch
      case Gesture::Move: if (dragging_) follow(a, e.x, e.y); break;
      case Gesture::Up: if (dragging_) { a.saveSettings(); a.sfx.play(row == kVolume ? "system/volume" : "system/click"); } dragging_ = false; break;
      case Gesture::Tap:
        if (onButton(e.x, e.y)) {  // the minus and plus near the ends
          setValue(a, row, getValue(a, row) + (e.x < CX ? -sp.step : sp.step));
          a.saveSettings();
          a.sfx.play(row == kVolume ? "system/volume" : "system/click");
        }
        break;
      default: break;
    }
  }

 private:
  static float angleOf(int x, int y) {
    float d = atan2f((float)(y - CY), (float)(x - CX)) * 57.29578f;
    return d < 0 ? d + 360 : d;
  }
  static bool onButton(int x, int y) { return y > 300 && (x < 120 || x > 346); }
  static bool onArc(int x, int y) {
    float r = sqrtf((float)((x - CX) * (x - CX) + (y - CY) * (y - CY))), d = angleOf(x, y);
    return r > kArcR - 60 && r < kArcR + 40 && d > kArcStart - 15 && d < kArcStart + kArcSweep + 15;
  }
  void follow(App& a, int x, int y) {
    const ValueSpec& sp = valueSpec(a.editSetting);
    float f = clampf((angleOf(x, y) - kArcStart) / kArcSweep, 0, 1);
    int v = sp.lo + (int)lroundf(f * (sp.hi - sp.lo) / sp.step) * sp.step;
    if (v != getValue(a, a.editSetting)) setValue(a, a.editSetting, v);
  }
  bool dragging_ = false;
};

// ---- 7. diagnostics, 8. about ------------------------------------------------------------------------
class DiagnosticsScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    c.text(CX, 46, "Диагностика", TXT_MIN, INK_2, Align::Center);
    const HalInfo& h = a.hal;
    char v[6][16];
    snprintf(v[0], 16, "%.0f", (double)h.fps);
    snprintf(v[1], 16, "%d%%", a.power.batteryPercent());
    snprintf(v[2], 16, "%uK", (unsigned)(h.freeHeap / 1024));
    snprintf(v[3], 16, "%.1fM", (double)(h.freePsram / 1048576.f));
    snprintf(v[4], 16, "%.0f", (double)a.mic.levelDb());
    snprintf(v[5], 16, "%.2f", (double)(a.power.batteryMv() / 1000.f));
    for (auto& s : v) for (char* p = s; *p; p++) if (*p == '.') *p = ',';  // decimal comma, Russian style
    static const char* const kCaps[6] = {"FPS", "Батарея", "RAM", "PSRAM", "Звук", "Напряж."};
    for (int i = 0; i < 6; i++) {
      int x = i % 2 ? 93 + 133 + 14 : 93, y = 92 + (i / 2) * 78;
      c.text(x, y, kCaps[i], TXT_MIN, INK_2);
      c.text(x, y + 24, v[i], NUM_S30, INK);
    }
    struct { const char* n; bool ok; } st[7] = {{"Сенсор", h.touchOk}, {"IMU", h.imuOk}, {"PMIC", h.pmicOk}, {"Кодек", h.codecOk},
                                               {"Микрофон", h.micOk}, {"BLE", h.bleConnected}, {"Wi-Fi", h.wifiConnected}};
    statusRow(c, 326, st, 3);  // narrowing rows: the circle narrows too
    statusRow(c, 356, st + 3, 2);
    statusRow(c, 386, st + 5, 2);
    // Touch calibration: a ring where the finger is (documentation/HARDWARE.md).
    if (a.touch.isDown()) c.ring((float)a.touch.x(), (float)a.touch.y(), 22, 3, ACCENT);
  }

 private:
  template <typename T>
  static void statusRow(Canvas& c, int y, const T* items, int n) {
    constexpr int kGap = 18, kDot = 14;
    int w = 0;
    for (int i = 0; i < n; i++) w += Canvas::textWidth(items[i].n, TXT_MIN) + 6 + kDot + (i ? kGap : 0);
    int x = CX - w / 2, mid = y + Canvas::capMiddle(TXT_MIN);
    for (int i = 0; i < n; i++) {
      if (i) x += kGap;
      x += c.text(x, y, items[i].n, TXT_MIN, INK) + 6;
      if (items[i].ok) c.fillCircle(x + kDot / 2.f, (float)mid, 5, INK);  // ● works
      else c.ring(x + kDot / 2.f, (float)mid, 4.5f, 2, ACCENT_2);         // ○ no link
      x += kDot;
    }
  }
};

class InfoScreen : public Screen {
 public:
  void draw(App& a, Canvas& c) override {
    a.face.drawMini(c, Emotion::Neutral, CX - 50, 66, 100, INK);
    const HalInfo& h = a.hal;
    char b[64];
    line(c, CX, 140, "AI-компаньон", TXT_TITLE, INK);
    snprintf(b, sizeof b, "Прошивка %s · протокол %d", kFirmwareVersion, kProtocolVersion);
    line(c, CX, 178, b, TXT_MIN, INK_2);
    // "BLE" in grey, the name in ink: two pieces centred together.
    char name[24];
    snprintf(name, sizeof name, "Companion-%s", strlen(h.mac) >= 17 ? h.mac + 12 : "0");
    for (char* p = name; *p; p++) if (*p == ':') memmove(p, p + 1, strlen(p));
    int w1 = Canvas::textWidth("BLE ", TXT_MIN), w2 = Canvas::textWidth(name, TXT_MIN), x = CX - (w1 + w2) / 2;
    int y = 206 - Canvas::capMiddle(TXT_MIN);
    c.text(x, y, "BLE ", TXT_MIN, INK_2);
    c.text(x + w1, y, name, TXT_MIN, INK);
    line(c, CX, 250, "PIN для приложений", TXT_MIN, INK_2);
    char pin[10];
    snprintf(pin, sizeof pin, "%.3s %.3s", a.settings.pin, strlen(a.settings.pin) > 3 ? a.settings.pin + 3 : "");
    line(c, CX, 292, pin, NUM_M, ACCENT);
    bool aiOk = strcmp(a.aiClient().name(), "offline") != 0, ipOk = strcmp(h.ip, "-") != 0;
    snprintf(b, sizeof b, "AI %s · IP %s", aiOk ? "онлайн" : "офлайн", ipOk ? h.ip : "—");
    line(c, CX, 340, b, TXT_MIN, aiOk && ipOk ? INK_2 : ACCENT_2);
  }
};

Screen* systemScreen(ScreenId id) {
  static FaceScreen face;
  static MenuScreen menu(kMain, sizeof kMain / sizeof kMain[0], 2);  // opens on "Лицо", as in the design
  static MenuScreen clocks(kClocks, sizeof kClocks / sizeof kClocks[0], 0);
  static MenuScreen remotes(kRemotes, sizeof kRemotes / sizeof kRemotes[0], 0);
  static TalkScreen talk;
  static NotificationsScreen notes;
  static SettingsScreen settings;
  static SettingValueScreen value;
  static DiagnosticsScreen diag;
  static InfoScreen info;
  switch (id) {
    case ScreenId::Face: return &face;
    case ScreenId::Menu: return &menu;
    case ScreenId::ClockMenu: return &clocks;
    case ScreenId::Remotes: return &remotes;
    case ScreenId::Talk: return &talk;
    case ScreenId::Notifications: return &notes;
    case ScreenId::Settings: return &settings;
    case ScreenId::SettingValue: return &value;
    case ScreenId::Diagnostics: return &diag;
    case ScreenId::Info: return &info;
    default: return nullptr;
  }
}

}  // namespace cc
