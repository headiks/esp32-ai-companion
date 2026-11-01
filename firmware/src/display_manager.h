// DisplayManager: CO5300 panel, strip rendering into internal DMA RAM, brightness.
#pragma once
#include <companion/app.h>

class DisplayManager {
 public:
  bool begin();
  void present(cc::App& app);
  void setBrightness(uint8_t percent);
  float fps() const { return fps_; }
 private:
  static constexpr int kStripH = 40;
  uint16_t* strip_ = nullptr;
  cc::Canvas canvas_;
  uint8_t wasBlack_[cc::kScreenH / kStripH + 1];
  bool wasPartial_ = false;
  uint8_t bright_ = 255;
  float fps_ = 0;
  uint32_t frames_ = 0, fpsAt_ = 0;
};
