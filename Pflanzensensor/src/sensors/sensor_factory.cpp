#include "sensors/sensor_factory.h"

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "sensors/sensor_config.h"

// Sensor-spezifische Includes (bedingte Kompilierung)
#if USE_DS18B20
#include "sensors/sensor_ds18b20.h"
#endif
#if USE_SDS011
#include "sensors/sensor_sds011.h"
#endif
#if USE_MHZ19
#include "sensors/sensor_mhz19.h"
#endif
#if USE_HX711
#include "sensors/sensor_hx711.h"
#endif
#if USE_BMP280
#include "sensors/sensor_bmp280.h"
#endif
#if USE_SERIAL_RECEIVER
#include "sensors/sensor_serial_receiver.h"
#endif

// Implementation of helper methods
bool SensorFactory::validateSensorConfig(const Sensor* sensor) {
  if (!sensor)
    return false;

  bool valid = true;
  if (sensor->getId().isEmpty()) {
    logger.error(F("SensorFactory"), F("Sensor hat keine ID"));
    valid = false;
  }

  if (sensor->getName().isEmpty()) {
    logger.error(F("SensorFactory"),
                 String(F("Sensor ")) + sensor->getId() + F(" hat keinen Namen"));
    valid = false;
  }

  // Add explicit check for measurement interval
  if (sensor->getMeasurementInterval() < MEASUREMENT_MINIMUM_DELAY) {
    logger.error(F("SensorFactory"), String(F("Sensor ")) + sensor->getId() +
                                         String(F(" hat ein ungültiges Messintervall: ")) +
                                         String(sensor->getMeasurementInterval()) +
                                         String(F(" (Minimum: ")) +
                                         String(MEASUREMENT_MINIMUM_DELAY) + F(")"));
    valid = false;
  }

  return valid;
}

void SensorFactory::logSensorStatus(const String& phase, const Sensor* sensor) {
  if (!sensor)
    return;

  logger.debug(F("SensorFactory"), phase + String(F(": Sensor ")) + sensor->getName() +
                                       String(F(" [ID: ")) + sensor->getId() +
                                       String(F(", Aktiv: ")) +
                                       String(sensor->isEnabled() ? "ja" : "nein") +
                                       String(F(", Fehler: ")) + String(sensor->getErrorCount()) +
                                       String(F(", Status: ")) + sensor->getStatus() + F("]"));
}

SensorResult SensorFactory::initializeSensor(std::unique_ptr<Sensor>& sensor) {
  if (!sensor) {
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, "Null sensor pointer");
  }

  logger.debug(F("SensorFactory"), String(F("Beginne Initialisierung für ")) + sensor->getName());

  // Basic initialization
  auto initResult = sensor->init();
  if (!initResult.isSuccess()) {
    logger.error(F("SensorFactory"),
                 String(F("Konnte ")) + sensor->getName() +
                     F(" nicht initialisieren - sensor->init() fehlgeschlagen"));
    sensor->setEnabled(false);
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR);
  }

  // After basic initialization, apply threshold overrides if present
  String sensorId = sensor->getId();

  // All per-sensor and per-measurement config is now loaded directly in config.
  // No action needed here unless you want to override from another source.

  sensor->setEnabled(true);
  logger.debug(F("SensorFactory"), sensor->getName() + F(" erfolgreich initialisiert"));
  return SensorResult::success();
}

SensorFactory::SensorResult
SensorFactory::createAllSensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                SensorManager* sensorManager) {
  logger.info(F("SensorFactory"), F("Starte Sensor-Erstellungsprozess"));

  try {
    logger.logMemoryStats(F("vor_sensorerstellung"));
    sensors.clear();

    std::vector<String> errors;

#if USE_ANALOG
    addAnalogSensors(sensors, sensorManager, errors);
#endif
#if USE_DHT
    addDHTSensors(sensors, sensorManager, errors);
#endif
#if USE_DS18B20
    addDS18B20Sensors(sensors, sensorManager, errors);
#endif
#if USE_SDS011
    addSDS011Sensors(sensors, sensorManager, errors);
#endif
#if USE_MHZ19
    addMHZ19Sensors(sensors, sensorManager, errors);
#endif
#if USE_HX711
    addHX711Sensors(sensors, sensorManager, errors);
#endif
#if USE_BMP280
    addBMP280Sensors(sensors, sensorManager, errors);
#endif
#if USE_SERIAL_RECEIVER
    addSerialReceiverSensors(sensors, sensorManager, errors);
#endif

    logger.logMemoryStats(F("nach_sensorerstellung"));

    // If we have any sensors initialized, consider it a partial success
    if (!sensors.empty()) {
      if (errors.empty()) {
        return SensorResult::success();
      } else {
        // Join all errors with semicolons
        String errorMsg;
        for (size_t i = 0; i < errors.size(); i++) {
          if (i > 0)
            errorMsg += F("; ");
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
                 String(F("Ausnahme während der Sensorerstellung: ")) + String(e.what()));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, e.what());
  } catch (...) {
    logger.error(F("SensorFactory"), F("Unbekannte Ausnahme während der Sensorerstellung"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR);
  }
}

SensorFactory::SensorResult
SensorFactory::createDHTSensors(std::vector<std::unique_ptr<Sensor>>& sensors,
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

SensorFactory::SensorResult
SensorFactory::createAnalogSensors(std::vector<std::unique_ptr<Sensor>>& sensors,
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
void SensorFactory::addDHTSensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                  SensorManager* sensorManager, std::vector<String>& errors) {
  auto dhtResult = createDHTSensors(sensors, sensorManager);
  if (!dhtResult.isSuccess()) {
    errors.push_back(String(F("DHT: ")) + dhtResult.getFullErrorMessage());
    logger.error(F("SensorFactory"),
                 F("Erstellung DHT-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
  }
}
#endif

#if USE_ANALOG
void SensorFactory::addAnalogSensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                     SensorManager* sensorManager, std::vector<String>& errors) {
  auto analogResult = createAnalogSensors(sensors, sensorManager);
  if (!analogResult.isSuccess()) {
    errors.push_back(String(F("Analog: ")) + analogResult.getFullErrorMessage());
    logger.error(F("SensorFactory"),
                 F("Erstellung Analog-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
  }
}
#endif

// ============================================================================
// Neue Sensor-Typen (bedingte Kompilierung)
// ============================================================================

#if USE_DS18B20
SensorFactory::SensorResult
SensorFactory::createDS18B20Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                    SensorManager* sensorManager) {
  DS18B20Config config;
  auto sensor = std::make_unique<DS18B20Sensor>(config, sensorManager);
  std::unique_ptr<Sensor> baseSensor(sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess())
    return result;
  sensors.push_back(std::move(baseSensor));
  return SensorResult::success();
}

void SensorFactory::addDS18B20Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                      SensorManager* sensorManager, std::vector<String>& errors) {
  auto result = createDS18B20Sensors(sensors, sensorManager);
  if (!result.isSuccess()) {
    errors.push_back(String(F("DS18B20: ")) + result.getFullErrorMessage());
    logger.error(F("SensorFactory"), F("Erstellung DS18B20-Sensor fehlgeschlagen"));
  }
}
#endif

#if USE_SDS011
SensorFactory::SensorResult
SensorFactory::createSDS011Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                   SensorManager* sensorManager) {
  SDS011Config config;
  auto sensor = std::make_unique<SDS011Sensor>(config, sensorManager);
  std::unique_ptr<Sensor> baseSensor(sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess())
    return result;
  sensors.push_back(std::move(baseSensor));
  return SensorResult::success();
}

void SensorFactory::addSDS011Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                     SensorManager* sensorManager, std::vector<String>& errors) {
  auto result = createSDS011Sensors(sensors, sensorManager);
  if (!result.isSuccess()) {
    errors.push_back(String(F("SDS011: ")) + result.getFullErrorMessage());
    logger.error(F("SensorFactory"), F("Erstellung SDS011-Sensor fehlgeschlagen"));
  }
}
#endif

#if USE_MHZ19
SensorFactory::SensorResult
SensorFactory::createMHZ19Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                  SensorManager* sensorManager) {
  MHZ19Config config;
  auto sensor = std::make_unique<MHZ19Sensor>(config, sensorManager);
  std::unique_ptr<Sensor> baseSensor(sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess())
    return result;
  sensors.push_back(std::move(baseSensor));
  return SensorResult::success();
}

void SensorFactory::addMHZ19Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                    SensorManager* sensorManager, std::vector<String>& errors) {
  auto result = createMHZ19Sensors(sensors, sensorManager);
  if (!result.isSuccess()) {
    errors.push_back(String(F("MHZ19: ")) + result.getFullErrorMessage());
    logger.error(F("SensorFactory"), F("Erstellung MHZ19-Sensor fehlgeschlagen"));
  }
}
#endif

#if USE_HX711
SensorFactory::SensorResult
SensorFactory::createHX711Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                  SensorManager* sensorManager) {
  HX711Config config;
  auto sensor = std::make_unique<HX711Sensor>(config, sensorManager);
  std::unique_ptr<Sensor> baseSensor(sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess())
    return result;
  sensors.push_back(std::move(baseSensor));
  return SensorResult::success();
}

void SensorFactory::addHX711Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                    SensorManager* sensorManager, std::vector<String>& errors) {
  auto result = createHX711Sensors(sensors, sensorManager);
  if (!result.isSuccess()) {
    errors.push_back(String(F("HX711: ")) + result.getFullErrorMessage());
    logger.error(F("SensorFactory"), F("Erstellung HX711-Sensor fehlgeschlagen"));
  }
}
#endif

#if USE_BMP280
SensorFactory::SensorResult
SensorFactory::createBMP280Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                   SensorManager* sensorManager) {
  BMP280Config config;
  auto sensor = std::make_unique<BMP280Sensor>(config, sensorManager);
  std::unique_ptr<Sensor> baseSensor(sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess())
    return result;
  sensors.push_back(std::move(baseSensor));
  return SensorResult::success();
}

void SensorFactory::addBMP280Sensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                     SensorManager* sensorManager, std::vector<String>& errors) {
  auto result = createBMP280Sensors(sensors, sensorManager);
  if (!result.isSuccess()) {
    errors.push_back(String(F("BMP280: ")) + result.getFullErrorMessage());
    logger.error(F("SensorFactory"), F("Erstellung BMP280-Sensor fehlgeschlagen"));
  }
}
#endif

#if USE_SERIAL_RECEIVER
SensorFactory::SensorResult
SensorFactory::createSerialReceiverSensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                           SensorManager* sensorManager) {
  SerialReceiverConfig config;
  auto sensor = std::make_unique<SerialReceiverSensor>(config, sensorManager);
  std::unique_ptr<Sensor> baseSensor(sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess())
    return result;
  sensors.push_back(std::move(baseSensor));
  return SensorResult::success();
}

void SensorFactory::addSerialReceiverSensors(std::vector<std::unique_ptr<Sensor>>& sensors,
                                             SensorManager* sensorManager,
                                             std::vector<String>& errors) {
  auto result = createSerialReceiverSensors(sensors, sensorManager);
  if (!result.isSuccess()) {
    errors.push_back(String(F("SerialReceiver: ")) + result.getFullErrorMessage());
    logger.error(F("SensorFactory"), F("Erstellung SerialReceiver-Sensor fehlgeschlagen"));
  }
}
#endif
