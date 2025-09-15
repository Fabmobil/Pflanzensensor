/**
 * @file manager_sensor_persistence.cpp
 * @brief Implementation of SensorPersistence class
 */

#include "manager_sensor_persistence.h"

#include <ArduinoJson.h>
using namespace ArduinoJson;

#include <LittleFS.h>

#include "../logger/logger.h"
#include "../utils/critical_section.h"
#include "managers/manager_config.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"
#include "sensors/sensors.h"
#if USE_ANALOG
#include "sensors/sensor_analog.h"
#endif
#include "../utils/persistence_utils.h"

// Static document for sensor operations
static StaticJsonDocument<1024> sensorDoc;

bool SensorPersistence::configFileExists() {
  return PersistenceUtils::fileExists("/sensors.json");
}

size_t SensorPersistence::getConfigFileSize() {
  if (!configFileExists()) {
    return 0;
  }

  File f = LittleFS.open("/sensors.json", "r");
  if (!f) {
    return 0;
  }

  size_t size = f.size();
  f.close();
  return size;
}

// Simple function to apply sensor settings directly during JSON parsing
void SensorPersistence::applySensorSettingsFromJson(
    const String& sensorId, const JsonObject& sensorConfig) {
  extern std::unique_ptr<SensorManager> sensorManager;

  // Early exit if sensor manager is not available
  if (!sensorManager ||
      sensorManager->getState() != ManagerState::INITIALIZED) {
    logger.warning(
        F("SensorP"),
        F("Sensor-Manager nicht bereit, Einstellungen übersprungen für: ") + sensorId);
    return;
  }

  // Find the sensor
  const auto& allSensors = sensorManager->getSensors();
  Sensor* sensor = nullptr;
  for (const auto& sensorPtr : allSensors) {
    if (sensorPtr && sensorPtr->config().id == sensorId) {
      sensor = sensorPtr.get();
      break;
    }
  }

  if (!sensor) {
    logger.debug(F("SensorP"), F("Sensor nicht gefunden: ") + sensorId);
    return;
  }

  logger.debug(F("SensorP"), F("Wende Einstellungen an Sensor: ") + sensorId);

  try {
    SensorConfig& config = sensor->mutableConfig();

    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"),
                   F("Got mutable config for sensor: ") + sensorId);
    }

    // Apply measurement interval
    if (sensorConfig["measurementInterval"].is<unsigned long>()) {
      unsigned long interval = sensorConfig["measurementInterval"] | 60000UL;
      config.measurementInterval = interval;
      sensor->setMeasurementInterval(interval);
    }

    // Apply measurements
    if (sensorConfig["measurements"].is<JsonObject>()) {
      JsonObject measurements = sensorConfig["measurements"];
      for (JsonPair measurementPair : measurements) {
        String indexStr = measurementPair.key().c_str();
        size_t index = indexStr.toInt();

        if (index >= config.activeMeasurements) {
          continue;
        }

        JsonObject measurement = measurementPair.value();

        // Apply basic measurement settings
        if (measurement["enabled"].is<bool>()) {
          config.measurements[index].enabled = measurement["enabled"] | true;
        }

        // Apply measurement name
        if (measurement["name"].is<const char*>()) {
          config.measurements[index].name =
              String(measurement["name"].as<const char*>());
          if (ConfigMgr.isDebugSensor()) {
            logger.debug(F("SensorP"), F("Geladener Name: '") +
                                           config.measurements[index].name +
                                           F("' für Sensor ") + sensorId +
                                           F(" Messung ") + String(index));
          }
        }

        // Apply measurement field name
        if (measurement["fieldName"].is<const char*>()) {
          config.measurements[index].fieldName =
              String(measurement["fieldName"].as<const char*>());
          if (ConfigMgr.isDebugSensor()) {
            logger.debug(F("SensorP"),
                         F("Geladener Feldname: '") +
                             config.measurements[index].fieldName +
                             F("' für Sensor ") + sensorId +
                             F(" Messung ") + String(index));
          }
        }

        // Apply measurement unit
        if (measurement["unit"].is<const char*>()) {
          config.measurements[index].unit =
              String(measurement["unit"].as<const char*>());
          if (ConfigMgr.isDebugSensor()) {
            logger.debug(F("SensorP"), F("Geladene Einheit: '") +
                                           config.measurements[index].unit +
                                           F("' für Sensor ") + sensorId +
                                           F(" Messung ") + String(index));
          }
        }

        // Apply thresholds
        if (measurement["thresholds"].is<JsonObject>()) {
          JsonObject thresholds = measurement["thresholds"];
          config.measurements[index].limits.yellowLow =
              thresholds["yellowLow"] | -10.0f;
          config.measurements[index].limits.greenLow =
              thresholds["greenLow"] | 0.0f;
          config.measurements[index].limits.greenHigh =
              thresholds["greenHigh"] | 30.0f;
          config.measurements[index].limits.yellowHigh =
              thresholds["yellowHigh"] | 40.0f;
        }

#if USE_ANALOG
        // Apply analog-specific settings
        if (measurement["min"].is<float>() && measurement["max"].is<float>() &&
            measurement["inverted"].is<bool>()) {
          config.measurements[index].minValue = measurement["min"] | 0.0f;
          config.measurements[index].maxValue = measurement["max"] | 0.0f;
          config.measurements[index].inverted = measurement["inverted"] | false;

          // Apply to analog sensor if it's an analog sensor
          if (config.id.startsWith("ANALOG")) {
            AnalogSensor* analogSensor = static_cast<AnalogSensor*>(sensor);
            if (analogSensor) {
              analogSensor->setMinValue(index,
                                        config.measurements[index].minValue);
              analogSensor->setMaxValue(index,
                                        config.measurements[index].maxValue);
            }
          }
        }
#endif

        // Apply absolute min/max values
        if (measurement["absoluteMin"].is<float>()) {
          config.measurements[index].absoluteMin =
              measurement["absoluteMin"] | INFINITY;
        }
        if (measurement["absoluteMax"].is<float>()) {
          config.measurements[index].absoluteMax =
              measurement["absoluteMax"] | -INFINITY;
        }

        // Apply absolute raw min/max values for analog sensors
        if (measurement["absoluteRawMin"].is<int>()) {
          int rawMin =
              measurement["absoluteRawMin"];  // CRITICAL FIX: Remove bitwise OR
          config.measurements[index].absoluteRawMin = rawMin;

          // CRITICAL FIX: Also apply to analog sensor if it's an analog sensor
#if USE_ANALOG
          if (config.id.startsWith("ANALOG")) {
            AnalogSensor* analogSensor = static_cast<AnalogSensor*>(sensor);
            if (analogSensor) {
              analogSensor->setAbsoluteRawMin(index, rawMin);
            }
          }
#endif

          if (ConfigMgr.isDebugSensor()) {
            logger.debug(F("SensorP"), F("Geladener absoluteRawMin: ") +
                                           String(rawMin) + F(" für Sensor ") +
                                           sensorId + F(" Messung ") +
                                           String(index));
          }
        } else {
          if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"),
             F("Kein absoluteRawMin in JSON für Sensor ") +
               sensorId + F(" Messung ") + String(index) +
               F(", Standard behalten: ") +
               String(config.measurements[index].absoluteRawMin));
          }
        }
        if (measurement["absoluteRawMax"].is<int>()) {
          int rawMax =
              measurement["absoluteRawMax"];  // CRITICAL FIX: Remove bitwise OR
          config.measurements[index].absoluteRawMax = rawMax;

          // CRITICAL FIX: Also apply to analog sensor if it's an analog sensor
#if USE_ANALOG
          if (config.id.startsWith("ANALOG")) {
            AnalogSensor* analogSensor = static_cast<AnalogSensor*>(sensor);
            if (analogSensor) {
              analogSensor->setAbsoluteRawMax(index, rawMax);
            }
          }
#endif

          if (ConfigMgr.isDebugSensor()) {
            logger.debug(F("SensorP"), F("Geladener absoluteRawMax: ") +
                                           String(rawMax) + F(" für Sensor ") +
                                           sensorId + F(" Messung ") +
                                           String(index));
          }
        } else {
          if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"),
             F("Kein absoluteRawMax in JSON für Sensor ") +
               sensorId + F(" Messung ") + String(index) +
               F(", Standard behalten: ") +
               String(config.measurements[index].absoluteRawMax));
          }
        }
      }
    }

    // Apply persistent error flag
    if (sensorConfig["hasPersistentError"].is<bool>()) {
      config.hasPersistentError = sensorConfig["hasPersistentError"] | false;
    }

  } catch (...) {
    logger.error(F("SensorP"),
                 F("Ausnahme beim Anwenden der Einstellungen für Sensor: ") + sensorId);
  }
}

SensorPersistence::PersistenceResult SensorPersistence::loadFromFile() {
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"),
                 F("Beginne Laden der Sensorkonfiguration aus Datei"));
  }

  String errorMsg;
  StaticJsonDocument<1024>& doc = sensorDoc;
  doc.clear();

  if (!PersistenceUtils::fileExists("/sensors.json")) {
    // File does not exist: use defaults, do not treat as error
    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"),
                   F("Sensorkonfigurationsdatei nicht gefunden, verwende Standardwerte."));
    }
    logger.info(F("SensorP"),
                F("Sensorkonfigurationsdatei nicht gefunden, verwende Standardwerte."));
    return PersistenceResult::success();
  }

  if (!PersistenceUtils::readJsonFile("/sensors.json", doc, errorMsg)) {
    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"),
                   F("Fehler beim Lesen von sensors.json: ") + errorMsg);
    }
    logger.error(F("SensorP"), F("Sensorkonfiguration konnte nicht geladen werden: ") + errorMsg);
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("sensors.json erfolgreich gelesen"));
  }

  // Load sensor configuration
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Beginne Verarbeitung der Sensoren aus JSON"));
  }

  for (JsonPair sensorPair : doc.as<JsonObject>()) {
    if (!sensorPair.key().c_str() || strlen(sensorPair.key().c_str()) == 0) {
      logger.warning(F("SensorP"), F("Überspringe Sensor mit leerem Schlüssel"));
      continue;
    }
    String sensorId = String(sensorPair.key().c_str());
    JsonObject sensorConfig = sensorPair.value();

    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"), F("Verarbeite Sensor: ") + sensorId);
    }

    applySensorSettingsFromJson(sensorId, sensorConfig);
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Verarbeitung aller Sensoren aus JSON beendet"));
  }
  return PersistenceResult::success();
}

// Remove saveToFile and saveToFileInternal implementations, keep
// saveToFileMinimal Remove saveToFile and saveToFileInternal implementations,
// keep saveToFileMinimal

SensorPersistence::PersistenceResult SensorPersistence::saveToFileMinimal() {
  StaticJsonDocument<1024>& doc = sensorDoc;
  doc.clear();
  ArduinoJson::JsonObject docRoot = doc.to<JsonObject>();
  extern std::unique_ptr<SensorManager> sensorManager;
  if (!sensorManager) {
    return PersistenceResult::success();
  }
  const auto& allSensors = sensorManager->getSensors();
  for (const auto& sensorPtr : allSensors) {
    if (!sensorPtr) continue;
    const SensorConfig& sensorConfig = sensorPtr->config();
    ArduinoJson::JsonObject sensorObj =
        docRoot[sensorConfig.id.c_str()].to<JsonObject>();
    sensorObj["measurementInterval"] = sensorConfig.measurementInterval;
    ArduinoJson::JsonObject measurements =
        sensorObj["measurements"].to<JsonObject>();
    for (size_t i = 0; i < sensorConfig.activeMeasurements; ++i) {
      char idxBuf[8];
      snprintf(idxBuf, sizeof(idxBuf), "%u", static_cast<unsigned>(i));
      ArduinoJson::JsonObject measurementObj =
          measurements[idxBuf].to<JsonObject>();
      measurementObj["enabled"] = sensorConfig.measurements[i].enabled;
      measurementObj["name"] = sensorConfig.measurements[i].name;
      measurementObj["fieldName"] = sensorConfig.measurements[i].fieldName;
      measurementObj["unit"] = sensorConfig.measurements[i].unit;
      ArduinoJson::JsonObject thresholds =
          measurementObj["thresholds"].to<JsonObject>();
      thresholds["yellowLow"] = sensorConfig.measurements[i].limits.yellowLow;
      thresholds["greenLow"] = sensorConfig.measurements[i].limits.greenLow;
      thresholds["greenHigh"] = sensorConfig.measurements[i].limits.greenHigh;
      thresholds["yellowHigh"] = sensorConfig.measurements[i].limits.yellowHigh;
#if USE_ANALOG
      if (sensorConfig.id.startsWith("ANALOG")) {
        measurementObj["min"] = sensorConfig.measurements[i].minValue;
        measurementObj["max"] = sensorConfig.measurements[i].maxValue;
        measurementObj["inverted"] = sensorConfig.measurements[i].inverted;
      }
      // Always save absolute min/max values to ensure they persist across
      // reboots
      measurementObj["absoluteMin"] = sensorConfig.measurements[i].absoluteMin;
      measurementObj["absoluteMax"] = sensorConfig.measurements[i].absoluteMax;
      measurementObj["absoluteRawMin"] =
          sensorConfig.measurements[i].absoluteRawMin;
      measurementObj["absoluteRawMax"] =
          sensorConfig.measurements[i].absoluteRawMax;
#endif
    }
    sensorObj["hasPersistentError"] = sensorConfig.hasPersistentError;
  }
  // Remove unused variable 'errorMsg'
  if (!PersistenceUtils::writeJsonFile("/sensors.json", doc, *(new String()))) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR);
  }
  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult SensorPersistence::updateSensorThresholds(
    const String& sensorId, size_t measurementIndex, float yellowLow,
    float greenLow, float greenHigh, float yellowHigh) {
  // Use critical section instead of callback manipulation to prevent memory
  // corruption
  CriticalSection cs;

  PersistenceResult result = updateSensorThresholdsInternal(
      sensorId, measurementIndex, yellowLow, greenLow, greenHigh, yellowHigh);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateSensorThresholdsInternal(
    const String& sensorId, size_t measurementIndex, float yellowLow,
    float greenLow, float greenHigh, float yellowHigh) {
  // Load existing config
  StaticJsonDocument<1024> doc;
  String errorMsg;
  if (!PersistenceUtils::readJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Navigate to the specific sensor and measurement
  JsonObject sensorObj = doc[sensorId.c_str()];
  if (sensorObj.isNull()) {
    return PersistenceResult::fail(ConfigError::PARSE_ERROR,
                                   F("Sensor not found: ") + sensorId);
  }

  JsonObject measurements = sensorObj["measurements"];
  if (measurements.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("No measurements object found for sensor: ") + sensorId);
  }

  char idxBuf[8];
  snprintf(idxBuf, sizeof(idxBuf), "%u",
           static_cast<unsigned>(measurementIndex));
  JsonObject measurementObj = measurements[idxBuf];
  if (measurementObj.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("Measurement not found: ") + String(measurementIndex));
  }

  JsonObject thresholds = measurementObj["thresholds"];
  if (thresholds.isNull()) {
    return PersistenceResult::fail(ConfigError::PARSE_ERROR,
                                   F("No thresholds object found"));
  }

  // Update only the threshold values
  thresholds["yellowLow"] = yellowLow;
  thresholds["greenLow"] = greenLow;
  thresholds["greenHigh"] = greenHigh;
  thresholds["yellowHigh"] = yellowHigh;

  // Write back to file
  if (!PersistenceUtils::writeJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  logger.info(F("SensorP"),
              F("Atomic threshold update for ") + sensorId +
                  F(" measurement ") + String(measurementIndex) +
                  F(", bytes written: ") +
                  String(PersistenceUtils::getFileSize("/sensors.json")));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult SensorPersistence::updateAnalogMinMax(
    const String& sensorId, size_t measurementIndex, float minValue,
    float maxValue, bool inverted) {
  // Use critical section instead of callback manipulation to prevent memory
  // corruption
  CriticalSection cs;

  PersistenceResult result = updateAnalogMinMaxInternal(
      sensorId, measurementIndex, minValue, maxValue, inverted);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogMinMaxInternal(const String& sensorId,
                                              size_t measurementIndex,
                                              float minValue, float maxValue,
                                              bool inverted) {
  // Load existing config
  StaticJsonDocument<1024> doc;
  String errorMsg;
  if (!PersistenceUtils::readJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Navigate to the specific sensor and measurement
  JsonObject sensorObj = doc[sensorId.c_str()];
  if (sensorObj.isNull()) {
    return PersistenceResult::fail(ConfigError::PARSE_ERROR,
                                   F("Sensor not found: ") + sensorId);
  }

  JsonObject measurements = sensorObj["measurements"];
  if (measurements.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("No measurements object found for sensor: ") + sensorId);
  }

  char idxBuf[8];
  snprintf(idxBuf, sizeof(idxBuf), "%u",
           static_cast<unsigned>(measurementIndex));
  JsonObject measurementObj = measurements[idxBuf];
  if (measurementObj.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("Measurement not found: ") + String(measurementIndex));
  }

  // Update only the min/max values
  measurementObj["min"] = minValue;
  measurementObj["max"] = maxValue;
  measurementObj["inverted"] = inverted;

  // Write back to file
  if (!PersistenceUtils::writeJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  logger.info(F("SensorP"),
              F("Atomic analog minmax update for ") + sensorId +
                  F(" measurement ") + String(measurementIndex) +
                  F(", bytes written: ") +
                  String(PersistenceUtils::getFileSize("/sensors.json")));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementInterval(const String& sensorId,
                                             unsigned long interval) {
  // Use critical section instead of callback manipulation to prevent memory
  // corruption
  CriticalSection cs;

  PersistenceResult result =
      updateMeasurementIntervalInternal(sensorId, interval);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementIntervalInternal(const String& sensorId,
                                                     unsigned long interval) {
  // Load existing config
  StaticJsonDocument<1024> doc;
  String errorMsg;
  if (!PersistenceUtils::readJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Navigate to the specific sensor
  JsonObject sensorObj = doc[sensorId.c_str()];
  if (sensorObj.isNull()) {
    return PersistenceResult::fail(ConfigError::PARSE_ERROR,
                                   F("Sensor not found: ") + sensorId);
  }

  // Update measurement interval
  sensorObj["measurementInterval"] = interval;

  // Write back to file
  if (!PersistenceUtils::writeJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  logger.info(F("SensorP"),
              F("Atomic measurement interval update for ") + sensorId +
                  F(", interval: ") + String(interval) +
                  F(", bytes written: ") +
                  String(PersistenceUtils::getFileSize("/sensors.json")));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementEnabled(const String& sensorId,
                                            size_t measurementIndex,
                                            bool enabled) {
  // Use critical section instead of callback manipulation to prevent memory
  // corruption
  CriticalSection cs;

  PersistenceResult result =
      updateMeasurementEnabledInternal(sensorId, measurementIndex, enabled);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementEnabledInternal(const String& sensorId,
                                                    size_t measurementIndex,
                                                    bool enabled) {
  // Load existing config
  StaticJsonDocument<1024> doc;
  String errorMsg;
  if (!PersistenceUtils::readJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Navigate to the specific sensor and measurement
  JsonObject sensorObj = doc[sensorId.c_str()];
  if (sensorObj.isNull()) {
    return PersistenceResult::fail(ConfigError::PARSE_ERROR,
                                   F("Sensor not found: ") + sensorId);
  }

  JsonObject measurements = sensorObj["measurements"];
  if (measurements.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("No measurements object found for sensor: ") + sensorId);
  }

  char idxBuf[8];
  snprintf(idxBuf, sizeof(idxBuf), "%u",
           static_cast<unsigned>(measurementIndex));
  JsonObject measurementObj = measurements[idxBuf];
  if (measurementObj.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("Measurement not found: ") + String(measurementIndex));
  }

  // Update enabled state
  measurementObj["enabled"] = enabled;

  // Write back to file
  if (!PersistenceUtils::writeJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  logger.info(F("SensorP"),
              F("Atomic measurement enabled update for ") + sensorId +
                  F(" measurement ") + String(measurementIndex) +
                  F(", enabled: ") + String(enabled) + F(", bytes written: ") +
                  String(PersistenceUtils::getFileSize("/sensors.json")));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult SensorPersistence::updateAbsoluteMinMax(
    const String& sensorId, size_t measurementIndex, float absoluteMin,
    float absoluteMax) {
  // Load existing config
  StaticJsonDocument<1024> doc;
  String errorMsg;
  if (!PersistenceUtils::readJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Navigate to the specific sensor and measurement
  JsonObject sensorObj = doc[sensorId.c_str()];
  if (sensorObj.isNull()) {
    return PersistenceResult::fail(ConfigError::PARSE_ERROR,
                                   F("Sensor not found: ") + sensorId);
  }

  JsonObject measurements = sensorObj["measurements"];
  if (measurements.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("No measurements object found for sensor: ") + sensorId);
  }

  char idxBuf[8];
  snprintf(idxBuf, sizeof(idxBuf), "%u",
           static_cast<unsigned>(measurementIndex));
  JsonObject measurementObj = measurements[idxBuf];
  if (measurementObj.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("Measurement not found: ") + String(measurementIndex));
  }

  // Always save absolute min/max values to ensure they persist across reboots
  measurementObj["absoluteMin"] = absoluteMin;
  measurementObj["absoluteMax"] = absoluteMax;

  // Write back to file
  if (!PersistenceUtils::writeJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  logger.info(F("SensorP"),
              F("Atomic absolute min/max update for ") + sensorId +
                  F(" measurement ") + String(measurementIndex) +
                  F(", bytes written: ") +
                  String(PersistenceUtils::getFileSize("/sensors.json")));

  // No need to manually sync configs anymore - we now have a single source of
  // truth
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Updated config for sensor ") + sensorId +
                                   F(" measurement ") +
                                   String(measurementIndex) + F(" - Min: ") +
                                   String(absoluteMin) + F(", Max: ") +
                                   String(absoluteMax));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult SensorPersistence::updateAnalogRawMinMax(
    const String& sensorId, size_t measurementIndex, int absoluteRawMin,
    int absoluteRawMax) {
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"),
                 F("updateAnalogRawMinMax called for ") + sensorId +
                     F(" measurement ") + String(measurementIndex) +
                     F(" with values Min: ") + String(absoluteRawMin) +
                     F(", Max: ") + String(absoluteRawMax));
  }

  // CRITICAL FIX: Move CriticalSection to cover the ENTIRE operation
  // This ensures file write and reload happen atomically
  CriticalSection cs;

  PersistenceResult result = updateAnalogRawMinMaxInternal(
      sensorId, measurementIndex, absoluteRawMin, absoluteRawMax);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogRawMinMaxInternal(const String& sensorId,
                                                 size_t measurementIndex,
                                                 int absoluteRawMin,
                                                 int absoluteRawMax) {
  // Load existing config
  StaticJsonDocument<1024> doc;
  String errorMsg;
  if (!PersistenceUtils::readJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Navigate to the specific sensor and measurement
  JsonObject sensorObj = doc[sensorId.c_str()];
  if (sensorObj.isNull()) {
    return PersistenceResult::fail(ConfigError::PARSE_ERROR,
                                   F("Sensor not found: ") + sensorId);
  }

  JsonObject measurements = sensorObj["measurements"];
  if (measurements.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("No measurements object found for sensor: ") + sensorId);
  }

  char idxBuf[8];
  snprintf(idxBuf, sizeof(idxBuf), "%u",
           static_cast<unsigned>(measurementIndex));
  JsonObject measurementObj = measurements[idxBuf];
  if (measurementObj.isNull()) {
    return PersistenceResult::fail(
        ConfigError::PARSE_ERROR,
        F("Measurement not found: ") + String(measurementIndex));
  }

  // Always save absolute raw min/max values to ensure they persist across
  // reboots
  measurementObj["absoluteRawMin"] = absoluteRawMin;
  measurementObj["absoluteRawMax"] = absoluteRawMax;

  // Write back to file
  if (!PersistenceUtils::writeJsonFile("/sensors.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  logger.info(F("SensorP"),
              F("Atomic analog raw min/max update for ") + sensorId +
                  F(" measurement ") + String(measurementIndex) +
                  F(", bytes written: ") +
                  String(PersistenceUtils::getFileSize("/sensors.json")));

  // CRITICAL FIX: Reload sensor configuration from file to sync in-memory
  // config This ensures the web interface reads the updated raw min/max values
  auto reloadResult = loadFromFile();
  if (!reloadResult.isSuccess()) {
    logger.warning(
        F("SensorP"),
        F("Failed to reload sensor config after raw min/max update: ") +
            reloadResult.getMessage());
  } else {
    if (ConfigMgr.isDebugSensor()) {
      logger.debug(
          F("SensorP"),
          F("Successfully reloaded sensor config after raw min/max update"));
    }
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Updated config for sensor ") + sensorId +
                                   F(" measurement ") +
                                   String(measurementIndex) + F(" - Min: ") +
                                   String(absoluteRawMin) + F(", Max: ") +
                                   String(absoluteRawMax));
  }

  return PersistenceResult::success();
}
