/**
 * @file hardware_check.h
 * @brief Prüfung einer Hardware-Beschreibung (Sensoren und Pins)
 * @details constexpr: Dieselbe Funktion prüft beim Bauen die Voreinstellung
 *          des Profils (profile_check.h) und zur Laufzeit die Hardware aus
 *          dem Webflasher. Der Webflasher hat dieselben Regeln in JavaScript
 *          (webflasher/hardware.js); gemeinsame Testvektoren:
 *          test/vectors/hardware_check.json.
 */
#pragma once

#include <stdint.h>

#include "core/board_types.h"

namespace ps {

enum class HwProblem : uint8_t {
  None = 0,
  DuplicatePin,   ///< ein Pin ist doppelt belegt
  MissingPin,     ///< bestückte Peripherie ohne Pin
  NoSuchPin,      ///< Pin gibt es auf dem Chip nicht
  FlashPin,       ///< GPIO6..11 hängen am Flash
  StrapPin,       ///< ESP8266 GPIO15 als Eingang mit Pull-up
  InputOnlyPin,   ///< ESP32 GPIO34..39 als Ausgang/Bus
  AdcPin,         ///< Pin kann nicht analog messen
  AnalogChannels, ///< analogChannels nicht 1..8
  NeedMultiplexer,
  Ds18b20Count,   ///< ds18b20Max nicht 1..8
  Mhz19Range,     ///< nicht 2000 oder 5000
  NoSensor,       ///< kein einziger Sensor bestückt
  TooManyChannels ///< mehr Kanäle als maxHardwareChannels()
};

/// Höchstens so viele Messkanäle gleichzeitig: Der ESP8266 hat für
/// Konfiguration, Zustand und Rundenpuffer je Kanal nur Platz für 16.
constexpr uint8_t maxHardwareChannels(Platform pl) { return pl == Platform::Esp8266 ? 16 : 31; }

struct HwCheckResult {
  HwProblem problem = HwProblem::None;
  int8_t pin = -1; ///< betroffener Pin, falls einer
};

namespace hwcheck {

struct PinUse {
  int8_t pin;
  bool used;
  bool pullupInput; ///< Eingang mit Pull-up (DHT, OneWire, UART-RX, I²C …)
  bool output;      ///< Ausgang oder Bus (I²C, UART-TX)
  bool adc;
};

constexpr uint8_t kUses = 18;

constexpr void uses(const Features& f, const Pins& p, PinUse (&u)[kUses]) {
  const bool mux = f.analog && f.multiplexer;
  const bool i2c = f.display || f.bmp280;
  u[0] = {p.adc, f.analog, false, false, true};
  u[1] = {p.muxA, mux, false, true, false};
  u[2] = {p.muxB, mux, false, true, false};
  u[3] = {p.muxC, mux, false, true, false};
  u[4] = {p.dht, f.dht, true, true, false};
  u[5] = {p.ledGreen, f.ledTraffic, false, true, false};
  u[6] = {p.ledYellow, f.ledTraffic, false, true, false};
  u[7] = {p.ledRed, f.ledTraffic, false, true, false};
  u[8] = {p.sda, i2c, true, true, false};
  u[9] = {p.scl, i2c, true, true, false};
  u[10] = {p.oneWire, f.ds18b20, true, true, false};
  u[11] = {p.sdsRx, f.sds011, true, false, false};
  u[12] = {p.sdsTx, f.sds011, false, true, false};
  u[13] = {p.mhz19Pwm, f.mhz19, true, false, false};
  u[14] = {p.hxSck, f.hx711, false, true, false};
  u[15] = {p.hxDout, f.hx711, true, false, false};
  u[16] = {p.serialRx, f.serialReceiver, true, false, false};
  u[17] = {p.serialTx, f.serialReceiver, false, true, false};
}

constexpr bool exists(Platform pl, int8_t pin) {
  if (pl == Platform::Esp8266)
    return (pin >= 0 && pin <= 16) || pin == 17; // 17 = A0
  if (pl == Platform::Esp32)
    return (pin >= 0 && pin <= 39) && pin != 20 && pin != 24 && !(pin >= 28 && pin <= 31);
  return pin >= 0; // native: Fantasiepins
}

constexpr bool adcCapable(Platform pl, int8_t pin) {
  if (pl == Platform::Esp8266)
    return pin == 17;
  if (pl == Platform::Esp32)
    return pin >= 32 && pin <= 39; // nur ADC1, ADC2 ist mit WLAN belegt
  return true;
}

} // namespace hwcheck

constexpr HwCheckResult checkHardware(Platform pl, const Features& f, const Pins& p) {
  if (!f.analog && !f.dht && !f.ds18b20 && !f.sds011 && !f.mhz19 && !f.hx711 && !f.bmp280 &&
      !f.serialReceiver)
    return {HwProblem::NoSensor, -1};
  if (f.analog && (f.analogChannels < 1 || f.analogChannels > 8))
    return {HwProblem::AnalogChannels, -1};
  if (f.analog && f.analogChannels > 1 && !f.multiplexer)
    return {HwProblem::NeedMultiplexer, -1};
  if (f.ds18b20 && (f.ds18b20Max < 1 || f.ds18b20Max > 8))
    return {HwProblem::Ds18b20Count, -1};
  if (f.mhz19 && f.mhz19Range != 2000 && f.mhz19Range != 5000)
    return {HwProblem::Mhz19Range, -1};
  {
    const int n = (f.analog ? f.analogChannels : 0) + (f.dht ? 2 : 0) +
                  (f.ds18b20 ? f.ds18b20Max : 0) + (f.sds011 ? 2 : 0) + (f.mhz19 ? 1 : 0) +
                  (f.hx711 ? 1 : 0) + (f.bmp280 ? 2 : 0) + (f.serialReceiver ? 7 : 0);
    if (n > maxHardwareChannels(pl))
      return {HwProblem::TooManyChannels, -1};
  }
  hwcheck::PinUse u[hwcheck::kUses] = {};
  hwcheck::uses(f, p, u);
  for (uint8_t i = 0; i < hwcheck::kUses; i++) {
    if (!u[i].used)
      continue;
    const int8_t pin = u[i].pin;
    if (pin < 0)
      return {HwProblem::MissingPin, -1};
    if (!hwcheck::exists(pl, pin))
      return {HwProblem::NoSuchPin, pin};
    if (pl != Platform::Native && pin >= 6 && pin <= 11)
      return {HwProblem::FlashPin, pin};
    if (u[i].adc && !hwcheck::adcCapable(pl, pin))
      return {HwProblem::AdcPin, pin};
    if (pl == Platform::Esp8266 && !u[i].adc && pin == 17)
      return {HwProblem::AdcPin, pin}; // A0 ist kein GPIO
    if (pl == Platform::Esp8266 && pin == 15 && u[i].pullupInput)
      return {HwProblem::StrapPin, pin}; // muss beim Booten LOW sein
    if (pl == Platform::Esp32 && pin >= 34 && u[i].output)
      return {HwProblem::InputOnlyPin, pin};
    for (uint8_t j = static_cast<uint8_t>(i + 1); j < hwcheck::kUses; j++)
      if (u[j].used && u[j].pin == pin)
        return {HwProblem::DuplicatePin, pin};
  }
  return {};
}

/// Kurzer Code für Log und API ("duplicate_pin" …)
const char* hwProblemCode(HwProblem p);

} // namespace ps
