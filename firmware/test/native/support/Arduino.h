/**
 * @file Arduino.h
 * @brief Minimaler Arduino-Shim für Treibertests (nur nativ, nur test_drivers)
 * @details Simuliert Pins, ADC (Wert je Multiplexer-Adresse), micros() und
 *          Flankeninterrupts. Die Treiber werden mit ARDUINO=1 direkt in den
 *          Test eingebunden.
 */
#pragma once

#include <stdint.h>
#include <string.h>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define CHANGE 3
#define IRAM_ATTR

namespace shim {
inline int pinMode[64];
inline int pinLevel[64];
inline int pinWrites[64];
inline int adcValue[8] = {}; ///< je Mux-Adresse (A|B<<1|C<<2)
inline int muxPins[3] = {-1, -1, -1};
inline bool stuckPin = false; ///< Rücklesen liefert falschen Pegel
inline int analogReads = 0;
inline uint32_t microsNow = 0;
inline void (*isr)() = nullptr;
inline int isrPin = -1;
inline int adcPin = -1;
inline void reset() {
  memset(pinMode, 0, sizeof(pinMode));
  memset(pinLevel, 0, sizeof(pinLevel));
  memset(pinWrites, 0, sizeof(pinWrites));
  stuckPin = false;
  analogReads = 0;
  microsNow = 0;
  isr = nullptr;
}
inline int muxAddress() {
  if (muxPins[0] < 0)
    return 0;
  return pinLevel[muxPins[0]] | (pinLevel[muxPins[1]] << 1) | (pinLevel[muxPins[2]] << 2);
}
/// Flanke auf dem Interrupt-Pin erzeugen
inline void edge(int level, uint32_t atUs) {
  microsNow = atUs;
  pinLevel[isrPin] = level;
  if (isr)
    isr();
}
} // namespace shim

inline void pinMode(int pin, int mode) { shim::pinMode[pin] = mode; }
inline void digitalWrite(int pin, int v) {
  shim::pinLevel[pin] = v ? 1 : 0;
  shim::pinWrites[pin]++;
}
inline int digitalRead(int pin) {
  if (shim::stuckPin && pin == shim::muxPins[0])
    return 0;
  return shim::pinLevel[pin];
}
inline int analogRead(int pin) {
  shim::analogReads++;
  shim::adcPin = pin;
  return shim::adcValue[shim::muxAddress()];
}
inline void delayMicroseconds(unsigned) {}
inline uint32_t micros() { return shim::microsNow; }
inline int digitalPinToInterrupt(int p) { return p; }
inline void attachInterrupt(int pin, void (*fn)(), int) {
  shim::isr = fn;
  shim::isrPin = pin;
}
inline void noInterrupts() {}
inline void interrupts() {}
