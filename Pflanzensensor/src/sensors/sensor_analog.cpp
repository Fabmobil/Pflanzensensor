#include "sensors/sensor_analog.h"
#include "sensors/sensor_autocalibration.h"

#if USE_ANALOG
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"

AnalogSensor::~AnalogSensor() { m_state.samples.clear(); }

AnalogSensor::AnalogSensor(const AnalogConfig& config, SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_useMultiplexer(config.useMultiplexer) {
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
  if (mutableConfig().activeMeasurements > maxChannels) {
    logger.warning(getName(), String(F("Begrenze activeMeasurements von ")) +
                                  String(mutableConfig().activeMeasurements) + String(F(" auf ")) +
                                  String(maxChannels));
    mutableConfig().activeMeasurements = maxChannels;
  }
  m_lastRawValues.clear();
  for (size_t i = 0; i < mutableConfig().activeMeasurements; ++i) {
    const auto& def = analogDefaults[i];
    auto& meas = mutableConfig().measurements[i];
    meas.minValue = def.min;
    meas.maxValue = def.max;
    meas.inverted = def.inverted;
    m_lastRawValues.push_back(-1); // Initialize with -1 (invalid)
    initMeasurement(i, def.name, def.fieldName, "%", def.yellowLow, def.greenLow, def.greenHigh,
                    def.yellowHigh);
  }
  // Initialize clamping warning flags
  m_clampWarningShown.resize(mutableConfig().activeMeasurements, false);
#if USE_MULTIPLEXER
  if (config.useMultiplexer) {
    m_multiplexer = std::make_unique<Multiplexer>();
  }
#endif
}

void AnalogSensor::logDebugDetails() const {
  logDebug(String(F("Analog-Konfig: pin=")) + String(config().pin) +
           String(F(", activeMeasurements=")) + String(config().activeMeasurements));
}

SensorResult AnalogSensor::init() {
  logDebug(String(F("Initialisiere Analog-Sensor an Pin ")) + String(mutableConfig().pin));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }
  m_state.samples.clear();
#if USE_MULTIPLEXER
  if (m_useMultiplexer) {
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
  pinMode(mutableConfig().pin, INPUT);
  logger.debug(getName(), String(F(": Initialisiert an Pin ")) + String(mutableConfig().pin));
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
  if (mutableConfig().activeMeasurements > SensorConfig::MAX_MEASUREMENTS) {
    logger.warning(getName(), String(F("Begrenze activeMeasurements von ")) +
                                  String(mutableConfig().activeMeasurements) + String(F(" auf ")) +
                                  String(SensorConfig::MAX_MEASUREMENTS));
    mutableConfig().activeMeasurements = SensorConfig::MAX_MEASUREMENTS;
  }
  if (!isInitialized()) {
    logger.error(getName(), F(": Versuch, Messung ohne Initialisierung zu starten"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Sensor nicht initialisiert"));
  }
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();
  // Reset clamping warning flags for new measurement cycle
  std::fill(m_clampWarningShown.begin(), m_clampWarningShown.end(), false);
  logger.debug(getName(), String(F(": Starte neuen Messzyklus für ")) +
                              String(mutableConfig().activeMeasurements) + F(" Sensoren"));
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
    logger.error(getName(), String(F(": Messzeitüberschreitung nach ")) +
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
  if (measurementIndex >= config().measurements.size()) {
    logDebug(String(F("AnalogSensor: Index außerhalb des Bereichs für Messungen! index=")) +
             String(measurementIndex));
    return false;
  }
  // Für analoge Sensoren akzeptieren wir jetzt alle Werte, da wir sie in fetchSample begrenzen.
  // Diese Methode bleibt zur Kompatibilität, gibt aber immer true für gültige Indizes zurück.
  return true;
}

float AnalogSensor::mapAnalogValue(int rawValue, size_t measurementIndex) const {
  if (measurementIndex >= config().measurements.size()) {
    logDebug(String(F("AnalogSensor: Index außerhalb des Bereichs für Messungen! index=")) +
             String(measurementIndex));
    return 0.0f;
  }
  // Use accessor helpers so autocal (when active) is taken into account.
  float minValue = getMinValue(measurementIndex);
  float maxValue = getMaxValue(measurementIndex);
  bool inverted = config().measurements[measurementIndex].inverted;

  if (maxValue == minValue)
    return 0.0f;

  // Wenn invertiert, wird der Rohwert umgekehrt auf den Prozentwert abgebildet
  if (inverted) {
    float percentage = 100.0f * (maxValue - rawValue) / (maxValue - minValue);
    logDebug(String(F("Invertierte Abbildung: roh=")) + String(rawValue) + String(F(", min=")) +
             String(minValue) + String(F(", max=")) + String(maxValue) + String(F(", Ergebnis=")) +
             String(percentage) + F("%"));
    return percentage;
  } else {
    float percentage = 100.0f * (rawValue - minValue) / (maxValue - minValue);
    logDebug(String(F("Normale Abbildung: roh=")) + String(rawValue) + String(F(", min=")) +
             String(minValue) + String(F(", max=")) + String(maxValue) + String(F(", Ergebnis=")) +
             String(percentage) + F("%"));
    return percentage;
  }
}

bool AnalogSensor::fetchSample(float& value, size_t index) {
  logDebug(String(F("Lese analogen Messwert für Index ")) + String(index));
#if USE_MULTIPLEXER
  if (m_useMultiplexer && m_multiplexer) {
    if (!m_multiplexer->switchToSensor(index + 1)) {
      logger.error(getName(),
                   String(F(": Konnte Kanal ")) + String(index + 1) + F(" nicht auswählen"));
      value = NAN;
      return false;
    }
    // Sehr kurzes Delay für ADC-Stabilisierung nach Multiplexer-Umschaltung
    delayMicroseconds(500); // 0.5ms statt 2ms
  }
#endif
  if (index >= mutableConfig().measurements.size()) {
    logDebug(String(F("AnalogSensor: Index außerhalb des Bereichs für Messungen! index=")) +
             String(index));
    value = NAN;
    return false;
  }
  int raw = analogRead(mutableConfig().pin);
  // Letzten Rohwert merken (einmal - es gibt nur noch eine Config)
  if (index < m_lastRawValues.size()) {
    m_lastRawValues[index] = raw;
  }
  if (index < mutableConfig().measurements.size()) {
    mutableConfig().measurements[index].lastRawValue = raw;
  }

  // Aktualisiere und persistiere die historischen Roh-Extrema unabhängig
  // von Autocal. Setze bei der ersten Messung beide Werte auf den aktuellen
  // Rohwert; bei späteren Messungen persistieren wir nur, wenn ein neuer
  // Extremwert (kleiner als min oder größer als max) auftritt.
  if (index < mutableConfig().measurements.size()) {
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
      cfg.measurements[index].absoluteRawMin = newRawMin;
      cfg.measurements[index].absoluteRawMax = newRawMax;

      if (ConfigMgr.isDebugSensor()) {
        logger.debug(getName(), String(F("Neue absolute Roh-Extrema erkannt; persistiere: Min=")) +
                                    String(newRawMin) + String(F(", Max=")) + String(newRawMax));
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
    String dbg = String(F("fetchSample debug: idx=")) + String(index) + String(F(", raw=")) +
                 String(raw) + String(F(", runtime.calibrationMode=")) +
                 String(mutableConfig().measurements[index].calibrationMode ? "1" : "0") +
                 String(F(", cfg.calibrationMode=")) + String(cfgCal ? "1" : "0") +
                 String(F(", calcMin=")) + String(mutableConfig().measurements[index].minValue) +
                 String(F(", calcMax=")) + String(mutableConfig().measurements[index].maxValue) +
                 String(F(", autocalIntMin=")) +
                 String(mutableConfig().measurements[index].autocal.min_value) +
                 String(F(", autocalIntMax=")) +
                 String(mutableConfig().measurements[index].autocal.max_value) +
                 String(F(", autocalMinF=")) +
                 String(mutableConfig().measurements[index].autocal.min_value_f) +
                 String(F(", autocalMaxF=")) +
                 String(mutableConfig().measurements[index].autocal.max_value_f);
    logger.debug(getName(), dbg);
  }

  // Nur noch eine Config - kein Abgleich zweier Kopien mehr nötig.
  bool unifiedCalibrationMode =
      (index < config().measurements.size()) && config().measurements[index].calibrationMode;

  // Auto-calibration: update exponential moving boundaries if enabled
  if (index < mutableConfig().measurements.size()) {
    auto& measurement = mutableConfig().measurements[index];
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
        SensorPersistence::enqueueAnalogMinMaxInteger(getId(), index, persistMin, persistMax,
                                                      measurement.inverted);
        persistedImmediate = true;
        if (ConfigMgr.isDebugSensor())
          logger.debug(getName(), String(F("Autocal: untere Grenze auf Rohwert gesetzt: ")) +
                                      String(persistMin));
      } else if (raw > curMaxInt) {
        // Expand upper bound immediately
        measurement.autocal.max_value_f = static_cast<float>(raw);
        measurement.autocal.max_value = static_cast<uint16_t>(raw);
        measurement.maxValue = static_cast<float>(measurement.autocal.max_value);
        int persistMin = static_cast<int>(measurement.autocal.min_value);
        int persistMax = static_cast<int>(measurement.autocal.max_value);

        // Enqueue instead of blocking write
        SensorPersistence::enqueueAnalogMinMaxInteger(getId(), index, persistMin, persistMax,
                                                      measurement.inverted);
        persistedImmediate = true;
        if (ConfigMgr.isDebugSensor())
          logger.debug(getName(), String(F("Autocal: obere Grenze auf Rohwert gesetzt: ")) +
                                      String(persistMax));
      }

      // If we didn't perform an immediate expansion, run the EMA-based
      // autocal update to slowly forget old extrema. Persist only when
      // the integer-rounded bounds change (reduces flash wear).
      if (!persistedImmediate) {
        if (ConfigMgr.isDebugSensor()) {
          logger.debug(getName(),
                       String(F("AutoCal update aufrufen: roh=")) + String(raw) +
                           String(F(", cal_min=")) + String(measurement.autocal.min_value) +
                           String(F(", cal_max=")) + String(measurement.autocal.max_value));
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
            logger.debug(
                getName(),
                String(F("Autocal-Inversion erkannt; min/max auf aktuellen Rohwert gesetzt: ")) +
                    String(raw));
          }
        }
        if (ConfigMgr.isDebugSensor() && !autocalChanged) {
          logger.debug(getName(),
                       String(F("AutoCal-Aufruf: keine Änderung (roh=")) + String(raw) + F(")"));
        }
        if (autocalChanged) {
          if (ConfigMgr.isDebugSensor()) {
            logger.debug(getName(),
                         String(F("Autokalibrierung geändert für Index ")) + String(index) +
                             String(F(": min=")) + String(measurement.autocal.min_value) +
                             String(F(", max=")) + String(measurement.autocal.max_value));
          }
          // Apply autocal result to the calculation limits
          measurement.minValue = static_cast<float>(measurement.autocal.min_value);
          measurement.maxValue = static_cast<float>(measurement.autocal.max_value);
          // Persist integer-rounded min/max only (avoid flash wear)
          int persistMin = static_cast<int>(measurement.autocal.min_value);
          int persistMax = static_cast<int>(measurement.autocal.max_value);

          // Enqueue instead of blocking write
          SensorPersistence::enqueueAnalogMinMaxInteger(getId(), index, persistMin, persistMax,
                                                        measurement.inverted);

          if (ConfigMgr.isDebugSensor())
            logger.debug(getName(),
                         String(F("Autocal int min/max in Queue für Index ")) + String(index));
        }
      }
    }
  }

  // Prüfe, ob Rohwert außerhalb des Bereichs liegt und begrenze ihn ggf.
  // Autokalibrierung schreibt ihre Ergebnisse in minValue/maxValue; die
  // Abbildung verwendet daher immer die gespeicherten Berechnungslimits.
  float minValue = mutableConfig().measurements[index].minValue;
  float maxValue = mutableConfig().measurements[index].maxValue;
  int clampedRaw = raw;

  // If autocalibration is active, DO NOT clamp the raw value; autocal
  // should expand/shrink the calculation limits instead so the mapping
  // window shifts. Only clamp when autocal is disabled.
  if (!mutableConfig().measurements[index].calibrationMode) {
    if (raw < static_cast<int>(roundf(minValue))) {
      clampedRaw = static_cast<int>(roundf(minValue));
      // Only log warning once per measurement cycle
      if (index < m_clampWarningShown.size() && !m_clampWarningShown[index]) {
        logger.warning(getName(),
                       String(F("Rohwert außerhalb der konfigurierten Grenzen; clamp auf min: ")) +
                           String(clampedRaw) + String(F(" für Index ")) + String(index));
        m_clampWarningShown[index] = true;
      }
    } else if (raw > static_cast<int>(roundf(maxValue))) {
      clampedRaw = static_cast<int>(roundf(maxValue));
      // Only log warning once per measurement cycle
      if (index < m_clampWarningShown.size() && !m_clampWarningShown[index]) {
        logger.warning(getName(),
                       String(F("Rohwert außerhalb der konfigurierten Grenzen; clamp auf max: ")) +
                           String(clampedRaw) + String(F(" für Index ")) + String(index));
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
  if (index < mutableConfig().measurements.size() && mutableConfig().measurements[index].inverted) {
    logDebug(String(F("Invertierter Sensor: roh=")) + String(clampedRaw) +
             String(F(", abgebildet=")) + String(value) + F("%"));
  }

  logDebug(String(F("Gelesener Wert: ")) + String(value));

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
