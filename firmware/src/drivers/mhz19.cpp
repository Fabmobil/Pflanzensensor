/**
 * @file mhz19.cpp
 */
#if defined(ARDUINO)

#include "drivers/mhz19.h"

#include <Arduino.h>

#include "board.h"
#include "core/sensors/protocols.h"

namespace ps {
namespace drivers {

namespace {
volatile uint32_t g_riseUs = 0;
volatile uint32_t g_fallUs = 0;
volatile uint32_t g_highUs = 0;
volatile uint32_t g_lowUs = 0;
volatile uint32_t g_cycles = 0;

void IRAM_ATTR onEdge() {
  const uint32_t t = micros();
  if (digitalRead(board::pins().mhz19Pwm)) {
    if (g_fallUs) {
      g_lowUs = t - g_fallUs;
      g_cycles++; // Zyklus vollständig: High gefolgt von Low
    }
    g_riseUs = t;
  } else {
    if (g_riseUs)
      g_highUs = t - g_riseUs;
    g_fallUs = t;
  }
}
} // namespace

Step Mhz19::begin(Instant) {
  if (!attached_) {
    pinMode(board::pins().mhz19Pwm, INPUT);
    attachInterrupt(digitalPinToInterrupt(board::pins().mhz19Pwm), onEdge, CHANGE);
    attached_ = true;
  }
  return Step::Ok;
}

Step Mhz19::startRound(Instant) {
  startCycle_ = g_cycles;
  return Step::Ok;
}

Step Mhz19::pollRound(Instant, Reading* out, uint8_t n) {
  // Ein frischer, vollständiger Zyklus nach Rundenbeginn
  if (g_cycles < startCycle_ + 2)
    return Step::Pending;
  noInterrupts();
  const uint32_t th = g_highUs, tl = g_lowUs;
  interrupts();
  const float ppm = proto::mhz19Ppm(th, tl, board::features().mhz19Range);
  if (n > 0 && ppm > 0 && ppm <= 5000)
    out[0].value = ppm;
  return Step::Ok;
}

} // namespace drivers
} // namespace ps

#endif
