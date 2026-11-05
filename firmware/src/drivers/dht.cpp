/**
 * @file dht.cpp
 */
#if defined(ARDUINO)

#include "drivers/dht.h"

#include <Arduino.h>

#include "board.h"
#include "core/sensors/protocols.h"
#include "services/log.h"

namespace ps {
namespace drivers {

namespace {
/// Typ und Pin kommen aus der Hardware zur Laufzeit (Webflasher)
inline bool isDht22() { return board::features().dhtType == DhtType::Dht22; }
inline uint8_t pin() { return static_cast<uint8_t>(board::pins().dht); }
/// Mindestabstand zweier Lesungen laut Datenblatt
inline int32_t minPeriodMs() { return isDht22() ? 2000 : 1000; }
/// DHT11: Startsignal 18-30 ms LOW (ASAIR-Datenblatt); länger ignoriert
/// der Sensor. Der Sensor-Task läuft alle 10 ms, den Rest bis 20 ms wartet
/// pollRound() aktiv ab (höchstens kBusyWaitMs).
constexpr int32_t kStartLowMs = 20;
constexpr int32_t kBusyWaitMs = 6;
/// Länger als 30 ms LOW (Schleife war blockiert, z.B. Flash-Schreiben):
/// Leitung freigeben und nach kRecoverMs HIGH neu starten statt zu lesen
constexpr int32_t kStartLowMaxMs = 30;
constexpr int32_t kRecoverMs = 20;
/// DHT22: Startsignal 1..20 ms LOW
constexpr uint32_t kDht22StartUs = 1100;
/// Längster erwarteter Puls (Antwort 80 µs, Bit-HIGH 70 µs) mit Reserve
constexpr uint32_t kPulseTimeoutUs = 120;

#if defined(ARDUINO_ARCH_ESP32)
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

/// Wartet, solange der Pin auf level steht. Dauer in µs, -1 bei Zeitüberschreitung.
int32_t pulse(int level) {
  const uint32_t start = micros();
  const uint8_t p = pin();
  while (digitalRead(p) == level) {
    if (micros() - start > kPulseTimeoutUs)
      return -1;
  }
  return static_cast<int32_t>(micros() - start);
}

enum class ReadResult : uint8_t { Ok, NoResponse, Timeout };

/// Leitung freigeben und den 40-Bit-Rahmen lesen (~4-5 ms ohne Interrupts)
ReadResult readFrame(uint8_t out[5]) {
  for (uint8_t i = 0; i < 5; i++)
    out[i] = 0;
  pinMode(pin(), INPUT_PULLUP); // freigeben; der Sensor antwortet frühestens nach 20 µs
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&g_mux);
#else
  noInterrupts();
#endif
  ReadResult r = ReadResult::Ok;
  // Antwort: nach 20-40 µs HIGH zieht der Sensor 80 µs LOW, dann 80 µs HIGH
  if (pulse(HIGH) < 0 || pulse(LOW) < 0 || pulse(HIGH) < 0) {
    r = ReadResult::NoResponse;
  } else {
    for (uint8_t i = 0; i < 40; i++) {
      if (pulse(LOW) < 0) { // 50 µs Bitanfang
        r = ReadResult::Timeout;
        break;
      }
      const int32_t high = pulse(HIGH);
      if (high < 0) {
        r = ReadResult::Timeout;
        break;
      }
      out[i / 8] =
          static_cast<uint8_t>((out[i / 8] << 1) | (high > proto::kDhtOneThresholdUs ? 1 : 0));
    }
  }
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&g_mux);
#else
  interrupts();
#endif
  return r;
}

void release() { pinMode(pin(), INPUT_PULLUP); }
} // namespace

SensorInfo Dht::info() const {
  SensorInfo i = catalog::info(SensorType::Dht);
  i.roundDelayMs = static_cast<uint16_t>(minPeriodMs());
  return i;
}

Step Dht::begin(Instant now) {
  if (!setup_) {
    release();
    setup_ = true;
    setupAt_ = now;
    return Step::Pending;
  }
  // Nach dem Einschalten braucht der Sensor etwa eine Sekunde
  if ((now - setupAt_).millis() < minPeriodMs())
    return Step::Pending;
  return Step::Ok;
}

Step Dht::startRound(Instant) {
  release();
  state_ = State::WaitLow;
  return Step::Ok;
}

Step Dht::pollRound(Instant now, Reading* out, uint8_t n) {
  switch (state_) {
  case State::WaitLow:
    // Sofortmessung kurz nach einer Lesung: Mindestabstand abwarten
    if (haveLastRead_ && (now - lastRead_).millis() < minPeriodMs())
      return Step::Pending;
    digitalWrite(pin(), LOW);
    pinMode(pin(), OUTPUT);
    lowSince_ = now;
    if (isDht22()) {
      delayMicroseconds(kDht22StartUs);
      break; // gleich lesen
    }
    state_ = State::Low;
    return Step::Pending;
  case State::Recover:
    if ((now - lowSince_).millis() < kRecoverMs)
      return Step::Pending;
    state_ = State::WaitLow;
    return Step::Pending;
  case State::Low: {
    const int32_t elapsed = (now - lowSince_).millis();
    if (elapsed > kStartLowMaxMs) {
      release();
      lowSince_ = now; // ab jetzt: Erholzeit HIGH
      state_ = State::Recover;
      return Step::Pending;
    }
    if (elapsed < kStartLowMs - kBusyWaitMs)
      return Step::Pending;
    if (elapsed < kStartLowMs)
      delayMicroseconds(static_cast<uint32_t>(kStartLowMs - elapsed) * 1000U);
    break;
  }
  case State::Idle:
  default:
    return Step::Error;
  }
  state_ = State::Idle;
  uint8_t b[5];
  const ReadResult r = readFrame(b);
  haveLastRead_ = true;
  lastRead_ = now;
  if (r != ReadResult::Ok) {
    LOGD(Mod::Sensor, "DHT: %s",
         r == ReadResult::NoResponse ? "keine Antwort" : "Rahmen abgebrochen");
    return Step::Error;
  }
  float t = NAN, h = NAN;
  if (!proto::dhtDecode(b, isDht22(), t, h)) {
    LOGD(Mod::Sensor, "DHT: Prüfsumme falsch (%02x %02x %02x %02x %02x)", b[0], b[1], b[2], b[3],
         b[4]);
    return Step::Error;
  }
  if (n > 0 && t >= -40 && t <= 80)
    out[0].value = t;
  if (n > 1 && h >= 0 && h <= 100)
    out[1].value = h;
  return Step::Ok;
}

void Dht::sleep() {
  state_ = State::Idle;
  release();
}

} // namespace drivers
} // namespace ps

#endif
