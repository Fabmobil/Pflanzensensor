/**
 * @file json_writer.h
 * @brief Streamender JSON-Schreiber ohne Heap
 * @details Schreibt direkt in einen Sink. Kommas setzt der Schreiber selbst;
 *          Texte werden escaped, NaN und ±Inf werden zu null (Befund B-28).
 *          Schachtelungstiefe bis 8.
 */
#pragma once

#include <math.h>
#include <stdio.h>

#include <type_traits>

#include "core/types.h"

namespace ps {

/// Ziel für Bytes. Antworten im Webserver, Dateien, Testpuffer.
class Sink {
public:
  virtual void write(const char* data, size_t len) = 0;
  void write(const char* s) { write(s, strlen(s)); }
  void writeP(FStr s);
  void put(char c) { write(&c, 1); }

protected:
  ~Sink() = default;
};

/// Sink in einen festen Puffer (Tests, kleine Antworten). Läuft nie über.
class BufferSink : public Sink {
public:
  BufferSink(char* buf, size_t cap) : buf_(buf), cap_(cap) {
    if (cap_)
      buf_[0] = '\0';
  }
  void write(const char* data, size_t len) override {
    for (size_t i = 0; i < len; i++) {
      if (len_ + 1 >= cap_) {
        overflow_ = true;
        return;
      }
      buf_[len_++] = data[i];
    }
    buf_[len_] = '\0';
  }
  using Sink::write;
  size_t size() const { return len_; }
  bool overflow() const { return overflow_; }
  const char* c_str() const { return buf_; }
  void clear() {
    len_ = 0;
    overflow_ = false;
    if (cap_)
      buf_[0] = '\0';
  }

private:
  char* buf_;
  size_t cap_;
  size_t len_ = 0;
  bool overflow_ = false;
};

/// Zählt nur Bytes (für Content-Length vorab).
class CountingSink : public Sink {
public:
  void write(const char*, size_t len) override { n_ += len; }
  using Sink::write;
  size_t count() const { return n_; }

private:
  size_t n_ = 0;
};

/// Schreibt einen Text JSON-escaped (ohne Anführungszeichen).
void jsonEscape(Sink& out, const char* s, size_t len);
void jsonEscapeP(Sink& out, FStr s);

/// Formatiert eine Gleitkommazahl mit höchstens `decimals` Nachkommastellen,
/// ohne überflüssige Nullen. NaN/Inf ergeben "null".
size_t formatFloat(char* buf, size_t cap, float v, uint8_t decimals);

class JsonWriter {
public:
  explicit JsonWriter(Sink& out) : out_(out) {}

  JsonWriter& beginObject() { return open('{'); }
  JsonWriter& endObject() { return close('}'); }
  JsonWriter& beginArray() { return open('['); }
  JsonWriter& endArray() { return close(']'); }

  JsonWriter& key(const char* k);
  JsonWriter& key(FStr k);
  /// Schlüssel aus zwei Teilen ("ANALOG" "_" "1"), ohne Zwischenpuffer.
  JsonWriter& keyParts(const char* a, char sep, const char* b);

  JsonWriter& value(const char* s);
  JsonWriter& value(FStr s);
  JsonWriter& value(const char* s, size_t len);
  JsonWriter& value(bool b);
  /// Ganzzahlen jeder Breite
  template <typename T,
            typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value &&
                                        !std::is_same<T, char>::value,
                                    int>::type = 0>
  JsonWriter& value(T v) {
    if (std::is_signed<T>::value)
      return valueSigned(static_cast<int64_t>(v));
    return valueUnsigned(static_cast<uint64_t>(v));
  }
  JsonWriter& value(float v, uint8_t decimals = 2);
  JsonWriter& null();
  /// Bereits fertiges JSON einfügen.
  JsonWriter& raw(const char* json);

  template <typename T> JsonWriter& kv(const char* k, T v) {
    key(k);
    return value(v);
  }
  template <typename T> JsonWriter& kv(FStr k, T v) {
    key(k);
    return value(v);
  }
  JsonWriter& kvf(FStr k, float v, uint8_t decimals) {
    key(k);
    return value(v, decimals);
  }

  Sink& sink() { return out_; }

private:
  JsonWriter& valueSigned(int64_t v);
  JsonWriter& valueUnsigned(uint64_t v);
  JsonWriter& open(char c);
  JsonWriter& close(char c);
  void separator();

  Sink& out_;
  uint8_t depth_ = 0;
  // Bit je Ebene: schon ein Element geschrieben?
  uint8_t hasItem_ = 0;
  bool afterKey_ = false;
};

} // namespace ps
