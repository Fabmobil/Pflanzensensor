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
  LOG_DEBUG(F("MeasurementCycle"),
            String(F("Verarbeite: Feldnamen=")) + String(SensorConfig::MAX_MEASUREMENTS) +
                String(F(", Einheiten=")) + String(SensorConfig::MAX_MEASUREMENTS) +
                String(F(", Werte=")) + String(m_currentResults.size()) +
                String(F(", currentResults=")) + String(m_currentResults.size()));

  // CRITICAL: Validate measurement data before processing
  if (!currentData.isValid()) {
    LOG_ERROR(F("MeasurementCycle"), F("Ungültige Messdatenstruktur"));
    handleStateError(F("Ungültige Messdatenstruktur"));
    return;
  }

  // Validate array sizes to prevent bounds violations
  if (m_currentResults.size() != currentData.activeValues) {
    LOG_ERROR(F("MeasurementCycle"),
              String(F("Größenabweichung der Messdatenarray: currentResults=")) +
                  String(m_currentResults.size()) + String(F(", activeValues=")) +
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
      LOG_ERROR(F("MeasurementCycle"),
                String(F("Index außerhalb des Bereichs bei Verarbeitung: ")) + String(i));
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
          LOG_DEBUG(F("MeasurementCycle"), String(F("Absolute Min/Max aktualisiert für Sensor ")) +
                                               m_sensor->getId() + String(F(" Messung ")) +
                                               String(i));
        }

        // Letzten Messwert aktualisieren.
        //
        // Der Wert steht sofort im RAM zur Verfügung (Web-UI, Display,
        // /metrics lesen aus der Runtime-Config). Das Schreiben läuft über
        // dieselbe Write-Behind-Queue wie Min/Max und wird beim Flush am Ende
        // des Messzyklus mit den übrigen Änderungen derselben Messung in einem
        // einzigen Lade-/Speicher-Zyklus zusammengefasst.
        //
        // Vorher wurde hier bei jeder Wertänderung sofort
        // updateMeasurementSettings() aufgerufen - also ein eigenes
        // Laden + Serialisieren + remove + rename der Messungs-JSON pro Kanal
        // und Messung, zusätzlich zum ohnehin folgenden Flush.
        {
          float prevLast = config.measurements[i].lastValue;
          if (isnan(prevLast) || (!isnan(value) && fabs(prevLast - value) > 1e-6f)) {
            config.measurements[i].lastValue = value;
            SensorPersistence::enqueueLastValue(m_sensor->getId(), i, value,
                                                config.measurements[i].lastRawValue);
          }
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

  // Messdaten am Sensor aktualisieren (Zeitstempel wurde bereits in handleMeasuring() gesetzt)
  m_sensor->updateMeasurementData(updatedData);

  // Update status for all measurements
  for (size_t i = 0; i < updatedData.activeValues; i++) {
    m_sensor->updateStatus(i);
  }
  logMeasurementResults();

  // NOTE: Slot will be released in handleDeinitializing() AFTER all cleanup
  // to prevent other sensors from interfering while we're still cleaning up

  m_state.setState(MeasurementState::DEINITIALIZING, m_sensor->getName());
}
void SensorMeasurementCycleManager::handleDeinitializing() {
  // CRITICAL: Flush pending updates for THIS sensor immediately after measurement cycle
  // This ensures data is persisted right away instead of waiting for periodic flush
  if (ConfigMgr.isDebugMeasurementCycle()) {
    LOG_DEBUG(F("MeasurementCycle"),
              m_sensor->getName() + F(": Starte Flush der ausstehenden Updates"));
  }
  SensorPersistence::flushPendingUpdatesForSensor(m_sensor->getId());
  if (ConfigMgr.isDebugMeasurementCycle()) {
    LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + F(": Flush abgeschlossen"));
  }

  // Check if this sensor needs deinitialization
  bool shouldDeinit = m_sensor->shouldDeinitializeAfterMeasurement();

  if (shouldDeinit) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + F(": Sensor deinitialisieren"));
    }
    m_sensor->deinitialize();
  }

  // Der Zyklus ist gelaufen — eine eventuell erzwungene Messung ist damit
  // erledigt und der Sensor kehrt in den normalen Takt zurück.
  m_forced = false;

  // CRITICAL: Release measurement slot AFTER all cleanup is done
  // This prevents other sensors from starting measurement while we're still
  // flushing data or deinitializing
  SensorManagerLimiter::getInstance().releaseSlot(m_sensor->getId());
  if (ConfigMgr.isDebugMeasurementCycle()) {
    LOG_DEBUG(F("MeasurementCycle"),
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

    LOG_DEBUG(F("MeasurementCycle"),
              m_sensor->getName() + String(F(": Messzyklus abgeschlossen in ")) + String(elapsed) +
                  String(F(" ms, nächste Messung in ")) + String(nextIn) + F(" ms"));
  }

  // **CRITICAL FIX: Add debug logging for measurement data if not already
  // logged**
  if (ConfigMgr.isDebugMeasurementCycle()) {
    const auto& data = m_sensor->getMeasurementData();

    // CRITICAL: Validate data before logging
    if (!data.isValid()) {
      LOG_DEBUG(F("MeasurementCycle"), F("Messdaten ungültig, Debug-Logging überspringen"));
    } else {
      LOG_DEBUG(F("MeasurementCycle"),
                String(F("Messdaten für ")) + m_sensor->getName() + String(F(": Felder=")) +
                    String(SensorConfig::MAX_MEASUREMENTS) + String(F(", Ergebnisse=")) +
                    String(m_currentResults.size()));

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
        LOG_DEBUG(F("MeasurementCycle"), String(F("Feld ")) + String(i) + String(F(": Name='")) +
                                             String(data.fieldNames[i]) + String(F("' Wert='")) +
                                             valueStr + String(F("' Einheit='")) +
                                             String(data.units[i]) + F("'"));
      }
    }
  }

  m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
}

void SensorMeasurementCycleManager::logMeasurementResults() {
  if (m_currentResults.empty())
    return;

  String summary = m_sensor->getName() + String(F(" Messungen:"));

  // **CRITICAL FIX: Use sensor's measurement data directly**
  const auto& measurementData = m_sensor->getMeasurementData();
  size_t maxFields = std::min(m_currentResults.size(), SensorConfig::MAX_MEASUREMENTS);
  maxFields = std::min(maxFields, SensorConfig::MAX_MEASUREMENTS);

  for (size_t i = 0; i < maxFields; i++) {
    if (i >= SensorConfig::MAX_MEASUREMENTS || i >= m_currentResults.size()) {
      LOG_ERROR(F("MeasurementCycle"), String(F("Index außerhalb des Bereichs: ")) + String(i));
      continue;
    }
    String fieldName = measurementData.fieldNames[i];
    if (fieldName.isEmpty()) {
      fieldName = String(F("wert_")) + String(i + 1);
    }
    // Safe string conversion for NaN values
    String valueStr;
    if (isnan(m_currentResults[i])) {
      valueStr = "NaN";
    } else {
      valueStr = String(m_currentResults[i], 2);
    }
    summary += String(F(" ")) + fieldName + String(F("=")) + valueStr + measurementData.units[i];
  }

  LOG_INFO(F("MeasurementCycle"), summary);
}
