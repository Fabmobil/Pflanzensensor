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
  return Step::Ok;
}

Step Hx711::pollRound(Instant, Reading* out, uint8_t n) {
  if (!scale().is_ready())
    return Step::Pending;
  const long raw = scale().read(); // < 1 ms, Daten liegen bereit
  lastRaw_ = static_cast<int32_t>(raw);
  hasRaw_ = true;
  if (n > 0 && scale_ != 0)
    out[0].value = static_cast<float>(raw - tare_) / scale_;
  return Step::Ok;
}

} // namespace drivers
} // namespace ps

#endif
