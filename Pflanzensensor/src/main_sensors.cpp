/**
 * @file main_sensors.cpp
 * @brief Sensor manager initialization module
 * @details Handles sensor factory initialization, configuration loading,
 *          and measurement cycle setup
 */

#include <Arduino.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_display.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"

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
  logger.info(F("main_sensors"), F("Sensor-Manager Initialisierung gestartet"));

  auto result = ResourceManager::getInstance().enterCriticalOperation(F("SensorInit"));
  if (!result.isSuccess()) {
    logger.error(F("main_sensors"),
                 String(F("Kritischer Operationseintritt fehlgeschlagen: ")) + result.getMessage());
    return result;
  }

  // Initialize sensor manager
  if (!sensorManager) {
    sensorManager = std::make_unique<SensorManager>();
  }

  ResourceResult sensorResult = sensorManager->init();
  if (!sensorResult.isSuccess()) {
    logger.error(F("main_sensors"), String(F("Sensor-Manager Initialisierung fehlgeschlagen: ")) +
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

  logger.info(F("main_sensors"), F("Sensor-Manager Initialisierung erfolgreich"));
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
