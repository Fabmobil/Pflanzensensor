#include "managers/manager_sensor_persistence.h"
#include "sensor_measurement_cycle.h"

void SensorMeasurementCycleManager::handleProcessing() {
  // Process measurement results
  if (m_currentResults.empty()) {
    handleStateError(F("Keine Messresultate verfügbar"));
    return;
  }

  // **CRITICAL FIX: Use proper updateMeasurementData method instead of
  // const_cast**
  const MeasurementData& currentData = m_sensor->getMeasurementData();
  logger.debug(F("MeasurementCycle"),
               F("Verarbeite: Feldnamen=") + String(SensorConfig::MAX_MEASUREMENTS) +
                   F(", Einheiten=") + String(SensorConfig::MAX_MEASUREMENTS) + F(", Werte=") +
                   String(m_currentResults.size()) + F(", currentResults=") +
                   String(m_currentResults.size()));

  // CRITICAL: Validate measurement data before processing
  if (!currentData.isValid()) {
    logger.error(F("MeasurementCycle"), F("Ungültige Messdatenstruktur"));
    handleStateError(F("Ungültige Messdatenstruktur"));
    return;
  }

  // Validate array sizes to prevent bounds violations
  if (m_currentResults.size() != currentData.activeValues) {
    logger.error(F("MeasurementCycle"), F("Größenabweichung der Messdatenarray: currentResults=") +
                                            String(m_currentResults.size()) + F(", activeValues=") +
                                            String(currentData.activeValues));
    handleStateError(F("Größenabweichung der Messdatenarray"));
    return;
  }

  // Create a new MeasurementData object with the updated values
  MeasurementData updatedData = currentData; // Copy current data

  // Validate and process the data
  bool hasValidData = false;
  size_t maxFields = std::min(m_currentResults.size(), currentData.activeValues);

  // CRITICAL: Add bounds checking for measurement data arrays
  if (maxFields > currentData.values.size()) {
    handleStateError(F("Messdaten-Array-Grenzverletzung"));
    return;
  }

  for (size_t i = 0; i < maxFields; i++) {
    if (i >= currentData.values.size() || i >= m_currentResults.size()) {
      logger.error(F("MeasurementCycle"),
                   F("Index außerhalb des Bereichs bei Verarbeitung: ") + String(i));
      continue;
    }
    float value = m_currentResults[i];
    if (!isnan(value) && m_sensor->isValidValue(value, i)) {
      updatedData.values[i] = value;
      hasValidData = true;

      // Update absolute min/max values
      SensorConfig& config = m_sensor->mutableConfig();
      if (i < config.measurements.size()) {
        bool minMaxChanged = false;
        if (value < config.measurements[i].absoluteMin) {
          config.measurements[i].absoluteMin = value;
          minMaxChanged = true;
        }
        if (value > config.measurements[i].absoluteMax) {
          config.measurements[i].absoluteMax = value;
          minMaxChanged = true;
        }

        // Enqueue configuration changes to be written in batches (reduces flash wear)
        if (minMaxChanged) {
          SensorPersistence::enqueueAbsoluteMinMax(m_sensor->getId(), i,
                                                   config.measurements[i].absoluteMin,
                                                   config.measurements[i].absoluteMax);
          logger.debug(F("MeasurementCycle"), F("Absolute Min/Max aktualisiert für Sensor ") +
                                                  m_sensor->getId() + F(" Messung ") + String(i));
        }
      }
    } else {
      updatedData.values[i] = 0.0f;
    }
  }

  if (!hasValidData) {
    handleStateError(F("Keine gültigen Messdaten nach Verarbeitung"));
    return;
  }

  updatedData.activeValues = maxFields;

  // **CRITICAL FIX: Use the proper updateMeasurementData method**
  m_sensor->updateMeasurementData(updatedData);
  m_sensor->updateLastMeasurementTime();

  // Update status for all measurements
  for (size_t i = 0; i < updatedData.activeValues; i++) {
    m_sensor->updateStatus(i);
  }
  logMeasurementResults();

  // NOTE: Slot will be released in handleDeinitializing() AFTER all cleanup
  // to prevent other sensors from interfering while we're still cleaning up

#if USE_INFLUXDB
  m_state.setState(MeasurementState::SENDING_INFLUX, m_sensor->getName());
#else
  m_state.setState(MeasurementState::DEINITIALIZING, m_sensor->getName());
#endif
}
void SensorMeasurementCycleManager::handleSendingInflux() {
#if USE_INFLUXDB

  auto result = influxdbSendMeasurement(m_sensor, m_sensor->getMeasurementData());
  if (!result.isSuccess()) {
    logger.error(F("MeasurementCycle"),
                 F("Fehler beim Senden der Daten an InfluxDB: ") + result.getMessage());
    m_state.setState(MeasurementState::DEINITIALIZING, m_sensor->getName());
    return;
  }

#endif

  m_state.setState(MeasurementState::DEINITIALIZING, m_sensor->getName());
}

void SensorMeasurementCycleManager::handleDeinitializing() {
  // CRITICAL: Flush pending updates for THIS sensor immediately after measurement cycle
  // This ensures data is persisted right away instead of waiting for periodic flush
  if (ConfigMgr.isDebugMeasurementCycle()) {
    logger.debug(F("MeasurementCycle"),
                 m_sensor->getName() + F(": Starte Flush der ausstehenden Updates"));
  }
  SensorPersistence::flushPendingUpdatesForSensor(m_sensor->getId());
  if (ConfigMgr.isDebugMeasurementCycle()) {
    logger.debug(F("MeasurementCycle"), m_sensor->getName() + F(": Flush abgeschlossen"));
  }

  // Check if this sensor needs deinitialization
  bool shouldDeinit = m_sensor->shouldDeinitializeAfterMeasurement();

  if (shouldDeinit) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"), m_sensor->getName() + F(": Sensor deinitialisieren"));
    }
    m_sensor->deinitialize();
  }

  // CRITICAL: Release measurement slot AFTER all cleanup is done
  // This prevents other sensors from starting measurement while we're still
  // flushing data or deinitializing
  SensorManagerLimiter::getInstance().releaseSlot(m_sensor->getId());
  if (ConfigMgr.isDebugMeasurementCycle()) {
    logger.debug(F("MeasurementCycle"),
                 m_sensor->getName() + F(": Messslot nach Cleanup freigegeben"));
  }

  // Calculate next measurement time safely
  unsigned long now = millis();
  unsigned long interval = m_state.measurementInterval;

  // Prevent overflow by checking if we're close to rollover
  if (now + interval < now) {
    // Handle rollover case by scheduling for immediate measurement
    m_state.scheduleNextMeasurement(now, 0);
  } else {
    m_state.scheduleNextMeasurement(now, interval);
  }

  if (ConfigMgr.isDebugMeasurementCycle()) {
    unsigned long elapsed = now - m_cycleStartTime;
    unsigned long nextIn = m_state.nextDueTime > now ? m_state.nextDueTime - now : 0;

    logger.debug(F("MeasurementCycle"), m_sensor->getName() + F(": Messzyklus abgeschlossen in ") +
                                            String(elapsed) + F(" ms, nächste Messung in ") +
                                            String(nextIn) + F(" ms"));
  }

  // **CRITICAL FIX: Add debug logging for measurement data if not already
  // logged**
  if (ConfigMgr.isDebugMeasurementCycle()) {
    const auto& data = m_sensor->getMeasurementData();

    // CRITICAL: Validate data before logging
    if (!data.isValid()) {
      logger.debug(F("MeasurementCycle"), F("Messdaten ungültig, Debug-Logging überspringen"));
    } else {
      logger.debug(F("MeasurementCycle"), F("Messdaten für ") + m_sensor->getName() +
                                              F(": Felder=") +
                                              String(SensorConfig::MAX_MEASUREMENTS) +
                                              F(", Ergebnisse=") + String(m_currentResults.size()));

      // Log each field name and unit with bounds checking
      size_t maxDebugFields = std::min(m_currentResults.size(), SensorConfig::MAX_MEASUREMENTS);
      maxDebugFields = std::min(maxDebugFields, SensorConfig::MAX_MEASUREMENTS);

      for (size_t i = 0; i < maxDebugFields; i++) {
        // Safe string conversion for NaN values
        String valueStr;
        if (isnan(m_currentResults[i])) {
          valueStr = "NaN";
        } else {
          valueStr = String(m_currentResults[i], 2);
        }
        logger.debug(F("MeasurementCycle"), F("Feld ") + String(i) + F(": Name='") +
                                                String(data.fieldNames[i]) + F("' Wert='") +
                                                valueStr + F("' Einheit='") +
                                                String(data.units[i]) + F("'"));
      }
    }
  }

  m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
}

void SensorMeasurementCycleManager::logMeasurementResults() {
  if (m_currentResults.empty())
    return;

  String summary = m_sensor->getName() + F(" Messungen:");

  // **CRITICAL FIX: Use sensor's measurement data directly**
  const auto& measurementData = m_sensor->getMeasurementData();
  size_t maxFields = std::min(m_currentResults.size(), SensorConfig::MAX_MEASUREMENTS);
  maxFields = std::min(maxFields, SensorConfig::MAX_MEASUREMENTS);

  for (size_t i = 0; i < maxFields; i++) {
    if (i >= SensorConfig::MAX_MEASUREMENTS || i >= m_currentResults.size()) {
      logger.error(F("MeasurementCycle"), F("Index außerhalb des Bereichs: ") + String(i));
      continue;
    }
    String fieldName = measurementData.fieldNames[i];
    if (fieldName.isEmpty()) {
      fieldName = F("wert_") + String(i + 1);
    }
    // Safe string conversion for NaN values
    String valueStr;
    if (isnan(m_currentResults[i])) {
      valueStr = "NaN";
    } else {
      valueStr = String(m_currentResults[i], 2);
    }
    summary += F(" ") + fieldName + F("=") + valueStr + measurementData.units[i];
  }

  logger.info(F("MeasurementCycle"), summary);
}
