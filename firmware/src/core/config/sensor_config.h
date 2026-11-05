/**
 * @file sensor_config.h
 * @brief Sensor- und Kanalkonfiguration als POD (Größe aus dem Board-Profil)
 */
#pragma once

#include "board.h"
#include "core/sensors/catalog.h"

namespace ps {

struct ChannelConfig {
  bool enabled = true;
  bool display = true; ///< auf dem Display zeigen
  FixedString<25> name;
  FixedString<10> unit;
  FixedString<25> field;
  Limits limits;
  int16_t rawMin = 0;
  int16_t rawMax = 1023;
  bool inverted = false;
  bool autocal = false;
  uint32_t halfLifeS = 86400;
};

struct SensorConfig {
  bool enabled = true;
  uint16_t intervalS = 60;
  float scale = -7050.0f; ///< nur HX711
  int32_t tare = 0;       ///< nur HX711, persistenter Tara-Offset (B-21)
};

struct SensorsConfig {
  SensorConfig sensors[board::kMaxSensors];
  ChannelConfig channels[board::kMaxSlots];
};

/// Erlaubte Halbwertszeiten der Autokalibrierung in Sekunden.
constexpr uint32_t kHalfLives[] = {21600, 43200, 86400, 259200, 604800, 2592000};

inline bool halfLifeAllowed(uint32_t s) {
  for (uint32_t h : kHalfLives)
    if (h == s)
      return true;
  return false;
}

/// Standardwerte aus Katalog und Profil.
void sensorDefaults(SensorsConfig& c);

} // namespace ps
