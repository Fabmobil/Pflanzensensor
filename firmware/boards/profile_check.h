/**
 * @file profile_check.h
 * @brief Compile-Zeit-Prüfungen des Board-Profils
 * @details Bricht den Build ab bei doppelt belegten Pins, unzulässig genutzten
 *          Strapping-Pins und unmöglichen Kombinationen. Eingebunden aus
 *          board.h, nachdem das Profil geladen ist.
 */
#pragma once

#include <stdint.h>

namespace board {
namespace check {

struct PinUse {
  int8_t pin;
  bool pullupInput; ///< Eingang mit Pull-up (DHT, OneWire, UART-RX …)
};

constexpr PinUse kUses[] = {
    {kFeatures.analog ? kPins.adc : int8_t(-1), false},
    {(kFeatures.analog && kFeatures.multiplexer) ? kPins.muxA : int8_t(-1), false},
    {(kFeatures.analog && kFeatures.multiplexer) ? kPins.muxB : int8_t(-1), false},
    {(kFeatures.analog && kFeatures.multiplexer) ? kPins.muxC : int8_t(-1), false},
    {kFeatures.dht ? kPins.dht : int8_t(-1), true},
    {kFeatures.ledTraffic ? kPins.ledGreen : int8_t(-1), false},
    {kFeatures.ledTraffic ? kPins.ledYellow : int8_t(-1), false},
    {kFeatures.ledTraffic ? kPins.ledRed : int8_t(-1), false},
    {(kFeatures.display || kFeatures.bmp280) ? kPins.sda : int8_t(-1), true},
    {(kFeatures.display || kFeatures.bmp280) ? kPins.scl : int8_t(-1), true},
    {kFeatures.ds18b20 ? kPins.oneWire : int8_t(-1), true},
    {kFeatures.sds011 ? kPins.sdsRx : int8_t(-1), true},
    {kFeatures.sds011 ? kPins.sdsTx : int8_t(-1), false},
    {kFeatures.mhz19 ? kPins.mhz19Pwm : int8_t(-1), true},
    {kFeatures.hx711 ? kPins.hxSck : int8_t(-1), false},
    {kFeatures.hx711 ? kPins.hxDout : int8_t(-1), true},
    {kFeatures.serialReceiver ? kPins.serialRx : int8_t(-1), true},
    {kFeatures.serialReceiver ? kPins.serialTx : int8_t(-1), false},
};
constexpr int kUseCount = sizeof(kUses) / sizeof(kUses[0]);

constexpr bool noDuplicatePins() {
  for (int i = 0; i < kUseCount; i++) {
    if (kUses[i].pin < 0)
      continue;
    for (int j = i + 1; j < kUseCount; j++)
      if (kUses[i].pin == kUses[j].pin)
        return false;
  }
  return true;
}

constexpr bool strappingOk() {
  for (int i = 0; i < kUseCount; i++) {
    const int8_t p = kUses[i].pin;
    if (p < 0)
      continue;
    if (kPlatform == ps::Platform::Esp8266) {
      // GPIO15 muss beim Booten LOW sein: kein Eingang mit Pull-up.
      if (p == 15 && kUses[i].pullupInput)
        return false;
      // GPIO6..11 hängen am Flash.
      if (p >= 6 && p <= 11)
        return false;
    }
    if (kPlatform == ps::Platform::Esp32) {
      if (p >= 6 && p <= 11)
        return false; // Flash
      if (p >= 34 && !kUses[i].pullupInput) {
        // 34..39 sind reine Eingänge; als Ausgang unzulässig (ADC ist ok).
        if (p != kPins.adc)
          return false;
      }
    }
  }
  return true;
}

static_assert(noDuplicatePins(), "Board-Profil: ein Pin ist doppelt belegt");
static_assert(strappingOk(), "Board-Profil: Strapping- oder Flash-Pin unzulässig genutzt");
static_assert(!kFeatures.analog || (kFeatures.analogChannels >= 1 && kFeatures.analogChannels <= 8),
              "Board-Profil: analogChannels muss 1..8 sein");
static_assert(!kFeatures.analog || kFeatures.analogChannels <= 1 || kFeatures.multiplexer,
              "Board-Profil: mehr als ein Analogkanal braucht den Multiplexer");
static_assert(!kFeatures.ds18b20 || (kFeatures.ds18b20Max >= 1 && kFeatures.ds18b20Max <= 8),
              "Board-Profil: ds18b20Max muss 1..8 sein");
static_assert(!kFeatures.mhz19 || kFeatures.mhz19Range == 2000 || kFeatures.mhz19Range == 5000,
              "Board-Profil: mhz19Range muss 2000 oder 5000 sein");

} // namespace check
} // namespace board
