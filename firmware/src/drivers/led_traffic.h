/**
 * @file led_traffic.h
 * @brief LED-Ampel an drei GPIOs (aktiv HIGH)
 */
#pragma once

#include "services/led_service.h"

namespace ps {
namespace drivers {

class LedTraffic : public LedPort {
public:
  void begin();
  void set(bool green, bool yellow, bool red) override;

private:
  bool ready_ = false;
};

} // namespace drivers
} // namespace ps
