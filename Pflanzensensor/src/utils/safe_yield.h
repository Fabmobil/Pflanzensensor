/**
 * @file safe_yield.h
 * @brief Kontextsicheres yield()
 */

#ifndef SAFE_YIELD_H
#define SAFE_YIELD_H

#include <Arduino.h>
#ifndef ESP32
#include <coredecls.h> // can_yield()
#endif

/**
 * @brief Watchdog füttern und nur dann yield(), wenn es erlaubt ist
 *
 * Der ESP8266-Core bricht mit "Panic core_esp8266_main.cpp __yield" ab, wenn
 * yield() außerhalb des cont-Stacks aufgerufen wird (can_yield() == false).
 * Das passiert unter anderem
 *   - im Shutdown-Pfad von /admin/config/update,
 *   - in Logger-Callbacks, die aus beliebigem Kontext gefeuert werden,
 *   - in Flash-Routinen, die aus dem OTA-Upload heraus laufen.
 *
 * delay() ist an diesen Stellen unauffällig, weil es im nicht-yieldbaren Fall
 * auf Busy-Waiting zurückfällt - yield() dagegen nicht.
 *
 * safeYield() füttert immer den Watchdog (in jedem Kontext erlaubt) und gibt
 * die CPU nur dann ab, wenn das gefahrlos möglich ist.
 */
inline void safeYield() {
#ifdef ESP32
  yield();
#else
  ESP.wdtFeed();
  if (can_yield()) {
    yield();
  }
#endif
}

#endif // SAFE_YIELD_H
