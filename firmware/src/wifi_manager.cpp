#include "wifi_manager.h"
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

static WebServer web(80);
static cc::App* gApp;

// LAN API, authenticated with the same PIN as BLE. Plain HTTP: intended for a trusted home network only.
static void handleCommandRoute() {
  static char out[1600];
  cc::Session s{web.header("X-Companion-Pin") == gApp->settings.pin, "wifi"};
  if (!s.authed) { web.send(401, "application/json", "{\"status\":\"error\",\"error\":\"auth_required\"}"); return; }
  gApp->handleCommand(web.arg("plain").c_str(), out, sizeof(out), s);
  web.send(200, "application/json", out);
}

bool WiFiManager::connected() const { return WiFi.status() == WL_CONNECTED; }

void WiFiManager::apply(cc::App& app) {
  gApp = &app;
  enabled_ = app.settings.wifiOn && app.settings.wifiSsid[0];
  if (!enabled_) {
    if (serverUp_) { web.stop(); serverUp_ = false; }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(app.settings.wifiSsid, app.settings.wifiPass);  // returns at once, connection continues in the background
  ntpDone_ = false;
}

void WiFiManager::poll(cc::App& app) {
  if (!enabled_) { app.hal.wifiConnected = false; return; }
  bool up = connected();
  app.hal.wifiConnected = up;
  if (!up) return;
  snprintf(app.hal.ip, sizeof(app.hal.ip), "%s", WiFi.localIP().toString().c_str());
  if (!serverUp_) {
    static const char* headers[] = {"X-Companion-Pin"};
    web.collectHeaders(headers, 1);
    web.on("/api/command", HTTP_POST, handleCommandRoute);
    web.begin();
    serverUp_ = true;
    configTime(0, 0, "pool.ntp.org", "time.google.com");
  }
  web.handleClient();
  if (!ntpDone_) {
    time_t t = time(nullptr);
    if (t > 1700000000) { app.clock.setEpoch((uint32_t)t); ntpDone_ = true; }
  }
}
