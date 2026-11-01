// App: owns all shared modules and connects them. Firmware and emulator are thin shells around it:
// they call tick()/render()/mix() and feed sensor data. Nothing here touches hardware.
#pragma once
#include "audio.h"
#include "canvas.h"
#include "emotion.h"
#include "face.h"
#include "input.h"
#include "pet.h"
#include "services.h"

namespace cc {

constexpr const char* kFirmwareVersion = "1.0.0";
constexpr int kProtocolVersion = 1;

enum class ScreenId : uint8_t {
  Face, Menu, Pet, AnalogClock, DigitalClock, Timer, Stopwatch, Notifications, Settings, Diagnostics, Games, Info,
  TicTacToe, Reaction, Guess, PetGame, Maze, Orbit, Snake, Simon, Weather, Count
};
const char* screenName(ScreenId s);
bool screenFromName(const char* n, ScreenId* out);

// Filled in by the platform for the diagnostics screen and protocol.
struct HalInfo {
  char platform[24] = "emulator";
  char mac[18] = "00:00:00:00:00:00";
  char ip[16] = "-";
  bool touchOk = true, imuOk = true, pmicOk = true, codecOk = true, micOk = true;
  bool bleConnected = false, wifiConnected = false;
  uint32_t freeHeap = 0, freePsram = 0;
  float fps = 0, ax = 0, ay = 0, az = 1;
  uint32_t flashMb = 0, psramMb = 0;
};

struct Session { bool authed; const char* source; };

class Screen;

class App {
 public:
  App();
  void begin(Storage* storage, const uint8_t* pack, uint32_t packSize, uint32_t seed);
  void tick(float dtMs);
  void render(Canvas& c);     // call once per strip
  Rect dirtyRect() const;
  void mix(int16_t* out, int samples) { audio.mix(out, samples); }

  // Sensor input
  void touchInput(bool down, int x, int y) { rawDown_ = down; rawX_ = x; rawY_ = y; }
  void imuInput(float ax, float ay, float az, float gx, float gy, float gz, float dtMs);
  void micInput(const int16_t* pcm, int n) { mic.feed(pcm, n); }

  // Protocol: one JSON command, one JSON response. Returns response length.
  int handleCommand(const char* json, char* out, int cap, Session& session);
  // Periodic state message for connected apps. Returns length.
  int stateEvent(char* out, int cap);

  // Behaviour API used by screens and the protocol.
  void showScreen(ScreenId s);
  void goBack();
  ScreenId screen() const { return screenId_; }
  void react(Emotion e, float holdMs, EmotionPriority p = EmotionPriority::Reaction) { emotion.request(e, p, holdMs); }
  void petAction(PetEvent e);
  void sleepNow();
  void wakeNow();
  void gesture(const GestureEvent& e);
  void motion(MotionEvent e);
  void sound(SoundEvent e);
  void applySettings();
  void saveSettings();
  void savePet();
  void factoryReset();
  void toast(const char* title, const char* text, const char* soundName);
  void caption(const char* text, float ms = 5000);
  void chat(const char* text);
  void listen();   // voice dialog, needs settings.aiVoiceConsent
  void setWeather(Weather w, float tempC);
  float alertness() const;     // 0 deep night .. 1 midday, from the clock
  uint16_t eyeColor() const;   // warmer at night, like a real light source
  WeatherState weather;
  float thunderFlashMs = 0;
  void setAIClient(AIClient* ai) { ai_ = ai ? ai : &offlineAi_; }
  AIClient& aiClient() { return *ai_; }
  float nowMs() const { return nowMs_; }
  bool netDirty = false;       // Wi-Fi/BLE settings changed: the platform re-applies them
  bool sleepRequested = false; // the platform may turn peripherals down

  // Shared timer/stopwatch state survives leaving their screens.
  float timerLeftMs = 0, timerSetMs = 5 * 60000.f; bool timerRunning = false;
  float swMs = 0; bool swRunning = false; float swLap = 0;

  AnimationEngine anim;
  FaceEngine face;
  EmotionEngine emotion;
  PetManager pet;
  TouchManager touch;
  GestureManager motionMgr;
  MicrophoneManager mic;
  AudioManager audio;
  SoundEmotionEngine sfx;
  SoundPack pack;
  ClockManager clock;
  NotificationManager notifications;
  PowerManager power;
  Settings settings;
  HalInfo hal;
  Storage* storage = nullptr;
  Rng rng;

 private:
  void tickBehaviour(float dt);
  void tickIdleLife(float dt);
  Emotion chooseThought();
  void drawOverlays(Canvas& c);
  Screen* cur() const { return screens_[(int)screenId_]; }

  Screen* screens_[(int)ScreenId::Count];
  ScreenId screenId_ = ScreenId::Face, prevId_ = ScreenId::Face;
  OfflineAI offlineAi_;
  AIClient* ai_ = &offlineAi_;
  bool rawDown_ = false; int rawX_ = 0, rawY_ = 0;
  float nowMs_ = 0, saveIn_ = 60000, idleLifeIn_ = 3500, blendIn_ = 2000, thunderIn_ = 9000, iris_ = 1.f, stateIn_ = 0, strokeAcc_ = 0;
  float toastMs_ = 0, captionMs_ = 0, aiThinkMs_ = 0;
  Notification toast_;
  char toastText_[160] = "", caption_[200] = "";
  bool lowBatteryWarned_ = false, wasAsleepPower_ = false, wasCharging_ = false, restingUntilRested_ = false;
  int authFails_ = 0; float authLockMs_ = 0;
  friend class FaceScreen;
};

}  // namespace cc
