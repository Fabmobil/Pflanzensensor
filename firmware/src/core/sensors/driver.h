/**
 * @file driver.h
 * @brief Schnittstelle der Sensortreiber (refactor.md 5.6)
 * @details Alle Methoden kehren sofort zurück. Was länger dauert, meldet
 *          Step::Pending und wird beim nächsten Tick erneut gerufen.
 *          begin() muss beliebig oft aufrufbar sein: nach jedem Fehlschlag
 *          beginnt der nächste Auftrag damit.
 */
#pragma once

#include "core/config/sensor_config.h"
#include "core/sensors/aggregate.h"
#include "core/sensors/catalog.h"
#include "core/types.h"

namespace ps {

class SensorDriver {
public:
  virtual SensorType type() const = 0;
  /// Takte und Ressourcen; Standard aus dem Katalog, Treiber dürfen anpassen
  /// (DHT22: 2 s Rundenabstand).
  virtual SensorInfo info() const { return catalog::info(type()); }
  /// Aktuell lieferbare Kanäle (z.B. gefundene DS18B20)
  virtual uint8_t channelCount() const = 0;
  virtual Step begin(Instant now) = 0;
  virtual Step wake(Instant now) { return Step::Ok; }
  virtual Step startRound(Instant now) { return Step::Ok; }
  /// Liefert alle Kanäle einer Runde. Ungültige Kanäle bleiben NaN.
  virtual Step pollRound(Instant now, Reading* out, uint8_t n) = 0;
  virtual void sleep() {}
  /// Konfiguration übernehmen (HX711: Skala, Tara)
  virtual void configure(const SensorConfig& c) {}
  /// HX711: Rohzählerstand der letzten Messung (für "Tarieren")
  virtual bool lastRawCounts(int32_t& out) const { return false; }

protected:
  ~SensorDriver() = default;
};

} // namespace ps
