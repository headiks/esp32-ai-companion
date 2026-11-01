#include "companion/app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

namespace cc {

static constexpr uint16_t kPetVersion = 2;
struct PetBlob { PetStats stats; uint8_t asleep; uint32_t savedAt; };

// Left/right swipe on these screens moves between them; other screens swipe right to go back.
static const ScreenId kCarousel[] = {ScreenId::Face, ScreenId::Pet, ScreenId::AnalogClock, ScreenId::DigitalClock, ScreenId::Games};
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
  if (s == ScreenId::TicTacToe || s == ScreenId::Reaction || s == ScreenId::Guess || s == ScreenId::PetGame) showScreen(ScreenId::Games);
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
      pet.event(e);
      sfx.play("interactions/feed");
      anim.kick(-0.2f);
      react(pet.stats.hunger <= 2 ? Emotion::Annoyed : Emotion::Happy, 2200);
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
      react(pet.stats.energy > 25 ? Emotion::Excited : Emotion::Sleepy, 2400);
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
      react(pet.stats.stress > 55 ? Emotion::Confused : Emotion::Surprised, 2000, EmotionPriority::Strong);
      break;
    case MotionEvent::Jerk:
      pet.event(PetEvent::Jerk);
      power.activity();
      if (pet.asleep()) wakeNow();
      react(Emotion::Scared, 1600, EmotionPriority::Strong);
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

void App::tickIdleLife(float dt) {
  // Occasional small behaviours while idle. Which one depends on the pet's state, not on chance alone.
  idleLifeIn_ -= dt;
  if (idleLifeIn_ > 0) return;
  idleLifeIn_ = rng.rangef(14000, 42000);
  if (pet.asleep() || screenId_ != ScreenId::Face) return;
  const PetStats& s = pet.stats;
  switch (rng.range(0, 4)) {
    case 0: anim.blink(170, true); break;
    case 1: anim.independentLook(-8, -6, 14, -4, 900); break;
    case 2: if (s.curiosity > 45) react(Emotion::Curious, 1600, EmotionPriority::Idle); else anim.slowBlink(); break;
    case 3: if (s.fun < 40) react(Emotion::Bored, 2200, EmotionPriority::Idle); else react(Emotion::Thinking, 1800, EmotionPriority::Idle); break;
    case 4: anim.lookOffset(rng.rangef(-36, 36), rng.rangef(-16, 16), 700); break;
  }
}

void App::tickBehaviour(float dt) {
  float simDt = dt * clock.scale();
  pet.tick(simDt);
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
  if (anim.soundCueReady()) sfx.onEmotionCue(anim.emotion(), nowMs_);

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
    c.fillCircle(kScreenW / 2.f - 118, y + 32, 6, kOrange);
    char fit[64];
    fitText(fit, sizeof(fit), toast_.title, FontSize::S, 216);
    c.text(kScreenW / 2 + 8, (int)y + 6, fit, FontSize::S, kWhite, Align::Center);
    fitText(fit, sizeof(fit), toastText_, FontSize::S, 216);
    c.text(kScreenW / 2 + 8, (int)y + 32, fit, FontSize::S, kYellow, Align::Center);
  }
  if (captionMs_ > 0) c.textWrapped(kScreenW / 2, kScreenH - 110, 330, caption_, FontSize::S, kYellow, Align::Center, 3);
}

void App::render(Canvas& c) {
  c.clear(kBlack);
  cur()->draw(*this, c);
  drawOverlays(c);
  if (iris_ < 1) c.maskOutsideCircle(kScreenW / 2.f, kScreenH / 2.f, lerpf(30, 340, smooth01(iris_)));
}

Rect App::dirtyRect() const {
  // The face leaves most of the panel untouched, pushing less reduces power and tearing.
  if (screenId_ == ScreenId::Face && toastMs_ <= 0 && captionMs_ <= 0 && iris_ >= 1) return face.bounds(anim);
  return Rect{0, 0, kScreenW, kScreenH};
}

}  // namespace cc
