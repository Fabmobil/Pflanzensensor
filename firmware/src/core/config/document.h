/**
 * @file document.h
 * @brief JSON-Dokumente der Konfiguration (Dateien, API, Export)
 * @details Formate (alle mit "schema": 1):
 *   device.cfg:  {"schema":1,"general":{…},"wifi":{…},"display":{…},…}
 *   sensors.cfg: {"schema":1,"sensors":{"ANALOG":{"enabled":…,"interval":…,
 *                  "channels":{"0":{…},"1":{…}}},…}}
 *   secrets.cfg: {"schema":1,"wifi":["…","…","…"],"admin":{"salt":"hex","hash":"hex"},
 *                  "mailKey":"…"}
 */
#pragma once

#include <ArduinoJson.h>

#include "core/config/schema.h"
#include "core/sensors/channel.h"

namespace ps {
namespace doc {

constexpr int kSchemaVersion = 1;

void writeDevice(JsonWriter& w, const DeviceConfig& c);
/// Liest auf eine mit Standardwerten vorbelegte Struktur; ungültige Felder
/// bleiben auf dem Standard und landen in warnings.
void readDevice(JsonObjectConst root, DeviceConfig& c, schema::FieldErrors& warnings);

/// Ein Sensor samt Kanälen als Objekt (für Datei und API)
void writeSensor(JsonWriter& w, const SensorsConfig& c, uint8_t sensor);
void writeSensors(JsonWriter& w, const SensorsConfig& c);
/**
 * @brief Sensor-Teildokument anwenden ({"ANALOG":{…},…})
 * @param strict unbekannte Schlüssel als Fehler (PATCH)
 * @param changedSensors Bitmaske der geänderten Sensoren
 * @return Anzahl übernommener Felder
 */
uint16_t applySensors(JsonObjectConst sensors, SensorsConfig& c, schema::FieldErrors& errs,
                      bool strict, uint32_t* changedSensors = nullptr);
void readSensors(JsonObjectConst root, SensorsConfig& c, schema::FieldErrors& warnings);

void writeSecrets(JsonWriter& w, const Secrets& s);
void readSecrets(JsonObjectConst root, Secrets& s);

/// Hex-Kodierung für Salt und Hash
void toHex(const uint8_t* in, size_t n, char* out);
bool fromHex(const char* in, uint8_t* out, size_t n);

/// Ist das ein Kanal mit nur oberen Grenzen?
bool slotOneSided(uint8_t slot);
/// Hat der Kanal einen Rohwert (Analog)?
bool slotHasRaw(uint8_t slot);

} // namespace doc
} // namespace ps
