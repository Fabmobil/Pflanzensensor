#include "managers/manager_sensor.h"

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"

// Global instance
std::unique_ptr<SensorManager> sensorManager;

// Most implementation is in the header due to templates and inline methods.
// This file mainly provides the global instance and any non-inline
// implementations that might be needed in the future.

void SensorManager::applySensorSettingsFromConfig() {
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorM"), F("Wende Sensoreinstellungen aus der Konfiguration an"));
  }

  logger.info(F("SensorM"), F("Sensoreinstellungen aus der Konfiguration werden angewendet"));

  // Load sensor configuration from file
  auto result = SensorPersistence::load();
  if (!result.isSuccess()) {
    logger.warning(F("SensorM"),
                   F("Sensor-Konfiguration konnte nicht geladen werden: ") + result.getMessage());
    return;
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorM"), F("Sensor-Konfiguration erfolgreich aus Datei geladen"));
  }

  logger.info(F("SensorM"), F("Sensoreinstellungen erfolgreich angewendet"));
}
