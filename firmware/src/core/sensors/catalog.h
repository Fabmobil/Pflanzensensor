/**
 * @file catalog.h
 * @brief Die eine Quelle der Wahrheit für Sensortypen und Kanäle
 * @details Je Sensortyp eine Beschreibung (ID, Ressourcen, Takte) und je Kanal
 *          eine ChannelDef (Name, Einheit, Gültigkeit, Standardschwellen).
 *          Die Tabellen liegen im Flash; abgefragt wird per Kopie.
 */
#pragma once

#include <math.h>

#include "core/types.h"

namespace ps {

enum class SensorType : uint8_t {
  Analog = 0,
  Dht,
  Ds18b20,
  Sds011,
  Mhz19,
  Hx711,
  Bmp280,
  SerialReceiver,
  Count
};
constexpr uint8_t kSensorTypeCount = static_cast<uint8_t>(SensorType::Count);

/// Geteilte Hardware. Belegt nur während einer Sample-Runde.
enum Resource : uint8_t { ResNone = 0, ResAdc = 1, ResI2c = 2, ResOneWire = 4, ResSoftUart = 8 };

/// Schwellen: yellowLow ≤ greenLow ≤ greenHigh ≤ yellowHigh.
struct Limits {
  float yellowLow = 0, greenLow = 0, greenHigh = 0, yellowHigh = 0;
};

/// Wie die Chronik einen Kanalwert speichert (refactor.md 5.8).
enum class ChronikEncoding : uint8_t { Half = 0, Int16Scaled = 1, Float32 = 2 };

/// Kanalbeschreibung, im Flash.
struct ChannelDef {
  char field[20]; ///< Feldname ("lufttemperatur"), Prometheus und /getLatestValues
  char name[24];  ///< Standardname
  char unit[10];
  float validMin, validMax;
  Limits limits;
  bool oneSided; ///< nur obere Grenzen bewerten (Feinstaub)
  bool hasRaw;   ///< Analog: Rohwert führen
  // Nur Analog: Standard-Rohgrenzen und Invertierung
  int16_t rawMin, rawMax;
  bool inverted;
  ChronikEncoding encoding;
  float encScale, encOffset; ///< nur bei Int16Scaled
};

/// Sensortyp, im Flash.
struct SensorInfo {
  char id[16]; ///< "ANALOG", "DHT", …
  uint8_t resources;
  uint8_t maxChannels;
  uint16_t defaultIntervalS;
  uint32_t startWarmupMs;   ///< einmalig nach dem Start (MH-Z19)
  uint32_t prepareWarmupMs; ///< pro Messung nach wake() (SDS011)
  uint16_t roundDelayMs;
  uint16_t roundTimeoutMs;
  uint8_t rounds;
};

namespace catalog {

SensorInfo info(SensorType t);
ChannelDef channel(SensorType t, uint8_t index);
/// Sensor-ID als Flash-Text.
FStr id(SensorType t);
/// Sucht einen Typ anhand seiner ID ("ANALOG"). Count, wenn unbekannt.
SensorType typeFromId(const char* id, size_t len);
/// Feldname eines Kanals in einen Puffer.
void field(SensorType t, uint8_t index, char* out, size_t cap);

} // namespace catalog
} // namespace ps
