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
#include "managers/manager_config_preferences.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"

bool ConfigPersistence::configExists() {
  // Check if any core Preferences namespace exists
  return PreferencesManager::namespaceExists(PreferencesNamespaces::GENERAL);
}

size_t ConfigPersistence::getConfigSize() {
  // Estimate configuration size in Preferences (approximate)
  // General: ~150 bytes, WiFi: ~200 bytes, Display: ~100 bytes,
  // Log: ~50 bytes, LED: ~50 bytes, Debug: ~20 bytes
  return 570; // Total estimated size
}

// --- Thresholds persistence helpers ---
// Sensor includes moved to manager_sensor_persistence.cpp

// Sensor settings are now handled by SensorPersistence
// This function has been moved to manager_sensor_persistence.cpp

ConfigPersistence::PersistenceResult ConfigPersistence::load(ConfigData& config) {
  logger.logMemoryStats(F("ConfigP_load_before"));

  // Check if Preferences exist, if not initialize with defaults
  if (!PreferencesManager::namespaceExists(PreferencesNamespaces::GENERAL)) {
    logger.info(F("ConfigP"),
                F("Keine Konfiguration gefunden, initialisiere mit Standardwerten..."));
    auto initResult = PreferencesManager::initializeAllNamespaces();
    if (!initResult.isSuccess()) {
      logger.error(F("ConfigP"),
                   F("Fehler beim Initialisieren der Preferences: ") + initResult.getMessage());
      auto result = resetToDefaults(config);
      logger.logMemoryStats(F("ConfigP_load_after"));
      return result;
    }
  }

  // Load from Preferences
  logger.info(F("ConfigP"), F("Lade Konfiguration aus Preferences..."));

  // Load general settings directly using generic getters
  Preferences generalPrefs;
  if (generalPrefs.begin(PreferencesNamespaces::GENERAL, true)) {
    config.deviceName = PreferencesManager::getString(generalPrefs, "device_name", DEVICE_NAME);
    config.adminPassword = PreferencesManager::getString(generalPrefs, "admin_pwd", ADMIN_PASSWORD);
    config.md5Verification = PreferencesManager::getBool(generalPrefs, "md5_verify", false);
    config.fileLoggingEnabled = PreferencesManager::getBool(generalPrefs, "file_log", FILE_LOGGING_ENABLED);
    generalPrefs.end();
  }

  // Load WiFi settings directly
  Preferences wifiPrefs;
  if (wifiPrefs.begin(PreferencesNamespaces::WIFI, true)) {
    config.wifiSSID1 = PreferencesManager::getString(wifiPrefs, "ssid1", "");
    config.wifiPassword1 = PreferencesManager::getString(wifiPrefs, "pwd1", "");
    config.wifiSSID2 = PreferencesManager::getString(wifiPrefs, "ssid2", "");
    config.wifiPassword2 = PreferencesManager::getString(wifiPrefs, "pwd2", "");
    config.wifiSSID3 = PreferencesManager::getString(wifiPrefs, "ssid3", "");
    config.wifiPassword3 = PreferencesManager::getString(wifiPrefs, "pwd3", "");
    wifiPrefs.end();
  }

  // Load debug settings directly
  Preferences debugPrefs;
  if (debugPrefs.begin(PreferencesNamespaces::DEBUG, true)) {
    config.debugRAM = PreferencesManager::getBool(debugPrefs, "ram", false);
    config.debugMeasurementCycle = PreferencesManager::getBool(debugPrefs, "meas_cycle", false);
    config.debugSensor = PreferencesManager::getBool(debugPrefs, "sensor", false);
    config.debugDisplay = PreferencesManager::getBool(debugPrefs, "display", false);
    config.debugWebSocket = PreferencesManager::getBool(debugPrefs, "websocket", false);
    debugPrefs.end();
  }

  // Load LED traffic light settings directly
  Preferences ledPrefs;
  if (ledPrefs.begin(PreferencesNamespaces::LED_TRAFFIC, true)) {
    config.ledTrafficLightMode = PreferencesManager::getUChar(ledPrefs, "mode", 0);
    config.ledTrafficLightSelectedMeasurement = PreferencesManager::getString(ledPrefs, "sel_meas", "");
    ledPrefs.end();
  }

  // Load flower status sensor directly
  Preferences flowerPrefs;
  if (flowerPrefs.begin(PreferencesNamespaces::GENERAL, true)) {
    config.flowerStatusSensor = PreferencesManager::getString(flowerPrefs, "flower_sens", "ANALOG_1");
    flowerPrefs.end();
  }

  logger.info(F("ConfigP"), F("Konfiguration erfolgreich aus Preferences geladen"));

  logger.logMemoryStats(F("ConfigP_load_after"));
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::resetToDefaults(ConfigData& config) {
  // Clear Preferences
  logger.info(F("ConfigP"), F("ResetToDefaults: Lösche alle Preferences"));

  // Clear all Preferences namespaces
  auto clearResult = PreferencesManager::clearAll();
  if (!clearResult.isSuccess()) {
    logger.warning(F("ConfigP"),
                   F("Fehler beim Löschen der Preferences: ") + clearResult.getMessage());
  }

  logger.info(F("ConfigP"), F("Factory Reset abgeschlossen"));
  // Return success; caller (UI) will handle reboot
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::save(const ConfigData& config) {
  // Save to Preferences using atomic update functions
  logger.info(F("ConfigP"), F("Speichere Konfiguration in Preferences..."));

  // Save general settings using atomic updates
  auto result = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "device_name", config.deviceName);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "admin_pwd", config.adminPassword);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "md5_verify", config.md5Verification);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "file_log", config.fileLoggingEnabled);
  if (!result.isSuccess()) return result;

  // Save WiFi settings using specialized method
  result = PreferencesManager::updateWiFiCredentials(1, config.wifiSSID1, config.wifiPassword1);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateWiFiCredentials(2, config.wifiSSID2, config.wifiPassword2);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateWiFiCredentials(3, config.wifiSSID3, config.wifiPassword3);
  if (!result.isSuccess()) return result;

  // Save debug settings using atomic updates
  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "ram", config.debugRAM);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "meas_cycle", config.debugMeasurementCycle);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "sensor", config.debugSensor);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "display", config.debugDisplay);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "websocket", config.debugWebSocket);
  if (!result.isSuccess()) return result;

  // Save LED traffic light settings using atomic updates
  result = PreferencesManager::updateUInt8Value(PreferencesNamespaces::LED_TRAFFIC, "mode", config.ledTrafficLightMode);
  if (!result.isSuccess()) return result;
  
  result = PreferencesManager::updateStringValue(PreferencesNamespaces::LED_TRAFFIC, "sel_meas", config.ledTrafficLightSelectedMeasurement);
  if (!result.isSuccess()) return result;

  // Save flower status sensor using atomic update
  result = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "flower_sens", config.flowerStatusSensor);
  if (!result.isSuccess()) return result;

  logger.info(F("ConfigP"), F("Konfiguration erfolgreich in Preferences gespeichert"));
  return PersistenceResult::success();
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
