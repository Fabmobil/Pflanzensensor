/**
 * @file manager_sensor_persistence.cpp
 * @brief Implementation of SensorPersistence class
 */

#include "manager_sensor_persistence.h"

#include <LittleFS.h>

#include "../logger/logger.h"
#include "../utils/critical_section.h"
#include "../utils/preferences_manager.h"
#include "managers/manager_config.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"
#include "sensors/sensors.h"
#if USE_ANALOG
#include "sensors/sensor_analog.h"
#endif
#include "sensors/sensor_autocalibration.h"

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

      auto measResult = PreferencesManager::loadSensorMeasurement(
          sensorId, i, enabled, measName, fieldName, unit, minValue, maxValue, yellowLow, greenLow,
          greenHigh, yellowHigh, inverted, calibrationMode, autocalDuration, absoluteRawMin,
          absoluteRawMax);

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
          meas.absoluteRawMin, meas.absoluteRawMax);

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
  // Use critical section instead of callback manipulation to prevent memory
  // corruption
  CriticalSection cs;

  PersistenceResult result = updateSensorThresholdsInternal(sensorId, measurementIndex, yellowLow,
                                                            greenLow, greenHigh, yellowHigh);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateSensorThresholdsInternal(const String& sensorId, size_t measurementIndex,
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
  PreferencesManager::putFloat(prefs, (prefix + "gl").c_str(), greenLow);
  PreferencesManager::putFloat(prefs, (prefix + "gh").c_str(), greenHigh);
  PreferencesManager::putFloat(prefs, (prefix + "yh").c_str(), yellowHigh);

  prefs.end();

  logger.info(F("SensorP"), String(F("Schwellenwerte aktualisiert für ")) + sensorId +
                                F(" Messung ") + String(measurementIndex));

  // Reload configuration
  auto reloadResult = load();
  return reloadResult;

  logger.info(F("SensorP"), F("Schwellwerte atomar aktualisiert für ") + sensorId + F(" Messung ") +
                                String(measurementIndex) + F(", geschätzte Größe: ") +
                                String(getConfigSize()) + F(" Bytes"));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogMinMax(const String& sensorId, size_t measurementIndex,
                                      float minValue, float maxValue, bool inverted) {
  // Kritischen Abschnitt nutzen, um Speicherfehler zu vermeiden
  CriticalSection cs;

  PersistenceResult result =
      updateAnalogMinMaxInternal(sensorId, measurementIndex, minValue, maxValue, inverted);

  return result;
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

  // Reload configuration to sync in-memory state
  auto reloadResult = load();
  if (!reloadResult.isSuccess()) {
    logger.warning(F("SensorP"),
                   F("Neuladen der Sensorkonfiguration nach int min/max Update fehlgeschlagen: ") +
                       reloadResult.getMessage());
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult SensorPersistence::updateAnalogMinMaxIntegerNoReload(
    const String& sensorId, size_t measurementIndex, int minValue, int maxValue, bool inverted) {
  // Update min/max/inverted directly in Preferences without reload
  CriticalSection cs;

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

  logger.info(F("SensorP"), F("Analog min/max aktualisiert für ") + sensorId +
                                String(measurementIndex) + F(". Min: ") + String(minValue) +
                                F(", Max: ") + String(maxValue) + F(", Inverted: ") +
                                String(inverted));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementInterval(const String& sensorId, unsigned long interval) {
  CriticalSection cs;

  PersistenceResult result = updateMeasurementIntervalInternal(sensorId, interval);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementIntervalInternal(const String& sensorId,
                                                     unsigned long interval) {
  // Update measurement interval directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  PreferencesManager::putUInt(prefs, "meas_int", interval);
  prefs.end();

  logger.info(F("SensorP"), F("Messintervall aktualisiert für ") + sensorId + F(", Intervall: ") +
                                String(interval));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementEnabled(const String& sensorId, size_t measurementIndex,
                                            bool enabled) {
  // Kritischen Abschnitt nutzen, um Speicherfehler zu vermeiden
  CriticalSection cs;

  PersistenceResult result = updateMeasurementEnabledInternal(sensorId, measurementIndex, enabled);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateMeasurementEnabledInternal(const String& sensorId, size_t measurementIndex,
                                                    bool enabled) {
  // Update enabled flag directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putBool(prefs, (prefix + "en").c_str(), enabled);
  prefs.end();

  logger.info(F("SensorP"), F("Messung aktiviert/deaktiviert für ") + sensorId + F(" Messung ") +
                                String(measurementIndex) + F(", aktiviert: ") + String(enabled));

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAbsoluteMinMax(const String& sensorId, size_t measurementIndex,
                                        float absoluteMin, float absoluteMax) {
  // Update absolute min/max directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putFloat(prefs, (prefix + "absMin").c_str(), absoluteMin);
  PreferencesManager::putFloat(prefs, (prefix + "absMax").c_str(), absoluteMax);

  prefs.end();

  logger.info(F("SensorP"), F("Absolute min/max atomar aktualisiert für ") + sensorId +
                                F(" Messung ") + String(measurementIndex));

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Konfiguration aktualisiert für Sensor ") + sensorId +
                                   F(" Messung ") + String(measurementIndex) + F(" - Min: ") +
                                   String(absoluteMin) + F(", Max: ") + String(absoluteMax));
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogRawMinMax(const String& sensorId, size_t measurementIndex,
                                         int absoluteRawMin, int absoluteRawMax) {
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("updateAnalogRawMinMax aufgerufen für ") + sensorId +
                                   F(" Messung ") + String(measurementIndex) +
                                   F(" mit Werten Min: ") + String(absoluteRawMin) + F(", Max: ") +
                                   String(absoluteRawMax));
  }

  // Kritischen Abschnitt über die gesamte Operation legen
  CriticalSection cs;

  PersistenceResult result =
      updateAnalogRawMinMaxInternal(sensorId, measurementIndex, absoluteRawMin, absoluteRawMax);

  return result;
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAnalogRawMinMaxInternal(const String& sensorId, size_t measurementIndex,
                                                 int absoluteRawMin, int absoluteRawMax) {
  // Update raw min/max directly in Preferences
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("SensorP"), String(F("Fehler beim Öffnen von Preferences für ")) + sensorId);
    return PersistenceResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }

  String prefix = "m" + String(measurementIndex) + "_";
  PreferencesManager::putInt(prefs, (prefix + "rmin").c_str(), absoluteRawMin);
  PreferencesManager::putInt(prefs, (prefix + "rmax").c_str(), absoluteRawMax);
  prefs.end();

  logger.info(F("SensorP"), F("Analog raw min/max aktualisiert für ") + sensorId + F(" Messung ") +
                                String(measurementIndex));

  // Nach dem Update Konfiguration neu laden, damit In-Memory-Config synchron bleibt
  auto reloadResult = load();
  if (!reloadResult.isSuccess()) {
    logger.warning(F("SensorP"),
                   F("Neuladen der Sensorkonfiguration nach raw min/max Update fehlgeschlagen: ") +
                       reloadResult.getMessage());
  } else {
    if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("SensorP"),
                   F("Sensorkonfiguration nach raw min/max Update erfolgreich neu geladen"));
    }
  }

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("SensorP"), F("Konfiguration aktualisiert für Sensor ") + sensorId +
                                   F(" Messung ") + String(measurementIndex) + F(" - Min: ") +
                                   String(absoluteRawMin) + F(", Max: ") + String(absoluteRawMax));
  }

  return PersistenceResult::success();
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

  // Reload configuration to sync in-memory state
  auto reloadResult = load();
  if (!reloadResult.isSuccess()) {
    logger.warning(
        F("SensorP"),
        F("Neuladen der Sensorkonfiguration nach Kalibrierungsmodus-Update fehlgeschlagen: ") +
            reloadResult.getMessage());
  }

  return PersistenceResult::success();
}

SensorPersistence::PersistenceResult
SensorPersistence::updateAutocalDuration(const String& sensorId, size_t measurementIndex,
                                         uint32_t halfLifeSeconds) {
  // Use critical section to avoid concurrent writes
  CriticalSection cs;

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

  // Reload configuration to sync in-memory state
  auto reloadResult = load();
  if (!reloadResult.isSuccess()) {
    logger.warning(
        F("SensorP"),
        F("Neuladen der Sensorkonfiguration nach Autocal-Dauer-Update fehlgeschlagen: ") +
            reloadResult.getMessage());
  }

  return PersistenceResult::success();
}
