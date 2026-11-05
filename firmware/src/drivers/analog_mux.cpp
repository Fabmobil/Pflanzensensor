/**
 * @file analog_mux.cpp
 */
#if defined(ARDUINO)

#include "drivers/analog_mux.h"

#include <Arduino.h>

#include "board.h"
#include "core/sensors/protocols.h"
#include "services/log.h"

namespace ps {
namespace drivers {

namespace {
constexpr auto& P = board::kPins;
constexpr auto& F = board::kFeatures;
} // namespace

uint8_t AnalogMux::channelCount() const { return F.analogChannels; }

Step AnalogMux::begin(Instant) {
  if (F.multiplexer) {
    pinMode(P.muxA, OUTPUT);
    pinMode(P.muxB, OUTPUT);
    pinMode(P.muxC, OUTPUT);
  }
#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
#endif
  ready_ = true;
  return Step::Ok;
}

bool AnalogMux::select(uint8_t ch) {
  const uint8_t addr = proto::muxAddress(ch);
  const int a = addr & 1, b = (addr >> 1) & 1, c = (addr >> 2) & 1;
  for (uint8_t attempt = 0; attempt < 2; attempt++) {
    digitalWrite(P.muxA, a);
    digitalWrite(P.muxB, b);
    digitalWrite(P.muxC, c);
    if (digitalRead(P.muxA) == a && digitalRead(P.muxB) == b && digitalRead(P.muxC) == c)
      return true;
  }
  LOGW(Mod::Sensor, "Multiplexer-Pins stehen nicht auf Kanal %u", ch + 1);
  return false;
}

Step AnalogMux::pollRound(Instant, Reading* out, uint8_t n) {
  if (!ready_)
    return Step::Error;
  for (uint8_t ch = 0; ch < n; ch++) {
    out[ch] = Reading{};
    if (F.multiplexer && !select(ch))
      continue;
    delayMicroseconds(kSettleUs);
    int raw = analogRead(P.adc);
#if defined(ARDUINO_ARCH_ESP32)
    raw >>= 2; // 12 Bit → 0..1023
#endif
    if (raw < 0 || raw > 1023)
      continue;
    out[ch].raw = static_cast<int16_t>(raw);
    out[ch].value = static_cast<float>(raw);
  }
  return Step::Ok;
}

} // namespace drivers
} // namespace ps

#endif
