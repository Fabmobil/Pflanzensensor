/**
 * @file board_types.h
 * @brief Typen, mit denen ein Board-Profil beschrieben wird
 * @details Arduino-frei: auch das native Testprofil benutzt sie.
 */
#pragma once

#include <stdint.h>

namespace ps {

enum class Platform : uint8_t { Esp8266, Esp32, Native };
enum class DhtType : uint8_t { Dht11 = 11, Dht22 = 22 };

/// Hardware des Boards. Pins mit -1 sind nicht belegt.
struct Pins {
  int8_t adc = -1;
  int8_t muxA = -1, muxB = -1, muxC = -1;
  int8_t dht = -1;
  int8_t ledGreen = -1, ledYellow = -1, ledRed = -1;
  int8_t sda = -1, scl = -1;
  int8_t oneWire = -1;
  int8_t sdsRx = -1, sdsTx = -1;
  int8_t mhz19Pwm = -1;
  int8_t hxSck = -1, hxDout = -1;
  int8_t serialRx = -1, serialTx = -1;
};

/// Welche Peripherie ist bestückt? Daraus folgen Sensoren und Kanäle.
struct Features {
  bool analog = false;
  uint8_t analogChannels = 0; ///< 1..8, über 1 nur mit Multiplexer
  bool multiplexer = false;
  bool dht = false;
  DhtType dhtType = DhtType::Dht11;
  bool ds18b20 = false;
  uint8_t ds18b20Max = 0; ///< höchstens so viele Fühler am Bus (1..8)
  bool sds011 = false;
  bool mhz19 = false;
  uint16_t mhz19Range = 5000; ///< 2000 oder 5000 ppm (Befund B-20)
  bool hx711 = false;
  bool bmp280 = false;
  uint8_t bmp280Address = 0x76;
  bool serialReceiver = false;
  uint32_t serialBaud = 9600;
  uint16_t serialTimeoutMs = 4000;
  bool display = false;
  uint8_t displayAddress = 0x3C;
  bool ledTraffic = false;
};

/// Optionale feste IP-Adresse (Build-Profil, wie im Altcode).
struct StaticIp {
  bool enabled = false;
  uint8_t ip[4] = {0, 0, 0, 0};
  uint8_t gateway[4] = {0, 0, 0, 0};
  uint8_t subnet[4] = {255, 255, 255, 0};
  uint8_t dns1[4] = {0, 0, 0, 0};
  uint8_t dns2[4] = {0, 0, 0, 0};
};

} // namespace ps
