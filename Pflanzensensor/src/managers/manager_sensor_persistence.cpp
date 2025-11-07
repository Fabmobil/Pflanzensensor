/**
 * @file manager_sensor_persistence.cpp
 * @brief Implementation of SensorPersistence class
 */

#include "manager_sensor_persistence.h"

#include <LittleFS.h>

#include "../logger/logger.h"
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

bool SensorPersistence::configExists() {
  // Check if any sensor has Preferences data
  extern std::unique_ptr<SensorManager> sensorManager;
  if (sensorManager && sensorManager->getState() == ManagerState::INITIALIZED) {
    const auto& allSensors = sensorManager->getSensors();
    for (const auto& sensorPtr : allSensors) {
      if (sensorPtr && PreferencesManager::sensorNamespaceExists(sensorPtr->config().id)) {
        return true;
      }
    }
  }
  return false;
}

size_t SensorPersistence::getConfigSize() {
  // Preferences don't have a direct "file size", return estimated size
  if (!configExists()) {
    return 0;
  }

  // Estimate based on number of sensors with Preferences data
  extern std::unique_ptr<SensorManager> sensorManager;
  if (!sensorManager || sensorManager->getState() != ManagerState::INITIALIZED) {
    return 0;
  }

  size_t estimatedSize = 0;
  const auto& allSensors = sensorManager->getSensors();
  for (const auto& sensorPtr : allSensors) {
    if (sensorPtr && PreferencesManager::sensorNamespaceExists(sensorPtr->config().id)) {
      // Rough estimate: ~1KB per sensor configuration
      estimatedSize += 1024;
    }
  }

  return estimatedSize;
}

SensorPersistence::PersistenceResult SensorPersistence::load() {
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Beginne Laden der Sensorkonfiguration aus Preferences"));
  }

  extern std::unique_ptr<SensorManager> sensorManager;

  // Early exit if sensor manager is not available
  if (!sensorManager || sensorManager->getState() != ManagerState::INITIALIZED) {
    logger.warning(F("SensorP"), F("Sensor-Manager nicht bereit, überspringe Laden"));
    return PersistenceResult::success();
  }

  const auto& allSensors = sensorManager->getSensors();

  // Load each sensor from Preferences
  for (const auto& sensorPtr : allSensors) {
    if (!sensorPtr)
      continue;

    String sensorId = sensorPtr->config().id;

    // Check if this sensor has Preferences data
    if (!PreferencesManager::sensorNamespaceExists(sensorId)) {
      if (ConfigMgr.isDebugSensor()) {
        logger.debug(F("SensorP"), String(F("Keine Preferences für Sensor: ")) + sensorId);
      }
      continue;
    }

    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"), String(F("Lade Sensor aus Preferences: ")) + sensorId);
    }

    // Load sensor settings
    String name;
    unsigned long measurementInterval;
    bool hasPersistentError;

    auto sensorResult = PreferencesManager::loadSensorSettings(sensorId, name, measurementInterval,
                                                               hasPersistentError);

    if (!sensorResult.isSuccess()) {
      logger.warning(F("SensorP"),
                     String(F("Fehler beim Laden der Sensor-Einstellungen für ")) + sensorId);
      continue;
    }

    // Apply sensor-level settings
    SensorConfig& config = sensorPtr->mutableConfig();
    if (!name.isEmpty()) {
      config.name = name;
    }
    config.measurementInterval = measurementInterval;
    config.hasPersistentError = hasPersistentError;
    sensorPtr->setMeasurementInterval(measurementInterval);

    // Load measurement settings
    for (size_t i = 0; i < config.activeMeasurements; ++i) {
      bool enabled;
      String measName, fieldName, unit;
      float minValue, maxValue;
      float yellowLow, greenLow, greenHigh, yellowHigh;
      bool inverted, calibrationMode;
      uint32_t autocalDuration;
      int absoluteRawMin, absoluteRawMax;
      float absoluteMin, absoluteMax;

      auto measResult = PreferencesManager::loadSensorMeasurement(
          sensorId, i, enabled, measName, fieldName, unit, minValue, maxValue, yellowLow, greenLow,
          greenHigh, yellowHigh, inverted, calibrationMode, autocalDuration, absoluteRawMin,
          absoluteRawMax, absoluteMin, absoluteMax);

      if (!measResult.isSuccess()) {
        if (ConfigMgr.isDebugSensor()) {
          logger.debug(F("SensorP"),
                       String(F("Keine Messung ")) + String(i) + F(" für ") + sensorId);
        }
        continue;
      }

      // Apply measurement settings
      config.measurements[i].enabled = enabled;
      if (!measName.isEmpty())
        config.measurements[i].name = measName;
      if (!fieldName.isEmpty())
        config.measurements[i].fieldName = fieldName;
      if (!unit.isEmpty())
        config.measurements[i].unit = unit;
      config.measurements[i].minValue = minValue;
      config.measurements[i].maxValue = maxValue;
      config.measurements[i].limits.yellowLow = yellowLow;
      config.measurements[i].limits.greenLow = greenLow;
      config.measurements[i].limits.greenHigh = greenHigh;
      config.measurements[i].limits.yellowHigh = yellowHigh;
      config.measurements[i].inverted = inverted;
      config.measurements[i].calibrationMode = calibrationMode;
      config.measurements[i].autocalHalfLifeSeconds = autocalDuration;
      config.measurements[i].absoluteRawMin = absoluteRawMin;
      config.measurements[i].absoluteRawMax = absoluteRawMax;
      config.measurements[i].absoluteMin = absoluteMin;
      config.measurements[i].absoluteMax = absoluteMax;

      if (ConfigMgr.isDebugSensor()) {
        logger.debug(F("SensorP"),
                     String(F("Messung ")) + String(i) + F(" geladen für ") + sensorId);
      }
    }
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Laden aller Sensoren aus Preferences beendet"));
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

  for (const auto& sensorPtr : allSensors) {
    if (!sensorPtr)
      continue;

    const SensorConfig& sensorConfig = sensorPtr->config();
    String sensorId = sensorConfig.id;

    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"), String(F("Speichere Sensor: ")) + sensorId);
    }

    // Save sensor-level settings
    auto sensorResult = PreferencesManager::saveSensorSettings(sensorId, sensorConfig.name,
                                                               sensorConfig.measurementInterval,
                                                               sensorConfig.hasPersistentError);

    if (!sensorResult.isSuccess()) {
      logger.error(F("SensorP"), String(F("Fehler beim Speichern von Sensor ")) + sensorId);
      return sensorResult;
    }

    // Save each measurement
    for (size_t i = 0; i < sensorConfig.activeMeasurements; ++i) {
      const auto& meas = sensorConfig.measurements[i];

      auto measResult = PreferencesManager::saveSensorMeasurement(
          sensorId, i, meas.enabled, meas.name, meas.fieldName, meas.unit, meas.minValue,
          meas.maxValue, meas.limits.yellowLow, meas.limits.greenLow, meas.limits.greenHigh,
          meas.limits.yellowHigh, meas.inverted, meas.calibrationMode, meas.autocalHalfLifeSeconds,
          meas.absoluteRawMin, meas.absoluteRawMax, meas.absoluteMin, meas.absoluteMax);

      if (!measResult.isSuccess()) {
        logger.error(F("SensorP"), String(F("Fehler beim Speichern von Messung ")) + String(i) +
                                       F(" für ") + sensorId);
        return measResult;
      }
    }
  }

  logger.info(F("SensorP"), F("Alle Sensoren in Preferences gespeichert"));
  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateSensorThresholds(const String& sensorId, size_t measurementIndex,
                                          float yellowLow, float greenLow, float greenHigh,
                                          float yellowHigh) {
  // Update thresholds directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putFloat(prefs, (prefix + "yl").c_str(), yellowLow);
  yield(); // Feed watchdog
  PreferencesManager::putFloat(prefs, (prefix + "gl").c_str(), greenLow);
  yield(); // Feed watchdog
  PreferencesManager::putFloat(prefs, (prefix + "gh").c_str(), greenHigh);
  yield(); // Feed watchdog
  PreferencesManager::putFloat(prefs, (prefix + "yh").c_str(), yellowHigh);

  prefs.end();

  logger.info(F("SensorP"), String(F("Schwellenwerte aktualisiert für ")) + sensorId +
                                F(" Messung ") + String(measurementIndex));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogMinMax(const String& sensorId, size_t measurementIndex,
                                      float minValue, float maxValue, bool inverted) {
  // Update min/max/inverted directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putFloat(prefs, (prefix + "min").c_str(), minValue);
  yield(); // Feed watchdog
  PreferencesManager::putFloat(prefs, (prefix + "max").c_str(), maxValue);
  yield(); // Feed watchdog
  PreferencesManager::putBool(prefs, (prefix + "inv").c_str(), inverted);

  prefs.end();

  logger.info(F("SensorP"), F("Analog min/max atomar aktualisiert für ") + sensorId +
                                F(" Messung ") + String(measurementIndex));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogMinMaxInteger(const String& sensorId, size_t measurementIndex,
                                             int minValue, int maxValue, bool inverted) {
  // Update min/max/inverted directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putFloat(prefs, (prefix + "min").c_str(), static_cast<float>(minValue));
  PreferencesManager::putFloat(prefs, (prefix + "max").c_str(), static_cast<float>(maxValue));
  PreferencesManager::putBool(prefs, (prefix + "inv").c_str(), inverted);

  prefs.end();

  logger.info(F("SensorP"), F("Analog min/max (int) atomar aktualisiert für ") + sensorId +
                                F(" Messung ") + String(measurementIndex));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult SensorPersistence::updateAnalogMinMaxIntegerNoReload(
    const String& sensorId, size_t measurementIndex, int minValue, int maxValue, bool inverted) {
  // Update min/max/inverted directly in Preferences without reload
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  String prefix = "m" + String(measurementIndex) + "_";

  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  PreferencesManager::putFloat(prefs, (prefix + "min").c_str(), static_cast<float>(minValue));
  yield(); // Feed watchdog between writes
  PreferencesManager::putFloat(prefs, (prefix + "max").c_str(), static_cast<float>(maxValue));
  yield(); // Feed watchdog between writes
  PreferencesManager::putBool(prefs, (prefix + "inv").c_str(), inverted);

  prefs.end();

  // Minimal logging - avoid String concatenation to save heap
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Analog min/max updated"));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementInterval(const String& sensorId, unsigned long interval) {
  // Update measurement interval directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);

  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  PreferencesManager::putUInt(prefs, "meas_int", interval);
  prefs.end();

  // Minimal logging
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Messintervall updated"));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementEnabled(const String& sensorId, size_t measurementIndex,
                                            bool enabled) {
  // Update enabled flag directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  String prefix = "m" + String(measurementIndex) + "_";

  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  PreferencesManager::putBool(prefs, (prefix + "en").c_str(), enabled);
  prefs.end();

  // Minimal logging
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Measurement enabled updated"));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementName(const String& sensorId, size_t measurementIndex,
                                         const String& name) {
  // Update name directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  String prefix = "m" + String(measurementIndex) + "_";

  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  PreferencesManager::putString(prefs, (prefix + "name").c_str(), name);
  prefs.end();

  // Minimal logging
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Measurement name updated"));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAbsoluteMinMax(const String& sensorId, size_t measurementIndex,
                                        float absoluteMin, float absoluteMax) {
  // Update absolute min/max directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  String prefix = "m" + String(measurementIndex) + "_";

  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  PreferencesManager::putFloat(prefs, (prefix + "absMin").c_str(), absoluteMin);
  yield(); // Feed watchdog between writes
  PreferencesManager::putFloat(prefs, (prefix + "absMax").c_str(), absoluteMax);

  prefs.end();

  // Minimal logging
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Absolute min/max updated"));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogRawMinMax(const String& sensorId, size_t measurementIndex,
                                         int absoluteRawMin, int absoluteRawMax) {
  // Update raw min/max directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  String prefix = "m" + String(measurementIndex) + "_";

  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  PreferencesManager::putInt(prefs, (prefix + "rmin").c_str(), absoluteRawMin);
  yield(); // Feed watchdog between writes
  PreferencesManager::putInt(prefs, (prefix + "rmax").c_str(), absoluteRawMax);
  prefs.end();

  // Minimal logging - avoid String concatenation to save heap
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Raw min/max updated"));
  }

  return PersistenceResult::success();
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

  // Open Preferences ONCE for this sensor
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;

  unsigned long prefsOpenTime = millis();
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), F("Konnte Namespace nicht öffnen: ") + ns);
    // Remove all updates for this sensor to prevent queue buildup
    g_pendingUpdates.erase(
        std::remove_if(g_pendingUpdates.begin(), g_pendingUpdates.end(),
                       [&sensorId](const PendingUpdate& u) { return u.sensorId == sensorId; }),
        g_pendingUpdates.end());
    return;
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"),
                 F("Preferences geöffnet in ") + String(millis() - prefsOpenTime) + F(" ms"));
  }

  // OPTIMIZED: Process ALL updates for this sensor in one transaction
  // Collect all keys and values FIRST (in RAM), then write in batch
  size_t successCount = 0;
  auto it = g_pendingUpdates.begin();

  // Write all updates for this sensor without yield() between individual writes
  // This minimizes NVS transaction overhead
  while (it != g_pendingUpdates.end()) {
    if (it->sensorId != sensorId) {
      ++it;
      continue;
    }

    // This update is for our sensor - process it
    size_t measurementIndex = it->measurementIndex;
    String prefix = "m" + String(measurementIndex) + "_";

    // Write directly to already-open Preferences
    // NOTE: We don't yield() between writes - this is intentional for performance
    // The prefs.end() call commits everything at once
    switch (it->type) {
    case PendingUpdateType::RAW_MIN_MAX:
      PreferencesManager::putInt(prefs, (prefix + "rmin").c_str(), it->data.raw.absoluteRawMin);
      PreferencesManager::putInt(prefs, (prefix + "rmax").c_str(), it->data.raw.absoluteRawMax);
      break;
    case PendingUpdateType::ABSOLUTE_MIN_MAX:
      PreferencesManager::putFloat(prefs, (prefix + "absMin").c_str(),
                                   it->data.absolute.absoluteMin);
      PreferencesManager::putFloat(prefs, (prefix + "absMax").c_str(),
                                   it->data.absolute.absoluteMax);
      break;
    case PendingUpdateType::CALIBRATED_MIN_MAX:
      PreferencesManager::putFloat(prefs, (prefix + "min").c_str(),
                                   static_cast<float>(it->data.calibrated.minValue));
      PreferencesManager::putFloat(prefs, (prefix + "max").c_str(),
                                   static_cast<float>(it->data.calibrated.maxValue));
      PreferencesManager::putBool(prefs, (prefix + "inv").c_str(), it->data.calibrated.inverted);
      break;
    }

    successCount++;
    it = g_pendingUpdates.erase(it); // Erase and get next iterator
  }

  // Close Preferences ONCE for all updates (this commits all changes in one NVS transaction)
  // This is much faster than committing after each individual write
  unsigned long prefsCloseStartTime = millis();
  prefs.end();
  unsigned long prefsCloseTime = millis() - prefsCloseStartTime;
  unsigned long totalFlushTime = millis() - flushStartTime;

  // Feed watchdog AFTER all writes are done
  yield();

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), String(successCount) + F(" Updates für ") + sensorId +
                                   F(" erfolgreich geschrieben"));
    logger.debug(F("SensorP"), F("prefs.end() dauerte ") + String(prefsCloseTime) +
                                   F(" ms, gesamt ") + String(totalFlushTime) + F(" ms"));
  }
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogCalibrationMode(const String& sensorId, size_t measurementIndex,
                                               bool enabled) {
  // Update calibration mode directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putBool(prefs, (prefix + "cal").c_str(), enabled);
  prefs.end();

  logger.info(F("SensorP"), F("Kalibrierungsmodus aktualisiert für ") + sensorId + F(" Messung ") +
                                String(measurementIndex));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAutocalDuration(const String& sensorId, size_t measurementIndex,
                                         uint32_t halfLifeSeconds) {
  // Update autocal duration directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putUInt(prefs, (prefix + "acd").c_str(), halfLifeSeconds);
  prefs.end();

  logger.info(F("SensorP"), F("Autocal-Dauer aktualisiert für ") + sensorId + F(" Messung ") +
                                String(measurementIndex));

  return PersistenceResult::success();
}
