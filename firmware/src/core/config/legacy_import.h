/**
 * @file legacy_import.h
 * @brief Import der config.json der alten Firmware 2.x (refactor.md 2.3, E2)
 * @details Erkannt wird das Altformat daran, dass "schema" fehlt und
 *          "general" oder "sensors" vorhanden ist.
 */
#pragma once

#include <ArduinoJson.h>

#include "core/config/document.h"

namespace ps {
namespace legacy {

/// Zusammenfassung eines Imports
struct Report {
  uint16_t applied = 0;
  uint16_t ignored = 0;
  uint16_t invalid = 0;
  schema::FieldErrors notes; ///< ignorierte/ungültige Pfade (die ersten)
  void ignore(const char* path) {
    ignored++;
    notes.add(path, Err::NotFound);
  }
  void bad(const char* path) {
    invalid++;
    notes.add(path, Err::InvalidArgument);
  }
};

/// Was nicht direkt in eine Konfigurationsstruktur passt
struct Extras {
  bool hasAdminPassword = false;
  FixedString<33> adminPassword; ///< Klartext, wird vom Dienst gehasht
  ChannelState state[board::kMaxSlots];
  bool hasState[board::kMaxSlots] = {};
};

bool isLegacy(JsonObjectConst root);

/**
 * @brief Altdatei übernehmen
 * @details Ziele sind mit den aktuellen Werten (bzw. Standards) vorbelegt;
 *          nur vorhandene und gültige Werte werden überschrieben.
 */
void import(JsonObjectConst root, DeviceConfig& dev, SensorsConfig& sens, Secrets& sec,
            Extras& extras, Report& report);

} // namespace legacy
} // namespace ps
