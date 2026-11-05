/**
 * @file fakes.h
 * @brief Gemeinsame Fakes der nativen Tests: Treiber, OTA, Display, LED, HTTP
 */
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/sensors/driver.h"
#include "core/util/base64.h"
#include "hal/display.h"
#include "hal/http_server.h"
#include "hal/ota.h"
#include "services/led_service.h"
#include "test_support.h"

namespace pstest {

/// Treiber, der feste Werte liefert (konfigurierbar)
struct ValueDriver final : ps::SensorDriver {
  ps::SensorType t;
  float value = 500;
  int16_t raw = 500;
  bool failBegin = false;
  bool hang = false;
  int begins = 0, sleeps = 0, configures = 0;
  int32_t rawCounts = 0;
  bool hasCounts = false;
  ps::SensorConfig lastCfg;
  explicit ValueDriver(ps::SensorType x) : t(x) {}
  ps::SensorType type() const override { return t; }
  uint8_t channelCount() const override { return 8; }
  ps::Step begin(ps::Instant) override {
    begins++;
    return failBegin ? ps::Step::Error : ps::Step::Ok;
  }
  ps::Step pollRound(ps::Instant, ps::Reading* out, uint8_t n) override {
    if (hang)
      return ps::Step::Pending;
    for (uint8_t i = 0; i < n; i++) {
      out[i].value = value;
      out[i].raw = raw;
    }
    return ps::Step::Ok;
  }
  void sleep() override { sleeps++; }
  void configure(const ps::SensorConfig& c) override {
    configures++;
    lastCfg = c;
  }
  bool lastRawCounts(int32_t& out) const override {
    out = rawCounts;
    return hasCounts;
  }
};

/// Alle Sensoren des Board-Profils als ValueDriver
struct DriverSet {
  std::vector<ValueDriver*> all;
  ps::SensorDriver* ptrs[board::kMaxSensors] = {};
  DriverSet() {
    for (uint8_t i = 0; i < board::sensorCount(); i++) {
      all.push_back(new ValueDriver(board::sensor(i).type));
      ptrs[i] = all.back();
    }
  }
  ~DriverSet() {
    for (auto* d : all)
      delete d;
  }
  ValueDriver& of(ps::SensorType t) { return *all[board::sensorIndexOf(t)]; }
};

struct FakeOta final : ps::hal::Ota {
  bool beginOk = true, endOk = true;
  size_t writeLimit = SIZE_MAX;
  std::string written, md5;
  uint32_t size = 0;
  bool aborted = false, ended = false;
  bool begin(uint32_t s, const char* m) override {
    size = s;
    md5 = m ? m : "";
    written.clear();
    aborted = ended = false;
    return beginOk;
  }
  size_t write(const uint8_t* d, size_t n) override {
    const size_t k = n < writeLimit ? n : writeLimit;
    written.append(reinterpret_cast<const char*>(d), k);
    return k;
  }
  bool end() override {
    ended = true;
    return endOk;
  }
  void abort() override { aborted = true; }
  const char* error() override { return "Fake-Fehler"; }
};

struct FakeBus final : ps::hal::DisplayBus {
  bool present = true;
  int flushes = 0;
  uint8_t last[1024] = {};
  bool on = true;
  bool begin(uint8_t) override { return present; }
  int pageWrites = 0;
  int maxPagesPerCall = 0;
  /// Ein Bild gilt als übertragen, wenn die letzte Seite geschrieben ist
  void flushPages(const uint8_t* fb, uint8_t first, uint8_t count) override {
    pageWrites++;
    if (count > maxPagesPerCall)
      maxPagesPerCall = count;
    memcpy(last + first * 128, fb + first * 128, count * 128);
    if (first + count >= kPages)
      flushes++;
  }
  void power(bool o) override { on = o; }
};

struct FakeLeds final : ps::LedPort {
  bool g = false, y = false, r = false;
  int sets = 0;
  void set(bool a, bool b, bool c) override {
    g = a;
    y = b;
    r = c;
    sets++;
  }
};

struct FakeReq final : ps::hal::Request {
  ps::hal::HttpMethod m = ps::hal::HttpMethod::Get;
  std::string p, b;
  std::map<std::string, std::string> args, headers;
  ps::hal::HttpMethod method() const override { return m; }
  const char* path() const override { return p.c_str(); }
  bool arg(const char* n, char* out, size_t cap) const override {
    auto it = args.find(n);
    if (it == args.end())
      return false;
    snprintf(out, cap, "%s", it->second.c_str());
    return true;
  }
  bool hasArg(const char* n) const override { return args.count(n) > 0; }
  const char* body() const override { return b.empty() ? nullptr : b.c_str(); }
  size_t bodyLength() const override { return b.size(); }
  bool header(const char* n, char* out, size_t cap) const override {
    auto it = headers.find(n);
    if (it == headers.end())
      return false;
    snprintf(out, cap, "%s", it->second.c_str());
    return true;
  }
  void auth(const char* user, const char* pw) {
    std::string cred = std::string(user) + ":" + pw;
    char b64[160];
    ps::base64::encode(reinterpret_cast<const uint8_t*>(cred.data()), cred.size(), b64,
                       sizeof(b64));
    headers["Authorization"] = std::string("Basic ") + b64;
  }
};

struct FakeRes final : ps::hal::Response {
  int code = 0;
  int32_t length = -2;
  std::string type, body;
  std::map<std::string, std::string> headers;
  bool isStarted = false, ended = false;
  void header(const char* n, const char* v) override { headers[n] = v; }
  void begin(int c, const char* t, int32_t len) override {
    code = c;
    type = t;
    length = len;
    isStarted = true;
  }
  void write(const char* d, size_t n) override { body.append(d, n); }
  using Sink::write;
  void writeFlash(const uint8_t* d, size_t n) override {
    body.append(reinterpret_cast<const char*>(d), n);
  }
  void end() override { ended = true; }
  bool started() const override { return isStarted; }
  /// Antwort als JSON (Test schlägt fehl, wenn ungültig)
  JsonDocument json() const {
    JsonDocument d;
    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, deserializeJson(d, body).code(),
                              body.c_str());
    return d;
  }
};

} // namespace pstest
