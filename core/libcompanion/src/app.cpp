#include "companion/app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

namespace cc {

static constexpr uint16_t kPetVersion = 2;
struct PetBlob { PetStats stats; uint8_t asleep; uint32_t savedAt; };

// Left/right swipe on these screens moves between them; other screens swipe right to go back.
static const ScreenId kCarousel[] = {ScreenId::Face, ScreenId::Pet, ScreenId::Weather, ScreenId::DigitalClock, ScreenId::AnalogClock, ScreenId::Games};
static constexpr int kCarouselN = sizeof(kCarousel) / sizeof(kCarousel[0]);

App::App() {
  for (int i = 0; i < (int)ScreenId::Count; i++) screens_[i] = screenInstance((ScreenId)i);
  memset(&toast_, 0, sizeof(toast_));
}

void App::begin(Storage* st, const uint8_t* packData, uint32_t packSize, uint32_t seed) {
  storage = st;
  rng.seed(seed);
  anim.reseed(seed ^ 0x5bd1e995u);
  pack.load(packData, packSize);
  audio.setPack(&pack);
  sfx.attach(&audio, &pack);
  Settings loaded;
  if (loadChecked(storage, "settings", &loaded, sizeof(loaded), kSettingsVersion)) settings = loaded;
  if (!settings.pin[0]) {
    snprintf(settings.pin, sizeof(settings.pin), "%06d", rng.range(0, 999999));
    saveSettings();
  }
  PetBlob blob;
  if (loadChecked(storage, "pet", &blob, sizeof(blob), kPetVersion)) {
    pet.stats = blob.stats;
    pet.setAsleep(blob.asleep != 0);
    clock.restore(blob.savedAt);  // a coarse time until NTP or an app sets it
  }
  clock.setTz(settings.tzMinutes);
  applySettings();
  // Booting always shows a little awakening, whatever state was saved.
  bool asleep = pet.asleep();
  emotion.reset(Emotion::Sleeping);
  anim.setEmotion(Emotion::Sleeping);
  if (!asleep) { pet.setAsleep(false); emotion.request(Emotion::WakingUp, EmotionPriority::Forced, 1500); }
  sfx.play("system/boot");
}

void App::applySettings() {
  audio.setVolume(settings.volume);
  audio.setMuted(!settings.soundOn);
  audio.setCategoryEnabled(SoundCategory::Emotion, settings.catEmotion);
  audio.setCategoryEnabled(SoundCategory::Interaction, settings.catInteraction);
  audio.setCategoryEnabled(SoundCategory::System, settings.catSystem);
  audio.setCategoryEnabled(SoundCategory::Game, settings.catGame);
  mic.setEnabled(settings.micOn);
  mic.setSensitivity(settings.micSensitivity);
  touch.setSensitivity(settings.touchSensitivity);
  anim.setSpeed(settings.animSpeed / 100.f);
  anim.setBlinkScale(settings.blinkRate / 100.f);
  power.configure(settings.sleepAfterS, settings.brightness, settings.powerSave);
  pet.setPersonality((Personality)(settings.personality < (int)Personality::Count ? settings.personality : 0));
  clock.setTz(settings.tzMinutes);
}

void App::saveSettings() { saveChecked(storage, "settings", &settings, sizeof(settings), kSettingsVersion); }

void App::savePet() {
  PetBlob b;
  b.stats = pet.stats;
  b.asleep = pet.asleep();
  b.savedAt = clock.epoch();
  saveChecked(storage, "pet", &b, sizeof(b), kPetVersion);
}

void App::factoryReset() {
  char pin[7];
  memcpy(pin, settings.pin, sizeof(pin));  // keep pairing: apps stay connected after a reset
  settings = Settings{};
  memcpy(settings.pin, pin, sizeof(pin));
  pet.stats = PetStats{};
  pet.setAsleep(false);
  notifications.clear();
  if (storage) { storage->erase("pet"); }
  applySettings();
  saveSettings();
  savePet();
  emotion.request(Emotion::WakingUp, EmotionPriority::Forced, 1500);
  netDirty = true;
}

void App::showScreen(ScreenId s) {
  if (s == screenId_) return;
  prevId_ = screenId_;
  screenId_ = s;
  iris_ = 0;
  touch.setDoubleTapEnabled(cur()->wantsDoubleTap());
  cur()->enter(*this);
  if (s != ScreenId::Face) anim.clearLook();
}

void App::goBack() {
  ScreenId s = screenId_;
  if (s >= ScreenId::TicTacToe && s <= ScreenId::Mole) showScreen(ScreenId::Games);
  else if (s == ScreenId::Face) return;
  else if (s == ScreenId::Menu || s == ScreenId::Settings || s == ScreenId::Notifications) showScreen(ScreenId::Face);
  else showScreen(ScreenId::Menu);
}

void App::sleepNow() {
  if (pet.asleep()) return;
  pet.event(PetEvent::GoSleep);
  emotion.request(Emotion::Sleeping, EmotionPriority::Forced);
  sfx.play("interactions/flip");
}

void App::wakeNow() {
  if (!pet.asleep() && emotion.current() != Emotion::Sleeping) return;
  pet.event(PetEvent::WakeUp);
  restingUntilRested_ = false;
  power.activity();
  emotion.request(Emotion::WakingUp, EmotionPriority::Forced, 1200);
}

void App::petAction(PetEvent e) {
  power.activity();
  switch (e) {
    case PetEvent::Feed:
      if (pet.asleep()) wakeNow();
      {
        bool wasHungry = pet.stats.hunger >= 55;
        pet.event(e);
        sfx.play("interactions/feed");
        anim.kick(-0.2f);
        // Thanks for a meal it needed, delight for a snack, disgust at being stuffed.
        react(pet.stats.hunger <= 0 ? Emotion::Disgusted : (wasHungry ? Emotion::Grateful : Emotion::Happy), 2200);
      }
      break;
    case PetEvent::Pet:
      if (pet.asleep()) wakeNow();
      pet.event(e);
      sfx.play("interactions/pet");
      react(Emotion::Affection, 2800);
      break;
    case PetEvent::Play:
      if (pet.asleep()) wakeNow();
      pet.event(e);
      sfx.play("interactions/double_tap");
      anim.kick(-0.32f);
      react(pet.stats.energy > 25 ? (rng.range(0, 1) ? Emotion::Excited : Emotion::Playful) : Emotion::Exhausted, 2400);
      break;
    case PetEvent::GoSleep: sleepNow(); break;
    case PetEvent::WakeUp: wakeNow(); break;
    default: pet.event(e); break;
  }
}

void App::imuInput(float ax, float ay, float az, float gx, float gy, float gz, float dtMs) {
  hal.ax = ax; hal.ay = ay; hal.az = az;
  motionMgr.feed(ax, ay, az, gx, gy, gz, dtMs);
}

void App::motion(MotionEvent e) {
  if (cur()->onMotion(*this, e)) { power.activity(); return; }
  switch (e) {
    case MotionEvent::FaceDown:
      pet.event(PetEvent::FaceDown);
      restingUntilRested_ = false;
      sleepNow();
      break;
    case MotionEvent::Lifted:
      pet.event(PetEvent::Lift);
      power.activity();
      if (pet.asleep()) wakeNow();
      else react(Emotion::Attentive, 1000);
      break;
    case MotionEvent::Shake:
      pet.event(PetEvent::Shake);
      power.activity();
      sfx.play("interactions/shake");
      if (pet.asleep()) wakeNow();
      react(pet.stats.stress > 70 ? Emotion::Dizzy : (pet.stats.stress > 55 ? Emotion::Confused : Emotion::Surprised), 2000, EmotionPriority::Strong);
      break;
    case MotionEvent::Jerk:
      pet.event(PetEvent::Jerk);
      power.activity();
      if (pet.asleep()) wakeNow();
      react(pet.stats.stress > 60 ? Emotion::Terrified : Emotion::Scared, 1600, EmotionPriority::Strong);
      break;
    default: break;
  }
}

void App::sound(SoundEvent e) {
  switch (e) {
    case SoundEvent::Loud:
      pet.event(PetEvent::LoudNoise);
      sfx.play("interactions/clap");
      if (pet.asleep()) wakeNow();
      react(pet.stats.stress > 40 ? Emotion::Scared : Emotion::Surprised, 1500, EmotionPriority::Strong);
      anim.lookOffset(rng.rangef(-30, 30), -10, 800);
      break;
    case SoundEvent::Clap:
      pet.event(PetEvent::Clap);
      power.activity();
      if (pet.asleep()) { if (pet.stats.sleepiness < 60) wakeNow(); break; }  // a deep sleeper ignores one clap
      react(Emotion::Attentive, 1400);
      anim.kick(-0.14f);
      break;
    case SoundEvent::VoiceStart:
      pet.event(PetEvent::Voice);
      power.activity();
      if (!pet.asleep()) react(Emotion::Attentive, 1800, EmotionPriority::Idle);
      break;
    default: break;
  }
}

void App::gesture(const GestureEvent& e) {
  power.activity();
  bool cap = cur()->capturesSwipes();
  bool isSwipe = e.g == Gesture::SwipeLeft || e.g == Gesture::SwipeRight || e.g == Gesture::SwipeUp || e.g == Gesture::SwipeDown;
  if (isSwipe && !cap && (e.g == Gesture::SwipeLeft || e.g == Gesture::SwipeRight)) {
    int idx = -1;
    for (int i = 0; i < kCarouselN; i++) if (kCarousel[i] == screenId_) idx = i;
    if (idx >= 0) {
      sfx.play("interactions/swipe");
      showScreen(kCarousel[(idx + (e.g == Gesture::SwipeLeft ? 1 : kCarouselN - 1)) % kCarouselN]);
      return;
    }
    if (e.g == Gesture::SwipeRight) { sfx.play("interactions/swipe"); goBack(); return; }
  }
  // Long press returns to the face from anywhere, the face itself opens the menu.
  if (e.g == Gesture::LongPress && screenId_ != ScreenId::Face && !cap) { sfx.play("interactions/hold"); showScreen(ScreenId::Face); return; }
  cur()->onGesture(*this, e);
}

void App::toast(const char* title, const char* text, const char* soundName) {
  snprintf(toast_.title, sizeof(toast_.title), "%s", title);
  snprintf(toastText_, sizeof(toastText_), "%s", text);
  toastMs_ = 3800;
  if (soundName) sfx.play(soundName);
}

void App::caption(const char* text, float ms) {
  snprintf(caption_, sizeof(caption_), "%s", text);
  captionMs_ = ms;
}

void App::chat(const char* text) {
  ai_->submitText(text);
  aiThinkMs_ = 1000;  // even an instant answer looks considered
  react(Emotion::Thinking, 1500, EmotionPriority::Strong);
}

void App::listen() {
  if (!settings.aiVoiceConsent) { toast("Голос выключен", "Разреши в настройках", "system/error"); return; }
  if (!ai_->submitVoice()) { toast("Нет AI-сервера", "Голос требует Wi-Fi и сервер", "system/error"); return; }
  showScreen(ScreenId::Face);
  react(Emotion::Attentive, 4000, EmotionPriority::Strong);
  sfx.play("system/notify");
}

float App::alertness() const {
  if (!clock.isSet()) return 0.8f;  // unknown time: behave like daytime
  DateTime d = clock.now();
  float h = d.hour + d.minute / 60.f;
  return 0.5f + 0.5f * cosf((h - 14.f) * 0.2618f);
}

uint16_t App::eyeColor() const {
  float night = clampf((0.45f - alertness()) * 2.5f, 0, 1);
  return rgb565(255, (uint8_t)(255 - night * 42), (uint8_t)(255 - night * 90));
}

void App::setWeather(Weather w, float tempC) {
  bool changed = !weather.known || w != weather.cond;
  weather.cond = w; weather.tempC = tempC; weather.known = true; weather.at = clock.epoch();
  if (!changed || pet.asleep()) return;
  // Glance at the sky: a new kind of weather is an event in this creature's small world.
  switch (w) {
    case Weather::Rain: react(Emotion::Sad, 2600); break;
    case Weather::Snow: react(Emotion::Excited, 2600); break;
    case Weather::Storm: react(Emotion::Scared, 2400); break;
    case Weather::Clear: react(Emotion::Happy, 2400); break;
    case Weather::Fog: react(Emotion::Confused, 2200); break;
    default: react(Emotion::Curious, 2000); break;
  }
}

// Weighted choice of the next thought: the state decides what is likely, chance decides which it is.
Emotion App::chooseThought() {
  const PetStats& s = pet.stats;
  float alert = alertness();
  float valence = (s.happiness - 50) / 50 - s.stress / 200;
  float w[kEmotionCount] = {};
  auto add = [&](Emotion e, float v) { if (v > 0) w[(int)e] += v; };
  add(Emotion::Neutral, 0.8f);
  add(Emotion::Curious, s.curiosity / 50);
  add(Emotion::Thinking, 0.5f);
  add(Emotion::Focused, 0.25f * alert);
  add(Emotion::Attentive, s.attention / 120);
  add(Emotion::Bored, (100 - s.fun) / 70);
  add(Emotion::Dreamy, (1 - alert) * 1.4f + 0.15f);
  add(Emotion::Yawning, (s.sleepiness - 20) / 60 * (1.2f - alert));
  add(Emotion::Sleepy, (s.sleepiness - 40) / 80);
  add(Emotion::Mischievous, s.fun / 110 * (valence > -0.2f ? 1 : 0));
  add(Emotion::Happy, valence * 1.2f);
  add(Emotion::Affection, s.affection / 140);
  add(Emotion::Winking, 0.25f * (valence + 0.6f));
  add(Emotion::Proud, valence > 0.3f ? 0.25f : 0);
  add(Emotion::Sad, -valence * 1.2f);
  add(Emotion::Annoyed, s.stress / 80);
  add(Emotion::Smug, valence > 0.2f ? 0.3f : 0);
  add(Emotion::Skeptical, 0.25f);
  add(Emotion::Sneaky, s.fun / 200);
  add(Emotion::Playful, s.fun / 90 * alert);
  add(Emotion::Suspicious, s.stress / 90 + 0.1f);
  add(Emotion::Hopeful, s.attention < 40 ? 0.5f : 0.1f);
  add(Emotion::Lonely, pet.idleSeconds() > 120 ? 0.7f : 0);
  add(Emotion::Jealous, 0.08f);
  add(Emotion::Nostalgic, (1 - alert) * 0.5f + 0.1f);
  add(Emotion::Amazed, s.curiosity / 300);
  add(Emotion::Hungry, (s.hunger - 45) / 40);
  add(Emotion::Exhausted, (45 - s.energy) / 40);
  add(Emotion::Relieved, s.stress < 15 ? 0.15f : 0);
  if (pet.personality() == Personality::Shy) add(Emotion::Shy, 1.2f); else add(Emotion::Shy, 0.15f);
  if (pet.personality() == Personality::Grumpy) add(Emotion::Annoyed, 0.8f);
  if (pet.personality() == Personality::Playful) { add(Emotion::Mischievous, 0.6f); add(Emotion::Excited, 0.4f); }
  if (pet.personality() == Personality::Calm) add(Emotion::Dreamy, 0.6f);
  if (weather.known) {
    switch (weather.cond) {
      case Weather::Rain: add(Emotion::Sad, 0.5f); add(Emotion::Dreamy, 0.7f); break;
      case Weather::Snow: add(Emotion::Cold, 0.8f); add(Emotion::Excited, 0.4f); break;
      case Weather::Storm: add(Emotion::Scared, 0.8f); add(Emotion::Terrified, 0.3f); add(Emotion::Attentive, 0.5f); break;
      case Weather::Clear: add(Emotion::Happy, 0.6f * alert); add(Emotion::Amazed, 0.15f); break;
      case Weather::Fog: add(Emotion::Confused, 0.5f); break;
      case Weather::Wind: add(Emotion::Curious, 0.5f); break;
      default: break;
    }
    if (weather.tempC >= 28) add(Emotion::Hot, 1.5f);
    if (weather.tempC <= 0) add(Emotion::Cold, 1.5f);
  }
  float total = 0;
  for (float v : w) total += v;
  float pick = rng.rangef(0, total);
  for (int i = 0; i < kEmotionCount; i++) {
    pick -= w[i];
    if (pick <= 0 && w[i] > 0) return (Emotion)i;
  }
  return Emotion::Neutral;
}

void App::pushInput(const char* action, int index) {
  int n = (inputHead_ + 1) % kInputQueue;
  if (n == inputTail_) return;  // host not reading: dropping beats blocking the game loop
  inputQ_[inputHead_] = InputEvent{action, index};
  inputHead_ = n;
}

int App::pollInput(char* out, int cap) {
  if (!controller.active) { inputTail_ = inputHead_; return 0; }
  if (inputTail_ != inputHead_) {
    InputEvent e = inputQ_[inputTail_];
    inputTail_ = (inputTail_ + 1) % kInputQueue;
    return snprintf(out, (size_t)cap, "{\"event\":\"input\",\"action\":\"%s\",\"index\":%d}", e.action, e.index);
  }
  if (nowMs_ < tiltDueMs_) return 0;
  tiltDueMs_ = nowMs_ + 50;
  return snprintf(out, (size_t)cap, "{\"event\":\"input\",\"ax\":%.2f,\"ay\":%.2f}", (double)hal.ax, (double)hal.ay);
}

bool App::pollMouse(MouseReport* r) {
  MouseState& m = mouseState;
  int dx = (int)clampf(m.x, -127, 127), dy = (int)clampf(m.y, -127, 127), wheel = (int)clampf(m.wheel, -127, 127);
  if (!dx && !dy && !wheel && !m.buttonsChanged) return false;
  m.x -= dx; m.y -= dy; m.wheel -= wheel;
  m.buttonsChanged = false;
  *r = MouseReport{dx, dy, wheel, m.buttons};
  return true;
}

void App::tickIdleLife(float dt) {
  // Slow background mix: the current face always carries a trace of how the creature really feels.
  blendIn_ -= dt;
  if (blendIn_ <= 0) {
    blendIn_ = rng.rangef(3000, 8000);
    Emotion second = pet.asleep() ? Emotion::Sleeping : (rng.range(0, 2) == 0 ? chooseThought() : pet.moodEmotion());
    anim.setBlend(second, rng.rangef(0.06f, 0.24f));
  }
  // The stream of thoughts: quicker when lively, slower when tired. The creature is never on pause for long.
  idleLifeIn_ -= dt;
  if (idleLifeIn_ > 0) return;
  float a = (pet.stats.energy * 0.5f + pet.stats.attention * 0.3f + pet.stats.fun * 0.2f) / 100 * (0.5f + 0.5f * alertness());
  idleLifeIn_ = rng.rangef(0.7f, 1.3f) * lerpf(9000, 3200, a);
  if (pet.asleep() || screenId_ != ScreenId::Face) return;
  switch (rng.range(0, 9)) {
    case 0: anim.blink(170, true); break;
    case 1: anim.independentLook(-10, -6, 16, -4, 1000); break;
    case 2: anim.startPursuit(rng.rangef(2500, 6000)); break;
    case 3: anim.lookOffset(rng.rangef(-40, 40), rng.rangef(-18, 18), 900); break;
    case 4: anim.kick(-0.08f); break;
    default: react(chooseThought(), rng.rangef(1800, 4200), EmotionPriority::Idle); break;
  }
}

void App::tickBehaviour(float dt) {
  float simDt = dt * clock.scale();
  float alert = alertness();
  pet.setAlertness(alert);
  pet.tick(simDt);
  const PetStats& ps = pet.stats;
  float arousal = (ps.energy * 0.45f + ps.attention * 0.25f + ps.fun * 0.15f + ps.stress * 0.15f) / 100 * (0.45f + 0.55f * alert);
  anim.setDrive(arousal, (ps.happiness - 50) / 50 - ps.stress / 200, alert, hal.ax, hal.ay);
  // Weather older than six hours says nothing about now.
  if (weather.known && clock.epoch() - weather.at > 6 * 3600) weather.known = false;
  if (thunderFlashMs > 0) thunderFlashMs -= dt;
  if (weather.known && weather.cond == Weather::Storm) {
    thunderIn_ -= simDt;
    if (thunderIn_ <= 0) {
      thunderIn_ = rng.rangef(12000, 42000);
      thunderFlashMs = 130;
      if (screenId_ == ScreenId::Face && !pet.asleep()) { react(Emotion::Scared, 1300); sfx.play("interactions/shake"); }
    }
  }
  // Night: left alone, it goes to bed by itself.
  if (alert < 0.2f && !pet.asleep() && pet.idleSeconds() > 180) sleepNow();
  emotion.setMood(pet.moodEmotion());
  // Only a sleep caused by tiredness ends by itself: a nap from inactivity or being put down lasts until something wakes it.
  if (pet.wantsSleep() && !pet.asleep()) { sleepNow(); restingUntilRested_ = true; }
  if (restingUntilRested_ && pet.wantsWake()) wakeNow();
  if (pet.asleep() && emotion.current() != Emotion::Sleeping && emotion.mood() == Emotion::Sleeping)
    emotion.request(Emotion::Sleeping, EmotionPriority::Reaction);

  // Inactivity: dim, then nap. Interaction wakes through gesture().
  PowerState before = power.state();
  power.tick(dt);
  if (power.state() == PowerState::Asleep && before != PowerState::Asleep) {
    if (screenId_ != ScreenId::Timer && screenId_ != ScreenId::Stopwatch) showScreen(ScreenId::Face);
    if (!power.charging()) sleepNow();
    sleepRequested = true;
  }
  if (power.state() != PowerState::Asleep) sleepRequested = false;

  // Battery reactions.
  if (power.lowBattery() && !lowBatteryWarned_) {
    lowBatteryWarned_ = true;
    notifications.add("Батарея", "Заряд низкий, подключи USB", "warning", "system", clock.epoch());
  }
  if (!power.lowBattery()) lowBatteryWarned_ = false;
  if (power.charging() && !wasCharging_) { power.activity(); react(Emotion::Happy, 1500); }
  wasCharging_ = power.charging();

  if (timerRunning) {
    timerLeftMs -= simDt;
    if (timerLeftMs <= 0) {
      timerLeftMs = 0; timerRunning = false;
      power.activity();
      if (pet.asleep()) wakeNow();
      react(Emotion::Excited, 2500, EmotionPriority::Strong);
      toast("Таймер", "Время вышло!", "system/timer_done");
    }
  }
  if (swRunning) swMs += simDt;

  Notification n;
  if (toastMs_ <= 0 && notifications.popToast(&n)) {
    // Arrival: glance at it, show a small pill and leave the face alone.
    pet.event(PetEvent::Notification);
    power.activity();
    if (!pet.asleep()) react(Emotion::Curious, 1600, EmotionPriority::Reaction);
    toast(n.title, n.text, "system/notify");
  }
  if (aiThinkMs_ > 0) aiThinkMs_ -= dt;
  AIReply r;
  if (aiThinkMs_ <= 0 && ai_->poll(&r)) {
    caption(r.text);
    react(r.emotion, 3000, EmotionPriority::Strong);
  }
}

void App::tick(float dt) {
  if (dt < 0) dt = 0;
  if (dt > 250) dt = 250;
  nowMs_ += dt;
  clock.tick(dt);

  touch.feed(rawDown_, rawX_, rawY_, dt);
  GestureEvent ge;
  while (touch.poll(&ge)) gesture(ge);
  MotionEvent me;
  while (motionMgr.poll(&me)) motion(me);
  SoundEvent se;
  while (mic.poll(&se)) sound(se);

  tickBehaviour(dt);
  tickIdleLife(dt);
  cur()->tick(*this, dt);

  emotion.update(dt);
  if (emotion.consumeChanged()) anim.setEmotion(emotion.current());
  anim.update(dt);
  // Idle thoughts are mostly silent: a creature that comments on every glance would be unbearable.
  if (anim.soundCueReady() && (emotion.enteredPriority() > EmotionPriority::Idle || rng.range(0, 4) == 0)) sfx.onEmotionCue(anim.emotion(), nowMs_);

  if (iris_ < 1) iris_ += dt / 220.f;
  if (toastMs_ > 0) toastMs_ -= dt;
  if (captionMs_ > 0) captionMs_ -= dt;
  if (authLockMs_ > 0) authLockMs_ -= dt;

  saveIn_ -= dt;
  if (saveIn_ <= 0) { saveIn_ = 60000; savePet(); }
}

// Shortens text to a pixel width on a character boundary, adding an ellipsis.
static void fitText(char* out, size_t cap, const char* src, FontSize f, int maxW) {
  snprintf(out, cap, "%s", src);
  size_t n = strlen(out);
  if (Canvas::textWidth(out, f) <= maxW) return;
  while (n > 0) {
    n--;
    while (n > 0 && ((uint8_t)out[n] & 0xC0) == 0x80) n--;
    if (n + 4 > cap) continue;
    memcpy(out + n, "\xE2\x80\xA6", 4);  // UTF-8 ellipsis with terminator
    if (Canvas::textWidth(out, f) <= maxW) return;
  }
}

void App::drawOverlays(Canvas& c) {
  if (toastMs_ > 0) {
    // A slim pill near the top edge, sliding in: the face stays the main thing on screen.
    float inT = smooth01((3800 - toastMs_) / 180), outT = smooth01(toastMs_ / 180);
    float y = lerpf(-74, 46, inT < outT ? inT : outT);
    c.roundRect(kScreenW / 2.f - 140, y, 280, 64, 26, kDark);
    c.fillCircle(kScreenW / 2.f - 118, y + 32, 6, kAccent);
    char fit[64];
    fitText(fit, sizeof(fit), toast_.title, FontSize::S, 216);
    c.text(kScreenW / 2 + 8, (int)y + 6, fit, FontSize::S, kWhite, Align::Center);
    fitText(fit, sizeof(fit), toastText_, FontSize::S, 216);
    c.text(kScreenW / 2 + 8, (int)y + 32, fit, FontSize::S, kSoft, Align::Center);
  }
  if (captionMs_ > 0) c.textWrapped(kScreenW / 2, kScreenH - 110, 330, caption_, FontSize::S, kSoft, Align::Center, 3);
}

void App::render(Canvas& c) {
  c.clear(kBlack);
  cur()->draw(*this, c);
  drawOverlays(c);
  if (iris_ < 1) c.maskOutsideCircle(kScreenW / 2.f, kScreenH / 2.f, lerpf(30, 340, smooth01(iris_)));
}

Rect App::dirtyRect() const {
  // The face leaves most of the panel untouched, pushing less reduces power and tearing.
  if (screenId_ == ScreenId::Face && toastMs_ <= 0 && captionMs_ <= 0 && iris_ >= 1 && !weather.known) return face.bounds(anim);
  return Rect{0, 0, kScreenW, kScreenH};
}

}  // namespace cc
