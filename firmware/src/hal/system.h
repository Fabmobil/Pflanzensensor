/**
 * @file system.h
 * @brief Plattformfunktionen ohne eigenen Zustand (Zeit, Zufall, Heap, Neustart)
 * @details Implementiert in platform/esp8266, platform/esp32, platform/native.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/types.h"

namespace ps {
namespace hal {

/// millis() als Instant
Instant now();
/// Unix-Zeit (0 bzw. < kMinValidEpoch, solange die Uhr nicht gestellt ist)
uint32_t epoch();
/// Lokale Zeit zerlegen (Zeitzone Europe/Berlin); false ohne gültige Uhr
struct LocalTime {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
};
bool toLocalTime(uint32_t epoch, LocalTime& out);

/// SNTP starten (nicht blockierend); TZ als POSIX-String
void timeBegin(const char* tz, const char* server);

void yieldNow();
void restart();
void fillRandom(uint8_t* buf, size_t len);

struct HeapInfo {
  uint32_t free = 0;
  uint32_t maxBlock = 0;
  uint8_t fragmentation = 0;
};
HeapInfo heap();

void macAddress(uint8_t mac[6]);
/// "AA:BB:CC:DD:EE:FF"
void macString(char out[18]);

enum class ResetReason : uint8_t {
  PowerOn,
  External,
  Software,
  Watchdog,
  Exception,
  DeepSleep,
  Brownout,
  Unknown
};
ResetReason resetReason();

/// Platz für ein Firmware-Update in Byte
uint32_t freeSketchSpace();

/// Text auf die serielle Konsole
void consoleWrite(const char* data, size_t len);
void consoleBegin(uint32_t baud);
/// Empfangene Bytes der Konsole (ohne zu warten); Anzahl gelesener Bytes
size_t consoleRead(uint8_t* buf, size_t cap);

/// Temporärer Speicher für seltene Vorgänge (Mailversand, Import).
/// Dokumentierte Ausnahme von "keine Heap-Allokation" (refactor.md 5.3).
void* tempAlloc(size_t n);
void tempFree(void* p);

/// RAII für tempAlloc
class TempBuffer {
public:
  explicit TempBuffer(size_t n) : p_(static_cast<uint8_t*>(tempAlloc(n))), n_(p_ ? n : 0) {}
  ~TempBuffer() { tempFree(p_); }
  TempBuffer(const TempBuffer&) = delete;
  TempBuffer& operator=(const TempBuffer&) = delete;
  uint8_t* data() { return p_; }
  char* chars() { return reinterpret_cast<char*>(p_); }
  size_t size() const { return n_; }
  explicit operator bool() const { return p_ != nullptr; }

private:
  uint8_t* p_;
  size_t n_;
};

} // namespace hal
} // namespace ps
