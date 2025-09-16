#include "sensor_measurement_cycle.h"

#if USE_DS18B20
#include "sensors/sensor_ds18b20.h"  // Add include for DS18B20 sensor
#endif
SensorMeasurementCycleManager::SensorMeasurementCycleManager(Sensor* sensor)
    : m_sensor(sensor),
      m_state(),
      m_lastState(MeasurementState::WAITING_FOR_DUE),
      m_lastSlotAttemptTime(0) {
  if (m_sensor) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(
          F("MeasurementCycle"),
          F("Initialisiere Zyklus-Manager für Sensor: ") + m_sensor->getName());
    }

    // Check warmup requirements
    m_state.needsWarmup = m_sensor->requiresWarmup(m_state.warmupTimeNeeded);
    if (m_state.needsWarmup) {
      m_state.warmupStartTime = millis();  // Starte Aufwärmphase sofort
      if (ConfigMgr.isDebugMeasurementCycle()) {
        logger.debug(F("MeasurementCycle"),
                     m_sensor->getName() + F(": Starte Aufwärmphase von ") +
                         String(m_state.warmupTimeNeeded / 1000UL) + F("s"));
      }
    }

    // Store the measurement interval
    m_state.measurementInterval = m_sensor->getMeasurementInterval();

    // Record the start time of the first cycle
    m_cycleStartTime = millis();

    // Schedule first measurement based on cycle start time
    m_state.scheduleNextMeasurement(m_cycleStartTime, 0);  // Start immediately
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"),
                   F("Erste Messung für sofortige Ausführung geplant"));
    }
  } else {
    logger.error(F("MeasurementCycle"), F("Created with null sensor!"));
  }
}

bool SensorMeasurementCycleManager::updateMeasurementCycle() {
  if (!m_sensor) {
    return false;
  }

  // Update measurement interval in case it changed
  unsigned long currentInterval = m_sensor->getMeasurementInterval();
  if (currentInterval != m_state.measurementInterval) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"),
                   m_sensor->getName() +
                       F(": Messintervall aktualisiert von ") +
                       String(m_state.measurementInterval) + F("ms auf ") +
                       String(currentInterval) + F("ms"));
    }
    m_state.measurementInterval = currentInterval;
  }

  try {
    switch (m_state.state) {
      case MeasurementState::WAITING_FOR_DUE:
        return handleWaitingForDue();
      case MeasurementState::WAITING_FOR_SLOT:
        handleWaitingForSlot();
        break;
      case MeasurementState::WAITING_FOR_DELAY:
        handleWaitingForDelay();
        break;
      case MeasurementState::INITIALIZING:
        handleInitializing();
        break;
      case MeasurementState::WARMUP:
        handleWarmup();
        break;
      case MeasurementState::MEASURING:
        handleMeasuring();
        break;
      case MeasurementState::PROCESSING:
        handleProcessing();
        break;
      case MeasurementState::SENDING_INFLUX:
        handleSendingInflux();
        break;
      case MeasurementState::DEINITIALIZING:
        handleDeinitializing();
        break;
      case MeasurementState::ERROR:
        handleError();
        break;
      default:
        handleUnknownState();
        break;
    }
  } catch (const std::exception& e) {
    handleException(e);
  }

  return false;
}

void SensorMeasurementCycleManager::reset() {
  m_state.reset();
  m_lastState = MeasurementState::WAITING_FOR_DUE;
  m_currentResults.clear();
  m_cycleStartTime = 0;
  m_lastDebugTime = 0;
  m_lastSlotAttemptTime = 0;  // Reset slot attempt time
}

MeasurementState SensorMeasurementCycleManager::getCurrentState() const {
  return m_state.state;
}

const String& SensorMeasurementCycleManager::getLastError() const {
  return m_state.lastError;
}
