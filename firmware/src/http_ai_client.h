// AIClient over HTTP to the user's gateway (settings.aiUrl). Provider details live in the gateway only.
// Without a URL or Wi-Fi it silently falls back to the offline rules.
#pragma once
#include <Arduino.h>
#include <companion/app.h>
#include "audio_driver.h"

class HttpAIClient : public cc::AIClient {
 public:
  void begin(cc::App* app, AudioDriver* audio);
  bool submitText(const char* text) override;
  bool submitVoice() override;
  bool poll(cc::AIReply* reply) override;
  const char* name() const override { return ready() ? "http" : "offline"; }
  void pollRecording();   // logic thread: ends capture and starts the upload
 private:
  bool ready() const;
  static void task(void* arg);
  void run();
  cc::App* app_ = nullptr;
  AudioDriver* audio_ = nullptr;
  cc::OfflineAI offline_;
  QueueHandle_t done_ = nullptr;
  TaskHandle_t handle_ = nullptr;
  char text_[256];
  int16_t* voice_ = nullptr;
  size_t voiceSamples_ = 0;
  bool isVoice_ = false, busy_ = false, recording_ = false;
  uint32_t recordAt_ = 0;
  int16_t* reply_ = nullptr;
};
