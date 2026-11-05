/**
 * @file board.h
 * @brief Bindet das gewählte Board-Profil ein und leitet das Kanal-Layout ab
 * @details BOARD_PROFILE kommt aus platformio.ini. Aus den Features entstehen
 *          zur Compile-Zeit: die Liste der Sensoren (feste Reihenfolge), die
 *          Kanalplätze (Slots) und deren Anzahl. Ein Slot ist die stabile
 *          Kanalnummer, die auch die Chronik verwendet (refactor.md 5.8).
 */
#pragma once

#ifndef BOARD_PROFILE
#error "BOARD_PROFILE ist nicht gesetzt (platformio.ini)"
#endif
#include BOARD_PROFILE

#include <array>

#include "core/sensors/catalog.h"
#include "profile_check.h"

namespace board {

constexpr uint8_t channelsOf(ps::SensorType t) {
  switch (t) {
  case ps::SensorType::Analog:
    return kFeatures.analog ? kFeatures.analogChannels : 0;
  case ps::SensorType::Dht:
    return kFeatures.dht ? 2 : 0;
  case ps::SensorType::Ds18b20:
    return kFeatures.ds18b20 ? kFeatures.ds18b20Max : 0;
  case ps::SensorType::Sds011:
    return kFeatures.sds011 ? 2 : 0;
  case ps::SensorType::Mhz19:
    return kFeatures.mhz19 ? 1 : 0;
  case ps::SensorType::Hx711:
    return kFeatures.hx711 ? 1 : 0;
  case ps::SensorType::Bmp280:
    return kFeatures.bmp280 ? 2 : 0;
  case ps::SensorType::SerialReceiver:
    return kFeatures.serialReceiver ? 7 : 0;
  default:
    return 0;
  }
}

constexpr uint8_t countSensors() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < ps::kSensorTypeCount; i++)
    if (channelsOf(static_cast<ps::SensorType>(i)) > 0)
      n++;
  return n;
}

constexpr uint8_t countSlots() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < ps::kSensorTypeCount; i++)
    n += channelsOf(static_cast<ps::SensorType>(i));
  return n;
}

constexpr uint8_t maxChannelsPerSensor() {
  uint8_t m = 1;
  for (uint8_t i = 0; i < ps::kSensorTypeCount; i++) {
    const uint8_t c = channelsOf(static_cast<ps::SensorType>(i));
    if (c > m)
      m = c;
  }
  return m;
}

/// Anzahl Sensoren dieses Boards
constexpr uint8_t kSensorCount = countSensors();
/// Anzahl Kanalplätze dieses Boards
constexpr uint8_t kSlotCount = countSlots();
constexpr uint8_t kMaxChannelsPerSensor = maxChannelsPerSensor();

static_assert(kSensorCount >= 1, "Das Profil hat keinen Sensor");
static_assert(kSlotCount <= 32, "Mehr als 32 Kanalplätze sind nicht vorgesehen");

struct SensorEntry {
  ps::SensorType type;
  uint8_t firstSlot;
  uint8_t channels;
};

struct SlotEntry {
  uint8_t sensor;  ///< Index in kSensors
  uint8_t channel; ///< Kanalindex innerhalb des Sensors
};

constexpr std::array<SensorEntry, kSensorCount> makeSensors() {
  std::array<SensorEntry, kSensorCount> a{};
  uint8_t n = 0, slot = 0;
  for (uint8_t i = 0; i < ps::kSensorTypeCount; i++) {
    const auto t = static_cast<ps::SensorType>(i);
    const uint8_t c = channelsOf(t);
    if (c == 0)
      continue;
    a[n] = SensorEntry{t, slot, c};
    slot = static_cast<uint8_t>(slot + c);
    n++;
  }
  return a;
}

constexpr std::array<SlotEntry, kSlotCount> makeSlots() {
  std::array<SlotEntry, kSlotCount> a{};
  uint8_t slot = 0, sensor = 0;
  for (uint8_t i = 0; i < ps::kSensorTypeCount; i++) {
    const uint8_t c = channelsOf(static_cast<ps::SensorType>(i));
    if (c == 0)
      continue;
    for (uint8_t ch = 0; ch < c; ch++)
      a[slot++] = SlotEntry{sensor, ch};
    sensor++;
  }
  return a;
}

constexpr auto kSensors = makeSensors();
constexpr auto kSlots = makeSlots();

/// Index des Sensors eines Typs oder -1
constexpr int sensorIndexOf(ps::SensorType t) {
  for (uint8_t i = 0; i < kSensorCount; i++)
    if (kSensors[i].type == t)
      return i;
  return -1;
}

} // namespace board
