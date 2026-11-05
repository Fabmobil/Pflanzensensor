/**
 * @file schema.h
 * @brief Konfigurationsschema: eine Tabelle je Struktur
 * @details Aus der Tabelle entstehen Laden, Speichern, Validierung, PATCH,
 *          Export und Import (refactor.md 5.1 Punkt 6). Felder werden über
 *          ihren Offset in der POD-Struktur angesprochen.
 */
#pragma once

#include <ArduinoJson.h>

#include "core/config/device_config.h"
#include "core/config/sensor_config.h"
#include "core/json_writer.h"

namespace ps {
namespace schema {

enum class FieldType : uint8_t { Bool, U8, U16, U32, I16, I32, F32, Str, Enum };

enum FieldFlag : uint8_t {
  FNone = 0,
  FNonEmpty = 1,   ///< Text darf nicht leer sein
  FChannelKey = 2, ///< Text muss ein Kanalschlüssel des Boards sein (oder leer)
  FUrl = 4,        ///< http:// oder https://
  FHalfLife = 8,   ///< nur erlaubte Halbwertszeiten
  FPrintable = 16, ///< nur ASCII 0x20..0x7E
};

struct FieldDef {
  char section[8];
  char key[16];
  FieldType type;
  uint8_t flags;
  uint16_t offset;
  uint16_t size; ///< Str: Kapazität inkl. Nullbyte
  float min, max;
  char enumNames[26]; ///< "DEBUG|INFO|WARNING|ERROR"
};

struct Table {
  const FieldDef* fields; ///< im Flash
  uint8_t count;
  bool sectioned; ///< Felder liegen unter "section": {...}
};

const Table& deviceTable();
const Table& sensorTable();
const Table& channelTable();

/// Ein Feld aus dem Flash holen
FieldDef field(const Table& t, uint8_t i);

/// Fehler je Feld (für API-Antworten)
struct FieldErrors {
  static constexpr uint8_t kMax = 8;
  struct Item {
    FixedString<40> path;
    Err err;
  } items[kMax];
  uint8_t count = 0;
  uint8_t dropped = 0;
  void add(const char* path, Err e);
  void addParts(const char* a, const char* b, const char* c, Err e);
  bool empty() const { return count == 0 && dropped == 0; }
  void writeJson(JsonWriter& w) const;
};

/// Prüft, ob ein Kanalschlüssel zu diesem Board gehört
bool isChannelKey(const char* s);
/// Zerlegt "ANALOG_1" in Slot-Index. -1, wenn unbekannt.
int slotOfKey(const char* key);
/// Schreibt den Schlüssel eines Slots
void keyOfSlot(uint8_t slot, char* out, size_t cap);

/// Wert validieren und setzen. Gibt Err::None zurück, wenn übernommen.
Err setField(const FieldDef& f, JsonVariantConst v, void* base);
/// Feld als JSON-Wert schreiben (ohne Schlüssel)
void writeValue(JsonWriter& w, const FieldDef& f, const void* base);

/**
 * @brief JSON-Objekt auf eine Struktur anwenden
 * @param strict true: unbekannte Schlüssel zählen als Fehler (PATCH)
 * @param prefix Pfadpräfix für Fehlermeldungen
 * @return Anzahl übernommener Felder
 */
uint8_t apply(const Table& t, JsonObjectConst obj, void* base, FieldErrors& errs, bool strict,
              const char* prefix = "");

/// Alle Felder schreiben (sectioned: gruppiert; sonst flach in das offene Objekt)
void writeAll(JsonWriter& w, const Table& t, const void* base);

/// Prüft zusammengesetzte Bedingungen eines Kanals (Schwellenordnung)
Err checkChannel(const ChannelConfig& c, bool oneSided);

} // namespace schema
} // namespace ps
