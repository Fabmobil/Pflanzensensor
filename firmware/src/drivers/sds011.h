/**
 * @file sds011.h
 * @brief SDS011 Feinstaub: wecken → 30 s Warmup → 3 Runden → schlafen (B-19)
 */
#pragma once

#include "core/sensors/driver.h"
#include "core/sensors/protocols.h"

namespace ps {
namespace drivers {

class Sds011 : public SensorDriver {
public:
  SensorType type() const override { return SensorType::Sds011; }
  uint8_t channelCount() const override { return 2; }
  Step begin(Instant now) override;
  Step wake(Instant now) override;
  Step startRound(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;
  void sleep() override;

private:
  void send(const uint8_t* cmd);
  proto::SdsParser parser_;
  bool begun_ = false;
};

} // namespace drivers
} // namespace ps
