// Sensor drivers on the shared I2C bus: CST9217 touch, QMI8658 IMU, AXP2101 PMIC, buttons.
#pragma once
#include <companion/app.h>

class BoardInputs {
 public:
  void begin(cc::App& app);
  void poll(cc::App& app, float dtMs);
 private:
  void pollTouch(cc::App& app);
  void pollImu(cc::App& app, float dtMs);
  void pollPower(cc::App& app);
  void pollButtons(cc::App& app);
  uint32_t lastTouchAt_ = 0, powerAt_ = 0;
  bool down_ = false, imuOk_ = false, pmuOk_ = false, touchOk_ = false, pwrWas_ = false, bootWas_ = false;
  int x_ = 0, y_ = 0;
};
