// Firmware shell: starts drivers and runs the shared App. All behaviour lives in core/libcompanion.
#include <Arduino.h>
#include <esp_mac.h>
#include <companion/app.h>
#include "audio_driver.h"
#include "bluetooth_manager.h"
#include "board.h"
#include "board_inputs.h"
#include "display_manager.h"
#include "http_ai_client.h"
#include "storage_nvs.h"
#include "wifi_manager.h"

extern const uint8_t sound_pack_start[] asm("sound_pack_start");
extern const uint8_t sound_pack_end[] asm("sound_pack_end");

static cc::App* app;  // too big for the stack, and allocation here is once at boot
static DisplayManager display;
static BoardInputs inputs;
static AudioDriver audioDrv;
static BluetoothManager ble;
static WiFiManager wifi;
static NvsStorage storage;
static HttpAIClient aiClient;
static cc::Session usbSession{true, "usb"};  // USB means physical access to the device

static void pollSerial() {
  static char line[1100];
  static int len = 0;
  static char out[1600];
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line[len] = 0;
      len = 0;
      if (app->handleCommand(line, out, sizeof(out), usbSession) > 0) Serial.println(out);
    } else if (c != '\r' && len < (int)sizeof(line) - 1) line[len++] = c;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never let a missing USB host stall the animation
  app = new cc::App();
  app->debugOverlay = CC_DEBUG_OVERLAY;
  storage.begin();

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(app->hal.mac, sizeof(app->hal.mac), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(app->hal.platform, sizeof(app->hal.platform), "esp32-s3");
  app->hal.flashMb = ESP.getFlashChipSize() / (1024 * 1024);
  app->hal.psramMb = ESP.getPsramSize() / (1024 * 1024);

  app->begin(&storage, sound_pack_start, (uint32_t)(sound_pack_end - sound_pack_start), esp_random());
  inputs.begin(*app);  // first: creates the I2C bus the codecs use
  if (!display.begin()) Serial.println("# display init failed");
  display.setBrightness(app->power.panelBrightness());
  if (!audioDrv.begin(*app)) Serial.println("# audio init failed");
  aiClient.begin(app, &audioDrv);
  app->setAIClient(&aiClient);
  ble.begin(*app);
  wifi.apply(*app);
  Serial.println("# companion started");
}

void loop() {
  static uint32_t lastMs = millis(), eventAt = 0;
  uint32_t now = millis();
  float dt = (float)(now - lastMs);
  lastMs = now;

  inputs.poll(*app, dt);
  audioDrv.pollMic(*app);
  audioDrv.setMicEnabled(app->settings.micOn);
  pollSerial();
  ble.poll(*app);
  wifi.poll(*app);
  aiClient.pollRecording();
  if (app->netDirty) {
    app->netDirty = false;
    ble.setEnabled(*app, app->settings.bleOn);
    wifi.apply(*app);
  }

  app->hal.bleConnected = ble.connected();
  app->hal.fps = display.fps();
  app->hal.freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  app->hal.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  app->tick(dt);
  display.setBrightness(app->power.panelBrightness());
  display.present(*app);

  cc::App::MouseReport mouse;
  while (app->pollMouse(&mouse)) ble.sendMouse(mouse);

  static char input[160];
  while (app->pollInput(input, sizeof(input)) > 0) {
    if (Serial) Serial.println(input);
    ble.send(input);
  }

  if (now - eventAt >= 2000) {
    eventAt = now;
    static char out[1200];
    app->stateEvent(out, sizeof(out));
    if (Serial) Serial.println(out);
    ble.send(out);
  }

  // Frame pacing without blocking delays elsewhere: other tasks get this time.
  uint32_t frame = 1000 / app->power.targetFps();
  uint32_t used = millis() - now;
  if (used < frame) vTaskDelay(pdMS_TO_TICKS(frame - used));
  else taskYIELD();
}
