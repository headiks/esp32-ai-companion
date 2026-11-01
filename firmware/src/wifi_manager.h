// WiFiManager: optional station mode, NTP time and a small LAN API. Basic functions never need it.
#pragma once
#include <companion/app.h>

class WiFiManager {
 public:
  void apply(cc::App& app);     // re-reads SSID/enable from settings
  void poll(cc::App& app);
  bool connected() const;
 private:
  bool serverUp_ = false, ntpDone_ = false, enabled_ = false;
};
