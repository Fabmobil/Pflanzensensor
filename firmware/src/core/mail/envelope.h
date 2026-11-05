/**
 * @file envelope.h
 * @brief Mailbot-Protokoll (refactor.md 3.3), bitgenau zum Altcode
 * @details Ablauf mit einem einzigen Puffer:
 *   1. PlaintextBuilder schreibt {"to":…,"subject":…,"body":" in den Puffer,
 *      der Mailrumpf wird direkt JSON-escaped angehängt, dann ","name":…"}.
 *   2. Der Puffer wird in place mit AES-256-GCM verschlüsselt
 *      (AAD = device_id + dezimal(ts)).
 *   3. writeEnvelope() streamt den Umschlag samt Base64 in den Socket; die
 *      Content-Length liefert derselbe Aufruf mit einem CountingSink.
 */
#pragma once

#include <stdio.h>

#include "core/json_writer.h"
#include "core/util/base64.h"

namespace ps {
namespace mailbot {

constexpr size_t kKeyLen = 32;
constexpr size_t kNonceLen = 12;
constexpr size_t kTagLen = 16;
constexpr size_t kMaxSubject = 120;
constexpr size_t kMaxBody = 2000;

/// Sink, der JSON-escaped in einen festen Puffer schreibt und die rohen
/// Bytes zählt (für die Grenze von 2000 B Rumpf).
class EscapingBufferSink : public Sink {
public:
  EscapingBufferSink(char* buf, size_t cap, size_t at) : buf_(buf), cap_(cap), at_(at) {}
  void write(const char* data, size_t len) override {
    raw_ += len;
    BufferAppender app(*this);
    jsonEscape(app, data, len);
  }
  using Sink::write;
  size_t at() const { return at_; }
  size_t rawBytes() const { return raw_; }
  bool overflow() const { return overflow_; }

private:
  struct BufferAppender : Sink {
    explicit BufferAppender(EscapingBufferSink& o) : o_(o) {}
    void write(const char* d, size_t n) override {
      if (o_.at_ + n > o_.cap_) {
        o_.overflow_ = true;
        return;
      }
      memcpy(o_.buf_ + o_.at_, d, n);
      o_.at_ += n;
    }
    using Sink::write;
    EscapingBufferSink& o_;
  };
  char* buf_;
  size_t cap_;
  size_t at_;
  size_t raw_ = 0;
  bool overflow_ = false;
};

/// Baut den Klartext-JSON stückweise in einen Puffer.
class PlaintextBuilder {
public:
  PlaintextBuilder(char* buf, size_t cap) : buf_(buf), cap_(cap) {}

  /// Kopf bis einschließlich `"body":"`
  bool begin(const char* to, const char* subject) {
    BufferSink s(buf_, cap_);
    JsonWriter w(s);
    w.beginObject().kv("to", to).kv("subject", subject);
    s.write(",\"body\":\"");
    if (s.overflow())
      return false;
    at_ = s.size();
    return true;
  }

  /// Sink für den Rumpf (escaped direkt in den Puffer)
  EscapingBufferSink bodySink() { return EscapingBufferSink(buf_, cap_, at_); }

  /// Rumpf abschließen und Namen anhängen. @return Gesamtlänge oder 0
  size_t finish(const EscapingBufferSink& body, const char* name) {
    if (body.overflow())
      return 0;
    at_ = body.at();
    BufferSink tail(buf_ + at_, cap_ - at_);
    tail.write("\",\"name\":\"");
    jsonEscape(tail, name, strlen(name));
    tail.write("\"}");
    if (tail.overflow())
      return 0;
    return at_ + tail.size();
  }

private:
  char* buf_;
  size_t cap_;
  size_t at_ = 0;
};

/// AAD = device_id + Dezimaldarstellung von ts (ohne Trennzeichen)
inline size_t buildAad(char* out, size_t cap, const char* deviceId, uint32_t ts) {
  const int n = snprintf(out, cap, "%s%lu", deviceId, static_cast<unsigned long>(ts));
  return n < 0 ? 0 : static_cast<size_t>(n);
}

/// Umschlag schreiben (zweimal benutzt: zählen und senden)
inline void writeEnvelope(Sink& out, const char* deviceId, uint32_t ts, const uint8_t* nonce,
                          const uint8_t* cipher, size_t cipherLen, const uint8_t* tag) {
  out.write("{\"device_id\":\"");
  jsonEscape(out, deviceId, strlen(deviceId));
  char num[16];
  snprintf(num, sizeof(num), "%lu", static_cast<unsigned long>(ts));
  out.write("\",\"ts\":");
  out.write(num);
  out.write(",\"nonce\":\"");
  base64::encodeTo(out, nonce, kNonceLen);
  out.write("\",\"ciphertext\":\"");
  base64::encodeTo(out, cipher, cipherLen);
  out.write("\",\"tag\":\"");
  base64::encodeTo(out, tag, kTagLen);
  out.write("\"}");
}

/// Grobe Prüfung einer Empfängeradresse (verbindlich prüft der Mailbot)
inline bool plausibleAddress(const char* a) {
  const char* at = strchr(a, '@');
  if (!at || at == a || strchr(at + 1, '@'))
    return false;
  const char* dot = strchr(at + 2, '.');
  return dot && dot[1] != '\0';
}

} // namespace mailbot
} // namespace ps
