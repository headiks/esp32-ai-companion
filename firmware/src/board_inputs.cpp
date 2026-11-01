#include "board_inputs.h"
#include "board.h"
#include <SensorQMI8658.hpp>
#include <TouchDrvCSTXXX.hpp>
#include <Wire.h>
#include <XPowersLib.h>

static TouchDrvCST92xx touch;
static SensorQMI8658 imu;
static XPowersPMU pmu;
static volatile bool touchIrq = false;
static void IRAM_ATTR onTouchIrq() { touchIrq = true; }

void BoardInputs::begin(cc::App& app) {
  pinMode(PIN_SYS_OUT, INPUT);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  // Same init order as the official 1.75C example.
  pinMode(PIN_TP_RESET, OUTPUT);
  digitalWrite(PIN_TP_RESET, LOW);
  delay(30);
  digitalWrite(PIN_TP_RESET, HIGH);
  delay(50);
  touch.setPins(PIN_TP_RESET, PIN_TP_INT);
  touchOk_ = touch.begin(Wire, 0x5A, PIN_I2C_SDA, PIN_I2C_SCL);
  if (touchOk_) {
    touch.reset();
    touch.setMaxCoordinates(cc::kScreenW, cc::kScreenH);
    touch.setMirrorXY(true, true);
    attachInterrupt(PIN_TP_INT, onTouchIrq, FALLING);
  }
  app.hal.touchOk = touchOk_;

  imuOk_ = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (imuOk_) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_8G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
    imu.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_112_1Hz, SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
    imu.enableGyroscope();
  }
  app.hal.imuOk = imuOk_;

  // Rails stay as the bootloader-time defaults: display, touch and codecs all depend on them.
  pmuOk_ = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (pmuOk_) {
    pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    pmu.clearIrqStatus();
    pmu.setChargeTargetVoltage(3);  // value from the official example
    pmu.enableBattDetection();
    pmu.enableVbusVoltageMeasure();
    pmu.enableBattVoltageMeasure();
    pmu.enableSystemVoltageMeasure();
  }
  app.hal.pmicOk = pmuOk_;
}

void BoardInputs::pollTouch(cc::App& app) {
  if (!touchOk_) return;
  uint32_t now = millis();
  if (touchIrq) {
    touchIrq = false;
    int16_t x[5], y[5];
    if (touch.getPoint(x, y, touch.getSupportTouchPoint())) {
      down_ = true;
      x_ = cc::clampi(x[0], 0, cc::kScreenW - 1);
      y_ = cc::clampi(y[0], 0, cc::kScreenH - 1);
      lastTouchAt_ = now;
    }
  }
  // The controller only pulses INT while touched: silence means release.
  if (down_ && now - lastTouchAt_ > 60) down_ = false;
  app.touchInput(down_, x_, y_);
}

void BoardInputs::pollImu(cc::App& app, float dtMs) {
  if (!imuOk_ || !imu.getDataReady()) return;
  IMUdata acc, gyr;
  if (imu.getAccelerometer(acc.x, acc.y, acc.z) && imu.getGyroscope(gyr.x, gyr.y, gyr.z))
    app.imuInput(acc.x, acc.y, acc.z, gyr.x, gyr.y, gyr.z, dtMs);
}

void BoardInputs::pollPower(cc::App& app) {
  uint32_t now = millis();
  if (!pmuOk_ || now - powerAt_ < 2000) return;
  powerAt_ = now;
  bool present = pmu.isBatteryConnect();
  app.power.setBattery(present ? pmu.getBatteryPercent() : 100, pmu.isCharging() || pmu.isVbusIn(), present, present ? pmu.getBattVoltage() : 0);
}

void BoardInputs::pollButtons(cc::App& app) {
  bool pwr = digitalRead(PIN_SYS_OUT) == HIGH, boot = digitalRead(PIN_BOOT) == LOW;
  if (pwr && !pwrWas_) {   // short press: nap or wake up. A long press is handled by the PMIC itself (power off).
    app.power.activity();
    if (app.pet.asleep()) app.wakeNow(); else app.sleepNow();
  }
  if (boot && !bootWas_) { app.power.activity(); app.showScreen(cc::ScreenId::Face); }
  pwrWas_ = pwr;
  bootWas_ = boot;
}

void BoardInputs::poll(cc::App& app, float dtMs) {
  pollTouch(app);
  pollImu(app, dtMs);
  pollPower(app);
  pollButtons(app);
}
