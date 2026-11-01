#include "companion/services.h"
#include <stdio.h>
#include <stdlib.h>

namespace cc {

// ---- storage ---------------------------------------------------------------------------------
static uint32_t crc32(const uint8_t* d, uint32_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (uint32_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

struct BlobHeader { uint16_t version; uint16_t size; uint32_t crc; };

bool saveChecked(Storage* s, const char* key, const void* data, uint32_t size, uint16_t version) {
  if (!s || size > 4000) return false;
  static uint8_t buf[4096];  // only the logic thread persists
  BlobHeader h{version, (uint16_t)size, crc32((const uint8_t*)data, size)};
  memcpy(buf, &h, sizeof(h));
  memcpy(buf + sizeof(h), data, size);
  return s->save(key, buf, size + sizeof(h));
}

bool loadChecked(Storage* s, const char* key, void* data, uint32_t size, uint16_t version) {
  if (!s || size > 4000) return false;
  static uint8_t buf[4096];
  if (!s->load(key, buf, size + sizeof(BlobHeader))) return false;
  BlobHeader h;
  memcpy(&h, buf, sizeof(h));
  if (h.version != version || h.size != size || crc32(buf + sizeof(h), size) != h.crc) return false;
  memcpy(data, buf + sizeof(h), size);
  return true;
}

// ---- clock -----------------------------------------------------------------------------------
void ClockManager::setEpoch(uint32_t e, int tz) {
  epoch_ = e; frac_ = 0; set_ = true;
  if (tz != 0x7FFF) tz_ = tz;
}

void ClockManager::tick(float dt) {
  frac_ += dt * scale_;
  while (frac_ >= 1000) { frac_ -= 1000; epoch_++; }
}

// Howard Hinnant's civil-from-days algorithm.
DateTime civilFromEpoch(uint32_t t) {
  DateTime d;
  int32_t days = (int32_t)(t / 86400);
  uint32_t rem = t % 86400;
  d.hour = rem / 3600; d.minute = (rem % 3600) / 60; d.second = rem % 60;
  d.weekday = (days + 4) % 7;  // 1970-01-01 was Thursday; 0 = Sunday
  int32_t z = days + 719468;
  int32_t era = z / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t y = (int32_t)yoe + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  d.day = (int)(doy - (153 * mp + 2) / 5 + 1);
  d.month = (int)(mp < 10 ? mp + 3 : mp - 9);
  d.year = y + (d.month <= 2);
  return d;
}

DateTime ClockManager::now() const { return civilFromEpoch(epoch_ + tz_ * 60); }

// ---- notifications ---------------------------------------------------------------------------
static const char* const kIconNames[] = {"bell", "message", "mail", "phone", "calendar", "warning"};
NotifIcon notifIconFromName(const char* s) {
  for (int i = 0; i < (int)NotifIcon::Count; i++)
    if (!strcmp(s, kIconNames[i])) return (NotifIcon)i;
  return NotifIcon::Bell;
}
const char* notifIconName(NotifIcon i) { return kIconNames[(int)i]; }

static void copyStr(char* dst, const char* src, size_t cap) {
  // Truncate on a UTF-8 boundary so a cut-off Cyrillic letter never renders as garbage.
  size_t n = strlen(src);
  if (n >= cap) { n = cap - 1; while (n > 0 && ((uint8_t)src[n] & 0xC0) == 0x80) n--; }
  memcpy(dst, src, n);
  dst[n] = 0;
}

void NotificationManager::add(const char* title, const char* text, const char* icon, const char* source, uint32_t time) {
  Notification& n = items_[head_];
  copyStr(n.title, title, sizeof(n.title));
  copyStr(n.text, text, sizeof(n.text));
  copyStr(n.source, source, sizeof(n.source));
  n.icon = (uint8_t)notifIconFromName(icon);
  n.time = time;
  n.read = false;
  head_ = (head_ + 1) % kCap;
  if (count_ < kCap) count_++;
  if (toast_ < kCap) toast_++;
}

const Notification& NotificationManager::at(int i) const { return items_[(head_ - 1 - i + 2 * kCap) % kCap]; }

int NotificationManager::unread() const {
  int u = 0;
  for (int i = 0; i < count_; i++) if (!at(i).read) u++;
  return u;
}

void NotificationManager::markRead(int i) {
  if (i >= 0 && i < count_) items_[(head_ - 1 - i + 2 * kCap) % kCap].read = true;
}

void NotificationManager::markAllRead() { for (int i = 0; i < count_; i++) markRead(i); }

bool NotificationManager::popToast(Notification* out) {
  if (toast_ <= 0) return false;
  if (toast_ > count_) toast_ = count_;
  *out = at(toast_ - 1);   // oldest not yet shown goes first
  toast_--;
  return true;
}

// ---- power -----------------------------------------------------------------------------------
void PowerManager::configure(uint16_t s, uint8_t b, bool save) { sleepAfter_ = s < 15 ? 15 : s; bright_ = b; save_ = save; }

void PowerManager::tick(float dt) {
  idle_ += dt;
  float limit = sleepAfter_ * 1000.f * (save_ ? 0.5f : 1.f);
  if (idle_ >= limit) state_ = PowerState::Asleep;
  else if (idle_ >= limit * 0.6f) state_ = PowerState::Dimmed;
  else state_ = PowerState::Active;
}

uint8_t PowerManager::panelBrightness() const {
  int b = bright_;
  if (save_ && b > 50) b = 50;
  if (lowBattery() && b > 30) b = 30;
  if (state_ == PowerState::Dimmed) b = b * 45 / 100;
  if (state_ == PowerState::Asleep) b = b * 12 / 100;  // a sleeping face stays faintly visible
  if (b < 3) b = 3;
  return (uint8_t)b;
}

int PowerManager::targetFps() const {
  if (state_ == PowerState::Asleep) return 12;
  if (state_ == PowerState::Dimmed || save_) return 24;
  return 40;
}

// ---- offline AI ------------------------------------------------------------------------------
struct Rule { const char* word; const char* reply; Emotion e; };
static const Rule kRules[] = {
    {"привет", "Привет! Я рад тебя видеть.", Emotion::Happy},
    {"hello", "Hello! Good to see you.", Emotion::Happy},
    {"люблю", "И я тебя! Погладь меня.", Emotion::Affection},
    {"хорош", "Спасибо! Мне приятно.", Emotion::Happy},
    {"молодец", "Я стараюсь!", Emotion::VeryHappy},
    {"плох", "Ой. Мне грустно это слышать.", Emotion::Sad},
    {"дурак", "Эй! Обидно вообще-то.", Emotion::Annoyed},
    {"спать", "Да, глазки уже закрываются…", Emotion::Sleepy},
    {"спокойной", "Спокойной ночи.", Emotion::Sleepy},
    {"есть", "Я бы перекусил!", Emotion::Excited},
    {"голод", "Покорми меня, пожалуйста!", Emotion::Sad},
    {"игр", "Давай играть! Открой меню игр.", Emotion::Excited},
    {"поиграем", "Ура, играем!", Emotion::Excited},
    {"почему", "Хм. Надо подумать…", Emotion::Thinking},
    {"что", "Интересный вопрос…", Emotion::Curious},
    {"как дела", "У меня всё отлично!", Emotion::Happy},
    {"страш", "Ой, где страшное?!", Emotion::Scared},
    {"бу!", "Ай! Напугал!", Emotion::Scared},
};

static char lowerRu(char* s) {  // ASCII and Cyrillic А-Я in UTF-8, enough for matching keywords
  for (uint8_t* p = (uint8_t*)s; *p; p++) {
    if (*p >= 'A' && *p <= 'Z') *p += 32;
    else if (*p == 0xD0 && p[1] >= 0x90 && p[1] <= 0x9F) p[1] += 0x20;
    else if (*p == 0xD0 && p[1] >= 0xA0 && p[1] <= 0xAF) { p[0] = 0xD1; p[1] -= 0x20; }
  }
  return 0;
}

bool OfflineAI::submitText(const char* text) {
  char low[200];
  copyStr(low, text, sizeof(low));
  lowerRu(low);
  reply_ = AIReply{};
  reply_.ok = true;
  reply_.offline = true;
  copyStr(reply_.text, "Я пока не понимаю, но внимательно слушаю.", sizeof(reply_.text));
  reply_.emotion = Emotion::Confused;
  for (const Rule& r : kRules) {
    if (strstr(low, r.word)) {
      copyStr(reply_.text, r.reply, sizeof(reply_.text));
      reply_.emotion = r.e;
      break;
    }
  }
  ready_ = true;
  return true;
}

bool OfflineAI::poll(AIReply* out) {
  if (!ready_) return false;
  *out = reply_;
  ready_ = false;
  return true;
}

}  // namespace cc
