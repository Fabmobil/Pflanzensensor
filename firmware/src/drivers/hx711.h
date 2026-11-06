/**
 * @file hx711.h
 * @brief Wägezelle HX711 mit persistentem Tara-Offset (B-21)
 */
#pragma once

#include "core/sensors/driver.h"

namespace ps {
namespace drivers {

class Hx711 : public SensorDriver {
public:
  SensorType type() const override { return SensorType::Hx711; }
  uint8_t channelCount() const override { return 1; }
  Step begin(Instant now) override;
  Step wake(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;
  // Kein sleep(): Der HX711 bleibt dauerhaft an. Mit power_down() zwischen
  // den Messungen schwankte der Wert von Messung zu Messung um einige Gramm
  // (vermutlich braucht die Messbrücke nach dem Einschalten länger als eine
  // Messung, bis sie stabil ist). Kostet am Netzteil nur ein paar mA.
  void configure(const SensorConfig& c) override {
    scale_ = c.scale;
    tare_ = c.tare;
  }
  bool lastRawCounts(int32_t& out) const override {
    if (!hasRaw_)
      return false;
    out = lastRaw_;
    return true;
  }

private:
  float scale_ = -7050.0f;
  int32_t tare_ = 0;
  int32_t lastRaw_ = 0;
  bool hasRaw_ = false;
  bool begun_ = false;
};

} // namespace drivers
} // namespace ps
