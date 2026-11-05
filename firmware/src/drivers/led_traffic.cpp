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
  if (!board::features().ledTraffic)
    return;
  pinMode(board::pins().ledGreen, OUTPUT);
  pinMode(board::pins().ledYellow, OUTPUT);
  pinMode(board::pins().ledRed, OUTPUT);
  ready_ = true;
  set(false, false, false);
}

void LedTraffic::set(bool g, bool y, bool r) {
  if (!ready_)
    return;
  digitalWrite(board::pins().ledGreen, g ? HIGH : LOW);
  digitalWrite(board::pins().ledYellow, y ? HIGH : LOW);
  digitalWrite(board::pins().ledRed, r ? HIGH : LOW);
}

} // namespace drivers
} // namespace ps

#endif
