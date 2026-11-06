/**
 * @file board.h
 * @brief Board-Profil (Voreinstellung) und das Kanal-Layout zur Laufzeit
 * @details BOARD_PROFILE aus platformio.ini liefert nur noch die
 *          Voreinstellung (namespace board_profile). Welche Sensoren an welchen
 *          Pins hängen, steht zur Laufzeit in board::hardware(): beim Start aus
 *          dem Provisionierungsblock des Webflashers, sonst aus dem Profil.
 *
 *          Daraus entstehen die Liste der Sensoren (feste Reihenfolge nach Typ)
 *          und die Kanalplätze (Slots). Arrays werden mit den Höchstwerten
 *          kMaxSensors / kMaxSlots angelegt; Schleifen laufen über
 *          sensorCount() / slotCount(). Ein Slot ist nur bei gleicher Hardware
 *          stabil - Konfiguration und Chronik speichern deshalb Schlüssel
 *          (z.B. "ANALOG_1"), nie Slotnummern.
 */
#pragma once

#ifndef BOARD_PROFILE
#error "BOARD_PROFILE ist nicht gesetzt (platformio.ini)"
#endif
#include BOARD_PROFILE

#include "core/hardware_check.h"
#include "core/sensors/catalog.h"
#include "profile_check.h"

namespace board {

/// Plattform des Builds (aus dem Profil)
constexpr ps::Platform kPlatform = board_profile::kPlatform;
constexpr const char* kPlatformName = board_profile::kPlatformName;

/// Höchstwerte je Sensortyp (Kanäle)
constexpr uint8_t maxChannelsOf(ps::SensorType t) {
  switch (t) {
  case ps::SensorType::Analog:
    return 8;
  case ps::SensorType::Dht:
    return 2;
  case ps::SensorType::Ds18b20:
    return 8;
  case ps::SensorType::Sds011:
    return 2;
  case ps::SensorType::Mhz19:
    return 1;
  case ps::SensorType::Hx711:
    return 2;
  case ps::SensorType::Bmp280:
    return 2;
  case ps::SensorType::SerialReceiver:
    return 7;
  default:
    return 0;
  }
}

/// Kanäle eines Sensortyps bei dieser Hardware (0 = nicht bestückt)
constexpr uint8_t channelsOf(const ps::Features& f, ps::SensorType t) {
  switch (t) {
  case ps::SensorType::Analog:
    return f.analog ? f.analogChannels : 0;
  case ps::SensorType::Dht:
    return f.dht ? 2 : 0;
  case ps::SensorType::Ds18b20:
    return f.ds18b20 ? f.ds18b20Max : 0;
  case ps::SensorType::Sds011:
    return f.sds011 ? 2 : 0;
  case ps::SensorType::Mhz19:
    return f.mhz19 ? 1 : 0;
  case ps::SensorType::Hx711:
    return f.hx711 ? 2 : 0; // Gewicht, Wasservorrat
  case ps::SensorType::Bmp280:
    return f.bmp280 ? 2 : 0;
  case ps::SensorType::SerialReceiver:
    return f.serialReceiver ? 7 : 0;
  default:
    return 0;
  }
}

constexpr uint8_t sumMaxSlots() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < ps::kSensorTypeCount; i++)
    n = static_cast<uint8_t>(n + maxChannelsOf(static_cast<ps::SensorType>(i)));
  return n;
}

constexpr uint8_t kMaxSensors = ps::kSensorTypeCount;
/// Höchstens so viele Kanäle gleichzeitig (RAM: Konfiguration, Zustand und
/// Rundenpuffer je Kanal). Mehr lehnt checkHardware() ab (TooManyChannels).
constexpr uint8_t kMaxSlots = ps::maxHardwareChannels(kPlatform);
constexpr uint8_t kMaxChannelsPerSensor = 8;
static_assert(kMaxSlots <= sumMaxSlots(), "kMaxSlots größer als alle Sensoren zusammen");
static_assert(kMaxSlots <= 32, "Mehr als 32 Kanalplätze sind nicht vorgesehen");

struct SensorEntry {
  ps::SensorType type;
  uint8_t firstSlot;
  uint8_t channels;
};

struct SlotEntry {
  uint8_t sensor;  ///< Index in der Sensorliste
  uint8_t channel; ///< Kanalindex innerhalb des Sensors
};

/// Voreinstellung aus dem Board-Profil
ps::Hardware defaultHardware();
/// Hardware setzen (beim Start, Tests). Ungültige Hardware wird abgelehnt:
/// dann bleibt die bisherige und das Ergebnis nennt das Problem.
ps::HwCheckResult setHardware(const ps::Hardware& hw);

const ps::Hardware& hardware();
inline const ps::Features& features() { return hardware().features; }
inline const ps::Pins& pins() { return hardware().pins; }
inline const char* name() { return hardware().name; }

uint8_t sensorCount();
uint8_t slotCount();
const SensorEntry& sensor(uint8_t i);
const SlotEntry& slot(uint8_t s);
/// Index des Sensors eines Typs oder -1
int sensorIndexOf(ps::SensorType t);

} // namespace board
