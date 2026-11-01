// C ABI over cc::App for desktop hosts (emulator, simulated device, tests), loaded with ctypes.
#include "companion/app.h"
#include "companion/games.h"
#include "companion/services.h"
#include <stdio.h>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#define CC_API extern "C" __declspec(dllexport)
#else
#define CC_API extern "C" __attribute__((visibility("default")))
#endif

using namespace cc;

// Project paths may be non-ASCII: on Windows narrow fopen() would treat UTF-8 as the ANSI code page.
static FILE* openUtf8(const std::string& path, const wchar_t* wmode, const char* mode) {
#if defined(_WIN32)
  (void)mode;
  wchar_t w[1024];
  if (!MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w, 1024)) return nullptr;
  return _wfopen(w, wmode);
#else
  (void)wmode;
  return fopen(path.c_str(), mode);
#endif
}

static int removeUtf8(const std::string& path) {
#if defined(_WIN32)
  wchar_t w[1024];
  if (!MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w, 1024)) return -1;
  return _wremove(w);
#else
  return remove(path.c_str());
#endif
}

// Mirrors the NVS blobs on the device: one file per key.
class FileStorage : public Storage {
 public:
  explicit FileStorage(std::string dir) : dir_(std::move(dir)) {}
  std::string path(const char* k) const { return dir_ + "/" + k + ".bin"; }
  bool load(const char* k, void* d, uint32_t n) override {
    if (dir_.empty()) return false;
    FILE* f = openUtf8(path(k), L"rb", "rb");
    if (!f) return false;
    size_t r = fread(d, 1, n, f);
    fclose(f);
    return r == n;
  }
  bool save(const char* k, const void* d, uint32_t n) override {
    if (dir_.empty()) return true;
    FILE* f = openUtf8(path(k), L"wb", "wb");
    if (!f) return false;
    size_t w = fwrite(d, 1, n, f);
    fclose(f);
    return w == n;
  }
  bool erase(const char* k) override { return dir_.empty() || removeUtf8(path(k)) == 0; }
 private:
  std::string dir_;
};

struct Host {
  App app;
  std::unique_ptr<FileStorage> storage;
  std::vector<uint8_t> pack;
  Canvas canvas;
  Session session{true, "emulator"};
};

CC_API void* cc_create(const char* pack_path, const char* storage_dir, uint32_t seed) {
  Host* h = new Host();
  h->storage.reset(new FileStorage(storage_dir ? storage_dir : ""));
  if (pack_path && *pack_path) {
    FILE* f = openUtf8(pack_path, L"rb", "rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      long n = ftell(f);
      fseek(f, 0, SEEK_SET);
      h->pack.resize(n > 0 ? (size_t)n : 0);
      if (n > 0 && fread(h->pack.data(), 1, (size_t)n, f) != (size_t)n) h->pack.clear();
      fclose(f);
    }
  }
  h->app.begin(h->storage.get(), h->pack.data(), (uint32_t)h->pack.size(), seed ? seed : 1);
  return h;
}

CC_API void cc_destroy(void* p) {
  Host* h = (Host*)p;
  h->app.savePet();
  delete h;
}

CC_API void cc_tick(void* p, float dt_ms) { ((Host*)p)->app.tick(dt_ms); }
CC_API void cc_render(void* p, uint16_t* fb) {
  Host* h = (Host*)p;
  h->canvas.setTarget(fb, 0, 0, kScreenW, kScreenH);
  h->app.render(h->canvas);
}
// Renders in strips exactly as the firmware does; tests compare the result with a full-screen render.
CC_API void cc_render_strips(void* p, uint16_t* fb, int strip_h) {
  Host* h = (Host*)p;
  for (int y = 0; y < kScreenH; y += strip_h) {
    int sh = y + strip_h > kScreenH ? kScreenH - y : strip_h;
    h->canvas.setTarget(fb + y * kScreenW, 0, y, kScreenW, sh);
    h->app.render(h->canvas);
  }
}
CC_API void cc_touch(void* p, int down, int x, int y) { ((Host*)p)->app.touchInput(down != 0, x, y); }
CC_API void cc_imu(void* p, float ax, float ay, float az, float gx, float gy, float gz, float dt) { ((Host*)p)->app.imuInput(ax, ay, az, gx, gy, gz, dt); }
CC_API void cc_mic(void* p, const int16_t* pcm, int n) { ((Host*)p)->app.micInput(pcm, n); }
CC_API void cc_mix(void* p, int16_t* out, int n) { ((Host*)p)->app.mix(out, n); }
CC_API int cc_command(void* p, const char* json, char* out, int cap) {
  Host* h = (Host*)p;
  return h->app.handleCommand(json, out, cap, h->session);
}
// Untrusted-transport session, used by tests of the authentication rules.
CC_API int cc_command_unauth(void* p, const char* json, char* out, int cap, int* authed) {
  Host* h = (Host*)p;
  Session s{*authed != 0, "ble"};
  int n = h->app.handleCommand(json, out, cap, s);
  *authed = s.authed;
  return n;
}
CC_API int cc_state(void* p, char* out, int cap) { return ((Host*)p)->app.stateEvent(out, cap); }
CC_API void cc_set_battery(void* p, int percent, int charging, int present, int mv) { ((Host*)p)->app.power.setBattery(percent, charging != 0, present != 0, mv); }
CC_API void cc_set_time_scale(void* p, float s) { ((Host*)p)->app.clock.setScale(s); }
CC_API void cc_set_fps(void* p, float fps) { ((Host*)p)->app.hal.fps = fps; }
// Test autopilot: steers the real maze physics by tilt only. Returns milliseconds to the exit, -1 if it never gets there.
CC_API int cc_maze_selftest(int size, uint32_t seed) {
  static MazeGame m;
  Rng rng(seed);
  m.generate(size, rng);
  for (int ms = 0; ms < 600000; ms += 16) {
    int cx, cy;
    m.nextCellToExit(&cx, &cy);
    float vx0 = m.x(), vy0 = m.y();
    static float px = 0, py = 0;
    float vx = (vx0 - px) / 0.016f, vy = (vy0 - py) / 0.016f;
    px = vx0; py = vy0;
    float ax = clampf(((cx + 0.5f) - m.x()) * 0.9f - vx * 0.12f, -0.7f, 0.7f);
    float ay = clampf(((cy + 0.5f) - m.y()) * 0.9f - vy * 0.12f, -0.7f, 0.7f);
    m.step(ax, ay, 16);
    if (m.x() < 0 || m.y() < 0 || m.x() > m.size() || m.y() > m.size()) return -2;
    if (m.won()) return ms;
  }
  return -1;
}
// Walks into a wall: the ball must stay inside its cell whatever the tilt.
CC_API int cc_maze_wall_selftest(uint32_t seed) {
  static MazeGame m;
  Rng rng(seed);
  m.generate(7, rng);
  for (int i = 0; i < 2000; i++) {
    m.step(i % 300 < 150 ? 1.f : -1.f, 1.f, 16);
    if (m.x() < 0 || m.y() < 0 || m.x() > 7 || m.y() > 7) return 0;
  }
  return 1;
}
// Snake rules: eats and grows, refuses to reverse, dies on the wall.
CC_API int cc_snake_selftest(uint32_t seed) {
  static SnakeGame s;
  Rng rng(seed);
  s.reset(rng);
  s.placeFoodForTest(s.segX(0) + 1, s.segY(0));
  if (!s.step(rng) || s.length() != 4) return 1;
  s.turn(-1, 0);
  int x = s.segX(0);
  s.step(rng);
  if (s.segX(0) != x + 1) return 2;
  for (int i = 0; i < 40 && s.alive(); i++) s.step(rng);
  if (s.alive()) return 3;
  return 0;
}
// Returns 0 when all rules hold, otherwise the number of the failed check.
CC_API int cc_blackjack_selftest(uint32_t seed) {
  using C = BlackjackGame::Card;
  static const C ace{14, 0}, king{13, 0}, five{5, 0}, nine{9, 1}, seven{7, 2}, ten{10, 3};
  C aces[2] = {ace, ace}, soft[3] = {ace, five, five}, hard[3] = {ace, five, seven}, bust[3] = {king, king, king};
  if (BlackjackGame::value(aces, 2) != 12) return 1;
  if (BlackjackGame::value(soft, 3) != 21) return 2;
  if (BlackjackGame::value(hard, 3) != 13) return 3;
  if (BlackjackGame::value(bust, 3) != 30) return 4;
  Rng rng(seed);
  for (int round = 0; round < 500; round++) {
    static BlackjackGame g;
    g.resetChips();
    g.deal(rng);
    if (g.chips() < 90) return 5;
    if (g.playerCount() != 2 && !g.natural()) return 6;
    int want = round % 4;
    while (g.phase() == BlackjackGame::Phase::Player && g.playerValue() < 12 + want * 3) g.hit();
    g.stand();
    int guard = 0;
    while (g.dealerStep()) if (++guard > 12) return 7;
    if (g.phase() != BlackjackGame::Phase::Done) { if (g.playerValue() <= 21) return 8; }
    if (g.playerValue() <= 21 && g.dealerValue() < 17) return 9;
    int expected = g.playerValue() > 21 || (g.dealerValue() <= 21 && g.dealerValue() > g.playerValue()) ? -1
                 : (g.dealerValue() > 21 || g.playerValue() > g.dealerValue()) ? 1 : 0;
    if (g.outcome() != expected) return 10;
    int chips = g.chips();
    if (expected == -1 && chips != 90) return 11;
    if (expected == 0 && chips != 100) return 12;
    if (expected == 1 && chips != (g.natural() ? 115 : 110)) return 13;
  }
  return 0;
}
CC_API int cc_poll_mouse(void* p, int* out) {
  App::MouseReport r;
  if (!((Host*)p)->app.pollMouse(&r)) return 0;
  out[0] = r.dx; out[1] = r.dy; out[2] = r.wheel; out[3] = r.buttons;
  return 1;
}
CC_API const char* cc_pin(void* p) { return ((Host*)p)->app.settings.pin; }
CC_API int cc_poll_input(void* p, char* out, int cap) { return ((Host*)p)->app.pollInput(out, cap); }
CC_API int cc_parse_weather(const char* body, float* temp) {
  Weather w;
  return parseOpenMeteo(body, &w, temp) ? (int)w : -1;
}
CC_API int cc_emotion_count() { return kEmotionCount; }
CC_API const char* cc_emotion_name(int i) { return emotionName((Emotion)i); }
CC_API int cc_allowed(int from, int to) { return EmotionEngine::allowed((Emotion)from, (Emotion)to); }
CC_API int cc_sound_count(void* p) { return ((Host*)p)->app.pack.count(); }
CC_API const char* cc_sound_name(void* p, int i) { return ((Host*)p)->app.pack.name(i); }
CC_API int cc_emotion_sound(void* p, int e, int variant) { return ((Host*)p)->app.sfx.emotionSound((Emotion)e, variant); }
CC_API int cc_last_sound(void* p) { return ((Host*)p)->app.audio.lastSound(); }
CC_API uint32_t cc_play_count(void* p) { return ((Host*)p)->app.audio.playCount(); }
CC_API float cc_blink_amount(void* p) { return ((Host*)p)->app.anim.blinkAmount(); }
CC_API float cc_pose(void* p, int eye, int field) {
  const EyePose& ep = eye ? ((Host*)p)->app.anim.rightPose() : ((Host*)p)->app.anim.leftPose();
  return ((const float*)&ep)[field];
}
CC_API float cc_gaze(void* p, int y) { App& a = ((Host*)p)->app; return y ? a.anim.gazeY() : a.anim.gazeX(); }
