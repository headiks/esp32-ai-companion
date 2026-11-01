#include "http_ai_client.h"
#include <companion/protocol.h>
#include <HTTPClient.h>
#include <WiFi.h>

static constexpr size_t kVoiceSamples = cc::kSampleRate * 5;
static constexpr size_t kReplyMaxBytes = cc::kSampleRate * 2 * 15;

void HttpAIClient::begin(cc::App* app, AudioDriver* audio) {
  app_ = app;
  audio_ = audio;
  done_ = xQueueCreate(1, sizeof(cc::AIReply));
}

bool HttpAIClient::ready() const { return app_ && app_->settings.aiUrl[0] && WiFi.status() == WL_CONNECTED; }

bool HttpAIClient::submitText(const char* text) {
  if (!ready()) return offline_.submitText(text);
  if (busy_) return false;
  snprintf(text_, sizeof(text_), "%s", text);
  isVoice_ = false;
  busy_ = true;
  xTaskCreatePinnedToCore(task, "ai", 8192, this, 2, &handle_, 0);
  return true;
}

// Called only after the App has checked settings.aiVoiceConsent.
bool HttpAIClient::submitVoice() {
  if (!ready() || busy_ || recording_) return false;
  if (!voice_) voice_ = (int16_t*)heap_caps_malloc(kVoiceSamples * 2, MALLOC_CAP_SPIRAM);
  if (!voice_) return false;
  audio_->setRecordSink(voice_, kVoiceSamples);
  recording_ = true;
  recordAt_ = millis();
  return true;
}

void HttpAIClient::pollRecording() {
  if (!recording_ || millis() - recordAt_ < 4000) return;
  voiceSamples_ = audio_->recordedSamples();
  audio_->setRecordSink(nullptr, 0);
  recording_ = false;
  isVoice_ = true;
  busy_ = true;
  xTaskCreatePinnedToCore(task, "ai", 8192, this, 2, &handle_, 0);
}

bool HttpAIClient::poll(cc::AIReply* r) {
  if (offline_.poll(r)) return true;
  return done_ && xQueueReceive(done_, r, 0) == pdTRUE;
}

void HttpAIClient::task(void* arg) {
  ((HttpAIClient*)arg)->run();
  vTaskDelete(nullptr);
}

static void escapeJson(String& out, const char* s) {
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    if ((uint8_t)*s >= 0x20) out += *s;
  }
}

void HttpAIClient::run() {
  cc::AIReply reply{};
  reply.emotion = cc::Emotion::Confused;
  snprintf(reply.text, sizeof(reply.text), "Сервер не отвечает, я пока сам по себе.");
  HTTPClient http;
  String url = app_->settings.aiUrl;
  http.setTimeout(30000);
  int code = -1;
  if (isVoice_) {
    http.begin(url + "/v1/voice");
    http.addHeader("Content-Type", "audio/L16;rate=16000");
    code = http.POST((uint8_t*)voice_, voiceSamples_ * 2);
  } else {
    http.begin(url + "/v1/chat");
    http.addHeader("Content-Type", "application/json");
    String body = "{\"text\":\"";
    escapeJson(body, text_);
    body += "\"}";
    code = http.POST(body);
  }
  String audioUrl;
  if (code == 200) {
    String resp = http.getString();
    cc::JsonObject j;
    if (j.parse((char*)resp.c_str())) {
      const char* t = j.str("text");
      const char* e = j.str("emotion");
      if (t) { snprintf(reply.text, sizeof(reply.text), "%s", t); reply.ok = true; }
      if (e) cc::emotionFromName(e, &reply.emotion);
      if (const char* a = j.str("audio_url")) audioUrl = a;
    }
  }
  http.end();
  xQueueOverwrite(done_, &reply);

  if (audioUrl.length()) {
    // Voice reply: raw 16 kHz mono s16 from the gateway, played through the normal mixer.
    HTTPClient get;
    get.begin(audioUrl);
    if (get.GET() == 200) {
      int len = get.getSize();
      if (len > 0 && (size_t)len <= kReplyMaxBytes) {
        if (!reply_) reply_ = (int16_t*)heap_caps_malloc(kReplyMaxBytes, MALLOC_CAP_SPIRAM);
        if (reply_) {
          WiFiClient* s = get.getStreamPtr();
          int got = 0;
          uint32_t start = millis();
          while (got < len && millis() - start < 20000) {
            int r = s->read((uint8_t*)reply_ + got, len - got);
            if (r > 0) got += r; else delay(5);
          }
          if (got == len) app_->audio.playPcm(reply_, (uint32_t)len / 2);
        }
      }
    }
    get.end();
  }
  busy_ = false;
}
