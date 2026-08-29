/**
 * @file web_helper_sensors.cpp
 * @brief Implementierung der Sensor-Hilfsklasse
 */

#include "web/core/web_helper_sensors.h"
#include "managers/manager_sensor.h"

const std::vector<std::unique_ptr<Sensor>>& SensorHelper::getAllSensors(SensorManager& manager) {
  return manager.getSensors();
}

std::vector<Sensor*> SensorHelper::getEnabledSensors(SensorManager& manager) {
  std::vector<Sensor*> enabled;
  const auto& sensors = manager.getSensors();

  for (const auto& sensor : sensors) {
    if (sensor && sensor->isEnabled() && sensor->isInitialized()) {
      enabled.push_back(sensor.get());
    }
  }

  return enabled;
}

Sensor* SensorHelper::findById(SensorManager& manager, const String& sensorId) {
  const auto& sensors = manager.getSensors();
  return findById(sensors, sensorId);
}

Sensor* SensorHelper::findById(const std::vector<std::unique_ptr<Sensor>>& sensors,
                               const String& sensorId) {
  for (const auto& sensor : sensors) {
    if (sensor && sensor->getId() == sensorId) {
      return sensor.get();
    }
  }
  return nullptr;
}

bool SensorHelper::isValidMeasurementIndex(const Sensor* sensor, size_t measurementIndex) {
  if (!sensor)
    return false;

  const auto& config = sensor->config();
  return measurementIndex < config.activeMeasurements;
}

bool SensorHelper::isReady(const Sensor* sensor) {
  return sensor && sensor->isInitialized() && sensor->isEnabled();
}

String SensorHelper::getSensorIdsAsString(SensorManager& manager) {
  String result;
  const auto& sensors = getEnabledSensors(manager);

  for (size_t i = 0; i < sensors.size(); i++) {
    if (i > 0)
      result += ",";
    result += sensors[i]->getId();
  }

  return result;
}

size_t SensorHelper::getEnabledCount(SensorManager& manager) {
  return getEnabledSensors(manager).size();
}

bool SensorHelper::hasEnabledSensors(SensorManager& manager) {
  return getEnabledCount(manager) > 0;
}