/**
 * @file led_traffic.cpp
 */
#if defined(ARDUINO)

#include "drivers/led_traffic.h"

#include <Arduino.h>

#include "board.h"

namespace ps {
namespace drivers {

void LedTraffic::begin() {
  if (!board::kFeatures.ledTraffic)
    return;
  pinMode(board::kPins.ledGreen, OUTPUT);
  pinMode(board::kPins.ledYellow, OUTPUT);
  pinMode(board::kPins.ledRed, OUTPUT);
  ready_ = true;
  set(false, false, false);
}

void LedTraffic::set(bool g, bool y, bool r) {
  if (!ready_)
    return;
  digitalWrite(board::kPins.ledGreen, g ? HIGH : LOW);
  digitalWrite(board::kPins.ledYellow, y ? HIGH : LOW);
  digitalWrite(board::kPins.ledRed, r ? HIGH : LOW);
}

} // namespace drivers
} // namespace ps

#endif
