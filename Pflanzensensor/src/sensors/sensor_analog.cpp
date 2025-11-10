#include "sensors/sensor_analog.h"
#include "sensors/sensor_autocalibration.h"

#if USE_ANALOG
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"

AnalogSensor::~AnalogSensor() { m_state.samples.clear(); }

AnalogSensor::AnalogSensor(const AnalogConfig& config, SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_analogConfig(config) {
  struct SensorDefaults {
    const char* name;
    const char* fieldName;
    float yellowLow;
    float greenLow;
    float greenHigh;
    float yellowHigh;
    float min;
    float max;
    bool inverted;
  };

  static const SensorDefaults analogDefaults[] = {
#if ANALOG_SENSOR_COUNT > 0
    {ANALOG_1_NAME, ANALOG_1_FIELD_NAME, ANALOG_1_YELLOW_LOW, ANALOG_1_GREEN_LOW,
     ANALOG_1_GREEN_HIGH, ANALOG_1_YELLOW_HIGH, ANALOG_1_MIN, ANALOG_1_MAX, ANALOG_1_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 1
    {ANALOG_2_NAME, ANALOG_2_FIELD_NAME, ANALOG_2_YELLOW_LOW, ANALOG_2_GREEN_LOW,
     ANALOG_2_GREEN_HIGH, ANALOG_2_YELLOW_HIGH, ANALOG_2_MIN, ANALOG_2_MAX, ANALOG_2_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 2
    {ANALOG_3_NAME, ANALOG_3_FIELD_NAME, ANALOG_3_YELLOW_LOW, ANALOG_3_GREEN_LOW,
     ANALOG_3_GREEN_HIGH, ANALOG_3_YELLOW_HIGH, ANALOG_3_MIN, ANALOG_3_MAX, ANALOG_3_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 3
    {ANALOG_4_NAME, ANALOG_4_FIELD_NAME, ANALOG_4_YELLOW_LOW, ANALOG_4_GREEN_LOW,
     ANALOG_4_GREEN_HIGH, ANALOG_4_YELLOW_HIGH, ANALOG_4_MIN, ANALOG_4_MAX, ANALOG_4_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 4
    {ANALOG_5_NAME, ANALOG_5_FIELD_NAME, ANALOG_5_YELLOW_LOW, ANALOG_5_GREEN_LOW,
     ANALOG_5_GREEN_HIGH, ANALOG_5_YELLOW_HIGH, ANALOG_5_MIN, ANALOG_5_MAX, ANALOG_5_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 5
    {ANALOG_6_NAME, ANALOG_6_FIELD_NAME, ANALOG_6_YELLOW_LOW, ANALOG_6_GREEN_LOW,
     ANALOG_6_GREEN_HIGH, ANALOG_6_YELLOW_HIGH, ANALOG_6_MIN, ANALOG_6_MAX, ANALOG_6_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 6
    {ANALOG_7_NAME, ANALOG_7_FIELD_NAME, ANALOG_7_YELLOW_LOW, ANALOG_7_GREEN_LOW,
     ANALOG_7_GREEN_HIGH, ANALOG_7_YELLOW_HIGH, ANALOG_7_MIN, ANALOG_7_MAX, ANALOG_7_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 7
    {ANALOG_8_NAME, ANALOG_8_FIELD_NAME, ANALOG_8_YELLOW_LOW, ANALOG_8_GREEN_LOW,
     ANALOG_8_GREEN_HIGH, ANALOG_8_YELLOW_HIGH, ANALOG_8_MIN, ANALOG_8_MAX, ANALOG_8_INVERTED},
#endif
  };

  size_t maxChannels = sizeof(analogDefaults) / sizeof(analogDefaults[0]);
  if (m_analogConfig.activeMeasurements > maxChannels) {
    logger.warning(getName(), F("Begrenze activeMeasurements von ") +
                                  String(m_analogConfig.activeMeasurements) + F(" auf ") +
                                  String(maxChannels));
    m_analogConfig.activeMeasurements = maxChannels;
  }
  m_lastRawValues.clear();
  for (size_t i = 0; i < m_analogConfig.activeMeasurements; ++i) {
    const auto& def = analogDefaults[i];
    auto& meas = m_analogConfig.measurements[i];
    meas.minValue = def.min;
    meas.maxValue = def.max;
    meas.inverted = def.inverted;
    m_lastRawValues.push_back(-1); // Initialize with -1 (invalid)
    initMeasurement(i, def.name, def.fieldName, "%", def.yellowLow, def.greenLow, def.greenHigh,
                    def.yellowHigh);
  }
  // Initialize clamping warning flags
  m_clampWarningShown.resize(m_analogConfig.activeMeasurements, false);
#if USE_MULTIPLEXER
  if (config.useMultiplexer) {
    m_multiplexer = std::make_unique<Multiplexer>();
  }
#endif
}

void AnalogSensor::logDebugDetails() const {
  logDebug(F("Analog-Konfig: pin=") + String(m_analogConfig.pin) + F(", activeMeasurements=") +
           String(m_analogConfig.activeMeasurements));
}

SensorResult AnalogSensor::init() {
  logDebug(F("Initialisiere Analog-Sensor an Pin ") + String(m_analogConfig.pin));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }
  m_state.samples.clear();
#if USE_MULTIPLEXER
  if (m_analogConfig.useMultiplexer) {
    if (!m_multiplexer) {
      m_multiplexer = std::make_unique<Multiplexer>();
    }
    auto muxResult = m_multiplexer->init();
    if (!muxResult.isSuccess()) {
      logger.error(getName(), F(": Multiplexer-Initialisierung fehlgeschlagen"));
      return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                                F("Multiplexer-Initialisierung fehlgeschlagen"));
    }
  }
#endif
  pinMode(m_analogConfig.pin, INPUT);
  logger.debug(getName(), F(": Initialisiert an Pin ") + String(m_analogConfig.pin));
  m_initialized = true;
  return SensorResult::success();
}

SensorResult AnalogSensor::startMeasurement() {
  logDebug(F("Starte Analogmessung"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }
  // Log memory snapshot at the beginning of the measurement cycle
  logger.logMemoryStats(F("AnalogSensor::startMeasurement"));
  if (m_analogConfig.activeMeasurements > SensorConfig::MAX_MEASUREMENTS) {
    logger.warning(getName(), F("Begrenze activeMeasurements von ") +
                                  String(m_analogConfig.activeMeasurements) + F(" auf ") +
                                  String(SensorConfig::MAX_MEASUREMENTS));
    m_analogConfig.activeMeasurements = SensorConfig::MAX_MEASUREMENTS;
  }
  if (!isInitialized()) {
    logger.error(getName(), F(": Versuch, Messung ohne Initialisierung zu starten"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Sensor nicht initialisiert"));
  }
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();
  // Reset clamping warning flags for new measurement cycle
  std::fill(m_clampWarningShown.begin(), m_clampWarningShown.end(), false);
  logger.debug(getName(), F(": Starte neuen Messzyklus für ") +
                              String(m_analogConfig.activeMeasurements) + F(" Sensoren"));
  return SensorResult::success();
}

SensorResult AnalogSensor::continueMeasurement() {
  logDebug(F("Setze Analogmessung fort"));
  // DRY: The base class handles measurement cycling, so just validate state and
  // return success
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess() || !m_state.readInProgress) {
    return memoryResult;
  }
  if (!isInitialized()) {
    logger.error(getName(), F(": Versuch, Messung fortzusetzen ohne Initialisierung"));
    m_state.readInProgress = false;
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Sensor nicht initialisiert"));
  }
  if (millis() - m_state.operationStartTime > 5000) { // Hardcoded timeout
    logger.error(getName(), F(": Messzeitüberschreitung nach ") +
                                String(millis() - m_state.operationStartTime) + F("ms"));
    m_state.readInProgress = false;
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR, F("Messzeitüberschreitung"));
  }
  if (!canAccessHardware()) {
    return SensorResult::success();
  }
  // All actual measurement is handled by the base class via fetchSample
  return SensorResult::success();
}

void AnalogSensor::deinitialize() {
  logDebug(F("Deinitialisiere Analog-Sensor"));
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.samples);
#if USE_MULTIPLEXER
  if (m_multiplexer) {
    m_multiplexer.reset();
  }
#endif
}

bool AnalogSensor::validateReading(int reading, size_t measurementIndex) const {
  if (measurementIndex >= m_analogConfig.measurements.size()) {
    logDebug(F("AnalogSensor: Index außerhalb des Bereichs für Messungen! index=") +
             String(measurementIndex));
    return false;
  }
  // Für analoge Sensoren akzeptieren wir jetzt alle Werte, da wir sie in fetchSample begrenzen.
  // Diese Methode bleibt zur Kompatibilität, gibt aber immer true für gültige Indizes zurück.
  return true;
}

float AnalogSensor::mapAnalogValue(int rawValue, size_t measurementIndex) const {
  if (measurementIndex >= m_analogConfig.measurements.size()) {
    logDebug(F("AnalogSensor: Index außerhalb des Bereichs für Messungen! index=") +
             String(measurementIndex));
    return 0.0f;
  }
  // Use accessor helpers so autocal (when active) is taken into account.
  float minValue = getMinValue(measurementIndex);
  float maxValue = getMaxValue(measurementIndex);
  bool inverted = m_analogConfig.measurements[measurementIndex].inverted;

  if (maxValue == minValue)
    return 0.0f;

  // Wenn invertiert, wird der Rohwert umgekehrt auf den Prozentwert abgebildet
  if (inverted) {
    float percentage = 100.0f * (maxValue - rawValue) / (maxValue - minValue);
    logDebug(F("Invertierte Abbildung: roh=") + String(rawValue) + F(", min=") + String(minValue) +
             F(", max=") + String(maxValue) + F(", Ergebnis=") + String(percentage) + F("%"));
    return percentage;
  } else {
    float percentage = 100.0f * (rawValue - minValue) / (maxValue - minValue);
    logDebug(F("Normale Abbildung: roh=") + String(rawValue) + F(", min=") + String(minValue) +
             F(", max=") + String(maxValue) + F(", Ergebnis=") + String(percentage) + F("%"));
    return percentage;
  }
}

bool AnalogSensor::fetchSample(float& value, size_t index) {
  logDebug(F("Lese analogen Messwert für Index ") + String(index));
#if USE_MULTIPLEXER
  if (m_analogConfig.useMultiplexer && m_multiplexer) {
    if (!m_multiplexer->switchToSensor(index + 1)) {
      logger.error(getName(), F(": Konnte Kanal ") + String(index + 1) + F(" nicht auswählen"));
      value = NAN;
      return false;
    }
    // Sehr kurzes Delay für ADC-Stabilisierung nach Multiplexer-Umschaltung
    delayMicroseconds(500); // 0.5ms statt 2ms
  }
#endif
  if (index >= m_analogConfig.measurements.size()) {
    logDebug(F("AnalogSensor: Index außerhalb des Bereichs für Messungen! index=") + String(index));
    value = NAN;
    return false;
  }
  int raw = analogRead(m_analogConfig.pin);
  // Speichere letzten Rohwert
  if (index < m_lastRawValues.size()) {
    m_lastRawValues[index] = raw;
  }
  // Update runtime copy and central config so later persistence sees the last raw value
  if (index < m_analogConfig.measurements.size()) {
    m_analogConfig.measurements[index].lastRawValue = raw;
    SensorConfig& cfg = this->mutableConfig();
    if (index < cfg.measurements.size()) {
      cfg.measurements[index].lastRawValue = raw;
    }
  }

  // Aktualisiere und persistiere die historischen Roh-Extrema unabhängig
  // von Autocal. Setze bei der ersten Messung beide Werte auf den aktuellen
  // Rohwert; bei späteren Messungen persistieren wir nur, wenn ein neuer
  // Extremwert (kleiner als min oder größer als max) auftritt.
  if (index < m_analogConfig.measurements.size()) {
    SensorConfig& cfg = this->mutableConfig();
    int storedRawMin = cfg.measurements[index].absoluteRawMin;
    int storedRawMax = cfg.measurements[index].absoluteRawMax;
    int newRawMin = storedRawMin;
    int newRawMax = storedRawMax;
    bool needPersistRaw = false;

    if (storedRawMin == INT_MAX && storedRawMax == INT_MIN) {
      // Erste gültige Messung: seed sowohl Min als auch Max
      newRawMin = raw;
      newRawMax = raw;
      needPersistRaw = true;
    } else {
      if (raw < storedRawMin) {
        newRawMin = raw;
        needPersistRaw = true;
      } else if (raw > storedRawMax) {
        newRawMax = raw;
        needPersistRaw = true;
      }
    }

    if (needPersistRaw) {
      // Aktualisiere Laufzeit-Kopie und zentrale Konfiguration
      m_analogConfig.measurements[index].absoluteRawMin = newRawMin;
      m_analogConfig.measurements[index].absoluteRawMax = newRawMax;
      cfg.measurements[index].absoluteRawMin = newRawMin;
      cfg.measurements[index].absoluteRawMax = newRawMax;

      if (ConfigMgr.isDebugSensor()) {
        logger.debug(getName(), F("Neue absolute Roh-Extrema erkannt; persistiere: Min=") +
                                    String(newRawMin) + F(", Max=") + String(newRawMax));
      }

      // Defer persistence to avoid blocking in the measurement path
      SensorPersistence::enqueueAnalogRawMinMax(this->getId(), index, newRawMin, newRawMax);
      if (ConfigMgr.isDebugSensor())
        logger.debug(getName(), F("Absolute Roh-Extrema enqueued for persistence"));
    }
  }

  // Debug: print runtime calibration and autocal state so we can see why
  // clamping or autocal updates happen during measurement cycles.
  if (ConfigMgr.isDebugSensor()) {
    bool cfgCal = false;
    if (index < this->mutableConfig().measurements.size())
      cfgCal = this->mutableConfig().measurements[index].calibrationMode;
    String dbg =
        F("fetchSample debug: idx=") + String(index) + F(", raw=") + String(raw) +
        F(", runtime.calibrationMode=") +
        String(m_analogConfig.measurements[index].calibrationMode ? "1" : "0") +
        F(", cfg.calibrationMode=") + String(cfgCal ? "1" : "0") + F(", calcMin=") +
        String(m_analogConfig.measurements[index].minValue) + F(", calcMax=") +
        String(m_analogConfig.measurements[index].maxValue) + F(", autocalIntMin=") +
        String(m_analogConfig.measurements[index].autocal.min_value) + F(", autocalIntMax=") +
        String(m_analogConfig.measurements[index].autocal.max_value) + F(", autocalMinF=") +
        String(m_analogConfig.measurements[index].autocal.min_value_f) + F(", autocalMaxF=") +
        String(m_analogConfig.measurements[index].autocal.max_value_f);
    logger.debug(getName(), dbg);
  }

  // Derive a unified 'calibration mode' flag from both the runtime copy
  // and the central mutable config. This avoids transient races where one
  // copy is updated but the other is not yet in sync.
  bool cfgCalMode = false;
  if (index < this->mutableConfig().measurements.size())
    cfgCalMode = this->mutableConfig().measurements[index].calibrationMode;
  bool unifiedCalibrationMode = m_analogConfig.measurements[index].calibrationMode || cfgCalMode;

  // Auto-calibration: update exponential moving boundaries if enabled
  if (index < m_analogConfig.measurements.size()) {
    auto& measurement = m_analogConfig.measurements[index];
    if (unifiedCalibrationMode) {
      uint32_t minutes = millis() / 60000UL;

      // Immediate expansion: if the new raw is outside the current
      // calculation limits, anchor that side immediately to the raw
      // reading and persist the integer change. This guarantees no
      // clamping will occur in the same cycle.
      int curMinInt = static_cast<int>(roundf(measurement.minValue));
      int curMaxInt = static_cast<int>(roundf(measurement.maxValue));
      bool persistedImmediate = false;
      if (raw < curMinInt) {
        // Expand lower bound immediately
        measurement.autocal.min_value_f = static_cast<float>(raw);
        measurement.autocal.min_value = static_cast<uint16_t>(raw);
        measurement.minValue = static_cast<float>(measurement.autocal.min_value);
        int persistMin = static_cast<int>(measurement.autocal.min_value);
        int persistMax = static_cast<int>(measurement.autocal.max_value);

        // Enqueue instead of blocking write
        SensorPersistence::enqueueAnalogMinMaxInteger(m_analogConfig.id, index, persistMin,
                                                      persistMax, measurement.inverted);
        persistedImmediate = true;
        if (ConfigMgr.isDebugSensor())
          logger.debug(getName(),
                       F("Autocal: untere Grenze auf Rohwert gesetzt: ") + String(persistMin));
      } else if (raw > curMaxInt) {
        // Expand upper bound immediately
        measurement.autocal.max_value_f = static_cast<float>(raw);
        measurement.autocal.max_value = static_cast<uint16_t>(raw);
        measurement.maxValue = static_cast<float>(measurement.autocal.max_value);
        int persistMin = static_cast<int>(measurement.autocal.min_value);
        int persistMax = static_cast<int>(measurement.autocal.max_value);

        // Enqueue instead of blocking write
        SensorPersistence::enqueueAnalogMinMaxInteger(m_analogConfig.id, index, persistMin,
                                                      persistMax, measurement.inverted);
        persistedImmediate = true;
        if (ConfigMgr.isDebugSensor())
          logger.debug(getName(),
                       F("Autocal: obere Grenze auf Rohwert gesetzt: ") + String(persistMax));
      }

      // If we didn't perform an immediate expansion, run the EMA-based
      // autocal update to slowly forget old extrema. Persist only when
      // the integer-rounded bounds change (reduces flash wear).
      if (!persistedImmediate) {
        if (ConfigMgr.isDebugSensor()) {
          logger.debug(getName(), F("AutoCal update aufrufen: roh=") + String(raw) +
                                      F(", cal_min=") + String(measurement.autocal.min_value) +
                                      F(", cal_max=") + String(measurement.autocal.max_value));
        }
        // Compute alpha from configured autocal half-life and current
        // measurement interval so alpha adapts automatically when interval
        // changes.
        unsigned long intervalMs = this->getMeasurementInterval();
        uint32_t halfLife = measurement.autocalHalfLifeSeconds;
        float alpha = AutoCal_computeAlphaForHalfLifeSeconds(halfLife, intervalMs);
        bool autocalChanged =
            AutoCal_update(measurement.autocal, static_cast<uint16_t>(raw), minutes, alpha);
        // Guard: if autocal bounds are inverted, anchor to current raw reading
        if (measurement.autocal.min_value > static_cast<uint16_t>(raw) &&
            measurement.autocal.max_value < static_cast<uint16_t>(raw)) {
          measurement.autocal.min_value = static_cast<uint16_t>(raw);
          measurement.autocal.max_value = static_cast<uint16_t>(raw);
          measurement.autocal.min_value_f = static_cast<float>(raw);
          measurement.autocal.max_value_f = static_cast<float>(raw);
          measurement.autocal.last_update_time = minutes;
          autocalChanged = true;
          if (ConfigMgr.isDebugSensor()) {
            logger.debug(getName(),
                         F("Autocal-Inversion erkannt; min/max auf aktuellen Rohwert gesetzt: ") +
                             String(raw));
          }
        }
        if (ConfigMgr.isDebugSensor() && !autocalChanged) {
          logger.debug(getName(), F("AutoCal-Aufruf: keine Änderung (roh=") + String(raw) + F(")"));
        }
        if (autocalChanged) {
          if (ConfigMgr.isDebugSensor()) {
            logger.debug(getName(), F("Autokalibrierung geändert für Index ") + String(index) +
                                        F(": min=") + String(measurement.autocal.min_value) +
                                        F(", max=") + String(measurement.autocal.max_value));
          }
          // Apply autocal result to the calculation limits
          measurement.minValue = static_cast<float>(measurement.autocal.min_value);
          measurement.maxValue = static_cast<float>(measurement.autocal.max_value);
          // Persist integer-rounded min/max only (avoid flash wear)
          int persistMin = static_cast<int>(measurement.autocal.min_value);
          int persistMax = static_cast<int>(measurement.autocal.max_value);

          // Enqueue instead of blocking write
          SensorPersistence::enqueueAnalogMinMaxInteger(m_analogConfig.id, index, persistMin,
                                                        persistMax, measurement.inverted);

          if (ConfigMgr.isDebugSensor())
            logger.debug(getName(), F("Autocal int min/max in Queue für Index ") + String(index));
        }
      }
    }
  }

  // Prüfe, ob Rohwert außerhalb des Bereichs liegt und begrenze ihn ggf.
  // Autokalibrierung schreibt ihre Ergebnisse in minValue/maxValue; die
  // Abbildung verwendet daher immer die gespeicherten Berechnungslimits.
  float minValue = m_analogConfig.measurements[index].minValue;
  float maxValue = m_analogConfig.measurements[index].maxValue;
  int clampedRaw = raw;

  // If autocalibration is active, DO NOT clamp the raw value; autocal
  // should expand/shrink the calculation limits instead so the mapping
  // window shifts. Only clamp when autocal is disabled.
  if (!m_analogConfig.measurements[index].calibrationMode) {
    if (raw < static_cast<int>(roundf(minValue))) {
      clampedRaw = static_cast<int>(roundf(minValue));
      // Only log warning once per measurement cycle
      if (index < m_clampWarningShown.size() && !m_clampWarningShown[index]) {
        logger.warning(getName(),
                       F("Rohwert außerhalb der konfigurierten Grenzen; clamp auf min: ") +
                           String(clampedRaw) + F(" für Index ") + String(index));
        m_clampWarningShown[index] = true;
      }
    } else if (raw > static_cast<int>(roundf(maxValue))) {
      clampedRaw = static_cast<int>(roundf(maxValue));
      // Only log warning once per measurement cycle
      if (index < m_clampWarningShown.size() && !m_clampWarningShown[index]) {
        logger.warning(getName(),
                       F("Rohwert außerhalb der konfigurierten Grenzen; clamp auf max: ") +
                           String(clampedRaw) + F(" für Index ") + String(index));
        m_clampWarningShown[index] = true;
      }
    }
  } else {
    // In autocal mode, allow raw to pass through and let AutoCal expand
    // the runtime limits. Do not log a warning for these expected cases.
    clampedRaw = raw;
  }

  // Map raw value to percentage USING the (possibly autocal-adjusted)
  // calculation limits and log the mapped result.
  value = mapAnalogValue(clampedRaw, index);

  // Debug-Log für invertierte Sensoren
  if (index < m_analogConfig.measurements.size() && m_analogConfig.measurements[index].inverted) {
    logDebug(F("Invertierter Sensor: roh=") + String(clampedRaw) + F(", abgebildet=") +
             String(value) + F("%"));
  }

  logDebug(F("Gelesener Wert: ") + String(value));

  // Persist updated calculation limits immediately if autocal changed
  // (this is done earlier in the autocal update block). No further action
  // needed here.
  return !isnan(value);
}

bool AnalogSensor::canAccessHardware() const {
  // Für Analog-Sensoren mit Multiplexer ist kein Delay zwischen Kanälen nötig,
  // da der Multiplexer bereits ein kurzes Stabilisierungs-Delay (2ms) hat.
  // Das minimumDelay wird stattdessen über das SensorMeasurementCycleManager
  // auf Zyklus-Ebene durchgesetzt.
  return true;
}

#endif // USE_ANALOG
