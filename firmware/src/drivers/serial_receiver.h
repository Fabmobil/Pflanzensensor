/**
 * @file serial_receiver.h
 * @brief Externer Mikrocontroller per Zeilenprotokoll ("GET:<idx>"), nicht blockierend
 */
#pragma once

#include "core/sensors/driver.h"
#include "core/sensors/protocols.h"

namespace ps {
namespace drivers {

class SerialReceiver : public SensorDriver {
public:
  SensorType type() const override { return SensorType::SerialReceiver; }
  SensorInfo info() const override;
  uint8_t channelCount() const override { return 7; }
  Step begin(Instant now) override;
  Step startRound(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;

private:
  void request(uint8_t idx, Instant now);
  proto::LineBuffer line_;
  uint8_t idx_ = 0;
  Instant sentAt_;
  bool begun_ = false;
};

} // namespace drivers
} // namespace ps
