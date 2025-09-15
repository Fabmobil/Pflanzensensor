/**
 * @file critical_section.h
 * @brief Thread-safe critical section implementation for ESP8266
 * @details Provides RAII-style critical section management for protecting
 *          shared resources in interrupt-sensitive code sections.
 *          Uses ESP8266's interrupt level control for thread safety.
 */

#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

#include <Arduino.h>

/**
 * @class CriticalSection
 * @brief RAII-style critical section implementation
 * @details Implements the Resource Acquisition Is Initialization (RAII) pattern
 *          for managing critical sections in ESP8266 code. Automatically
 * handles interrupt enable/disable to prevent race conditions.
 *
 * Example usage:
 * @code
 * void updateSharedResource() {
 *     CriticalSection cs; // Interrupts disabled here
 *     // Modify shared resource safely
 *     // Interrupts automatically restored when cs goes out of scope
 * }
 * @endcode
 */
class CriticalSection {
 public:
  /**
   * @brief Constructor - enters critical section
   * @details Disables interrupts by setting processor interrupt level to
   * maximum (15) and saves the previous interrupt state for later restoration.
   *          This ensures exclusive access to shared resources.
   */
  CriticalSection() {
    // Save current interrupt state and disable all interrupts
    savedPS = (uint32_t)xt_rsil(15);
  }

  /**
   * @brief Destructor - exits critical section
   * @details Restores the processor state to what it was before entering
   *          the critical section, re-enabling interrupts if they were
   *          previously enabled.
   */
  ~CriticalSection() {
    // Restore previous interrupt state
    xt_wsr_ps(savedPS);
  }

 private:
  uint32_t savedPS;  ///< Saved processor state register value

  // Prevent copying and assignment
  CriticalSection(const CriticalSection&) =
      delete;  ///< Copy constructor disabled
  CriticalSection& operator=(const CriticalSection&) =
      delete;  ///< Assignment operator disabled
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

#endif  // CRITICAL_SECTION_H
