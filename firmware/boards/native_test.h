/**
 * @file native_test.h
 * @brief Profil für die nativen Tests: alle Sensortypen aktiv
 * @details Damit laufen alle Pfade (auch über 16 Kanäle) durch die Tests.
 *          Pins sind Fantasiewerte ohne Bedeutung.
 */
#pragma once

#include "core/board_types.h"

namespace board {

constexpr ps::Platform kPlatform = ps::Platform::Native;
constexpr char kName[] = "native";
constexpr char kPlatformName[] = "native";

constexpr ps::Pins kPins = [] {
  ps::Pins p;
  p.adc = 100;
  p.muxA = 1;
  p.muxB = 2;
  p.muxC = 3;
  p.dht = 4;
  p.ledGreen = 5;
  p.ledYellow = 6;
  p.ledRed = 7;
  p.sda = 8;
  p.scl = 9;
  p.oneWire = 10;
  p.sdsRx = 11;
  p.sdsTx = 12;
  p.mhz19Pwm = 13;
  p.hxSck = 14;
  p.hxDout = 15;
  p.serialRx = 16;
  p.serialTx = 17;
  return p;
}();

constexpr ps::Features kFeatures = [] {
  ps::Features f;
  f.analog = true;
  f.analogChannels = 2;
  f.multiplexer = true;
  f.dht = true;
  f.ds18b20 = true;
  f.ds18b20Max = 2;
  f.sds011 = true;
  f.mhz19 = true;
  f.mhz19Range = 5000;
  f.hx711 = true;
  f.bmp280 = true;
  f.serialReceiver = true;
  f.display = true;
  f.ledTraffic = true;
  return f;
}();

constexpr ps::StaticIp kStaticIp{};

} // namespace board
