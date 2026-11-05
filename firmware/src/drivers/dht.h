/**
 * @file dht.h
 * @brief DHT11/DHT22: eine Lesung pro Runde liefert beide Kanäle
 * @details Eigenes Protokoll statt DHTesp: Die Bibliothek wartet für das
 *          Startsignal des DHT11 mit delay(18) - auf dem ESP8266 wurden daraus
 *          mit WLAN-Last 35-40 ms Schleifenzeit. Hier zieht pollRound() die
 *          Leitung auf LOW und kehrt zurück; ein späterer Aufruf liest ab 20 ms
 *          (nur der eigentliche Rahmen, ~5 ms ohne Interrupts). Der DHT22
 *          verträgt höchstens 20 ms LOW und bekommt sein 1,1-ms-Startsignal
 *          direkt vor dem Lesen.
 */
#pragma once

#include "core/sensors/driver.h"

namespace ps {
namespace drivers {

class Dht : public SensorDriver {
public:
  SensorType type() const override { return SensorType::Dht; }
  SensorInfo info() const override;
  uint8_t channelCount() const override { return 2; }
  Step begin(Instant now) override;
  Step startRound(Instant now) override;
  Step pollRound(Instant now, Reading* out, uint8_t n) override;
  void sleep() override;

private:
  enum class State : uint8_t { Idle, WaitLow, Low, Recover };
  bool setup_ = false;
  Instant setupAt_;
  State state_ = State::Idle;
  Instant lowSince_;
  bool haveLastRead_ = false;
  Instant lastRead_;
};

} // namespace drivers
} // namespace ps
