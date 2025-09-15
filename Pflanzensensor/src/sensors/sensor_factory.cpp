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
    auto dhtResult = createDHTSensors(sensors, sensorManager);
    if (!dhtResult.isSuccess()) {
      errors.push_back(F("DHT: ") + dhtResult.getFullErrorMessage());
    logger.error(
      F("SensorFactory"),
      F("Erstellung DHT-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
    }
#endif

#if USE_DS18B20
    auto ds18b20Result = createDS18B20Sensors(
        sensors, sensorManager);  // bei 1 auto result, warum?
    if (!ds18b20Result.isSuccess()) {
      errors.push_back(F("DS18B20: ") + ds18b20Result.getFullErrorMessage());
    logger.error(
      F("SensorFactory"),
      F("Erstellung DS18B20-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
    }
#endif

#if USE_SDS011
    auto sds011Result = createSDS011Sensors(sensors, sensorManager);
    if (!sds011Result.isSuccess()) {
      errors.push_back(F("SDS011: ") + sds011Result.getFullErrorMessage());
    logger.error(
      F("SensorFactory"),
      F("Erstellung SDS011-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
    }
#endif

#if USE_MHZ19
    auto mhz19Result = createMHZ19Sensors(sensors, sensorManager);
    if (!mhz19Result.isSuccess()) {
      errors.push_back(F("MHZ19: ") + mhz19Result.getFullErrorMessage());
    logger.error(
      F("SensorFactory"),
      F("Erstellung MHZ19-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
    }
#endif

#if USE_HX711
    auto hx711Result = createHX711Sensors(sensors, sensorManager);
    if (!hx711Result.isSuccess()) {
      errors.push_back(F("HX711: ") + hx711Result.getFullErrorMessage());
    logger.error(
      F("SensorFactory"),
      F("Erstellung HX711-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
    }
#endif

#if USE_BMP280
    auto bmp280Result = createBMP280Sensors(sensors, sensorManager);
    if (!bmp280Result.isSuccess()) {
      errors.push_back(F("BMP280: ") + bmp280Result.getFullErrorMessage());
    logger.error(
      F("SensorFactory"),
      F("Erstellung BMP280-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
    }
#endif

#if USE_SERIAL_RECEIVER
    auto serialResult = createSerialReceiverSensors(sensors, sensorManager);
    if (!serialResult.isSuccess()) {
      errors.push_back(F("SerialReceiver: ") +
                       serialResult.getFullErrorMessage());
      logger.error(F("SensorFactory"),
                   F("Erstellung Serial-Receiver-Sensor fehlgeschlagen, fahre "
                     "mit anderen Sensoren fort"));
    }
#endif

#if USE_ANALOG
    auto analogResult = createAnalogSensors(sensors, sensorManager);
    if (!analogResult.isSuccess()) {
      errors.push_back(F("Analog: ") + analogResult.getFullErrorMessage());
    logger.error(
      F("SensorFactory"),
      F("Erstellung Analog-Sensor fehlgeschlagen, fahre mit anderen Sensoren fort"));
    }
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

SensorFactory::SensorResult SensorFactory::createBMP280Sensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_BMP280
  BMP280Config config;
  auto bmp280Sensor = std::make_unique<BMP280Sensor>(config, sensorManager);

  // Move into base class pointer for initialization
  std::unique_ptr<Sensor> baseSensor(bmp280Sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess()) {
    return result;
  }

  // Move into sensors vector
  sensors.push_back(std::move(baseSensor));
#endif
  return SensorResult::success();
}

SensorFactory::SensorResult SensorFactory::createHX711Sensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_HX711
  HX711Config config;
  auto hx711Sensor = std::make_unique<HX711Sensor>(config, sensorManager);

  // Move into base class pointer for initialization
  std::unique_ptr<Sensor> baseSensor(hx711Sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess()) {
    return result;
  }

  // Move into sensors vector
  sensors.push_back(std::move(baseSensor));
#endif
  return SensorResult::success();
}

SensorFactory::SensorResult SensorFactory::createDS18B20Sensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_DS18B20
  DS18B20Config config;
  auto ds18b20Sensor = std::make_unique<DS18B20Sensor>(config, sensorManager);

  // Move into base class pointer for initialization
  std::unique_ptr<Sensor> baseSensor(ds18b20Sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess()) {
    return result;
  }

  // Move into sensors vector
  sensors.push_back(std::move(baseSensor));
#endif
  return SensorResult::success();
}

SensorFactory::SensorResult SensorFactory::createSDS011Sensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_SDS011
  SDS011Config config;
  auto sds011Sensor = std::make_unique<SDS011Sensor>(config, sensorManager);

  // Move into base class pointer for initialization
  std::unique_ptr<Sensor> baseSensor(sds011Sensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess()) {
    return result;
  }

  // Move into sensors vector
  sensors.push_back(std::move(baseSensor));
#endif
  return SensorResult::success();
}

SensorFactory::SensorResult SensorFactory::createMHZ19Sensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_MHZ19
  MHZ19Config config;
  auto mhz19Sensor = std::make_unique<MHZ19Sensor>(config, sensorManager);

  // Store raw pointer before moving
  auto* rawSensor = mhz19Sensor.get();

  // For MHZ19, we only do basic initialization without test measurement
  auto initResult = mhz19Sensor->init();
  if (!initResult.isSuccess()) {
    logger.error(F("SensorFactory"), F("Failed to initialize MHZ19 sensor"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR);
  }

  // Enable the sensor - it will handle its own warmup
  mhz19Sensor->setEnabled(true);
  logger.debug(F("SensorFactory"),
               F("MHZ19 initialized successfully - warmup in progress"));

  // Move into sensors vector
  sensors.push_back(std::move(mhz19Sensor));
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

SensorFactory::SensorResult SensorFactory::createSerialReceiverSensors(
    std::vector<std::unique_ptr<Sensor>>& sensors,
    SensorManager* sensorManager) {
#if USE_SERIAL_RECEIVER
  SerialReceiverConfig config;
  config.id = "SERIAL_RECEIVER";
  config.name = "Serial Receiver";
  config.configureMeasurements();
  auto serialSensor =
      std::make_unique<SerialReceiverSensor>(config, sensorManager);

  // Move into base class pointer for initialization
  std::unique_ptr<Sensor> baseSensor(serialSensor.release());
  auto result = initializeSensor(baseSensor);
  if (!result.isSuccess()) {
    return result;
  }

  // Move into sensors vector
  sensors.push_back(std::move(baseSensor));
#endif
  return SensorResult::success();
}
