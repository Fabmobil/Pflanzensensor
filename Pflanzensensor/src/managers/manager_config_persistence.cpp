/**
 * @file manager_config_persistence.cpp
 * @brief Implementation of ConfigPersistence class
 */

#include "manager_config_persistence.h"

#include <ArduinoJson.h>
using namespace ArduinoJson;

#include <LittleFS.h>

#include "../logger/logger.h"
#include "../utils/critical_section.h"
#include "../utils/persistence_utils.h"
#include "../utils/preferences_manager.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"

bool ConfigPersistence::configFileExists() { return LittleFS.exists("/config.json"); }

size_t ConfigPersistence::getConfigFileSize() {
  if (!configFileExists()) {
    return 0;
  }

  File f = LittleFS.open("/config.json", "r");
  if (!f) {
    return 0;
  }

  size_t size = f.size();
  f.close();
  return size;
}

// --- Thresholds persistence helpers ---
// Sensor includes moved to manager_sensor_persistence.cpp

// Sensor settings are now handled by SensorPersistence
// This function has been moved to manager_sensor_persistence.cpp

ConfigPersistence::PersistenceResult ConfigPersistence::loadFromFile(ConfigData& config) {
  logger.logMemoryStats(F("ConfigP_load_before"));
  
  // Check if Preferences exist, if not initialize with defaults
  if (!PreferencesManager::namespaceExists(PreferencesNamespaces::GENERAL)) {
    logger.info(F("ConfigP"), F("Keine Konfiguration gefunden, initialisiere mit Standardwerten..."));
    auto initResult = PreferencesManager::initializeAllNamespaces();
    if (!initResult.isSuccess()) {
      logger.error(F("ConfigP"), F("Fehler beim Initialisieren der Preferences: ") + initResult.getMessage());
      auto result = resetToDefaults(config);
      logger.logMemoryStats(F("ConfigP_load_after"));
      return result;
    }
  }
  
  // Load from Preferences
  logger.info(F("ConfigP"), F("Lade Konfiguration aus Preferences..."));
  
  // Load general settings
  auto generalResult = PreferencesManager::loadGeneralSettings(
    config.deviceName, config.adminPassword, 
    config.md5Verification, config.fileLoggingEnabled);
  
  // Load WiFi settings
  auto wifiResult = PreferencesManager::loadWiFiSettings(
    config.wifiSSID1, config.wifiPassword1,
    config.wifiSSID2, config.wifiPassword2,
    config.wifiSSID3, config.wifiPassword3);
  
  // Load debug settings
  auto debugResult = PreferencesManager::loadDebugSettings(
    config.debugRAM, config.debugMeasurementCycle,
    config.debugSensor, config.debugDisplay, config.debugWebSocket);
  
  // Load LED traffic light settings
  auto ledResult = PreferencesManager::loadLedTrafficSettings(
    config.ledTrafficLightMode, config.ledTrafficLightSelectedMeasurement);
  
  // Load flower status sensor
  auto flowerResult = PreferencesManager::loadFlowerStatusSensor(config.flowerStatusSensor);
  
  if (generalResult.isSuccess() && wifiResult.isSuccess() && 
      debugResult.isSuccess() && ledResult.isSuccess()) {
    logger.info(F("ConfigP"), F("Konfiguration erfolgreich aus Preferences geladen"));
  } else {
    logger.warning(F("ConfigP"), F("Fehler beim Laden einiger Einstellungen, verwende Standardwerte"));
  }
  
  logger.logMemoryStats(F("ConfigP_load_after"));
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::resetToDefaults(ConfigData& config) {
  // Clear Preferences
  logger.info(F("ConfigP"), F("ResetToDefaults: Lösche alle Preferences"));

  // Clear all Preferences namespaces
  auto clearResult = PreferencesManager::clearAll();
  if (!clearResult.isSuccess()) {
    logger.warning(F("ConfigP"), F("Fehler beim Löschen der Preferences: ") + clearResult.getMessage());
  }

  logger.info(F("ConfigP"), F("Factory Reset abgeschlossen"));
  // Return success; caller (UI) will handle reboot
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult
ConfigPersistence::saveToFileMinimal(const ConfigData& config) {
  // Save to Preferences
  logger.info(F("ConfigP"), F("Speichere Konfiguration in Preferences..."));
  
  // Save general settings
  auto generalResult = PreferencesManager::saveGeneralSettings(
    config.deviceName, config.adminPassword,
    config.md5Verification, config.fileLoggingEnabled);
  
  if (!generalResult.isSuccess()) {
    logger.error(F("ConfigP"), F("Fehler beim Speichern der General-Einstellungen"));
    return generalResult;
  }
  
  // Save WiFi settings
  auto wifiResult = PreferencesManager::saveWiFiSettings(
    config.wifiSSID1, config.wifiPassword1,
    config.wifiSSID2, config.wifiPassword2,
    config.wifiSSID3, config.wifiPassword3);
  
  if (!wifiResult.isSuccess()) {
    logger.error(F("ConfigP"), F("Fehler beim Speichern der WiFi-Einstellungen"));
    return wifiResult;
  }
  
  // Save debug settings
  auto debugResult = PreferencesManager::saveDebugSettings(
    config.debugRAM, config.debugMeasurementCycle,
    config.debugSensor, config.debugDisplay, config.debugWebSocket);
  
  if (!debugResult.isSuccess()) {
    logger.error(F("ConfigP"), F("Fehler beim Speichern der Debug-Einstellungen"));
    return debugResult;
  }
  
  // Save LED traffic light settings
  auto ledResult = PreferencesManager::saveLedTrafficSettings(
    config.ledTrafficLightMode, config.ledTrafficLightSelectedMeasurement);
  
  if (!ledResult.isSuccess()) {
    logger.error(F("ConfigP"), F("Fehler beim Speichern der LED-Einstellungen"));
    return ledResult;
  }
  
  // Save flower status sensor
  auto flowerResult = PreferencesManager::saveFlowerStatusSensor(config.flowerStatusSensor);
  
  if (!flowerResult.isSuccess()) {
    logger.error(F("ConfigP"), F("Fehler beim Speichern des Flower-Status-Sensors"));
    return flowerResult;
  }
  
  logger.info(F("ConfigP"), F("Konfiguration erfolgreich in Preferences gespeichert"));
  return PersistenceResult::success();
}
  return result;
}

void ConfigPersistence::writeUpdateFlagsToFile(bool fs, bool fw) {
  File f = LittleFS.open("/update_flags.txt", "w");
  if (f) {
    f.printf("fs:%d,fw:%d\n", fs ? 1 : 0, fw ? 1 : 0);
    f.close();
  }
}

void ConfigPersistence::readUpdateFlagsFromFile(bool& fs, bool& fw) {
  File f = LittleFS.open("/update_flags.txt", "r");
  if (f) {
    String line = f.readStringUntil('\n');
    fs = line.indexOf("fs:1") != -1;
    fw = line.indexOf("fw:1") != -1;
    f.close();
  } else {
    fs = false;
    fw = false;
  }
}
