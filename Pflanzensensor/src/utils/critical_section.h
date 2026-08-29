/**
 * @file critical_section.h
 * @brief Thread-safe critical section implementation for ESP8266/ESP32
 * @details Provides RAII-style critical section management for protecting
 *          shared resources in interrupt-sensitive code sections.
 *          ESP8266: Uses interrupt level control
 *          ESP32: Uses FreeRTOS mutex (safe for long operations like NVS/I2C)
 */

#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

#include <Arduino.h>

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Global mutex for critical sections on ESP32
// Using a mutex instead of portENTER_CRITICAL allows for longer operations
// without triggering the interrupt watchdog
namespace CriticalSectionInternal {
inline SemaphoreHandle_t& getMutex() {
  static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
  return mutex;
}
} // namespace CriticalSectionInternal
#endif

/**
 * @class CriticalSection
 * @brief RAII-style critical section implementation
 * @details Implements the Resource Acquisition Is Initialization (RAII) pattern
 *          for managing critical sections.
 *          ESP8266: Disables interrupts (for short operations only)
 *          ESP32: Uses FreeRTOS recursive mutex (safe for NVS, I2C, etc.)
 *
 * WICHTIG - Einsatzbereich:
 * Auf dem ESP8266 sperrt diese Klasse ueber xt_rsil(15) ALLE maskierbaren
 * Interrupts. Solange sie aktiv ist, laeuft weder der SDK-Timer (WiFi) noch
 * kann der Watchdog gefuettert werden. Sie ist ausschliesslich fuer sehr
 * kurze Abschnitte gedacht - einzelne Register- oder Flash-Zugriffe.
 *
 * NICHT verwenden fuer:
 *   - Preferences-/NVS-Zugriffe
 *   - LittleFS-Operationen
 *   - Serial-Ausgaben (blockierend bei 115200 Baud)
 *   - Heap-Allokationen oder String-Operationen
 *   - Schleifen ueber mehrere Flash-Sektoren (Erase dauert 20-40 ms je Sektor)
 *
 * Der gesamte Anwendungscode laeuft einthreadig aus loop(); dort schuetzt eine
 * Interruptsperre vor nichts, kostet aber Stabilitaet.
 *
 * Example usage:
 * @code
 * bool ok;
 * {
 *     CriticalSection cs;          // nur der eine Flash-Aufruf
 *     ok = ESP.flashEraseSector(sector);
 * }
 * ESP.wdtFeed();                   // dazwischen Watchdog fuettern
 * @endcode
 */
class CriticalSection {
public:
  /**
   * @brief Constructor - enters critical section
   * @details ESP8266: Disables interrupts
   *          ESP32: Takes a recursive mutex (allows nested locking)
   */
  CriticalSection() {
#ifdef ESP32
    xSemaphoreTakeRecursive(CriticalSectionInternal::getMutex(), portMAX_DELAY);
#else
    // ESP8266: Save current interrupt state and disable all interrupts
    savedPS = (uint32_t)xt_rsil(15);
#endif
  }

  /**
   * @brief Destructor - exits critical section
   * @details ESP8266: Restores interrupt state
   *          ESP32: Releases the mutex
   */
  ~CriticalSection() {
#ifdef ESP32
    xSemaphoreGiveRecursive(CriticalSectionInternal::getMutex());
#else
    // ESP8266: Restore previous interrupt state
    xt_wsr_ps(savedPS);
#endif
  }

private:
#ifndef ESP32
  uint32_t savedPS; ///< Saved processor state register value (ESP8266 only)
#endif

  // Prevent copying and assignment
  CriticalSection(const CriticalSection&) = delete;            ///< Copy constructor disabled
  CriticalSection& operator=(const CriticalSection&) = delete; ///< Assignment operator disabled
};

/**
 * @class ScopedLock
 * @brief Alternative name for CriticalSection for better semantics in some
 * contexts
 * @details Provides a more intuitive name when used in contexts where
 *          "locking" terminology is more appropriate than "critical section".
 *          Functionally identical to CriticalSection.
 *
 * Example usage:
 * @code
 * void lockResource() {
 *     ScopedLock lock; // More intuitive name in locking contexts
 *     // Resource is locked here
 *     // Automatically unlocked when lock goes out of scope
 * }
 * @endcode
 */
using ScopedLock = CriticalSection;

#endif // CRITICAL_SECTION_H
