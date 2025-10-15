#include "sensors/sensor_factory.h"

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "sensors/sensor_config.h"

// Implementation of helper methods
bool SensorFactory::validateSensorConfig(const Sensor* sensor) {
  if (!sensor) return false;

  bool valid = true;
  if (sensor->getId().isEmpty()) {
    logger.error(F("SensorFactory"), F("Sensor hat keine ID"));
    valid = false;
  }

  if (sensor->getName().isEmpty()) {
    logger.error(F("SensorFactory"),
                 F("Sensor ") + sensor->getId() + F(" hat keinen Namen"));
    valid = false;
  }

  // Add explicit check for measurement interval
  if (sensor->getMeasurementInterval() < MEASUREMENT_MINIMUM_DELAY) {
  logger.error(F("SensorFactory"),
         F("Sensor ") + sensor->getId() +
           F(" hat ein ungültiges Messintervall: ") +
           String(sensor->getMeasurementInterval()) +
           F(" (Minimum: ") + String(MEASUREMENT_MINIMUM_DELAY) +
           F(")"));
    valid = false;
  }

  return valid;
}

void SensorFactory::logSensorStatus(const String& phase, const Sensor* sensor) {
  if (!sensor) return;

  logger.debug(F("SensorFactory"),
               phase + F(": Sensor ") + sensor->getName() + F(" [ID: ") +
                   sensor->getId() + F(", Aktiv: ") +
                   String(sensor->isEnabled() ? "ja" : "nein") +
                   F(", Fehler: ") + String(sensor->getErrorCount()) +
                   F(", Status: ") + sensor->getStatus() + F("]"));
}

SensorResult SensorFactory::initializeSensor(std::unique_ptr<Sensor>& sensor) {
  if (!sensor) {
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              "Null sensor pointer");
  }

  logger.debug(F("SensorFactory"),
               F("Beginne Initialisierung für ") + sensor->getName());

  // Basic initialization
  auto initResult = sensor->init();
  if (!initResult.isSuccess()) {
    logger.error(F("SensorFactory"), F("Konnte ") +
                                         sensor->getName() +
                                         F(" nicht initialisieren - sensor->init() fehlgeschlagen"));
    sensor->setEnabled(false);
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR);
  }

  // After basic initialization, apply threshold overrides if present
  String sensorId = sensor->getId();

  // All per-sensor and per-measurement config is now loaded directly in config.
  // No action needed here unless you want to override from another source.

  sensor->setEnabled(true);
  logger.debug(F("SensorFactory"),
               sensor->getName() + F(" erfolgreich initialisiert"));
  return SensorResult::success();
}

SensorFactory::SensorResult SensorFactory::createAllSensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
  logger.info(F("SensorFactory"), F("Starte Sensor-Erstellungsprozess"));

  try {
    logger.logMemoryStats(F("before_sensor_creation"));
    sensors.clear();

    std::vector<String> errors;

#if USE_DHT
    addDHTSensors(sensors, sensorManager, errors);
#endif


#if USE_ANALOG
    addAnalogSensors(sensors, sensorManager, errors);
#endif

    logger.logMemoryStats(F("nach_sensor_erstellung"));

    // If we have any sensors initialized, consider it a partial success
    if (!sensors.empty()) {
      if (errors.empty()) {
        return SensorResult::success();
      } else {
        // Join all errors with semicolons
        String errorMsg;
        for (size_t i = 0; i < errors.size(); i++) {
          if (i > 0) errorMsg += F("; ");
          errorMsg += errors[i];
        }
        return SensorResult::partialSuccess(errorMsg);
      }
    }

    // If no sensors were initialized at all, return failure
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Keine Sensoren konnten initialisiert werden"));
  } catch (const std::exception& e) {
    logger.error(F("SensorFactory"),
                 F("Ausnahme während der Sensorerstellung: ") + String(e.what()));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, e.what());
  } catch (...) {
    logger.error(F("SensorFactory"),
                 F("Unbekannte Ausnahme während der Sensorerstellung"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR);
  }
}

SensorFactory::SensorResult SensorFactory::createDHTSensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_DHT
  DHTConfig config;
  auto dhtSensor = std::make_unique<DHTSensor>(config, sensorManager);

  // Move into base class pointer for initialization
  std::unique_ptr<Sensor> baseSensor(dhtSensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess()) {
    return result;
  }

  // Move into sensors vector
  sensors.push_back(std::move(baseSensor));
#endif
  return SensorResult::success();
}

SensorFactory::SensorResult SensorFactory::createAnalogSensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_ANALOG
  AnalogConfig config;
  auto analogSensor = std::make_unique<AnalogSensor>(config, sensorManager);

  // Move into base class pointer for initialization
  std::unique_ptr<Sensor> baseSensor(analogSensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess()) {
    return result;
  }

  // Move into sensors vector
  sensors.push_back(std::move(baseSensor));
#endif
  return SensorResult::success();
}

// Memory-optimized sensor creation helper functions
#if USE_DHT
void SensorFactory::addDHTSensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager,
    std::vector<String>& errors) {
  auto dhtResult = createDHTSensors(sensors, sensorManager);
  if (!dhtResult.isSuccess()) {
    errors.push_back(F("DHT: ") + dhtResult.getFullErrorMessage());
    logger.error(F("SensorFactory"),
                 F("Erstellung DHT-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
  }
}
#endif

#if USE_ANALOG
void SensorFactory::addAnalogSensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager,
    std::vector<String>& errors) {
  auto analogResult = createAnalogSensors(sensors, sensorManager);
  if (!analogResult.isSuccess()) {
    errors.push_back(F("Analog: ") + analogResult.getFullErrorMessage());
    logger.error(F("SensorFactory"),
                 F("Erstellung Analog-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
  }
}
#endif
