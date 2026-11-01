// BluetoothManager: GATT JSON service. UUIDs and rules are in protocol/services.md.
#pragma once
#include <companion/app.h>

class BluetoothManager {
 public:
  void begin(cc::App& app);
  void setEnabled(cc::App& app, bool on);
  void poll(cc::App& app);       // handles received commands on the logic thread
  void send(const char* line);   // adds '\n', splits into notifications
  bool connected() const;
 private:
  bool started_ = false;
};
