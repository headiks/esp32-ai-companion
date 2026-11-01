#include "wifi_manager.h"
#include <HTTPClient.h>
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

// Blocking HTTP runs in its own short task, the render loop only picks up the result.
static volatile bool weatherReady = false, weatherBusy = false;
static cc::Weather weatherCond;
static float weatherTemp;
static char weatherUrl[160];

static void weatherTask(void*) {
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(weatherUrl);
  if (http.GET() == 200) {
    String body = http.getString();
    if (cc::parseOpenMeteo(body.c_str(), &weatherCond, &weatherTemp)) weatherReady = true;
  }
  http.end();
  weatherBusy = false;
  vTaskDelete(nullptr);
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
  uint32_t now = millis();
  if ((app.settings.latitude != 0 || app.settings.longitude != 0) && !weatherBusy && (!weatherAt_ || now - weatherAt_ > 30 * 60000UL)) {
    weatherAt_ = now | 1;
    snprintf(weatherUrl, sizeof(weatherUrl), "http://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f&current=temperature_2m,weather_code,wind_speed_10m",
             (double)app.settings.latitude, (double)app.settings.longitude);
    weatherBusy = true;
    xTaskCreatePinnedToCore(weatherTask, "weather", 6144, nullptr, 2, nullptr, 0);
  }
  if (weatherReady) { weatherReady = false; app.setWeather(weatherCond, weatherTemp); }
  if (!ntpDone_) {
    time_t t = time(nullptr);
    if (t > 1700000000) { app.clock.setEpoch((uint32_t)t); ntpDone_ = true; }
  }
}
