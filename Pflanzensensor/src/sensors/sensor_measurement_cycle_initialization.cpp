#include "sensor_measurement_cycle.h"
#include "sensors/sensor_ds18b20.h"

void SensorMeasurementCycleManager::handleInitializing() {
  if (ConfigMgr.isDebugMeasurementCycle()) {
    logger.debug(F("MeasurementCycle"),
                 m_sensor->getName() + F(": Beginne Initialisierung"));
  }

  // Validate memory state before initialization
  auto memoryResult = m_sensor->validateMemoryState();
  if (!memoryResult.isSuccess()) {
  logger.error(F("MeasurementCycle"),
         m_sensor->getName() +
           F(": Speicherüberprüfung vor Initialisierung fehlgeschlagen"));

    // Attempt memory state reset
    auto resetResult = m_sensor->resetMemoryState();
    if (!resetResult.isSuccess()) {
      logger.error(F("MeasurementCycle"),
                   m_sensor->getName() + F(": Speicherzurücksetzung fehlgeschlagen"));
      handleStateError(F("Speicherüberprüfung und Rücksetzung fehlgeschlagen"));
      return;
    }

    // Revalidate after reset
    memoryResult = m_sensor->validateMemoryState();
    if (!memoryResult.isSuccess()) {
      logger.error(F("MeasurementCycle"),
                   m_sensor->getName() +
                       F(": Speicherüberprüfung nach Rücksetzung weiterhin fehlgeschlagen"));
      handleStateError(F("Speicherüberprüfung nach Rücksetzung fehlgeschlagen"));
      return;
    }

  logger.info(
    F("MeasurementCycle"),
    m_sensor->getName() + F(": Speicherzustand erfolgreich wiederhergestellt"));
  }

  // Check if sensor needs initialization
  if (!m_sensor->isInitialized()) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(
          F("MeasurementCycle"),
          m_sensor->getName() + F(": Sensor nicht initialisiert, rufe init() auf"));
    }

    auto initResult = m_sensor->init();
    if (!initResult.isSuccess()) {
      logger.error(F("MeasurementCycle"),
                   m_sensor->getName() + F(": Sensorinitialisierung fehlgeschlagen: ") +
                       initResult.getMessage());
      handleStateError(F("Sensorinitialisierung fehlgeschlagen"));
      return;
    }

    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(
          F("MeasurementCycle"),
          m_sensor->getName() + F(": Sensorinitialisierung erfolgreich"));
    }
  } else {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"),
                   m_sensor->getName() + F(": Sensor bereits initialisiert"));
    }
  }

  // Check if this is a DS18B20 sensor and if it's requesting a restart
#if USE_DS18B20
  if (m_sensor->getSharedHardwareInfo().type == SensorType::DS18B20) {
    const DS18B20Sensor* ds18b20 = static_cast<const DS18B20Sensor*>(m_sensor);
    if (ds18b20->isRestartRequested()) {
      logger.warning(
          F("MeasurementCycle"),
          m_sensor->getName() +
              F(": Neustart vom Sensor angefordert, führe sauberen Neustart aus"));
      // Allow time for logging and cleanup
      delay(1000);
      ESP.restart();
      return;
    }
  }
#endif

  // Check for initialization errors and handle retries
  if (!m_sensor->isInitialized()) {
    // For all sensors, allow retry attempts during initialization phase
    // This prevents sensors from being immediately disabled due to temporary
    // failures
    if (m_state.errorCount < MEASUREMENT_ERROR_COUNT) {
      // Increment error count for this failed attempt
      m_state.errorCount++;

      // Stay in INITIALIZING state to allow retries
      if (ConfigMgr.isDebugMeasurementCycle()) {
        logger.debug(F("MeasurementCycle"),
                     m_sensor->getName() +
                         F(": Initialisierung fehlgeschlagen, versuche erneut (Versuch ") +
                         String(m_state.errorCount) + F("/") +
                         String(MEASUREMENT_ERROR_COUNT) + F(")"));
      }
      return;  // Stay in INITIALIZING state for retry
    }

    // Only treat as fatal error after max retries exceeded
  logger.error(F("MeasurementCycle"),
         m_sensor->getName() + F(": Initialisierung nach ") +
           String(MEASUREMENT_ERROR_COUNT) + F(" Versuchen fehlgeschlagen"));
  handleStateError(F("Initialisierung nach maximalen Versuchen fehlgeschlagen"));
    return;
  }

  // Validate memory state after initialization
  memoryResult = m_sensor->validateMemoryState();
  if (!memoryResult.isSuccess()) {
    logger.error(F("MeasurementCycle"),
                 m_sensor->getName() +
                     F(": Speicherüberprüfung nach Initialisierung fehlgeschlagen"));
    handleStateError(F("Speicherüberprüfung nach Initialisierung fehlgeschlagen"));
    return;
  }

  if (ConfigMgr.isDebugMeasurementCycle()) {
    logger.debug(F("MeasurementCycle"),
                 m_sensor->getName() + F(": Initialisierung erfolgreich"));
  }

  m_state.needsInitialization = false;
  m_state.setMinimumDelay(INIT_DELAY);
  m_state.setState(MeasurementState::WAITING_FOR_DELAY, m_sensor->getName());
}
