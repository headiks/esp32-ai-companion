// Clock, notifications, settings, persistence, power policy and the AI client abstraction.
#pragma once
#include "pet.h"
#include "types.h"
#include <string.h>

namespace cc {

// ---- StorageManager ---------------------------------------------------------------------------
// Platform-specific key/blob store: NVS on the device, a folder of files in the emulator.
class Storage {
 public:
  virtual ~Storage() {}
  virtual bool load(const char* key, void* data, uint32_t size) = 0;
  virtual bool save(const char* key, const void* data, uint32_t size) = 0;
  virtual bool erase(const char* key) = 0;
};

// Fixed header + checksum: a truncated write or an old layout falls back to defaults instead of garbage.
bool loadChecked(Storage* s, const char* key, void* data, uint32_t size, uint16_t version);
bool saveChecked(Storage* s, const char* key, const void* data, uint32_t size, uint16_t version);

// Copies UTF-8 text cutting only between characters, so a long string never ends in half a letter.
void copyUtf8(char* dst, const char* src, size_t cap);

// ---- ClockManager -----------------------------------------------------------------------------
struct DateTime { int year, month, day, hour, minute, second, weekday; };

class ClockManager {
 public:
  void setEpoch(uint32_t epoch, int tzOffsetMin = 0x7FFF);  // UTC seconds
  void restore(uint32_t epoch) { epoch_ = epoch; }  // keeps isSet() false: saved time has drifted
  void tick(float dtMs);
  uint32_t epoch() const { return epoch_; }
  bool isSet() const { return set_; }
  int tzMinutes() const { return tz_; }
  void setTz(int m) { tz_ = m; }
  DateTime now() const;
  float secondFraction() const { return frac_ / 1000.f; }
  void setScale(float s) { scale_ = s; }  // time acceleration in the emulator; 1 on the device
  float scale() const { return scale_; }
 private:
  uint32_t epoch_ = 1767225600;  // 2026-01-01: readable placeholder until the first sync
  float frac_ = 0, scale_ = 1;
  int tz_ = 180;
  bool set_ = false;
};
DateTime civilFromEpoch(uint32_t local);

// ---- NotificationManager ----------------------------------------------------------------------
struct Notification {
  char title[48]; char text[160]; char source[12];
  uint32_t time; uint8_t icon; bool read;
};
enum class NotifIcon : uint8_t { Bell, Message, Mail, Phone, Calendar, Warning, Count };
NotifIcon notifIconFromName(const char* s);
const char* notifIconName(NotifIcon i);

class NotificationManager {
 public:
  static constexpr int kCap = 24;
  void add(const char* title, const char* text, const char* icon, const char* source, uint32_t time);
  int count() const { return count_; }
  int unread() const;
  const Notification& at(int newestFirst) const;
  void markRead(int newestFirst);
  void markAllRead();
  void clear() { count_ = 0; head_ = 0; }
  bool popToast(Notification* out);  // queued arrival for the overlay
 private:
  Notification items_[kCap];
  int count_ = 0, head_ = 0;
  int toast_ = 0;                    // not yet displayed
};

// ---- SettingsManager --------------------------------------------------------------------------
struct Settings {
  uint8_t brightness = 70, volume = 60;
  bool soundOn = true, micOn = true;
  uint8_t micSensitivity = 60, touchSensitivity = 60;
  uint8_t animSpeed = 100;     // percent
  uint8_t blinkRate = 100;     // percent
  uint16_t sleepAfterS = 300;
  bool powerSave = false, wifiOn = false, bleOn = true;
  uint8_t personality = 0;
  bool catEmotion = true, catInteraction = true, catSystem = true, catGame = true;
  bool aiVoiceConsent = false;   // audio is sent to an external server only with this flag
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  char aiUrl[96] = "";
  char pin[7] = "";              // BLE/Wi-Fi pairing PIN, generated at first boot
  int16_t tzMinutes = 180;
  uint16_t reactionBest = 0;     // the reaction game is gone; the field keeps saved settings loading
  uint16_t tttWins = 0, tttLosses = 0, tttDraws = 0;
  uint16_t mazeLevel = 0, orbitBest = 0, snakeBest = 0, simonBest = 0;
  uint16_t balanceBest = 0, duelBest = 0, shakerBest = 0, moleBest = 0, workouts = 0;
  float latitude = 0, longitude = 0;  // for weather; both zero means not set
};
constexpr uint16_t kSettingsVersion = 5;

// ---- Weather ----------------------------------------------------------------------------------
struct WeatherState { Weather cond = Weather::Clear; float tempC = 20; bool known = false; uint32_t at = 0; };
const char* weatherName(Weather w);
bool weatherFromName(const char* s, Weather* out);
// Reads an Open-Meteo "current" response. False on anything unexpected: it comes from the network.
bool parseOpenMeteo(const char* body, Weather* w, float* tempC);

// ---- PowerManager -----------------------------------------------------------------------------
enum class PowerState : uint8_t { Active, Dimmed, Asleep };

class PowerManager {
 public:
  void configure(uint16_t sleepAfterS, uint8_t brightness, bool powerSave);
  void activity() { idle_ = 0; }
  void tick(float dtMs);
  PowerState state() const { return state_; }
  uint8_t panelBrightness() const;     // what to send to the CO5300
  int targetFps() const;               // renderer rate for the current state
  // Battery information reported by the platform.
  void setBattery(int percent, bool charging, bool present, int mv) { batt_ = percent; chg_ = charging; present_ = present; mv_ = mv; }
  int batteryPercent() const { return batt_; }
  bool charging() const { return chg_; }
  bool batteryPresent() const { return present_; }
  int batteryMv() const { return mv_; }
  bool lowBattery() const { return present_ && !chg_ && batt_ <= 15; }
  float idleSeconds() const { return idle_ * 0.001f; }
 private:
  float idle_ = 0;
  uint16_t sleepAfter_ = 300;
  uint8_t bright_ = 70;
  bool save_ = false, chg_ = false, present_ = false;
  int batt_ = 100, mv_ = 0;
  PowerState state_ = PowerState::Active;
};

// ---- AIClient ---------------------------------------------------------------------------------
struct AIReply { char text[200]; Emotion emotion; bool ok; bool offline; };

// Provider independent: firmware talks to a user-configured gateway URL (tools/ai_gateway.py), so
// changing the AI vendor never requires new firmware. Implementations must not block the caller.
class AIClient {
 public:
  virtual ~AIClient() {}
  virtual bool submitText(const char* text) = 0;
  virtual bool submitVoice() { return false; }   // record and send; only called with the user's consent
  virtual bool poll(AIReply* reply) = 0;
  virtual const char* name() const = 0;
};

// Autonomous mode: keyword rules, works without any network.
class OfflineAI : public AIClient {
 public:
  bool submitText(const char* text) override;
  bool poll(AIReply* reply) override;
  const char* name() const override { return "offline"; }
 private:
  bool ready_ = false;
  AIReply reply_{};
};

}  // namespace cc
