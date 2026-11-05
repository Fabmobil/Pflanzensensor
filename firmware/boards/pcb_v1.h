/**
 * @file pcb_v1.h
 * @brief Board-Profil: eigenes Pflanzensensor-PCB mit NodeMCU v2 (ESP8266)
 * @details Schaltplan: Dokumentation/Pflanzensensor PCB/Pflanzensensor_Schematic.pdf
 *          (Stand 2024-06-06). CD4051B-Multiplexer, DHT11, Fotowiderstand,
 *          LED-Ampel, SSD1306 an I²C, drei Bodenfeuchte-Stecker.
 *
 *          Optionale Sensoren teilen sich Pins mit LEDs bzw. I²C (siehe
 *          refactor.md 2.1). Wer einen davon bestückt, legt ein eigenes Profil
 *          an und schaltet die kollidierende Peripherie ab - profile_check.h
 *          bricht den Build sonst ab.
 */
#pragma once

#include "core/board_types.h"

namespace board {

constexpr ps::Platform kPlatform = ps::Platform::Esp8266;
constexpr char kName[] = "pcb_v1";
constexpr char kPlatformName[] = "ESP8266";

constexpr ps::Pins kPins = [] {
  ps::Pins p;
  p.adc = 17;  // A0
  p.muxA = 15; // LSB, Strapping-Pin (beim Booten LOW) - als Ausgang unkritisch
  p.muxB = 2;  // Strapping-Pin (beim Booten HIGH) - als Ausgang unkritisch
  p.muxC = 16; // MSB
  p.dht = 0;   // D3, Strapping-Pin mit 10-kOhm-Pull-up auf dem PCB
  p.ledGreen = 14;
  p.ledYellow = 12;
  p.ledRed = 13;
  p.sda = 4;
  p.scl = 5;
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
  f.displayAddress = 0x3C;
  f.ledTraffic = true;
  return f;
}();

constexpr ps::StaticIp kStaticIp{};

} // namespace board
