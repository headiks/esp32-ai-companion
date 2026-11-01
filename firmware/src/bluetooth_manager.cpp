#include "bluetooth_manager.h"
#include <NimBLEDevice.h>
#include <freertos/stream_buffer.h>

static const char* kService = "6e7c0001-5a1e-4c6b-9d3f-0c0a1f2b3c4d";
static const char* kRx = "6e7c0002-5a1e-4c6b-9d3f-0c0a1f2b3c4d";
static const char* kTx = "6e7c0003-5a1e-4c6b-9d3f-0c0a1f2b3c4d";

static NimBLECharacteristic* txChar;
static NimBLEServer* server;
static StreamBufferHandle_t rxStream;
static cc::Session session{false, "ble"};
static volatile bool resetSession = false;
static volatile bool isConnected = false;

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    NimBLEAttValue v = c->getValue();
    xStreamBufferSend(rxStream, v.data(), v.length(), 0);
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override { isConnected = true; resetSession = true; }
  void onDisconnect(NimBLEServer*) override {
    isConnected = false;
    resetSession = true;
    NimBLEDevice::startAdvertising();
  }
};

bool BluetoothManager::connected() const { return isConnected; }

void BluetoothManager::begin(cc::App& app) {
  rxStream = xStreamBufferCreate(2048, 1);
  setEnabled(app, app.settings.bleOn);
}

void BluetoothManager::setEnabled(cc::App& app, bool on) {
  if (on == started_) return;
  if (!on) {
    NimBLEDevice::deinit(true);
    txChar = nullptr;
    server = nullptr;
    isConnected = false;
    started_ = false;
    return;
  }
  char name[24];
  const char* mac = app.hal.mac;
  snprintf(name, sizeof(name), "Companion-%s", strlen(mac) >= 17 ? mac + 12 : "0");
  NimBLEDevice::init(name);
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  NimBLEDevice::setMTU(247);
  // LE Secure Connections encrypts the link when the phone pairs; commands still need the PIN.
  NimBLEDevice::setSecurityAuth(false, false, true);
  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  NimBLEService* svc = server->createService(kService);
  NimBLECharacteristic* rx = svc->createCharacteristic(kRx, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks());
  txChar = svc->createCharacteristic(kTx, NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(kService);
  adv->setScanResponse(true);
  adv->start();
  started_ = true;
}

void BluetoothManager::send(const char* line) {
  if (!isConnected || !txChar) return;
  size_t len = strlen(line), off = 0;
  uint16_t mtu = NimBLEDevice::getMTU();
  size_t chunk = mtu > 23 ? mtu - 3 : 20;
  if (chunk > 180) chunk = 180;
  while (off < len) {
    size_t n = len - off < chunk ? len - off : chunk;
    txChar->setValue((const uint8_t*)line + off, n);
    txChar->notify();
    off += n;
  }
  txChar->setValue((const uint8_t*)"\n", 1);
  txChar->notify();
}

void BluetoothManager::poll(cc::App& app) {
  static char line[1100];
  static int len = 0;
  static char out[1600];
  if (resetSession) { resetSession = false; session.authed = false; len = 0; }
  if (!rxStream) return;
  char c;
  while (xStreamBufferReceive(rxStream, &c, 1, 0) == 1) {
    if (c == '\n') {
      line[len] = 0;
      len = 0;
      if (app.handleCommand(line, out, sizeof(out), session) > 0) send(out);
    } else if (len < (int)sizeof(line) - 1) line[len++] = c;
  }
}
