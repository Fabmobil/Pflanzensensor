/**
 * @file ds18b20.cpp
 */
#if defined(ARDUINO)

#include "drivers/ds18b20.h"

#include <DallasTemperature.h>
#include <OneWire.h>

#include "board.h"
#include "services/log.h"

namespace ps {
namespace drivers {

namespace {
/// Erst beim ersten Gebrauch anlegen: der OneWire-Konstruktor fasst den Pin an
DallasTemperature& bus() {
  static OneWire wire(board::pins().oneWire);
  static DallasTemperature d(&wire);
  return d;
}
} // namespace

Step Ds18b20::begin(Instant) {
  bus().begin();
  const uint8_t n = bus().getDeviceCount();
  found_ = n > board::features().ds18b20Max ? board::features().ds18b20Max : n;
  if (!found_) {
    LOGW(Mod::Sensor, "Kein DS18B20 am Bus gefunden");
    return Step::Error;
  }
  bus().setResolution(10);
  bus().setWaitForConversion(false);
  LOGI(Mod::Sensor, "%u DS18B20 gefunden", found_);
  return Step::Ok;
}

Step Ds18b20::startRound(Instant now) {
  bus().requestTemperatures(); // eine Konversion für alle
  started_ = now;
  return Step::Ok;
}

Step Ds18b20::pollRound(Instant now, Reading* out, uint8_t n) {
  if ((now - started_).millis() < static_cast<int32_t>(kConversionMs) &&
      !bus().isConversionComplete())
    return Step::Pending;
  for (uint8_t i = 0; i < n && i < found_; i++) {
    const float t = bus().getTempCByIndex(i);
    if (t == DEVICE_DISCONNECTED_C || t < -55 || t > 125)
      continue;
    out[i].value = t;
  }
  return Step::Ok;
}

} // namespace drivers
} // namespace ps

#endif
