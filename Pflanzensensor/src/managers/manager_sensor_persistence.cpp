/**
 * @file manager_sensor_persistence.cpp
 * @brief Implementation of SensorPersistence class
 */

#include "manager_sensor_persistence.h"

#include <LittleFS.h>
#include <map>

#include "../logger/logger.h"
#include "../utils/json_file_utils.h"
#include "managers/manager_config.h"
#include "managers/manager_config_preferences.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"
#include "sensors/sensors.h"
#if USE_ANALOG
#include "sensors/sensor_analog.h"
#endif
#include "sensors/sensor_autocalibration.h"

// Write-behind cache: Instead of writing immediately, we collect all changes
// in RAM and flush them periodically (e.g., every 60 seconds). This drastically
// reduces flash wear and eliminates blocking writes during measurements.
enum class PendingUpdateType {
  RAW_MIN_MAX,       // int absoluteRawMin, absoluteRawMax
  ABSOLUTE_MIN_MAX,  // float absoluteMin, absoluteMax
  CALIBRATED_MIN_MAX // int minValue, maxValue, bool inverted
};

struct PendingUpdate {
  PendingUpdateType type;
  String sensorId;
  size_t measurementIndex;
  unsigned long timestamp; // When this update was queued

  // Union to save memory - only one set of values is active at a time
  union {
    struct {
      int absoluteRawMin;
      int absoluteRawMax;
    } raw;
    struct {
      float absoluteMin;
      float absoluteMax;
    } absolute;
    struct {
      int minValue;
      int maxValue;
      bool inverted;
    } calibrated;
  } data;
};

static std::vector<PendingUpdate> g_pendingUpdates;

SensorPersistence::PersistenceResult SensorPersistence::load() {
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Beginne Laden der Sensorkonfiguration aus JSON"));
  }

  extern std::unique_ptr<SensorManager> sensorManager;

  // Early exit if sensor manager is not available
  if (!sensorManager || sensorManager->getState() != ManagerState::INITIALIZED) {
    logger.warning(F("SensorP"), F("Sensor-Manager nicht bereit, überspringe Laden"));
    return PersistenceResult::success();
  }

  // Load measurement intervals from settings.json
  const char* settingsPath = "/config/settings.json";
  DynamicJsonDocument settingsDoc(4096);
  bool hasSettings = loadJsonFile(settingsPath, settingsDoc);

  const auto& allSensors = sensorManager->getSensors();
  bool anyLoaded = false;
  size_t filesCreated = 0;

  // Load each sensor's measurements from individual JSON files
  for (const auto& sensorPtr : allSensors) {
    if (!sensorPtr)
      continue;

    String sensorId = sensorPtr->config().id;
    SensorConfig& config = sensorPtr->mutableConfig();

    // Load measurement interval from settings.json if available
    if (hasSettings && settingsDoc.containsKey("sensors")) {
      JsonObjectConst sensors = settingsDoc["sensors"];
      if (sensors.containsKey(sensorId) && sensors[sensorId].containsKey("interval")) {
        unsigned long interval = sensors[sensorId]["interval"] | (MEASUREMENT_INTERVAL * 1000);
        config.measurementInterval = interval;
        sensorPtr->setMeasurementInterval(interval);

        if (ConfigMgr.isDebugSensor()) {
          logger.debug(F("SensorP"), F("Messintervall für ") + sensorId +
                                         F(" aus settings.json geladen: ") + String(interval) +
                                         F("ms"));
        }
      }
    }

    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"), String(F("Lade Messungen für Sensor: ")) + sensorId);
    }

    // Try to load each measurement from its JSON file
    for (size_t i = 0; i < config.activeMeasurements; ++i) {
      String path = getMeasurementFilePath(sensorId, i);

      // Check if file exists
      if (!LittleFS.exists(path)) {
        // File doesn't exist - create it with current defaults
        if (ConfigMgr.isDebugSensor()) {
          logger.debug(F("SensorP"), String(F("Erstelle Default-Datei: ")) + path);
        }

        auto saveResult = saveMeasurementToJson(sensorId, i, config.measurements[i]);
        if (saveResult.isSuccess()) {
          filesCreated++;
        } else {
          logger.warning(F("SensorP"), F("Konnte Default-Datei nicht erstellen: ") + path);
        }
        continue;
      }

      // Load from JSON
      MeasurementConfig loadedConfig;
      auto result = loadMeasurementFromJson(sensorId, i, loadedConfig);

      if (result.isSuccess()) {
        // Apply loaded settings
        config.measurements[i] = loadedConfig;
        anyLoaded = true;

        if (ConfigMgr.isDebugSensor()) {
          logger.debug(F("SensorP"), String(F("Messung geladen: ")) + path);
        }
      } else {
        logger.warning(F("SensorP"), F("Konnte Messung nicht laden: ") + path);
      }

      yield(); // Feed watchdog
    }
  }

  if (filesCreated > 0) {
    logger.info(F("SensorP"), String(filesCreated) + F(" Default-Messungs-Dateien erstellt"));
  }

  if (anyLoaded) {
    logger.info(F("SensorP"), F("Sensor-Konfiguration erfolgreich aus JSON geladen"));
  } else if (filesCreated == 0) {
    logger.warning(F("SensorP"), F("Keine Sensor-Konfiguration gefunden oder geladen"));
  }

  return PersistenceResult::success();
}

// Remove saveToFile and saveToFileInternal implementations, keep
// saveToFileMinimal Remove saveToFile and saveToFileInternal implementations,
// keep saveToFileMinimal

SensorPersistence::PersistenceResult SensorPersistence::save() {
  extern std::unique_ptr<SensorManager> sensorManager;
  if (!sensorManager) {
    return PersistenceResult::success();
  }

  const auto& allSensors = sensorManager->getSensors();
  size_t totalSaved = 0;

  // Save each measurement to its own JSON file
  for (const auto& sensorPtr : allSensors) {
    if (!sensorPtr)
      continue;

    const SensorConfig& sensorConfig = sensorPtr->config();
    String sensorId = sensorConfig.id;

    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"), String(F("Speichere Messungen für Sensor: ")) + sensorId);
    }

    // Save each measurement to individual JSON file
    for (size_t i = 0; i < sensorConfig.activeMeasurements; ++i) {
      auto result = saveMeasurementToJson(sensorId, i, sensorConfig.measurements[i]);
      if (result.isSuccess()) {
        totalSaved++;
      } else {
        logger.warning(F("SensorP"), String(F("Fehler beim Speichern von ")) + sensorId +
                                         F(" Messung ") + String(i));
      }
      yield(); // Feed watchdog
    }

    logger.info(F("SensorP"), String(F("Sensor gespeichert: ")) + sensorId + F(" (") +
                                  String(sensorConfig.activeMeasurements) + F(" Messungen)"));
  }

  logger.info(F("SensorP"), String(totalSaved) + F(" Messungs-Dateien gespeichert"));
  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateSensorThresholds(const String& sensorId, size_t measurementIndex,
                                          float yellowLow, float greenLow, float greenHigh,
                                          float yellowHigh) {
  // Now update JSON file directly
  extern std::unique_ptr<SensorManager> sensorManager;
  if (!sensorManager) {
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "SensorManager not available");
  }

  // Find sensor and update measurement
  for (const auto& sensorPtr : sensorManager->getSensors()) {
    if (sensorPtr && sensorPtr->config().id == sensorId) {
      const SensorConfig& config = sensorPtr->config();
      if (measurementIndex >= config.activeMeasurements) {
        return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Invalid measurement index");
      }

      // Create updated config
      MeasurementConfig updatedConfig = config.measurements[measurementIndex];
      updatedConfig.limits.yellowLow = yellowLow;
      updatedConfig.limits.greenLow = greenLow;
      updatedConfig.limits.greenHigh = greenHigh;
      updatedConfig.limits.yellowHigh = yellowHigh;

      // Save to JSON
      auto result = saveMeasurementToJson(sensorId, measurementIndex, updatedConfig);
      if (result.isSuccess()) {
        logger.info(F("SensorP"), String(F("Schwellenwerte aktualisiert für ")) + sensorId +
                                      F(" Messung ") + String(measurementIndex));
      }
      return result;
    }
  }

  return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Sensor not found");
}

// ============================================================================
// JSON-based Update Functions (replaces old Preferences-based code)
// ============================================================================

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogMinMax(const String& sensorId, size_t measurementIndex,
                                      float minValue, float maxValue, bool inverted) {
  // Batch update mehrerer Felder auf einmal (effizient!)
  DynamicJsonDocument doc(256);
  JsonObject settings = doc.to<JsonObject>();
  settings["minValue"] = minValue;
  settings["maxValue"] = maxValue;
  settings["inverted"] = inverted;
  return updateMeasurementSettings(sensorId, measurementIndex, settings);
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogMinMaxInteger(const String& sensorId, size_t measurementIndex,
                                             int minValue, int maxValue, bool inverted) {
  // Wrapper: Cast int to float und nutze generische Funktion
  DynamicJsonDocument doc(256);
  JsonObject settings = doc.to<JsonObject>();
  settings["minValue"] = static_cast<float>(minValue);
  settings["maxValue"] = static_cast<float>(maxValue);
  settings["inverted"] = inverted;
  return updateMeasurementSettings(sensorId, measurementIndex, settings);
}

// Alias für updateAnalogMinMaxInteger mit NoReload suffix (Kompatibilität)
SensorPersistence::PersistenceResult SensorPersistence::updateAnalogMinMaxIntegerNoReload(
    const String& sensorId, size_t measurementIndex, int minValue, int maxValue, bool inverted) {
  // Wrapper: JSON ist bereits schnell, kein "NoReload" nötig
  return updateAnalogMinMaxInteger(sensorId, measurementIndex, minValue, maxValue, inverted);
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementInterval(const String& sensorId, unsigned long interval) {
  // Measurement interval ist NICHT pro Messung, sondern sensor-weit
  // Speichere in settings.json

  const char* settingsPath = "/config/settings.json";

  // Load existing settings.json
  DynamicJsonDocument doc(4096);
  if (!loadJsonFile(settingsPath, doc)) {
    logger.error(F("SensorP"), F("Konnte settings.json nicht laden"));
    return PersistenceResult::fail(ConfigError::FILE_ERROR, "Cannot load settings.json");
  }

  // Ensure sensors object exists
  if (!doc.containsKey("sensors")) {
    doc.createNestedObject("sensors");
  }

  JsonObject sensors = doc["sensors"];

  // Ensure sensor object exists
  if (!sensors.containsKey(sensorId)) {
    sensors.createNestedObject(sensorId);
  }

  // Update interval
  sensors[sensorId]["interval"] = interval;

  // Save back to file
  if (!saveJsonFile(settingsPath, doc)) {
    logger.error(F("SensorP"), F("Konnte settings.json nicht speichern"));
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot save settings.json");
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Messintervall für ") + sensorId + F(" auf ") + String(interval) +
                                   F("ms gesetzt"));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementEnabled(const String& sensorId, size_t measurementIndex,
                                            bool enabled) {
  // Wrapper: Erstelle temporäres JsonDocument für Typ-Konvertierung
  StaticJsonDocument<32> doc;
  doc.set(enabled);
  return updateMeasurementSetting(sensorId, measurementIndex, "enabled", doc.as<JsonVariant>());
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementName(const String& sensorId, size_t measurementIndex,
                                         const String& name) {
  // Wrapper: Erstelle temporäres JsonDocument für Typ-Konvertierung
  StaticJsonDocument<128> doc;
  doc.set(name);
  return updateMeasurementSetting(sensorId, measurementIndex, "name", doc.as<JsonVariant>());
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAbsoluteMinMax(const String& sensorId, size_t measurementIndex,
                                        float absoluteMin, float absoluteMax) {
  // Wrapper: Batch update für beide Felder
  DynamicJsonDocument doc(256);
  JsonObject settings = doc.to<JsonObject>();
  settings["absoluteMin"] = absoluteMin;
  settings["absoluteMax"] = absoluteMax;
  return updateMeasurementSettings(sensorId, measurementIndex, settings);
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogRawMinMax(const String& sensorId, size_t measurementIndex,
                                         int absoluteRawMin, int absoluteRawMax) {
  // Wrapper: Batch update für beide Felder
  DynamicJsonDocument doc(256);
  JsonObject settings = doc.to<JsonObject>();
  settings["absoluteRawMin"] = absoluteRawMin;
  settings["absoluteRawMax"] = absoluteRawMax;
  return updateMeasurementSettings(sensorId, measurementIndex, settings);
}

void SensorPersistence::enqueueAnalogRawMinMax(const String& sensorId, size_t measurementIndex,
                                               int absoluteRawMin, int absoluteRawMax) {
  // Check if we already have a RAW_MIN_MAX pending update for this sensor/measurement
  for (auto& u : g_pendingUpdates) {
    if (u.type == PendingUpdateType::RAW_MIN_MAX && u.sensorId == sensorId &&
        u.measurementIndex == measurementIndex) {
      // Update existing entry with new values
      u.data.raw.absoluteRawMin = absoluteRawMin;
      u.data.raw.absoluteRawMax = absoluteRawMax;
      u.timestamp = millis();
      return;
    }
  }

  // Add new entry
  PendingUpdate u;
  u.type = PendingUpdateType::RAW_MIN_MAX;
  u.sensorId = sensorId;
  u.measurementIndex = measurementIndex;
  u.timestamp = millis();
  u.data.raw.absoluteRawMin = absoluteRawMin;
  u.data.raw.absoluteRawMax = absoluteRawMax;

  // Keep queue size reasonable — if it grows too large, flush oldest entries
  const size_t MAX_PENDING = 32;
  if (g_pendingUpdates.size() >= MAX_PENDING) {
    logger.warning(F("SensorP"), F("Pending updates queue full, forcing partial flush"));
    // Flush oldest entry
    if (!g_pendingUpdates.empty()) {
      PendingUpdate oldest = g_pendingUpdates.front();
      g_pendingUpdates.erase(g_pendingUpdates.begin());

      switch (oldest.type) {
      case PendingUpdateType::RAW_MIN_MAX:
        updateAnalogRawMinMax(oldest.sensorId, oldest.measurementIndex,
                              oldest.data.raw.absoluteRawMin, oldest.data.raw.absoluteRawMax);
        break;
      case PendingUpdateType::ABSOLUTE_MIN_MAX:
        updateAbsoluteMinMax(oldest.sensorId, oldest.measurementIndex,
                             oldest.data.absolute.absoluteMin, oldest.data.absolute.absoluteMax);
        break;
      case PendingUpdateType::CALIBRATED_MIN_MAX:
        updateAnalogMinMaxIntegerNoReload(
            oldest.sensorId, oldest.measurementIndex, oldest.data.calibrated.minValue,
            oldest.data.calibrated.maxValue, oldest.data.calibrated.inverted);
        break;
      }
    }
  }
  g_pendingUpdates.push_back(std::move(u));
}

void SensorPersistence::enqueueAbsoluteMinMax(const String& sensorId, size_t measurementIndex,
                                              float absoluteMin, float absoluteMax) {
  // Check if we already have an ABSOLUTE_MIN_MAX pending update for this sensor/measurement
  for (auto& u : g_pendingUpdates) {
    if (u.type == PendingUpdateType::ABSOLUTE_MIN_MAX && u.sensorId == sensorId &&
        u.measurementIndex == measurementIndex) {
      // Update existing entry with new values
      u.data.absolute.absoluteMin = absoluteMin;
      u.data.absolute.absoluteMax = absoluteMax;
      u.timestamp = millis();
      return;
    }
  }

  // Add new entry
  PendingUpdate u;
  u.type = PendingUpdateType::ABSOLUTE_MIN_MAX;
  u.sensorId = sensorId;
  u.measurementIndex = measurementIndex;
  u.timestamp = millis();
  u.data.absolute.absoluteMin = absoluteMin;
  u.data.absolute.absoluteMax = absoluteMax;

  const size_t MAX_PENDING = 32;
  if (g_pendingUpdates.size() >= MAX_PENDING) {
    logger.warning(F("SensorP"), F("Pending updates queue full, forcing partial flush"));
    if (!g_pendingUpdates.empty()) {
      PendingUpdate oldest = g_pendingUpdates.front();
      g_pendingUpdates.erase(g_pendingUpdates.begin());

      switch (oldest.type) {
      case PendingUpdateType::RAW_MIN_MAX:
        updateAnalogRawMinMax(oldest.sensorId, oldest.measurementIndex,
                              oldest.data.raw.absoluteRawMin, oldest.data.raw.absoluteRawMax);
        break;
      case PendingUpdateType::ABSOLUTE_MIN_MAX:
        updateAbsoluteMinMax(oldest.sensorId, oldest.measurementIndex,
                             oldest.data.absolute.absoluteMin, oldest.data.absolute.absoluteMax);
        break;
      case PendingUpdateType::CALIBRATED_MIN_MAX:
        updateAnalogMinMaxIntegerNoReload(
            oldest.sensorId, oldest.measurementIndex, oldest.data.calibrated.minValue,
            oldest.data.calibrated.maxValue, oldest.data.calibrated.inverted);
        break;
      }
    }
  }
  g_pendingUpdates.push_back(std::move(u));
}

void SensorPersistence::enqueueAnalogMinMaxInteger(const String& sensorId, size_t measurementIndex,
                                                   int minValue, int maxValue, bool inverted) {
  // Check if we already have a CALIBRATED_MIN_MAX pending update for this sensor/measurement
  for (auto& u : g_pendingUpdates) {
    if (u.type == PendingUpdateType::CALIBRATED_MIN_MAX && u.sensorId == sensorId &&
        u.measurementIndex == measurementIndex) {
      // Update existing entry with new values
      u.data.calibrated.minValue = minValue;
      u.data.calibrated.maxValue = maxValue;
      u.data.calibrated.inverted = inverted;
      u.timestamp = millis();
      return;
    }
  }

  // Add new entry
  PendingUpdate u;
  u.type = PendingUpdateType::CALIBRATED_MIN_MAX;
  u.sensorId = sensorId;
  u.measurementIndex = measurementIndex;
  u.timestamp = millis();
  u.data.calibrated.minValue = minValue;
  u.data.calibrated.maxValue = maxValue;
  u.data.calibrated.inverted = inverted;

  const size_t MAX_PENDING = 32;
  if (g_pendingUpdates.size() >= MAX_PENDING) {
    logger.warning(F("SensorP"), F("Pending updates queue full, forcing partial flush"));
    if (!g_pendingUpdates.empty()) {
      PendingUpdate oldest = g_pendingUpdates.front();
      g_pendingUpdates.erase(g_pendingUpdates.begin());

      switch (oldest.type) {
      case PendingUpdateType::RAW_MIN_MAX:
        updateAnalogRawMinMax(oldest.sensorId, oldest.measurementIndex,
                              oldest.data.raw.absoluteRawMin, oldest.data.raw.absoluteRawMax);
        break;
      case PendingUpdateType::ABSOLUTE_MIN_MAX:
        updateAbsoluteMinMax(oldest.sensorId, oldest.measurementIndex,
                             oldest.data.absolute.absoluteMin, oldest.data.absolute.absoluteMax);
        break;
      case PendingUpdateType::CALIBRATED_MIN_MAX:
        updateAnalogMinMaxIntegerNoReload(
            oldest.sensorId, oldest.measurementIndex, oldest.data.calibrated.minValue,
            oldest.data.calibrated.maxValue, oldest.data.calibrated.inverted);
        break;
      }
    }
  }
  g_pendingUpdates.push_back(std::move(u));
}

void SensorPersistence::flushPendingUpdatesForSensor(const String& sensorId) {
  if (g_pendingUpdates.empty()) {
    return;
  }

  // Count updates for this sensor
  size_t totalForSensor = 0;
  for (const auto& u : g_pendingUpdates) {
    if (u.sensorId == sensorId) {
      totalForSensor++;
    }
  }

  if (totalForSensor == 0) {
    return; // No updates for this sensor
  }

  unsigned long flushStartTime = millis();

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"),
                 F("Flushe ") + String(totalForSensor) + F(" Updates für ") + sensorId);
  }

  // JSON-based: Group updates by measurementIndex, then apply all changes to each file
  // This is much faster than writing individual fields multiple times
  std::map<size_t, MeasurementConfig> configsToUpdate;

  // First pass: Load all affected measurement configs
  auto it = g_pendingUpdates.begin();
  while (it != g_pendingUpdates.end()) {
    if (it->sensorId != sensorId) {
      ++it;
      continue;
    }

    size_t measurementIndex = it->measurementIndex;

    // Load config if not already loaded
    if (configsToUpdate.find(measurementIndex) == configsToUpdate.end()) {
      MeasurementConfig config;
      auto loadResult = loadMeasurementFromJson(sensorId, measurementIndex, config);
      if (!loadResult.isSuccess()) {
        logger.error(F("SensorP"), F("Fehler beim Laden von Messung ") + String(measurementIndex) +
                                       F(" für ") + sensorId);
        it = g_pendingUpdates.erase(it); // Remove failed update
        continue;
      }
      configsToUpdate[measurementIndex] = config;
    }

    ++it;
  }

  // Second pass: Apply all pending updates to loaded configs (in RAM)
  it = g_pendingUpdates.begin();
  size_t successCount = 0;
  while (it != g_pendingUpdates.end()) {
    if (it->sensorId != sensorId) {
      ++it;
      continue;
    }

    size_t measurementIndex = it->measurementIndex;
    auto& config = configsToUpdate[measurementIndex];

    // Apply update based on type
    switch (it->type) {
    case PendingUpdateType::RAW_MIN_MAX:
      config.absoluteRawMin = it->data.raw.absoluteRawMin;
      config.absoluteRawMax = it->data.raw.absoluteRawMax;
      break;
    case PendingUpdateType::ABSOLUTE_MIN_MAX:
      config.absoluteMin = it->data.absolute.absoluteMin;
      config.absoluteMax = it->data.absolute.absoluteMax;
      break;
    case PendingUpdateType::CALIBRATED_MIN_MAX:
      config.minValue = static_cast<float>(it->data.calibrated.minValue);
      config.maxValue = static_cast<float>(it->data.calibrated.maxValue);
      config.inverted = it->data.calibrated.inverted;
      break;
    }

    successCount++;
    it = g_pendingUpdates.erase(it); // Remove processed update
  }

  // Third pass: Save all modified configs back to JSON files
  for (auto& pair : configsToUpdate) {
    size_t measurementIndex = pair.first;
    const MeasurementConfig& config = pair.second;

    auto saveResult = saveMeasurementToJson(sensorId, measurementIndex, config);
    if (!saveResult.isSuccess()) {
      logger.error(F("SensorP"), F("Fehler beim Speichern von Messung ") +
                                     String(measurementIndex) + F(" für ") + sensorId);
    }
    yield(); // Feed watchdog between file writes
  }

  unsigned long totalFlushTime = millis() - flushStartTime;

  // Log flush performance
  logger.info(F("SensorP"), String(successCount) + F(" Updates für ") + sensorId + F(" in ") +
                                String(totalFlushTime) + F(" ms geflusht (JSON)"));
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogCalibrationMode(const String& sensorId, size_t measurementIndex,
                                               bool enabled) {
  // Wrapper: Erstelle temporäres JsonDocument für Typ-Konvertierung
  StaticJsonDocument<32> doc;
  doc.set(enabled);
  return updateMeasurementSetting(sensorId, measurementIndex, "calibrationMode",
                                  doc.as<JsonVariant>());
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAutocalDuration(const String& sensorId, size_t measurementIndex,
                                         uint32_t halfLifeSeconds) {
  // Wrapper: Erstelle temporäres JsonDocument für Typ-Konvertierung
  StaticJsonDocument<32> doc;
  doc.set(halfLifeSeconds);
  return updateMeasurementSetting(sensorId, measurementIndex, "autocalHalfLifeSeconds",
                                  doc.as<JsonVariant>());
}

// ============================================================================
// JSON-based persistence (new approach - replaces Preferences for sensor config)
// ============================================================================

#include "../utils/json_file_utils.h"

/**
 * @brief Save a single measurement configuration to JSON file
 * @param sensorId Sensor ID (e.g., "ANALOG", "DHT")
 * @param measurementIndex Measurement index (0-based)
 * @param config Measurement configuration to save
 * @return PersistenceResult indicating success or failure
 */
SensorPersistence::PersistenceResult
SensorPersistence::saveMeasurementToJson(const String& sensorId, size_t measurementIndex,
                                         const MeasurementConfig& config) {
  // Allocate small JSON document (~512 bytes)
  DynamicJsonDocument doc(512);

  // Serialize measurement config
  doc["enabled"] = config.enabled;
  doc["name"] = config.name;
  doc["fieldName"] = config.fieldName;
  doc["unit"] = config.unit;
  doc["minValue"] = config.minValue;
  doc["maxValue"] = config.maxValue;

  JsonObject thresholds = doc.createNestedObject("thresholds");
  thresholds["yellowLow"] = config.limits.yellowLow;
  thresholds["greenLow"] = config.limits.greenLow;
  thresholds["greenHigh"] = config.limits.greenHigh;
  thresholds["yellowHigh"] = config.limits.yellowHigh;

  // Analog-specific fields
  if (sensorId == "ANALOG") {
    doc["inverted"] = config.inverted;
    doc["calibrationMode"] = config.calibrationMode;
    doc["autocalHalfLifeSeconds"] = config.autocalHalfLifeSeconds;
    doc["absoluteRawMin"] = config.absoluteRawMin;
    doc["absoluteRawMax"] = config.absoluteRawMax;

    // Store null for INFINITY values
    if (isinf(config.absoluteMin)) {
      doc["absoluteMin"] = nullptr;
    } else {
      doc["absoluteMin"] = config.absoluteMin;
    }

    if (isinf(config.absoluteMax)) {
      doc["absoluteMax"] = nullptr;
    } else {
      doc["absoluteMax"] = config.absoluteMax;
    }
  }

  String path = getMeasurementFilePath(sensorId, measurementIndex);

  if (!saveJsonFile(path, doc)) {
    logger.error(F("SensorP"), F("Fehler beim Schreiben von ") + path);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot write measurement file");
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Messung gespeichert: ") + path);
  }

  return PersistenceResult::success();
}

/**
 * @brief Load a single measurement configuration from JSON file
 * @param sensorId Sensor ID (e.g., "ANALOG", "DHT")
 * @param measurementIndex Measurement index (0-based)
 * @param config Output parameter - loaded configuration
 * @return PersistenceResult indicating success or failure
 */
SensorPersistence::PersistenceResult
SensorPersistence::loadMeasurementFromJson(const String& sensorId, size_t measurementIndex,
                                           MeasurementConfig& config) {
  String path = getMeasurementFilePath(sensorId, measurementIndex);

  if (!LittleFS.exists(path)) {
    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"), F("Messung-Datei nicht gefunden: ") + path);
    }
    return PersistenceResult::fail(ConfigError::FILE_ERROR, "Measurement file not found");
  }

  // Allocate small JSON document (~512 bytes)
  DynamicJsonDocument doc(512);

  if (!loadJsonFile(path, doc)) {
    logger.error(F("SensorP"), F("Fehler beim Lesen von ") + path);
    return PersistenceResult::fail(ConfigError::FILE_ERROR, "Cannot read measurement file");
  }

  // Deserialize measurement config
  config.enabled = doc["enabled"] | true;
  config.name = doc["name"] | String("");
  config.fieldName = doc["fieldName"] | String("");
  config.unit = doc["unit"] | String("");
  config.minValue = doc["minValue"] | 0.0f;
  config.maxValue = doc["maxValue"] | 100.0f;

  JsonObjectConst thresholds = doc["thresholds"];
  config.limits.yellowLow = thresholds["yellowLow"] | 0.0f;
  config.limits.greenLow = thresholds["greenLow"] | 0.0f;
  config.limits.greenHigh = thresholds["greenHigh"] | 100.0f;
  config.limits.yellowHigh = thresholds["yellowHigh"] | 100.0f;

  // Analog-specific fields
  if (sensorId == "ANALOG") {
    config.inverted = doc["inverted"] | false;
    config.calibrationMode = doc["calibrationMode"] | false;
    config.autocalHalfLifeSeconds = doc["autocalHalfLifeSeconds"] | 0;
    config.absoluteRawMin = doc["absoluteRawMin"] | 0;
    config.absoluteRawMax = doc["absoluteRawMax"] | 1023;

    // Handle null for INFINITY
    if (doc["absoluteMin"].isNull()) {
      config.absoluteMin = INFINITY;
    } else {
      config.absoluteMin = doc["absoluteMin"] | INFINITY;
    }

    if (doc["absoluteMax"].isNull()) {
      config.absoluteMax = -INFINITY;
    } else {
      config.absoluteMax = doc["absoluteMax"] | -INFINITY;
    }
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Messung geladen: ") + path);
  }

  return PersistenceResult::success();
}

// ============================================================================
// Generische Update-Funktionen (Macro-basiert für DRY-Prinzip)
// ============================================================================

/**
 * @brief Zentrale Feld-Definition für alle MeasurementConfig-Felder
 * Diese Macro-Liste ist die EINZIGE Stelle, die bei neuen Feldern geändert werden muss!
 */
#define MEASUREMENT_FIELDS                                                                         \
  FIELD(enabled, bool)                                                                             \
  FIELD(name, String)                                                                              \
  FIELD(fieldName, String)                                                                         \
  FIELD(unit, String)                                                                              \
  FIELD(minValue, float)                                                                           \
  FIELD(maxValue, float)                                                                           \
  FIELD(inverted, bool)                                                                            \
  FIELD(calibrationMode, bool)                                                                     \
  FIELD(autocalHalfLifeSeconds, uint32_t)                                                          \
  FIELD(absoluteRawMin, int)                                                                       \
  FIELD(absoluteRawMax, int)                                                                       \
  FIELD(absoluteMin, float)                                                                        \
  FIELD(absoluteMax, float)

/**
 * @brief Helper: Setzt ein einzelnes Feld in MeasurementConfig
 * Nutzt Macro-Expansion um Code-Duplikation zu vermeiden
 */
static bool setConfigField(MeasurementConfig& config, const String& fieldName,
                           const JsonVariant& value) {
// Macro generiert automatisch alle if-Zweige
#define FIELD(name, type)                                                                          \
  if (fieldName == #name) {                                                                        \
    config.name = value.as<type>();                                                                \
    return true;                                                                                   \
  }

  MEASUREMENT_FIELDS // Expandiert zu if-Kette

#undef FIELD

      // Spezialfall: Nested "thresholds" Objekt
      if (fieldName == "thresholds") {
    JsonObject t = value.as<JsonObject>();
    if (t.containsKey("yellowLow"))
      config.limits.yellowLow = t["yellowLow"];
    if (t.containsKey("greenLow"))
      config.limits.greenLow = t["greenLow"];
    if (t.containsKey("greenHigh"))
      config.limits.greenHigh = t["greenHigh"];
    if (t.containsKey("yellowHigh"))
      config.limits.yellowHigh = t["yellowHigh"];
    return true;
  }

  // Spezialfall: Einzelne Threshold-Felder
  if (fieldName == "yellowLow") {
    config.limits.yellowLow = value.as<float>();
    return true;
  }
  if (fieldName == "greenLow") {
    config.limits.greenLow = value.as<float>();
    return true;
  }
  if (fieldName == "greenHigh") {
    config.limits.greenHigh = value.as<float>();
    return true;
  }
  if (fieldName == "yellowHigh") {
    config.limits.yellowHigh = value.as<float>();
    return true;
  }

  return false; // Unbekanntes Feld
}

/**
 * @brief Generische Update-Funktion für ein einzelnes Feld
 * Ersetzt alle spezialisierten update*() Funktionen
 */
SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementSetting(const String& sensorId, size_t measurementIndex,
                                            const String& fieldName, const JsonVariant& value) {
  // Load
  MeasurementConfig config;
  auto loadResult = loadMeasurementFromJson(sensorId, measurementIndex, config);
  if (!loadResult.isSuccess()) {
    return loadResult;
  }

  // Update (via Macro-generated code)
  if (!setConfigField(config, fieldName, value)) {
    return PersistenceResult::fail(ConfigError::VALIDATION_ERROR,
                                   String(F("Unbekanntes Feld: ")) + fieldName);
  }

  // Save
  return saveMeasurementToJson(sensorId, measurementIndex, config);
}

/**
 * @brief Batch-Update für mehrere Felder auf einmal
 * Effizient: Nur 1x laden/speichern statt mehrfach
 */
SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementSettings(const String& sensorId, size_t measurementIndex,
                                             const JsonObject& settings) {
  MeasurementConfig config;
  auto loadResult = loadMeasurementFromJson(sensorId, measurementIndex, config);
  if (!loadResult.isSuccess()) {
    return loadResult;
  }

  // Apply all settings
  for (JsonPair kv : settings) {
    if (!setConfigField(config, kv.key().c_str(), kv.value())) {
      logger.warning(F("SensorP"), F("Überspringe unbekanntes Feld: ") + String(kv.key().c_str()));
    }
  }

  return saveMeasurementToJson(sensorId, measurementIndex, config);
}

// Räume Macro-Definition auf
#undef MEASUREMENT_FIELDS

// ============================================================================
// Update-Wrapper (Kompatibilität mit bestehenden Code)
// Nutzen die generische updateMeasurementSetting() Funktion
// ============================================================================
