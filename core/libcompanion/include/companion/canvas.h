// Software renderer drawing into an RGB565 window of the screen.
// The window may be a horizontal strip: firmware renders in strips into internal DMA RAM,
// the emulator renders the whole screen at once. Drawing code is identical for both.
#pragma once
#include "types.h"

namespace cc {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t kBlack = 0x0000, kWhite = 0xFFFF;
// Interface palette: black, white, greys and muted pastels only.
constexpr uint16_t kAccent = rgb565(182, 190, 214), kSoft = rgb565(220, 214, 196);
constexpr uint16_t kRose = rgb565(218, 176, 184), kMint = rgb565(174, 208, 192), kSky = rgb565(170, 198, 220);
constexpr uint16_t kGray = rgb565(120, 120, 120), kDark = rgb565(38, 38, 38), kDim = rgb565(70, 70, 70);

// Alpha blend, a: 0..255.
inline uint16_t blend565(uint16_t bg, uint16_t fg, uint32_t a) {
  if (a >= 250) return fg;
  a = (a + 4) >> 3;
  uint32_t b = (bg | ((uint32_t)bg << 16)) & 0x07E0F81F;
  uint32_t f = (fg | ((uint32_t)fg << 16)) & 0x07E0F81F;
  uint32_t r = (b + (((f - b) * a) >> 5)) & 0x07E0F81F;
  return (uint16_t)(r | (r >> 16));
}

enum class Align : uint8_t { Left, Center, Right };
enum class FontSize : uint8_t { S, M, L };

struct Rect { int x, y, w, h; };

// Convex eye silhouette: ellipse clipped by two lids, minus a "cheek" ellipse rising from below.
struct EyeShape {
  float cx, cy, rx, ry;
  float upY, upSlope;    // inside when y >= upY + upSlope * (x - cx)
  float loY, loSlope;    // inside when y <= loY + loSlope * (x - cx)
  float cheekY;          // top of the cheek ellipse, absolute y; values below the eye have no effect
  float cheekRx, cheekRy;
};

class Canvas {
 public:
  void setTarget(uint16_t* buf, int x0, int y0, int w, int h);
  bool intersects(int x, int y, int w, int h) const;
  void clear(uint16_t c);
  void fillRect(int x, int y, int w, int h, uint16_t c);
  void fillCircle(float cx, float cy, float r, uint16_t c);
  void ring(float cx, float cy, float r, float thick, uint16_t c);
  // Angles in degrees, 0 = up, clockwise.
  void arc(float cx, float cy, float r, float thick, float a0, float a1, uint16_t c);
  void line(float x0, float y0, float x1, float y1, float thick, uint16_t c);
  void roundRect(float x, float y, float w, float h, float r, uint16_t c);
  void heart(float cx, float cy, float size, uint16_t c);
  void eye(const EyeShape& s, uint16_t c);
  // UTF-8 text. Returns the drawn width in pixels.
  int text(int x, int y, const char* utf8, FontSize f, uint16_t c, Align a = Align::Left);
  // Word-wrapped text, returns the y after the last line.
  int textWrapped(int x, int y, int maxW, const char* utf8, FontSize f, uint16_t c, Align a = Align::Left, int maxLines = 16);
  static int textWidth(const char* utf8, FontSize f);
  static int lineHeight(FontSize f);
  // Blacks out everything outside a circle: used for the iris screen transition and the bezel.
  void maskOutsideCircle(float cx, float cy, float r);

 private:
  inline void blend(int x, int y, uint16_t c, uint8_t a);
  template <typename F> void sdfFill(float x0, float y0, float x1, float y1, uint16_t c, F dist);
  uint16_t* buf_ = nullptr;
  int x0_ = 0, y0_ = 0, w_ = 0, h_ = 0;
};

}  // namespace cc
