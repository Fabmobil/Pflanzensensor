/**
 * @file analog_mux.h
 * @brief Analogeingang A0, optional über CD4051B (refactor.md 2.4)
 * @details Eine Runde liest alle Kanäle: Mux umschalten, Pins zurücklesen
 *          (einmal wiederholen), ~1 ms settle, ADC lesen. Der Treiber liefert
 *          Rohwerte; Prozent und Autokalibrierung rechnet SensorService.
 */
#pragma once

#include "core/sensors/driver.h"

namespace ps {
namespace drivers {

class AnalogMux : public SensorDriver {
public:
  static constexpr uint16_t kSettleUs = 1000;
  SensorType type() const override { return SensorType::Analog; }
  uint8_t channelCount() const override;
  Step begin(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;

private:
  bool select(uint8_t ch);
  bool ready_ = false;
};

} // namespace drivers
} // namespace ps
