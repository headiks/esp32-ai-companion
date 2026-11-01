// StorageManager backend: blobs in the NVS partition.
#pragma once
#include <companion/services.h>
#include <Preferences.h>

class NvsStorage : public cc::Storage {
 public:
  void begin() { prefs_.begin("companion", false); }
  bool load(const char* key, void* data, uint32_t size) override { return prefs_.getBytes(key, data, size) == size; }
  bool save(const char* key, const void* data, uint32_t size) override { return prefs_.putBytes(key, data, size) == size; }
  bool erase(const char* key) override { return prefs_.remove(key); }
 private:
  Preferences prefs_;
};
