/**
 * @file bmp280.cpp
 */
#if defined(ARDUINO)

#include "drivers/bmp280.h"

#include <Adafruit_BMP280.h>
#include <Wire.h>

#include "board.h"
#include "services/log.h"

namespace ps {
namespace drivers {

namespace {
Adafruit_BMP280& bmp() {
  static Adafruit_BMP280 b(&Wire);
  return b;
}
} // namespace

Step Bmp280::begin(Instant) {
  Wire.begin(board::kPins.sda, board::kPins.scl);
  if (!bmp().begin(board::kFeatures.bmp280Address)) {
    LOGW(Mod::Sensor, "BMP280 nicht gefunden (0x%02X)", board::kFeatures.bmp280Address);
    return Step::Error;
  }
  bmp().setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  return Step::Ok;
}

Step Bmp280::pollRound(Instant, Reading* out, uint8_t n) {
  const float t = bmp().readTemperature();
  const float p = bmp().readPressure() / 100.0f;
  if (n > 0 && !isnan(t) && t >= -40 && t <= 85)
    out[0].value = t;
  if (n > 1 && !isnan(p) && p >= 300 && p <= 1100)
    out[1].value = p;
  return Step::Ok;
}

} // namespace drivers
} // namespace ps

#endif
