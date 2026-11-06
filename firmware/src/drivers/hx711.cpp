/**
 * @file hx711.cpp
 */
#if defined(ARDUINO)

#include "drivers/hx711.h"

#include <HX711.h>

#include "board.h"

namespace ps {
namespace drivers {

namespace {
HX711& scale() {
  static HX711 s;
  return s;
}
} // namespace

Step Hx711::begin(Instant) {
  if (!begun_) {
    scale().begin(board::pins().hxDout, board::pins().hxSck);
    begun_ = true; // kein Tarieren beim Start: der Nullpunkt ist gespeichert
  }
  return Step::Ok;
}

Step Hx711::wake(Instant) {
  scale().power_up(); // ohne Wirkung, solange er an ist (kein sleep())
  jobSum_ = 0;
  jobN_ = 0;
  return Step::Ok;
}

Step Hx711::startRound(Instant now) {
  roundSum_ = 0;
  roundN_ = 0;
  roundStart_ = now;
  return Step::Ok;
}

Step Hx711::pollRound(Instant now, Reading* out, uint8_t n) {
  // Höchstens eine Wandlung je Aufruf: read() wartet sonst auf die nächste
  if (roundN_ < kSamplesPerRound && scale().is_ready()) {
    const long raw = scale().read(); // < 1 ms, Daten liegen bereit
    roundSum_ += raw;
    roundN_++;
    jobSum_ += raw;
    jobN_++;
  }
  const bool timeUp = (now - roundStart_).millis() >= kRoundMaxMs;
  if (roundN_ == 0 || (roundN_ < kSamplesPerRound && !timeUp))
    return Step::Pending;
  lastRaw_ = static_cast<int32_t>(jobSum_ / static_cast<int64_t>(jobN_));
  hasRaw_ = true;
  // double: Rohwerte bis 2^23 brauchen mehr als die 24 Bit eines float
  const double mean = static_cast<double>(roundSum_) / roundN_;
  if (n > 0 && scale_ != 0)
    out[0].value = static_cast<float>((mean - tare_) / scale_);
  if (n > 1)
    out[1] = Reading{}; // Wasservorrat: Sensordienst
  return Step::Ok;
}

} // namespace drivers
} // namespace ps

#endif
