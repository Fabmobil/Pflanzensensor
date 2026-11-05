/**
 * @file mhz19.h
 * @brief MH-Z19 CO₂ über den PWM-Ausgang, per Flankeninterrupt (B-17)
 * @details Die ISR misst High- und Low-Zeit dauerhaft; pollRound() wartet auf
 *          den nächsten vollständigen Zyklus (~1 s), ohne zu blockieren.
 */
#pragma once

#include "core/sensors/driver.h"

namespace ps {
namespace drivers {

class Mhz19 : public SensorDriver {
public:
  SensorType type() const override { return SensorType::Mhz19; }
  uint8_t channelCount() const override { return 1; }
  Step begin(Instant now) override;
  Step startRound(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;

private:
  uint32_t startCycle_ = 0;
  bool attached_ = false;
};

} // namespace drivers
} // namespace ps
