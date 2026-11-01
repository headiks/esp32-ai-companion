// DeviceProtocol: flat-JSON commands shared by BLE, USB serial, Wi-Fi and the emulator.
// See protocol/protocol.md. A tiny parser is enough: commands are flat objects with short values.
#include "companion/app.h"
#include "companion/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace cc {

bool JsonObject::parse(char* s) {
  n_ = 0;
  auto skip = [&] { while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++; };
  auto str = [&](char** out) -> bool {
    if (*s != '"') return false;
    s++;
    *out = s;
    char* w = s;
    while (*s && *s != '"') {
      if (*s != '\\') { *w++ = *s++; continue; }
      s++;
      switch (*s) {
        case 'n': *w++ = '\n'; s++; break;
        case 't': *w++ = '\t'; s++; break;
        case 'r': *w++ = '\r'; s++; break;
        case 'b': case 'f': s++; break;
        case 'u': {
          auto hex4 = [](const char* p, uint32_t* v) {
            *v = 0;
            for (int i = 0; i < 4; i++) {
              char c = p[i];
              int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
              if (d < 0) return false;
              *v = *v * 16 + (uint32_t)d;
            }
            return true;
          };
          uint32_t cp;
          if (!hex4(s + 1, &cp)) return false;
          s += 5;
          if (cp >= 0xD800 && cp < 0xDC00 && s[0] == '\\' && s[1] == 'u') {
            uint32_t lo;
            if (!hex4(s + 2, &lo)) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            s += 6;
          }
          // The in-place write never overtakes the read pointer: 6 input bytes give at most 4.
          if (cp < 0x80) *w++ = (char)cp;
          else if (cp < 0x800) { *w++ = (char)(0xC0 | (cp >> 6)); *w++ = (char)(0x80 | (cp & 63)); }
          else if (cp < 0x10000) { *w++ = (char)(0xE0 | (cp >> 12)); *w++ = (char)(0x80 | ((cp >> 6) & 63)); *w++ = (char)(0x80 | (cp & 63)); }
          else { *w++ = (char)(0xF0 | (cp >> 18)); *w++ = (char)(0x80 | ((cp >> 12) & 63)); *w++ = (char)(0x80 | ((cp >> 6) & 63)); *w++ = (char)(0x80 | (cp & 63)); }
          break;
        }
        case 0: return false;
        default: *w++ = *s++; break;
      }
    }
    if (*s != '"') return false;
    s++;
    *w = 0;
    return true;
  };
  skip();
  if (*s != '{') return false;
  s++;
  skip();
  if (*s == '}') return true;
  while (n_ < kMax) {
    skip();
    Item& it = items_[n_];
    if (!str(&it.key)) return false;
    skip();
    if (*s++ != ':') return false;
    skip();
    if (*s == '"') {
      // str() writes the terminator before the following separator is examined, so keep the position.
      it.isString = true;
      if (!str(&it.str)) return false;
      it.num = 0;
    } else if (*s == 't' && !strncmp(s, "true", 4)) { it.isString = false; it.num = 1; s += 4; it.str = nullptr; }
    else if (*s == 'f' && !strncmp(s, "false", 5)) { it.isString = false; it.num = 0; s += 5; it.str = nullptr; }
    else if (*s == 'n' && !strncmp(s, "null", 4)) { it.isString = false; it.num = 0; s += 4; it.str = nullptr; }
    else if (*s == '-' || (*s >= '0' && *s <= '9')) {
      char* end;
      it.isString = false;
      it.str = nullptr;
      it.num = strtod(s, &end);
      if (end == s) return false;
      s = end;
    } else return false;   // nested values are not part of the protocol
    n_++;
    skip();
    if (*s == ',') { s++; continue; }
    if (*s == '}') return true;
    return false;
  }
  return false;
}

const char* JsonObject::str(const char* key) const {
  for (int i = 0; i < n_; i++) if (!strcmp(items_[i].key, key) && items_[i].isString) return items_[i].str;
  return nullptr;
}

bool JsonObject::has(const char* key) const {
  for (int i = 0; i < n_; i++) if (!strcmp(items_[i].key, key)) return true;
  return false;
}

double JsonObject::num(const char* key, double def) const {
  for (int i = 0; i < n_; i++) if (!strcmp(items_[i].key, key) && !items_[i].isString) return items_[i].num;
  return def;
}

void JsonWriter::raw(const char* s) {
  while (*s && len_ + 1 < cap_) buf_[len_++] = *s++;
  buf_[len_] = 0;
}

void JsonWriter::key(const char* k) {
  raw(first_ ? "\"" : ",\"");
  first_ = false;
  raw(k);
  raw("\":");
}

void JsonWriter::add(const char* k, const char* v) {
  key(k);
  raw("\"");
  for (; *v && len_ + 7 < cap_; v++) {
    uint8_t c = (uint8_t)*v;
    if (c == '"' || c == '\\') { buf_[len_++] = '\\'; buf_[len_++] = (char)c; }
    else if (c == '\n') { buf_[len_++] = '\\'; buf_[len_++] = 'n'; }
    else if (c < 0x20) continue;
    else buf_[len_++] = (char)c;
  }
  buf_[len_] = 0;
  raw("\"");
}

void JsonWriter::add(const char* k, int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); key(k); raw(b); }
void JsonWriter::addf(const char* k, float v) { char b[24]; snprintf(b, sizeof(b), "%.1f", (double)v); key(k); raw(b); }
void JsonWriter::add(const char* k, bool v) { key(k); raw(v ? "true" : "false"); }
void JsonWriter::addU(const char* k, uint32_t v) { char b[16]; snprintf(b, sizeof(b), "%u", (unsigned)v); key(k); raw(b); }

static void addCore(App& a, JsonWriter& w) {
  w.add("emotion", emotionName(a.emotion.current()));
  w.add("energy", (int)(a.pet.stats.energy + .5f));
  w.add("happiness", (int)(a.pet.stats.happiness + .5f));
}

static void addFullState(App& a, JsonWriter& w) {
  addCore(a, w);
  const PetStats& s = a.pet.stats;
  w.add("curiosity", (int)(s.curiosity + .5f));
  w.add("attention", (int)(s.attention + .5f));
  w.add("affection", (int)(s.affection + .5f));
  w.add("stress", (int)(s.stress + .5f));
  w.add("sleepiness", (int)(s.sleepiness + .5f));
  w.add("hunger", (int)(s.hunger + .5f));
  w.add("fun", (int)(s.fun + .5f));
  w.add("health", (int)(s.health + .5f));
  w.add("pet_status", petStatusName(a.pet.status()));
  w.add("screen", screenName(a.screen()));
  w.add("battery", a.power.batteryPercent());
  w.add("charging", a.power.charging());
  w.add("volume", (int)a.settings.volume);
  w.add("brightness", (int)a.settings.brightness);
  w.add("sound", a.settings.soundOn);
  w.add("mic", a.settings.micOn);
  w.add("unread", a.notifications.unread());
  w.addU("time", a.clock.epoch());
}

int App::stateEvent(char* out, int cap) {
  JsonWriter w(out, cap);
  w.begin();
  w.add("event", "state");
  addFullState(*this, w);
  w.end();
  return w.length();
}

static int errorResp(char* out, int cap, const char* code, const char* msg, double id) {
  JsonWriter w(out, cap);
  w.begin();
  w.add("status", "error");
  w.add("error", code);
  w.add("message", msg);
  if (id >= 0) w.add("request_id", (int)id);
  w.end();
  return w.length();
}

// Settings reachable by name. Returning false means unknown name or invalid value.
static bool setSetting(App& a, const char* name, const JsonObject& j) {
  Settings& s = a.settings;
  double v = j.num("value", -1e9);
  const char* sv = j.str("value");
  auto rng = [&](uint8_t* t, int lo, int hi) { if (v < lo || v > hi) return false; *t = (uint8_t)v; return true; };
  auto flag = [&](bool* t) { if (j.has("value") && !sv) { *t = v != 0; return true; } return false; };
  if (!strcmp(name, "brightness")) return rng(&s.brightness, 5, 100);
  if (!strcmp(name, "volume")) return rng(&s.volume, 0, 100);
  if (!strcmp(name, "sound")) return flag(&s.soundOn);
  if (!strcmp(name, "mic")) return flag(&s.micOn);
  if (!strcmp(name, "mic_sensitivity")) return rng(&s.micSensitivity, 10, 100);
  if (!strcmp(name, "touch_sensitivity")) return rng(&s.touchSensitivity, 10, 100);
  if (!strcmp(name, "anim_speed")) return rng(&s.animSpeed, 25, 250);
  if (!strcmp(name, "blink_rate")) return rng(&s.blinkRate, 0, 250);
  if (!strcmp(name, "sleep_after")) { if (v < 15 || v > 7200) return false; s.sleepAfterS = (uint16_t)v; return true; }
  if (!strcmp(name, "power_save")) return flag(&s.powerSave);
  if (!strcmp(name, "wifi")) { a.netDirty = true; return flag(&s.wifiOn); }
  if (!strcmp(name, "ble")) { a.netDirty = true; return flag(&s.bleOn); }
  if (!strcmp(name, "cat_emotions")) return flag(&s.catEmotion);
  if (!strcmp(name, "cat_interactions")) return flag(&s.catInteraction);
  if (!strcmp(name, "cat_system")) return flag(&s.catSystem);
  if (!strcmp(name, "cat_games")) return flag(&s.catGame);
  if (!strcmp(name, "ai_voice_consent")) return flag(&s.aiVoiceConsent);
  if (!strcmp(name, "tz_minutes")) { if (v < -720 || v > 840) return false; s.tzMinutes = (int16_t)v; return true; }
  if (!strcmp(name, "personality") && sv) {
    for (int i = 0; i < (int)Personality::Count; i++)
      if (!strcmp(sv, personalityName((Personality)i))) { s.personality = (uint8_t)i; return true; }
    return false;
  }
  if (!strcmp(name, "ai_url") && sv) { if (strlen(sv) >= sizeof(s.aiUrl)) return false; strcpy(s.aiUrl, sv); a.netDirty = true; return true; }
  return false;
}

int App::handleCommand(const char* json, char* out, int cap, Session& session) {
  static char buf[1024];   // only the logic thread handles commands
  if (strlen(json) >= sizeof(buf)) return errorResp(out, cap, "too_long", "command exceeds 1023 bytes", -1);
  strcpy(buf, json);
  JsonObject j;
  if (!j.parse(buf)) return errorResp(out, cap, "bad_json", "expected a flat JSON object", -1);
  double id = j.num("request_id", -1);
  const char* type = j.str("type");
  if (!type) return errorResp(out, cap, "no_type", "field 'type' is required", id);

  JsonWriter w(out, cap);
  auto ok = [&]() { w.begin(); w.add("status", "success"); };
  auto done = [&]() {
    addCore(*this, w);
    if (id >= 0) w.add("request_id", (int)id);
    w.end();
    return w.length();
  };
  int proto = (int)j.num("protocol", kProtocolVersion);
  if (proto > kProtocolVersion) return errorResp(out, cap, "protocol_mismatch", "device speaks an older protocol", id);

  if (!strcmp(type, "ping")) { ok(); return done(); }
  if (!strcmp(type, "hello")) {
    ok();
    w.add("protocol", kProtocolVersion);
    w.add("firmware", kFirmwareVersion);
    w.add("device", "ESP32-S3-Touch-AMOLED-1.75C");
    w.add("platform", hal.platform);
    w.add("auth_required", !session.authed);
    return done();
  }
  if (!strcmp(type, "auth")) {
    if (authLockMs_ > 0) return errorResp(out, cap, "auth_locked", "too many attempts, wait a minute", id);
    const char* pin = j.str("pin");
    if (pin && settings.pin[0] && !strcmp(pin, settings.pin)) {
      session.authed = true;
      authFails_ = 0;
      ok();
      return done();
    }
    if (++authFails_ >= 5) { authFails_ = 0; authLockMs_ = 60000; }
    return errorResp(out, cap, "auth_failed", "wrong PIN, see device Info screen", id);
  }
  if (!session.authed) return errorResp(out, cap, "auth_required", "send {\"type\":\"auth\",\"pin\":\"...\"} first", id);

  power.activity();
  const char* value = j.str("value");

  if (!strcmp(type, "get_state")) { ok(); addFullState(*this, w); if (id >= 0) w.add("request_id", (int)id); w.end(); return w.length(); }
  if (!strcmp(type, "emotion")) {
    Emotion e;
    if (!value || !emotionFromName(value, &e)) return errorResp(out, cap, "bad_value", "unknown emotion", id);
    pet.event(PetEvent::RemoteAction);
    sfx.resetCooldowns();
    if (e == Emotion::Sleeping) sleepNow();
    else {
      if (pet.asleep()) wakeNow();
      emotion.request(e, EmotionPriority::Forced, (float)j.num("hold_ms", 3000));
    }
    ok();
    w.add("emotion_requested", value);
    return done();
  }
  if (!strcmp(type, "action")) {
    static const struct { const char* n; PetEvent e; } acts[] = {{"feed", PetEvent::Feed}, {"pet", PetEvent::Pet}, {"play", PetEvent::Play}, {"sleep", PetEvent::GoSleep}, {"wake", PetEvent::WakeUp}};
    for (auto& a : acts) {
      if (value && !strcmp(value, a.n)) { sfx.resetCooldowns(); petAction(a.e); ok(); return done(); }
    }
    return errorResp(out, cap, "bad_value", "action: feed, pet, play, sleep, wake", id);
  }
  if (!strcmp(type, "set_stat")) {
    const char* name = j.str("name");
    if (!name || !j.has("value") || !pet.set(name, (float)j.num("value", 0))) return errorResp(out, cap, "bad_value", "unknown stat or value", id);
    ok();
    return done();
  }
  if (!strcmp(type, "setting")) {
    const char* name = j.str("name");
    if (!name || !setSetting(*this, name, j)) return errorResp(out, cap, "bad_value", "unknown setting or value", id);
    applySettings();
    saveSettings();
    ok();
    return done();
  }
  if (!strcmp(type, "get_settings")) {
    ok();
    const Settings& s = settings;
    w.add("brightness", (int)s.brightness); w.add("volume", (int)s.volume); w.add("sound", s.soundOn); w.add("mic", s.micOn);
    w.add("mic_sensitivity", (int)s.micSensitivity); w.add("touch_sensitivity", (int)s.touchSensitivity);
    w.add("anim_speed", (int)s.animSpeed); w.add("blink_rate", (int)s.blinkRate); w.add("sleep_after", (int)s.sleepAfterS);
    w.add("power_save", s.powerSave); w.add("wifi", s.wifiOn); w.add("ble", s.bleOn); w.add("personality", personalityName((Personality)s.personality));
    w.add("cat_emotions", s.catEmotion); w.add("cat_interactions", s.catInteraction); w.add("cat_system", s.catSystem); w.add("cat_games", s.catGame);
    w.add("ai_voice_consent", s.aiVoiceConsent); w.add("wifi_ssid", s.wifiSsid); w.add("ai_url", s.aiUrl); w.add("tz_minutes", (int)s.tzMinutes);
    return done();
  }
  if (!strcmp(type, "notify")) {
    const char* title = j.str("title");
    if (!title) return errorResp(out, cap, "bad_value", "field 'title' is required", id);
    const char* text = j.str("text");
    const char* icon = j.str("icon");
    const char* src = j.str("source");
    notifications.add(title, text ? text : "", icon ? icon : "bell", src ? src : session.source, j.has("time") ? (uint32_t)j.num("time", 0) : clock.epoch());
    ok();
    w.add("unread", notifications.unread());
    return done();
  }
  if (!strcmp(type, "notifications")) {
    if (value && !strcmp(value, "clear")) notifications.clear();
    if (value && !strcmp(value, "read_all")) notifications.markAllRead();
    ok();
    w.add("count", notifications.count());
    w.add("unread", notifications.unread());
    return done();
  }
  if (!strcmp(type, "screen")) {
    ScreenId s;
    if (!value || !screenFromName(value, &s)) return errorResp(out, cap, "bad_value", "unknown screen", id);
    showScreen(s);
    ok();
    return done();
  }
  if (!strcmp(type, "play_sound")) {
    if (!value || pack.find(value) < 0) return errorResp(out, cap, "bad_value", "unknown sound", id);
    sfx.play(value);
    ok();
    return done();
  }
  if (!strcmp(type, "set_time")) {
    if (!j.has("epoch")) return errorResp(out, cap, "bad_value", "field 'epoch' is required", id);
    int tz = j.has("tz_minutes") ? (int)j.num("tz_minutes", 0) : 0x7FFF;
    clock.setEpoch((uint32_t)j.num("epoch", 0), tz);
    if (tz != 0x7FFF) { settings.tzMinutes = (int16_t)tz; saveSettings(); }
    ok();
    return done();
  }
  if (!strcmp(type, "wifi_config")) {
    const char* ssid = j.str("ssid");
    const char* pass = j.str("password");
    if (!ssid || strlen(ssid) >= sizeof(settings.wifiSsid) || (pass && strlen(pass) >= sizeof(settings.wifiPass)))
      return errorResp(out, cap, "bad_value", "ssid up to 32, password up to 64 bytes", id);
    strcpy(settings.wifiSsid, ssid);
    strcpy(settings.wifiPass, pass ? pass : "");
    settings.wifiOn = true;
    netDirty = true;
    saveSettings();
    ok();
    return done();
  }
  if (!strcmp(type, "chat")) {
    const char* text = j.str("text");
    if (!text) return errorResp(out, cap, "bad_value", "field 'text' is required", id);
    chat(text);
    ok();
    w.add("ai", ai_->name());
    return done();
  }
  if (!strcmp(type, "ai_listen")) {
    if (!settings.aiVoiceConsent) return errorResp(out, cap, "no_consent", "enable ai_voice_consent first", id);
    listen();
    ok();
    return done();
  }
  if (!strcmp(type, "gesture")) {
    static const struct { const char* n; Gesture g; } gs[] = {{"tap", Gesture::Tap}, {"double_tap", Gesture::DoubleTap}, {"long_press", Gesture::LongPress},
      {"swipe_left", Gesture::SwipeLeft}, {"swipe_right", Gesture::SwipeRight}, {"swipe_up", Gesture::SwipeUp}, {"swipe_down", Gesture::SwipeDown}};
    for (auto& g : gs) {
      if (value && !strcmp(value, g.n)) {
        GestureEvent e;
        e.g = g.g;
        e.x = clampi((int)j.num("x", kScreenW / 2), 0, kScreenW - 1);
        e.y = clampi((int)j.num("y", kScreenH / 2), 0, kScreenH - 1);
        gesture(e);
        ok();
        return done();
      }
    }
    return errorResp(out, cap, "bad_value", "unknown gesture", id);
  }
  if (!strcmp(type, "sim_event")) {
    static const struct { const char* n; int kind; int v; } ev[] = {
      {"shake", 0, (int)MotionEvent::Shake}, {"jerk", 0, (int)MotionEvent::Jerk}, {"face_down", 0, (int)MotionEvent::FaceDown}, {"lift", 0, (int)MotionEvent::Lifted},
      {"loud", 1, (int)SoundEvent::Loud}, {"clap", 1, (int)SoundEvent::Clap}, {"voice", 1, (int)SoundEvent::VoiceStart}};
    for (auto& e : ev) {
      if (value && !strcmp(value, e.n)) {
        if (e.kind == 0) motion((MotionEvent)e.v);
        else sound((SoundEvent)e.v);
        ok();
        return done();
      }
    }
    return errorResp(out, cap, "bad_value", "unknown event", id);
  }
  if (!strcmp(type, "diag")) {
    ok();
    w.add("platform", hal.platform); w.addf("fps", hal.fps); w.addU("free_heap", hal.freeHeap); w.addU("free_psram", hal.freePsram);
    w.add("touch_ok", hal.touchOk); w.add("imu_ok", hal.imuOk); w.add("pmic_ok", hal.pmicOk); w.add("codec_ok", hal.codecOk); w.add("mic_ok", hal.micOk);
    w.add("ble", hal.bleConnected); w.add("wifi", hal.wifiConnected); w.add("ip", hal.ip); w.add("mac", hal.mac);
    w.addf("mic_db", mic.levelDb()); w.addf("ax", hal.ax); w.addf("ay", hal.ay); w.addf("az", hal.az); w.add("battery_mv", power.batteryMv());
    w.add("sounds", pack.count()); w.addU("sounds_played", audio.playCount());
    return done();
  }
  if (!strcmp(type, "factory_reset")) {
    if (!(value && !strcmp(value, "confirm"))) return errorResp(out, cap, "bad_value", "value must be 'confirm'", id);
    factoryReset();
    ok();
    return done();
  }
  return errorResp(out, cap, "unknown_type", "unknown command", id);
}

}  // namespace cc
