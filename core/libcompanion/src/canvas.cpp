#include "companion/canvas.h"
#include "font_data.h"
#include <string.h>

namespace cc {

static inline uint16_t mix565(uint16_t bg, uint16_t fg, uint32_t a) { return blend565(bg, fg, a); }

void Canvas::setTarget(uint16_t* buf, int x0, int y0, int w, int h) {
  buf_ = buf; x0_ = x0; y0_ = y0; w_ = w; h_ = h;
}

bool Canvas::intersects(int x, int y, int w, int h) const {
  return x < x0_ + w_ && x + w > x0_ && y < y0_ + h_ && y + h > y0_;
}

inline void Canvas::blend(int x, int y, uint16_t c, uint8_t a) {
  uint16_t* p = buf_ + (y - y0_) * w_ + (x - x0_);
  *p = mix565(*p, c, a);
}

void Canvas::clear(uint16_t c) {
  if (c == 0) { memset(buf_, 0, (size_t)w_ * h_ * 2); return; }
  for (int i = 0; i < w_ * h_; i++) buf_[i] = c;
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t c) {
  int xa = x < x0_ ? x0_ : x, xb = x + w > x0_ + w_ ? x0_ + w_ : x + w;
  int ya = y < y0_ ? y0_ : y, yb = y + h > y0_ + h_ ? y0_ + h_ : y + h;
  for (int yy = ya; yy < yb; yy++) {
    uint16_t* p = buf_ + (yy - y0_) * w_ + (xa - x0_);
    for (int xx = xa; xx < xb; xx++) *p++ = c;
  }
}

// Evaluates a signed distance only inside the window-clipped bounding box.
template <typename F>
void Canvas::sdfFill(float bx0, float by0, float bx1, float by1, uint16_t c, F dist) {
  int xa = (int)floorf(bx0), xb = (int)ceilf(bx1), ya = (int)floorf(by0), yb = (int)ceilf(by1);
  if (xa < x0_) xa = x0_;
  if (ya < y0_) ya = y0_;
  if (xb > x0_ + w_) xb = x0_ + w_;
  if (yb > y0_ + h_) yb = y0_ + h_;
  for (int y = ya; y < yb; y++) {
    uint16_t* row = buf_ + (y - y0_) * w_ - x0_;
    for (int x = xa; x < xb; x++) {
      float d = dist(x + 0.5f, y + 0.5f);
      if (d >= 0.5f) continue;
      if (d <= -0.5f) row[x] = c;
      else row[x] = mix565(row[x], c, (uint32_t)((0.5f - d) * 255.0f));
    }
  }
}

// Row-wise fill of a shape mirrored around x = cx. For each row, span(|dy|, outer, core) gives half-widths:
// pixels farther than outer are outside, nearer than core are fully inside (filled at once, or skipped when
// hollow, as in a ring). Only the thin band between evaluates dist() for the anti-aliased edge, so a ring
// around the whole screen costs its circumference, not the 466x466 square around it.
template <typename S, typename D>
void Canvas::bandFill(float cx, float cy, float halfH, uint16_t c, bool hollow, S span, D dist) {
  int ya = (int)floorf(cy - halfH - 1), yb = (int)ceilf(cy + halfH + 1);
  if (ya < y0_) ya = y0_;
  if (yb > y0_ + h_) yb = y0_ + h_;
  for (int y = ya; y < yb; y++) {
    float py = y + 0.5f, outer = 0, core = 0;
    span(fabsf(py - cy), outer, core);
    if (outer <= 0) continue;
    int xa = (int)floorf(cx - outer), xb = (int)ceilf(cx + outer);
    if (xa < x0_) xa = x0_;
    if (xb > x0_ + w_) xb = x0_ + w_;
    if (xa >= xb) continue;
    int ca = xb, cb = xb;  // pixels whose centre lies within core
    if (core > 0) { ca = (int)floorf(cx - core - 0.5f) + 1; cb = (int)ceilf(cx + core - 0.5f); }
    if (ca < xa) ca = xa;
    if (cb > xb) cb = xb;
    if (cb < ca) cb = ca;
    uint16_t* row = buf_ + (y - y0_) * w_ - x0_;
    auto edge = [&](int from, int to) {
      for (int x = from; x < to; x++) {
        float d = dist(x + 0.5f, py);
        if (d >= 0.5f) continue;
        row[x] = d <= -0.5f ? c : mix565(row[x], c, (uint32_t)((0.5f - d) * 255.0f));
      }
    };
    edge(xa, ca);
    if (!hollow) for (int x = ca; x < cb; x++) row[x] = c;
    edge(cb, xb);
  }
}

static inline float halfChord(float r, float dy) { return r > 0 && r * r > dy * dy ? sqrtf(r * r - dy * dy) : 0; }

void Canvas::fillCircle(float cx, float cy, float r, uint16_t c) {
  if (r <= 0) return;
  bandFill(cx, cy, r, c, false,
           [&](float dy, float& outer, float& core) { outer = halfChord(r + 1, dy); core = halfChord(r - 1, dy); },
           [&](float x, float y) { return sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r; });
}

void Canvas::ring(float cx, float cy, float r, float t, uint16_t c) {
  float ro = r + t / 2, ri = r - t / 2;
  bandFill(cx, cy, ro, c, true,
           [&](float dy, float& outer, float& core) { outer = halfChord(ro + 1, dy); core = halfChord(ri - 1, dy); },
           [&](float x, float y) { return fabsf(sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r) - t / 2; });
}

void Canvas::arc(float cx, float cy, float r, float t, float a0, float a1, uint16_t c, bool roundCaps) {
  if (a1 <= a0) return;
  const float kDeg = 3.14159265f / 180.0f;
  float ro = r + t / 2, ri = r - t / 2, mid = (a0 + a1) / 2 * kDeg, half = (a1 - a0) / 2 * kDeg;
  float mx = sinf(mid), my = -cosf(mid), ch = cosf(half);
  float ex0 = cx + r * sinf(a0 * kDeg), ey0 = cy - r * cosf(a0 * kDeg);
  float ex1 = cx + r * sinf(a1 * kDeg), ey1 = cy - r * cosf(a1 * kDeg);
  bool full = half >= 3.14159f;
  bandFill(cx, cy, ro, c, true,
           [&](float dy, float& outer, float& core) { outer = halfChord(ro + 1, dy); core = halfChord(ri - 1, dy); },
           [&](float x, float y) {
             float dx = x - cx, dy = y - cy, len = sqrtf(dx * dx + dy * dy);
             if (full || (len > 0.001f && (dx * mx + dy * my) >= ch * len)) return fabsf(len - r) - t / 2;
             if (!roundCaps) return 1.0f;
             float d0 = sqrtf((x - ex0) * (x - ex0) + (y - ey0) * (y - ey0));
             float d1 = sqrtf((x - ex1) * (x - ex1) + (y - ey1) * (y - ey1));
             return (d0 < d1 ? d0 : d1) - t / 2;
           });
}

// Distance to a rounded rectangle centred at (cx, cy) with half straight parts hx, hy and corner radius r.
static inline float roundRectDist(float px, float py, float cx, float cy, float hx, float hy, float r) {
  float qx = fabsf(px - cx) - hx, qy = fabsf(py - cy) - hy;
  float ax = qx > 0 ? qx : 0, ay = qy > 0 ? qy : 0;
  float inner = qx > qy ? qx : qy;
  return sqrtf(ax * ax + ay * ay) + (inner < 0 ? inner : 0) - r;
}

static inline float roundRectHalf(float dy, float hx, float hy, float r) {
  if (dy <= hy) return hx + r;
  return hx + halfChord(r, dy - hy);
}

void Canvas::roundRect(float x, float y, float w, float h, float r, uint16_t c) {
  if (w <= 0 || h <= 0) return;
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  float cx = x + w / 2, cy = y + h / 2, hx = w / 2 - r, hy = h / 2 - r;
  bandFill(cx, cy, h / 2, c, false,
           [&](float dy, float& outer, float& core) {
             outer = roundRectHalf(dy - 1 < 0 ? 0 : dy - 1, hx, hy, r) + 1;
             core = dy + 1 <= hy + r ? roundRectHalf(dy + 1, hx, hy, r) - 1 : 0;
           },
           [&](float px, float py) { return roundRectDist(px, py, cx, cy, hx, hy, r); });
}

void Canvas::roundRectOutline(float x, float y, float w, float h, float r, float stroke, uint16_t c) {
  if (w <= 0 || h <= 0) return;
  if (r > w / 2) r = w / 2;
  if (r > h / 2) r = h / 2;
  float cx = x + w / 2, cy = y + h / 2, hx = w / 2 - r, hy = h / 2 - r;
  float ri = r - stroke > 0 ? r - stroke : 0, ihx = w / 2 - stroke - ri, ihy = h / 2 - stroke - ri;
  bandFill(cx, cy, h / 2, c, true,
           [&](float dy, float& outer, float& core) {
             outer = roundRectHalf(dy - 1 < 0 ? 0 : dy - 1, hx, hy, r) + 1;
             core = ihx >= 0 && dy + 1 <= ihy + ri ? roundRectHalf(dy + 1, ihx, ihy, ri) - 1 : 0;
           },
           [&](float px, float py) {
             float d = roundRectDist(px, py, cx, cy, hx, hy, r);
             float e = -d - stroke;
             return d > e ? d : e;
           });
}

void Canvas::diamond(float cx, float cy, float hw, float hh, uint16_t c) {
  if (hw <= 0 || hh <= 0) return;
  float k = hw * hh / sqrtf(hw * hw + hh * hh);
  bandFill(cx, cy, hh, c, false,
           [&](float dy, float& outer, float& core) {
             float w = hw * (1 - dy / hh);
             outer = w + 2;
             core = w - 2;
           },
           [&](float x, float y) { return (fabsf(x - cx) / hw + fabsf(y - cy) / hh - 1) * k; });
}

void Canvas::line(float x0, float y0, float x1, float y1, float t, uint16_t c) {
  float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, r = t / 2;
  float bx0 = (x0 < x1 ? x0 : x1) - r - 1, bx1 = (x0 > x1 ? x0 : x1) + r + 1;
  float by0 = (y0 < y1 ? y0 : y1) - r - 1, by1 = (y0 > y1 ? y0 : y1) + r + 1;
  sdfFill(bx0, by0, bx1, by1, c, [&](float x, float y) {
    float u = l2 > 0 ? clampf(((x - x0) * dx + (y - y0) * dy) / l2, 0, 1) : 0;
    float ex = x - (x0 + dx * u), ey = y - (y0 + dy * u);
    return sqrtf(ex * ex + ey * ey) - r;
  });
}

void Canvas::heart(float cx, float cy, float s, uint16_t c) {
  float r = s * 0.52f;
  fillCircle(cx - r * 0.92f, cy - r * 0.45f, r, c);
  fillCircle(cx + r * 0.92f, cy - r * 0.45f, r, c);
  // Lower wedge as scanlines narrowing to the tip.
  float top = cy - r * 0.1f, bottom = cy + s * 1.05f, half = r * 1.86f;
  sdfFill(cx - half - 1, top, cx + half + 1, bottom + 1, c, [&](float x, float y) {
    float t = (y - top) / (bottom - top);
    float w = half * (1 - t);
    float d = fabsf(x - cx) - w;
    return (top - y) > d ? (top - y) : d * 0.75f;
  });
}

// Span rasterizer with 4 vertical sub-samples and exact horizontal coverage. It is much cheaper
// than a per-pixel distance field, which matters because eyes are redrawn on every frame.
void Canvas::eye(const EyeShape& s, uint16_t c) {
  constexpr int kSub = 4;
  static int16_t delta[kScreenW + 4];  // only the render thread draws
  int ya = (int)floorf(s.cy - s.ry), yb = (int)ceilf(s.cy + s.ry) + 1;
  if (ya < y0_) ya = y0_;
  if (yb > y0_ + h_) yb = y0_ + h_;
  int xa = (int)floorf(s.cx - s.rx) - 1, xb = (int)ceilf(s.cx + s.rx) + 2;
  if (xa < x0_) xa = x0_;
  if (xb > x0_ + w_) xb = x0_ + w_;
  if (xa >= xb) return;
  const float cheekCy = s.cheekY + s.cheekRy;

  auto addSpan = [&](float l, float r) {
    if (l < xa) l = (float)xa;
    if (r > xb) r = (float)xb;
    if (r <= l) return;
    int li = (int)l, ri = (int)r;
    const int w = 256 / kSub;  // weight of one sub-scanline per pixel
    int covL = (int)((li + 1 - l) * w), covR = (int)((r - ri) * w);
    if (li == ri) {
      int cov = (int)((r - l) * w);
      delta[li - xa] += cov; delta[li - xa + 1] -= cov;
    } else {
      delta[li - xa] += covL; delta[li - xa + 1] += w - covL;
      delta[ri - xa] -= w - covR; delta[ri - xa + 1] -= covR;
    }
  };

  for (int y = ya; y < yb; y++) {
    int n = xb - xa + 2;
    memset(delta, 0, (size_t)n * sizeof(delta[0]));
    bool any = false;
    for (int k = 0; k < kSub; k++) {
      float sy = y + (k + 0.5f) / kSub;
      float dy = (sy - s.cy) / s.ry;
      float q = 1 - dy * dy;
      if (q <= 0) continue;
      float hw = s.rx * sqrtf(q);
      float l = s.cx - hw, r = s.cx + hw;
      // Upper lid: sy - upY >= slope * (x - cx)
      float e = sy - s.upY;
      if (fabsf(s.upSlope) < 1e-4f) { if (e < 0) continue; }
      else if (s.upSlope > 0) { float b = s.cx + e / s.upSlope; if (b < r) r = b; }
      else { float b = s.cx + e / s.upSlope; if (b > l) l = b; }
      // Lower lid: sy - loY <= slope * (x - cx)
      e = sy - s.loY;
      if (fabsf(s.loSlope) < 1e-4f) { if (e > 0) continue; }
      else if (s.loSlope > 0) { float b = s.cx + e / s.loSlope; if (b > l) l = b; }
      else { float b = s.cx + e / s.loSlope; if (b < r) r = b; }
      if (r <= l) continue;
      any = true;
      float cd = (sy - cheekCy) / s.cheekRy;
      float cq = 1 - cd * cd;
      if (sy > s.cheekY && cq > 0) {
        float chw = s.cheekRx * sqrtf(cq);
        addSpan(l, (s.cx - chw) < r ? (s.cx - chw) : r);
        addSpan((s.cx + chw) > l ? (s.cx + chw) : l, r);
      } else {
        addSpan(l, r);
      }
    }
    if (!any) continue;
    uint16_t* p = buf_ + (y - y0_) * w_ + (xa - x0_);
    int acc = 0;
    for (int i = 0; i < xb - xa; i++) {
      acc += delta[i];
      if (acc >= 255) p[i] = c;
      else if (acc > 4) p[i] = mix565(p[i], c, (uint32_t)acc);
    }
  }
}

// ---- text ------------------------------------------------------------------------------------

static_assert((int)FontSize::Count == kFontCount, "FontSize must list the fonts of tools/gen_font.py, in order");
static const FontInfo& fontOf(FontSize f) { return kFonts[(int)f]; }

static uint32_t nextCodepoint(const char*& s) {
  uint8_t c = (uint8_t)*s++;
  if (c < 0x80) return c;
  int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
  uint32_t cp = extra == 3 ? (c & 7) : extra == 2 ? (c & 15) : extra == 1 ? (c & 31) : '?';
  for (int i = 0; i < extra; i++) {
    uint8_t n = (uint8_t)*s;
    if ((n & 0xC0) != 0x80) return '?';
    cp = (cp << 6) | (n & 0x3F);
    s++;
  }
  return cp;
}

static const Glyph* findGlyph(const FontInfo& f, uint32_t cp) {
  int lo = 0, hi = f.count - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    if (f.glyphs[m].cp == cp) return &f.glyphs[m];
    if (f.glyphs[m].cp < cp) lo = m + 1; else hi = m - 1;
  }
  return nullptr;
}

int Canvas::textWidth(const char* s, FontSize f) {
  const FontInfo& fi = fontOf(f);
  int w = 0;
  while (*s) { const Glyph* g = findGlyph(fi, nextCodepoint(s)); w += g ? g->adv : fi.lineH / 3; }
  return w;
}

int Canvas::lineHeight(FontSize f) { return fontOf(f).lineH; }

int Canvas::text(int x, int y, const char* s, FontSize f, uint16_t c, Align a) {
  const FontInfo& fi = fontOf(f);
  int total = textWidth(s, f);
  if (a == Align::Center) x -= total / 2;
  else if (a == Align::Right) x -= total;
  if (!intersects(x, y, total, fi.lineH)) return total;
  while (*s) {
    const Glyph* g = findGlyph(fi, nextCodepoint(s));
    if (!g) { x += fi.lineH / 3; continue; }
    int gx = x + g->xoff, gy = y + g->yoff;
    if (g->w && intersects(gx, gy, g->w, g->h)) {
      // 4 bits per pixel, row aligned to a byte.
      const uint8_t* src = fi.bitmap + g->offset;
      int stride = (g->w + 1) / 2;
      for (int py = 0; py < g->h; py++) {
        int sy = gy + py;
        if (sy < y0_ || sy >= y0_ + h_) continue;
        const uint8_t* row = src + py * stride;
        for (int px = 0; px < g->w; px++) {
          int sx = gx + px;
          if (sx < x0_ || sx >= x0_ + w_) continue;
          uint8_t v = (px & 1) ? (row[px / 2] & 0x0F) : (row[px / 2] >> 4);
          if (v) blend(sx, sy, c, (uint8_t)(v * 17));
        }
      }
    }
    x += g->adv;
  }
  return total;
}

int Canvas::capMiddle(FontSize f) {
  const FontInfo& fi = fontOf(f);
  const Glyph* g = findGlyph(fi, '0');
  if (!g || !g->h) g = findGlyph(fi, 0x41D);  // Cyrillic En: fonts without digits are text fonts
  return g && g->h ? g->yoff + g->h / 2 : fi.lineH / 2;
}

int Canvas::textWrapped(int x, int y, int maxW, const char* s, FontSize f, uint16_t c, Align a, int maxLines, int lineH, bool ellipsis) {
  const FontInfo& fi = fontOf(f);
  if (lineH <= 0) lineH = fi.lineH;
  const Glyph* dots = findGlyph(fi, 0x2026);
  int dotsW = dots ? dots->adv : 0;
  char line[200];
  int lines = 0;
  while (*s && lines < maxLines) {
    bool last = lines == maxLines - 1;
    int w = 0, n = 0, lastBreak = -1;
    const char* p = s;
    while (*p && *p != '\n') {
      const char* q = p;
      uint32_t cp = nextCodepoint(q);
      const Glyph* g = findGlyph(fi, cp);
      int adv = g ? g->adv : fi.lineH / 3;
      if (w + adv > maxW || n + (q - p) >= (int)sizeof(line) - 4) break;
      if (cp == ' ') lastBreak = n;
      memcpy(line + n, p, (size_t)(q - p));
      n += (int)(q - p);
      w += adv;
      p = q;
    }
    bool cut = *p && *p != '\n';
    if (cut && last && ellipsis && dots) {
      // Room for the ellipsis: drop whole UTF-8 characters from the end until it fits.
      for (line[n] = 0; n > 0 && textWidth(line, f) + dotsW > maxW; line[n] = 0) {
        do { n--; } while (n > 0 && (line[n] & 0xC0) == 0x80);
      }
      while (n > 0 && line[n - 1] == ' ') n--;
      memcpy(line + n, "\xE2\x80\xA6", 3);
      n += 3;
      p = "";
    } else {
      if (cut && lastBreak > 0) { n = lastBreak; p = s + lastBreak + 1; }
      if (n == 0 && cut) { const char* q = p; nextCodepoint(q); memcpy(line, p, (size_t)(q - p)); n = (int)(q - p); p = q; }
    }
    line[n] = 0;
    text(x, y, line, f, c, a);
    y += lineH;
    lines++;
    s = *p == '\n' ? p + 1 : p;
  }
  return y;
}

void Canvas::maskOutsideCircle(float cx, float cy, float r) {
  for (int y = y0_; y < y0_ + h_; y++) {
    uint16_t* row = buf_ + (y - y0_) * w_ - x0_;
    float dy = y + 0.5f - cy, q = r * r - dy * dy;
    if (q <= 0) { for (int x = x0_; x < x0_ + w_; x++) row[x] = 0; continue; }
    float hw = sqrtf(q);
    int l = (int)(cx - hw), rr = (int)(cx + hw) + 1;
    for (int x = x0_; x < x0_ + w_ && x < l; x++) row[x] = 0;
    for (int x = rr < x0_ ? x0_ : rr; x < x0_ + w_; x++) row[x] = 0;
  }
}

}  // namespace cc
