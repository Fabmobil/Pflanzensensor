/**
 * @file ds18b20.h
 * @brief DS18B20 am OneWire-Bus: eine Konversion für alle Fühler (B-18)
 */
#pragma once

#include "core/sensors/driver.h"

namespace ps {
namespace drivers {

class Ds18b20 : public SensorDriver {
public:
  static constexpr uint32_t kConversionMs = 750;
  SensorType type() const override { return SensorType::Ds18b20; }
  uint8_t channelCount() const override { return found_; }
  Step begin(Instant now) override;
  Step startRound(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;

private:
  uint8_t found_ = 0;
  Instant started_;
};

} // namespace drivers
} // namespace ps
