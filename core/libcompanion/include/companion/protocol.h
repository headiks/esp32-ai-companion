// Minimal JSON reader/writer for protocol messages; exposed for tests.
#pragma once
#include "types.h"

namespace cc {

// Parses in place: pointers refer into the buffer passed to parse().
class JsonObject {
 public:
  bool parse(char* s);
  const char* str(const char* key) const;
  double num(const char* key, double def) const;
  bool has(const char* key) const;
 private:
  struct Item { char* key; char* str; double num; bool isString; };
  static constexpr int kMax = 16;
  Item items_[kMax];
  int n_ = 0;
};

class JsonWriter {
 public:
  JsonWriter(char* buf, int cap) : buf_(buf), cap_(cap) { if (cap_ > 0) buf_[0] = 0; }
  void begin() { len_ = 0; first_ = true; raw("{"); }
  void end() { raw("}"); }
  void add(const char* k, const char* v);
  void add(const char* k, int v);
  void add(const char* k, bool v);
  void addf(const char* k, float v);
  void addU(const char* k, uint32_t v);
  int length() const { return len_; }
 private:
  void raw(const char* s);
  void key(const char* k);
  char* buf_;
  int cap_, len_ = 0;
  bool first_ = true;
};

}  // namespace cc
