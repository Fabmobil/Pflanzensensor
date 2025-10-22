#include "sensor_measurement_cycle.h"

void SensorMeasurementCycleManager::handleError() {
  if (m_lastState != MeasurementState::WAITING_FOR_DUE &&
      m_lastState != MeasurementState::WAITING_FOR_SLOT) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"), m_sensor->getName() + F(": Releasing slot due to error"));
    }
    SensorManagerLimiter::getInstance().releaseSlot(m_sensor->getId());
  }

  // Only increment error count for sensor-related errors
  if (m_lastState != MeasurementState::SENDING_INFLUX) {
    m_state.errorCount++;
    if (m_state.errorCount >= MEASUREMENT_ERROR_COUNT) {
      // Try to reinitialize first
      logger.warning(F("MeasurementCycle"),
                     m_sensor->getName() + F(": Max errors reached, attempting reinitialization"));

      if (m_sensor->isInitialized()) {
        m_sensor->deinitialize();
      }

      if (!m_sensor->init()) {
        // Reinitialization failed, mark sensor as having persistent error
        logger.error(F("MeasurementCycle"),
                     m_sensor->getName() +
                         F(": Reinitialization failed, marking as persistently failed"));
        m_sensor->mutableConfig().hasPersistentError = true;

        // New: Check if this is a DS18B20 sensor and trigger reboot
        if (m_sensor->getSharedHardwareInfo().type == SensorType::DS18B20) {
          logger.error(F("MeasurementCycle"),
                       m_sensor->getName() + F(": DS18B20 failure detected, triggering reboot"));
          // Allow time for logging to complete
          delay(1000);
          ESP.restart();
          return; // Never reached, but good practice
        }

        // For non-DS18B20 sensors, continue with existing behavior
        if (!m_sensor->config().hasPersistentError) {
          logger.error(F("MeasurementCycle"),
                       m_sensor->getName() + F(": First-time failure, triggering reboot"));
          ESP.restart();
          return;
        }
      } else {
        // Reinitialization succeeded, clear any persistent error
        if (m_sensor->config().hasPersistentError) {
          logger.info(F("MeasurementCycle"),
                      m_sensor->getName() +
                          F(": Successfully reinitialized after persistent failure"));
          m_sensor->mutableConfig().hasPersistentError = false;
        }
        // Reset error count since reinitialization succeeded
        m_state.errorCount = 0;
        m_state.scheduleNextMeasurement(millis(), m_state.measurementInterval);
        m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
        return;
      }

      deactivateSensor();
      return;
    }
  }

  if (m_sensor->isInitialized()) {
    m_sensor->deinitialize();
    m_state.needsInitialization = true;
  }

  if (millis() - m_state.lastErrorTime >= ERROR_RETRY_DELAY) {
    m_state.scheduleNextMeasurement(millis(), m_state.measurementInterval);
    m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
  }
}

void SensorMeasurementCycleManager::handleUnknownState() {
  handleStateError(F("Unknown state encountered"));
}

void SensorMeasurementCycleManager::handleStateError(const String& error) {
  m_lastState = m_state.state;
  m_state.recordError(error);

  // Release slot if we were holding it
  if (m_lastState != MeasurementState::WAITING_FOR_DUE &&
      m_lastState != MeasurementState::WAITING_FOR_SLOT) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"), m_sensor->getName() + F(": Releasing slot due to error"));
    }
    SensorManagerLimiter::getInstance().releaseSlot(m_sensor->getId());
  }

  m_state.setState(MeasurementState::ERROR, m_sensor->getName());

  // Log error with appropriate severity based on error type
  if (m_lastState == MeasurementState::SENDING_INFLUX) {
    logger.warning(F("MeasurementCycle"), F("Network error: ") + error);
  } else {
    logger.error(F("MeasurementCycle"), F("Sensor error: ") + error);
  }
}

void SensorMeasurementCycleManager::handleException(const std::exception& e) {
  // Special handling for DS18B20 init retries
  if (String(e.what()) == "DS18B20_INIT_RETRY") {
    // Don't treat this as an error, just let the retry logic continue
    return;
  }

  String error = F("Exception in measurement cycle: ");
  error += e.what();
  handleStateError(error);
}

void SensorMeasurementCycleManager::deactivateSensor() {
  if (m_sensor) {
    logger.warning(F("MeasurementCycle"), F("Deactivated sensor after ") +
                                              String(m_state.errorCount) +
                                              F(" consecutive errors: ") + m_sensor->getName());
    m_sensor->setEnabled(false);
  }
}
