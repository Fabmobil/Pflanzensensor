/**
 * @file bmp280.h
 * @brief BMP280 Temperatur und Luftdruck an I²C (0x76)
 */
#pragma once

#include "core/sensors/driver.h"

namespace ps {
namespace drivers {

class Bmp280 : public SensorDriver {
public:
  SensorType type() const override { return SensorType::Bmp280; }
  uint8_t channelCount() const override { return 2; }
  Step begin(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;
};

} // namespace drivers
} // namespace ps
