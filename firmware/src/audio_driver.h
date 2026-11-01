// I2S full duplex at 16 kHz: mixer -> ES8311 -> NS4150B speaker amp; ES7210 mics -> ring buffer.
#pragma once
#include <companion/app.h>

class AudioDriver {
 public:
  bool begin(cc::App& app);
  // Logic thread: moves captured PCM into the analyser. Microphones are ignored while the speaker
  // talks, otherwise the character would react to its own voice.
  void pollMic(cc::App& app);
  void setMicEnabled(bool on);
  void setRecordSink(int16_t* buf, size_t capSamples);  // AI voice capture; nullptr stops
  size_t recordedSamples() const { return recorded_; }
 private:
  static void task(void* arg);
  bool micOn_ = true;
  int16_t* record_ = nullptr;
  size_t recordCap_ = 0, recorded_ = 0;
};
