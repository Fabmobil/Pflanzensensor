/**
 * @file protocols.h
 * @brief Hardwarefreie Protokoll-Logik der Treiber (nativ getestet, AP-13)
 */
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace ps {
namespace proto {

// ---------------------------------------------------------------------------
// CD4051B: Kanal n (1-basiert) → Adresse 7-(n-1); Kanal 1 = A=B=C=HIGH
// ---------------------------------------------------------------------------
inline uint8_t muxAddress(uint8_t channel0) { return static_cast<uint8_t>(7 - (channel0 & 7)); }

// ---------------------------------------------------------------------------
// SDS011
// ---------------------------------------------------------------------------
constexpr uint8_t kSdsQuery = 0x04;
constexpr uint8_t kSdsSleepWork = 0x06;

/// 19-Byte-Kommando: AA B4 cmd d1 d2 00×10 FF FF cksum AB
inline void sdsCommand(uint8_t cmd, uint8_t d1, uint8_t d2, uint8_t out[19]) {
  memset(out, 0, 19);
  out[0] = 0xAA;
  out[1] = 0xB4;
  out[2] = cmd;
  out[3] = d1;
  out[4] = d2;
  out[15] = 0xFF;
  out[16] = 0xFF;
  uint8_t sum = 0;
  for (int i = 2; i <= 16; i++)
    sum = static_cast<uint8_t>(sum + out[i]);
  out[17] = sum;
  out[18] = 0xAB;
}

inline void sdsQuery(uint8_t out[19]) { sdsCommand(kSdsQuery, 0, 0, out); }
/// work = true: Messbetrieb, false: Schlaf
inline void sdsWork(bool work, uint8_t out[19]) { sdsCommand(kSdsSleepWork, 1, work ? 1 : 0, out); }

/// Sammelt Bytes und erkennt 10-Byte-Antworten AA C0/C5 … cksum AB
class SdsParser {
public:
  /// @return true, wenn ein vollständiger Messrahmen (C0) erkannt wurde
  bool feed(uint8_t b) {
    if (n_ == 0 && b != 0xAA)
      return false;
    buf_[n_++] = b;
    if (n_ == 2 && b != 0xC0 && b != 0xC5) {
      n_ = (b == 0xAA) ? 1 : 0;
      return false;
    }
    if (n_ < 10)
      return false;
    n_ = 0;
    if (buf_[9] != 0xAB)
      return false;
    uint8_t sum = 0;
    for (int i = 2; i <= 7; i++)
      sum = static_cast<uint8_t>(sum + buf_[i]);
    if (sum != buf_[8] || buf_[1] != 0xC0)
      return false;
    pm25_ = static_cast<float>(buf_[2] + 256 * buf_[3]) / 10.0f;
    pm10_ = static_cast<float>(buf_[4] + 256 * buf_[5]) / 10.0f;
    return true;
  }
  void reset() { n_ = 0; }
  float pm25() const { return pm25_; }
  float pm10() const { return pm10_; }

private:
  uint8_t buf_[10] = {};
  uint8_t n_ = 0;
  float pm25_ = NAN, pm10_ = NAN;
};

// ---------------------------------------------------------------------------
// MH-Z19 (PWM): ppm = range·(th−2)/(th+tl−4), Zeiten in ms
// ---------------------------------------------------------------------------
inline float mhz19Ppm(uint32_t thUs, uint32_t tlUs, uint16_t range) {
  const float th = static_cast<float>(thUs) / 1000.0f;
  const float tl = static_cast<float>(tlUs) / 1000.0f;
  const float den = th + tl - 4.0f;
  if (den <= 0 || th < 2.0f)
    return NAN;
  // Ein Zyklus dauert 1004 ms ± 5 %; alles andere ist kein gültiger Zyklus
  if (th + tl < 950.0f || th + tl > 1060.0f)
    return NAN;
  return static_cast<float>(range) * (th - 2.0f) / den;
}

// ---------------------------------------------------------------------------
// Serial Receiver: Zeilenprotokoll "GET:<idx>\n" → "<zahl>\n"
// ---------------------------------------------------------------------------
/// Wert aus einer Antwortzeile; false bei Unsinn
inline bool parseValueLine(const char* line, float& out) {
  while (*line == ' ' || *line == '\t')
    line++;
  if (!*line)
    return false;
  char* end = nullptr;
  const float v = strtof(line, &end);
  if (end == line)
    return false;
  while (*end == ' ' || *end == '\r' || *end == '\t')
    end++;
  if (*end != '\0')
    return false;
  if (isnan(v) || isinf(v))
    return false;
  out = v;
  return true;
}

// ---------------------------------------------------------------------------
// DHT11/DHT22: 40 Bit = Feuchte (2 Byte), Temperatur (2 Byte), Prüfsumme
// ---------------------------------------------------------------------------
/// Ein Bit ist 1, wenn sein HIGH-Puls länger als das dauert (0: 26–28 µs, 1: 70 µs)
constexpr uint16_t kDhtOneThresholdUs = 40;

/**
 * @brief Rahmen dekodieren (Formeln wie DHTesp)
 * @return false bei falscher Prüfsumme oder leerem Rahmen (Leitung tot)
 */
inline bool dhtDecode(const uint8_t b[5], bool dht22, float& tempC, float& humidity) {
  if (static_cast<uint8_t>(b[0] + b[1] + b[2] + b[3]) != b[4])
    return false;
  if ((b[0] | b[1] | b[2] | b[3]) == 0)
    return false; // 0 % bei 0 °C: kein Messwert
  if (dht22) {
    humidity = static_cast<float>((b[0] << 8) | b[1]) * 0.1f;
    const int16_t t = static_cast<int16_t>(((b[2] & 0x7F) << 8) | b[3]);
    tempC = static_cast<float>(b[2] & 0x80 ? -t : t) * 0.1f;
  } else {
    humidity = static_cast<float>(b[0]) + static_cast<float>(b[1]) * 0.1f;
    tempC = static_cast<float>(b[2]) + static_cast<float>(b[3] & 0x7F) * 0.1f;
    if (b[3] & 0x80)
      tempC = -tempC;
  }
  return true;
}

/// Sammelt Zeichen zu Zeilen (ohne Heap)
class LineBuffer {
public:
  /// @return true, wenn eine Zeile fertig ist (line())
  bool feed(char c) {
    if (c == '\n') {
      buf_[n_] = '\0';
      n_ = 0;
      return true;
    }
    if (c == '\r')
      return false;
    if (static_cast<size_t>(n_) + 1 < sizeof(buf_))
      buf_[n_++] = c;
    return false;
  }
  const char* line() const { return buf_; }
  void reset() {
    n_ = 0;
    buf_[0] = '\0';
  }

private:
  char buf_[32] = {};
  uint8_t n_ = 0;
};

} // namespace proto
} // namespace ps
