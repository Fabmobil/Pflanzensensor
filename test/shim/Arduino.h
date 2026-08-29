/**
 * @file Arduino.h
 * @brief Minimaler Arduino-Ersatz für native Unit-Tests
 *
 * Deckt nur ab, was die getesteten Einheiten wirklich benutzen: String,
 * millis(), F()/PROGMEM und ein paar Typaliase. Kein Anspruch auf
 * Vollständigkeit - was fehlt, fällt beim Übersetzen sofort auf.
 *
 * Der wichtigste Punkt ist die STEUERBARE UHR: millis() liefert einen Wert,
 * den der Test über setMillis()/advanceMillis() vorgibt. Damit lassen sich
 * Zeitschranken und Wiederholungsabstände in Mikrosekunden durchfahren,
 * statt in Echtzeit darauf zu warten.
 */

#ifndef NATIVE_TEST_ARDUINO_H
#define NATIVE_TEST_ARDUINO_H

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// ---------------------------------------------------------------- Uhr

namespace native_test {
/// Inline-Variable (C++17): der Shim kommt damit ohne eigene
/// Ubersetzungseinheit aus, die PlatformIO im Testlauf nicht mit einsammeln
/// wurde.
inline unsigned long g_millis = 0;
} // namespace native_test

inline unsigned long millis() { return native_test::g_millis; }

/// Uhr auf einen festen Wert setzen (Testhilfe, nicht auf dem Gerät vorhanden)
inline void setMillis(unsigned long value) { native_test::g_millis = value; }

/// Uhr weiterstellen (Testhilfe)
inline void advanceMillis(unsigned long delta) { native_test::g_millis += delta; }

inline void delay(unsigned long) {}
inline void yield() {}
inline void randomSeed(unsigned long) {}

using std::isnan;
using std::max;
using std::min;

// ------------------------------------------------------------ PROGMEM

#define PROGMEM
#define PGM_P const char*
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t*>(addr))
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(string_literal))
#define PSTR(s) (s)

// ------------------------------------------------------------- String

/**
 * @brief Nachbau der Arduino-String-Klasse auf Basis von std::string
 * @details Nur die im Projekt benutzten Methoden. Die Semantik entspricht
 *          der Arduino-Vorlage, inklusive der Verkettung mit Zahlen.
 */
class String {
public:
  String() = default;
  String(const char* value) : m_data(value ? value : "") {}
  String(const std::string& value) : m_data(value) {}
  String(const __FlashStringHelper* value)
      : m_data(value ? reinterpret_cast<const char*>(value) : "") {}
  String(char value) : m_data(1, value) {}
  String(int value) : m_data(std::to_string(value)) {}
  String(long value) : m_data(std::to_string(value)) {}
  String(unsigned int value) : m_data(std::to_string(value)) {}
  String(unsigned long value) : m_data(std::to_string(value)) {}
  String(uint8_t value) : m_data(std::to_string(static_cast<unsigned>(value))) {}

  String(float value, int decimals = 2) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    m_data = buffer;
  }

  const char* c_str() const { return m_data.c_str(); }
  size_t length() const { return m_data.size(); }
  bool isEmpty() const { return m_data.empty(); }
  bool startsWith(const char* prefix) const { return m_data.rfind(prefix, 0) == 0; }
  void clear() { m_data.clear(); }

  bool operator==(const String& other) const { return m_data == other.m_data; }
  bool operator==(const char* other) const { return m_data == (other ? other : ""); }
  bool operator!=(const String& other) const { return !(*this == other); }

  String& operator+=(const String& other) {
    m_data += other.m_data;
    return *this;
  }
  String& operator+=(const char* other) {
    if (other)
      m_data += other;
    return *this;
  }

  friend String operator+(String lhs, const String& rhs) {
    lhs += rhs;
    return lhs;
  }
  friend String operator+(String lhs, const char* rhs) {
    lhs += rhs;
    return lhs;
  }

  const std::string& str() const { return m_data; }

private:
  std::string m_data;
};

using boolean = bool;
using byte = uint8_t;

// -------------------------------------------------------------- ESP-Objekt

/**
 * @brief Nachbau der globalen ESP-Instanz des Arduino-Cores
 * @details Nur getFreeHeap() und restart() werden von der getesteten Logik
 *          benutzt. getFreeHeap() liefert testbar einen festen, ausreichend
 *          großen Wert; restart() zählt nur, ob es aufgerufen wurde - ein
 *          echter Neustart ist im Testprozess weder möglich noch gewollt.
 */
class ESPClass {
public:
  uint32_t freeHeap = 40000;
  unsigned restartCount = 0;

  uint32_t getFreeHeap() const { return freeHeap; }
  void restart() { restartCount++; }
};
inline ESPClass ESP;

#endif // NATIVE_TEST_ARDUINO_H
