/**
 * @file sensor_service.h
 * @brief Verbindet Mess-Scheduler, Konfiguration und Kanalzustand
 * @details Rechnet im Finish Rohwert → Prozent, führt Autokalibrierung,
 *          Status, Extrema und letzten Wert nach und veröffentlicht
 *          MeasurementDone. Ein Sensor, dessen begin() beim Start scheitert,
 *          bleibt im Plan und durchläuft das Fehlermodell (B-10);
 *          Aktivieren/Deaktivieren wirkt sofort (B-11).
 */
#pragma once

#include "core/sensors/scheduler.h"
#include "core/util/event_queue.h"
#include "core/util/throttle.h"
#include "services/config_service.h"
#include "services/state_store.h"

namespace ps {

using Events = EventQueue<16>;

enum class MeasureResult : uint8_t { Accepted, Busy, NotReady, Disabled, NotFound, Throttled };

class SensorService : public MeasurementListener {
public:
  using Scheduler =
      MeasurementScheduler<board::kMaxSensors, board::kMaxChannelsPerSensor, 3, board::kMaxSlots>;
  /// Darf ein dauerhaft fehlerhafter Sensor das Gerät neu starten? (DS18B20)
  using RestartGate = bool (*)(void* ctx);

  SensorService(ConfigService& cfg, StateStore& state, Events& events)
      : cfg_(cfg), state_(state), events_(events), sched_(*this) {}

  /// Treiber in Reihenfolge der Sensorliste (board::sensor)
  void begin(SensorDriver* const* drivers, Instant now);
  void setRestartGate(RestartGate gate, void* ctx) {
    restartGate_ = gate;
    restartCtx_ = ctx;
  }
  void tick(Instant now) { sched_.tick(now); }

  /**
   * @brief Sofortmessung
   * @param publicRequest true: globale Drossel 2 s (429)
   * @param waitMs erhält Restzeit (Drossel bzw. Start-Warmup)
   */
  MeasureResult measureNow(uint8_t sensor, Instant now, bool publicRequest, uint32_t& waitMs);
  /// Konfiguration geänderter Sensoren übernehmen (Bitmaske)
  void applyConfig(uint32_t changedSensors, Instant now);
  void resetExtrema(uint8_t slot, bool raw, Instant now);
  /// HX711: aktuellen Rohwert als Nullpunkt übernehmen (nächste Messung)
  Err tare(uint8_t sensor, Instant now);
  /// HX711: Rohwert der nächsten Messung als Ausgangswert für scaleCalibrate
  /// merken (Topf darf auf der Waage stehen bleiben)
  Err scaleReference(uint8_t sensor, Instant now);
  /**
   * @brief HX711: Skalierung aus einem bekannten Gewicht (nächste Messung)
   * @details Skalierung = (Rohwert − Ausgangswert) / grams. Ausgangswert ist
   *          der mit scaleReference gemerkte Rohwert, sonst der Nullpunkt.
   */
  Err scaleCalibrate(uint8_t sensor, float grams, Instant now);
  /**
   * @brief Aktuellen Messwert als 0 % bzw. 100 % übernehmen
   * @details Analog: Rohwert wird zur passenden Rohgrenze, Autokalibrierung
   *          aus. HX711 Kanal 1 (Wasservorrat): aktuelles Gewicht wird
   *          Trocken- bzw. Nassgewicht, der Kanal wird eingeschaltet.
   */
  Err calibratePoint(uint8_t slot, uint8_t percent, Instant now);
  /// Analog: Vorschlag aus den Lerndaten übernehmen (Rohgrenzen, Invertierung)
  Err applySoilFit(uint8_t slot, Instant now);
  /// Analog: Lerndaten verwerfen
  Err resetSoilFit(uint8_t slot, Instant now);
  void pause(bool paused) { sched_.pause(paused); }

  // Lesen
  const ChannelState& channel(uint8_t slot) const { return state_.channel(slot); }
  ChannelStatus status(uint8_t slot, Instant now) const;
  const SensorRunState& run(uint8_t sensor) const { return sched_.state(sensor); }
  bool inWarmup(uint8_t sensor, Instant now) const { return sched_.inStartWarmup(sensor, now); }
  bool measuredOnce(uint8_t sensor) const { return lastOk_[sensor].ok; }
  Instant lastMeasurement(uint8_t sensor) const { return lastOk_[sensor].at; }
  bool allMeasuredOnce() const;
  uint8_t processedSensors() const;
  /// Lerndaten eines Analogkanals, sonst nullptr
  const SoilFit* soilFit(uint8_t slot) const;
  const WateringDetector& watering() const { return water_; }
  /// HX711: Ausgangswert für die Skalierung gemerkt bzw. angefordert
  bool scaleReferenceSet() const { return hasRef_ || pendingRef_; }
  /// HX711: Tara oder Skalierung wartet auf die nächste Messung
  bool scalePending() const { return pendingTare_ != 0 || pendingRef_ || !isnan(pendingGrams_); }
  /// Aktueller Wasservorrat in %, NaN wenn unbekannt, veraltet oder beim Abtropfen
  float currentWater(Instant now) const;
  /// Sensor-Index eines Typs/einer ID ("ANALOG") oder -1
  static int sensorByName(const char* id);

  // MeasurementListener
  void onMeasurement(uint8_t sensor, const Reading* values, uint8_t n, Instant now) override;
  void onFailure(uint8_t sensor, Health health, uint8_t failures, bool becameFailed,
                 Instant now) override;
  void onJobStart(uint8_t sensor, bool priority, Instant now) override;
  void onRounds(uint8_t sensor, const Reading* samples, uint8_t rounds, uint8_t stride,
                uint8_t n) override;

private:
  /// Messwert eines Kanals für das Log ("17.2 %", "--")
  void logResult(uint8_t slot, float value, int16_t raw, ChannelStatus before, ChannelStatus after,
                 bool newMin, bool newMax);
  /// HX711 nach der Messung: Gießen erkennen, Tara und Skalierung übernehmen.
  /// true, wenn die Konfiguration geändert wurde.
  bool afterScale(uint8_t sensor, float weight, Instant now);
  /// Sofortmessung für eine HX711-Aktion anfordern (Ergebnis in afterScale)
  Err requestScaleAction(uint8_t sensor, Instant now);
  Instant jobStart_[board::kMaxSensors];
  uint8_t failuresBefore_[board::kMaxSensors] = {};
  static_assert(board::kMaxSlots <= 32, "clampWarned_ ist eine 32-Bit-Maske");
  /// Kanäle, deren Rohwert gerade begrenzt wird (schon gemeldet)
  uint32_t clampWarned_ = 0;
  ConfigService& cfg_;
  StateStore& state_;
  Events& events_;
  Scheduler sched_;
  SensorDriver* drivers_[board::kMaxSensors] = {};
  struct LastOk {
    bool ok = false;
    Instant at;
  } lastOk_[board::kMaxSensors];
  uint32_t pendingTare_ = 0;
  bool pendingRef_ = false;
  float pendingGrams_ = NAN;
  Instant pendingAt_;   ///< erst Messungen, die danach begannen, zählen
  bool hasRef_ = false; ///< refCounts_ gilt
  int32_t refCounts_ = 0;
  WateringDetector water_;
  float seenScale_ = NAN; ///< Skalierung/Tara der letzten Gießprüfung
  int32_t seenTare_ = 0;
  RequestThrottle publicThrottle_{2000};
  RestartGate restartGate_ = nullptr;
  void* restartCtx_ = nullptr;
};

} // namespace ps
