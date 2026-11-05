/**
 * @file esp32_devkit.h
 * @brief Referenzprofil ESP32-DevKit (WROOM-32)
 * @details Dieselbe Peripherie wie das PCB, aber auf ESP32-Pins. ADC1-Pin
 *          (GPIO34), weil ADC2 bei aktivem WLAN nicht nutzbar ist. Der ESP32
 *          misst 12 Bit; der Treiber skaliert auf 0..1023, damit Grenzen und
 *          Kalibrierung zwischen den Plattformen übertragbar bleiben.
 */
#pragma once

#include "core/board_types.h"

namespace board {

constexpr ps::Platform kPlatform = ps::Platform::Esp32;
constexpr char kName[] = "esp32_devkit";
constexpr char kPlatformName[] = "ESP32";

constexpr ps::Pins kPins = [] {
  ps::Pins p;
  p.adc = 34;
  p.muxA = 25;
  p.muxB = 26;
  p.muxC = 27;
  p.dht = 4;
  p.ledGreen = 16;
  p.ledYellow = 17;
  p.ledRed = 18;
  p.sda = 21;
  p.scl = 22;
  return p;
}();

constexpr ps::Features kFeatures = [] {
  ps::Features f;
  f.analog = true;
  f.analogChannels = 2;
  f.multiplexer = true;
  f.dht = true;
  f.dhtType = ps::DhtType::Dht11;
  f.display = true;
  f.ledTraffic = true;
  return f;
}();

constexpr ps::StaticIp kStaticIp{};

} // namespace board
