/**
 * @file main_sensors.cpp
 * @brief Sensor manager initialization module
 * @details Handles sensor factory initialization, configuration loading,
 *          and measurement cycle setup
 */

#include <Arduino.h>

#include "chronik/chronik_recorder.h"
#include "chronik/chronik_store.h"
#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_display.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"
#include "sensors/sensor_measurement_cycle.h"

// Forward declarations for globals defined in main.cpp
extern std::unique_ptr<SensorManager> sensorManager;
extern std::unique_ptr<DisplayManager> displayManager;
extern WebManager& webManager;

/**
 * @brief Initialize sensor manager
 * @return ResourceResult indicating success or failure
 * @details
 * 1. Create sensors via factory
 * 2. Load sensor configuration
 * 3. Apply settings
 * 4. Log status
 */
ResourceResult initializeSensors() {
  LOG_INFO(F("main_sensors"), F("Sensor-Manager Initialisierung gestartet"));

  auto result = ResourceManager::getInstance().enterCriticalOperation(F("SensorInit"));
  if (!result.isSuccess()) {
    LOG_ERROR(F("main_sensors"),
              String(F("Kritischer Operationseintritt fehlgeschlagen: ")) + result.getMessage());
    return result;
  }

  // Initialize sensor manager
  if (!sensorManager) {
    sensorManager = std::make_unique<SensorManager>();
  }

  ResourceResult sensorResult = sensorManager->init();
  if (!sensorResult.isSuccess()) {
    LOG_ERROR(F("main_sensors"), String(F("Sensor-Manager Initialisierung fehlgeschlagen: ")) +
                                     sensorResult.getMessage());
    ResourceManager::getInstance().exitCriticalOperation();
#if USE_DISPLAY
    if (displayManager) {
      displayManager->updateLogStatus(F("Sensor Fehler"), true);
    }
#endif
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, sensorResult.getMessage());
  }

#if USE_DISPLAY
  if (displayManager) {
    displayManager->logEnabledSensors();
  }
#endif

  // Chronik ankoppeln: der Messzyklus kennt nur einen Funktionszeiger, der
  // Empfänger sitzt hier. Erst nach der Sensorinitialisierung, weil die
  // Kanaltabelle die fertig aufgebauten Sensoren braucht.
  SensorMeasurementCycleManager::setMeasurementDoneCallback(&ChronikRecorder::onMeasurementDone);
  ChronikStore::instance().setTableProvider(&ChronikRecorder::writeChannelTable);
  ChronikStore::instance().begin();

  LOG_INFO(F("main_sensors"), F("Sensor-Manager Initialisierung erfolgreich"));
  ResourceManager::getInstance().exitCriticalOperation();

  return ResourceResult::success();
}

/**
 * @brief Display sensor status on screen
 */
void showSensorStatus() {
#if USE_DISPLAY
  if (displayManager && sensorManager && sensorManager->isHealthy()) {
    displayManager->logEnabledSensors();
  }
#endif
}
