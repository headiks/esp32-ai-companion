#include "display_manager.h"
#include "board.h"
#include <Arduino_GFX_Library.h>

static Arduino_DataBus* bus;
static Arduino_CO5300* gfx;

bool DisplayManager::begin() {
  bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);
  gfx = new Arduino_CO5300(bus, PIN_LCD_RESET, 0, cc::kScreenW, cc::kScreenH, LCD_COL_OFFSET, 0, 0, 0);
  if (!gfx->begin()) return false;
  gfx->fillScreen(0);
  memset(wasBlack_, 0, sizeof(wasBlack_));
  // Internal RAM is much faster than PSRAM to draw into and can be used by DMA directly.
  strip_ = (uint16_t*)heap_caps_malloc(cc::kScreenW * kStripH * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  fpsAt_ = millis();
  return strip_ != nullptr;
}

void DisplayManager::setBrightness(uint8_t percent) {
  uint8_t v = (uint8_t)(percent * 255 / 100);
  if (v != bright_) { bright_ = v; gfx->setBrightness(v); }
}

void DisplayManager::present(cc::App& app) {
  cc::Rect r = app.dirtyRect();
  // The panel accepts only even window origins and sizes.
  int y0 = r.y / kStripH * kStripH, y1 = (r.y + r.h + 1) & ~1;  // strip grid stays fixed for wasBlack_
  if (y1 > cc::kScreenH) y1 = cc::kScreenH;
  bool partial = r.h < cc::kScreenH;
  if (partial != wasPartial_) memset(wasBlack_, 0, sizeof(wasBlack_));  // a shorter last strip changes its meaning
  for (int y = y0; y < y1; y += kStripH) {
    int h = y + kStripH > y1 ? y1 - y : kStripH;
    canvas_.setTarget(strip_, 0, y, cc::kScreenW, h);
    app.render(canvas_);
    // Most strips of the face are black from frame to frame: skip those to save bus time and power.
    bool black = true;
    const uint32_t* p = (const uint32_t*)strip_;
    int words = cc::kScreenW * h / 2;
    for (int i = 0; i < words; i++) if (p[i]) { black = false; break; }
    int idx = y / kStripH;
    if (black && wasBlack_[idx]) continue;
    wasBlack_[idx] = black;
    gfx->draw16bitRGBBitmap(0, y, strip_, cc::kScreenW, h);
  }
  // Rows outside the dirty rectangle may still hold the previous full-screen picture: clear them once.
  if (partial && !wasPartial_) {
    gfx->fillRect(0, 0, cc::kScreenW, y0, 0);
    gfx->fillRect(0, y1, cc::kScreenW, cc::kScreenH - y1, 0);
  }
  wasPartial_ = partial;
  frames_++;
  uint32_t now = millis();
  if (now - fpsAt_ >= 1000) {
    fps_ = frames_ * 1000.f / (now - fpsAt_);
    frames_ = 0;
    fpsAt_ = now;
  }
}
