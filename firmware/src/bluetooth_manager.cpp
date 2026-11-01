#include "bluetooth_manager.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <freertos/stream_buffer.h>

static const char* kService = "6e7c0001-5a1e-4c6b-9d3f-0c0a1f2b3c4d";
static const char* kRx = "6e7c0002-5a1e-4c6b-9d3f-0c0a1f2b3c4d";
static const char* kTx = "6e7c0003-5a1e-4c6b-9d3f-0c0a1f2b3c4d";

static NimBLECharacteristic* txChar;
static NimBLECharacteristic* mouseInput;
static NimBLEHIDDevice* hid;

// Standard mouse: five buttons, relative X and Y, wheel.
static const uint8_t kMouseReportMap[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01, 0x95, 0x05, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
    0xC0, 0xC0};
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
    mouseInput = nullptr;
    hid = nullptr;
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
  // Bonding is what makes a computer accept this as a mouse. Commands on the companion service still need the PIN.
  NimBLEDevice::setSecurityAuth(true, false, true);
  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  NimBLEService* svc = server->createService(kService);
  NimBLECharacteristic* rx = svc->createCharacteristic(kRx, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks());
  txChar = svc->createCharacteristic(kTx, NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  hid = new NimBLEHIDDevice(server);
  mouseInput = hid->inputReport(1);
  hid->manufacturer()->setValue("Companion");
  hid->pnp(0x02, 0x303A, 0x4001, 0x0100);
  hid->hidInfo(0x00, 0x02);
  hid->reportMap((uint8_t*)kMouseReportMap, sizeof(kMouseReportMap));
  hid->startServices();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(0x03C2);  // HID mouse
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->addServiceUUID(kService);
  adv->setScanResponse(true);
  adv->start();
  started_ = true;
}

void BluetoothManager::send(const char* line) {
  if (!isConnected || !txChar) return;
  size_t len = strlen(line), off = 0;
  static char framed[180];
  // Short lines such as controller input go out as a single notification, newline included.
  if (len + 1 < sizeof(framed) && len + 1 <= (NimBLEDevice::getMTU() > 23 ? NimBLEDevice::getMTU() - 3u : 20u)) {
    memcpy(framed, line, len);
    framed[len] = '\n';
    txChar->setValue((const uint8_t*)framed, len + 1);
    txChar->notify();
    return;
  }
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

void BluetoothManager::sendMouse(const cc::App::MouseReport& r) {
  if (!isConnected || !mouseInput) return;
  uint8_t report[4] = {r.buttons, (uint8_t)(int8_t)r.dx, (uint8_t)(int8_t)r.dy, (uint8_t)(int8_t)r.wheel};
  mouseInput->setValue(report, sizeof(report));
  mouseInput->notify();
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
