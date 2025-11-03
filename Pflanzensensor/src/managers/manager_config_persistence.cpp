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
  
  // Try to load from Preferences first
  bool preferencesLoaded = false;
  if (PreferencesManager::namespaceExists(PreferencesNamespaces::GENERAL)) {
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
      preferencesLoaded = true;
      logger.info(F("ConfigP"), F("Konfiguration erfolgreich aus Preferences geladen"));
    }
  }
  
  // If Preferences not found, try to migrate from JSON
  if (!preferencesLoaded) {
    if (PersistenceUtils::fileExists("/config.json")) {
      logger.info(F("ConfigP"), F("Preferences nicht gefunden, migriere von JSON..."));
      auto jsonResult = loadFromJSON(config);
      if (jsonResult.isSuccess()) {
        // Migration successful, save to Preferences
        logger.info(F("ConfigP"), F("Migriere Konfiguration zu Preferences..."));
        auto saveResult = saveToPreferences(config);
        if (saveResult.isSuccess()) {
          logger.info(F("ConfigP"), F("Migration zu Preferences erfolgreich"));
          // Optionally, backup JSON file
          if (LittleFS.exists("/config.json")) {
            if (LittleFS.rename("/config.json", "/config.json.bak")) {
              logger.info(F("ConfigP"), F("JSON-Backup erstellt: /config.json.bak"));
            }
          }
        } else {
          logger.warning(F("ConfigP"), F("Fehler beim Migrieren zu Preferences: ") + saveResult.getMessage());
        }
      } else {
        logger.error(F("ConfigP"), F("Fehler beim Laden der JSON-Konfiguration: ") + jsonResult.getMessage());
        logger.logMemoryStats(F("ConfigP_load_after"));
        return jsonResult;
      }
    } else {
      // Neither Preferences nor JSON exist, initialize with defaults
      logger.info(F("ConfigP"), F("Keine Konfiguration gefunden, initialisiere mit Standardwerten..."));
      auto initResult = PreferencesManager::initializeAllNamespaces();
      if (!initResult.isSuccess()) {
        logger.error(F("ConfigP"), F("Fehler beim Initialisieren der Preferences: ") + initResult.getMessage());
        auto result = resetToDefaults(config);
        logger.logMemoryStats(F("ConfigP_load_after"));
        return result;
      }
      
      // Load the defaults we just wrote
      auto generalResult = PreferencesManager::loadGeneralSettings(
        config.deviceName, config.adminPassword, 
        config.md5Verification, config.fileLoggingEnabled);
      
      auto wifiResult = PreferencesManager::loadWiFiSettings(
        config.wifiSSID1, config.wifiPassword1,
        config.wifiSSID2, config.wifiPassword2,
        config.wifiSSID3, config.wifiPassword3);
      
      auto debugResult = PreferencesManager::loadDebugSettings(
        config.debugRAM, config.debugMeasurementCycle,
        config.debugSensor, config.debugDisplay, config.debugWebSocket);
      
      auto ledResult = PreferencesManager::loadLedTrafficSettings(
        config.ledTrafficLightMode, config.ledTrafficLightSelectedMeasurement);
      
      auto flowerResult = PreferencesManager::loadFlowerStatusSensor(config.flowerStatusSensor);
      
      logger.info(F("ConfigP"), F("Standardwerte aus Preferences geladen"));
    }
  }
  
  logger.logMemoryStats(F("ConfigP_load_after"));
  return PersistenceResult::success();
}

// Helper function to load from JSON (for migration)
ConfigPersistence::PersistenceResult ConfigPersistence::loadFromJSON(ConfigData& config) {
  String errorMsg;
  StaticJsonDocument<512> doc;
  doc.clear();

  if (!PersistenceUtils::readJsonFile("/config.json", doc, errorMsg)) {
    logger.error(F("ConfigP"), F("Konfiguration konnte nicht geladen werden: ") + errorMsg);
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Load main configuration values
  config.adminPassword = doc["admin_password"] | ADMIN_PASSWORD;
  config.md5Verification = doc["md5_verification"] | false;
  config.fileLoggingEnabled = doc["file_logging_enabled"] | FILE_LOGGING_ENABLED;
  config.deviceName = doc["device_name"] | String(DEVICE_NAME);

  // Load WiFi credentials
  config.wifiSSID1 = doc["wifi_ssid_1"] | "";
  config.wifiPassword1 = doc["wifi_password_1"] | "";
  config.wifiSSID2 = doc["wifi_ssid_2"] | "";
  config.wifiPassword2 = doc["wifi_password_2"] | "";
  config.wifiSSID3 = doc["wifi_ssid_3"] | "";
  config.wifiPassword3 = doc["wifi_password_3"] | "";

  // Load debug flags
  config.debugRAM = doc.containsKey("debug_ram") ? doc["debug_ram"] : DEBUG_RAM;
  config.debugMeasurementCycle = doc.containsKey("debug_measurement_cycle")
                                     ? doc["debug_measurement_cycle"]
                                     : DEBUG_MEASUREMENT_CYCLE;
  config.debugSensor = doc.containsKey("debug_sensor") ? doc["debug_sensor"] : DEBUG_SENSOR;
  config.debugDisplay = doc.containsKey("debug_display") ? doc["debug_display"] : DEBUG_DISPLAY;
  config.debugWebSocket =
      doc.containsKey("debug_websocket") ? doc["debug_websocket"] : DEBUG_WEBSOCKET;

  // LED Traffic Light settings
  config.ledTrafficLightMode = doc.containsKey("led_traffic_light_mode")
                                   ? doc["led_traffic_light_mode"]
                                   : 2;
  config.ledTrafficLightSelectedMeasurement =
      (doc.containsKey("led_traffic_light_selected_measurement") &&
       doc["led_traffic_light_selected_measurement"].as<String>() != "")
          ? doc["led_traffic_light_selected_measurement"].as<String>()
          : String("ANALOG_1");

  // Flower Status settings
  config.flowerStatusSensor =
      (doc.containsKey("flower_status_sensor") && doc["flower_status_sensor"].as<String>() != "")
          ? doc["flower_status_sensor"].as<String>()
          : String("ANALOG_1");

  return PersistenceResult::success();
}

// Helper function to save to Preferences
ConfigPersistence::PersistenceResult ConfigPersistence::saveToPreferences(const ConfigData& config) {
  // Save general settings
  auto generalResult = PreferencesManager::saveGeneralSettings(
    config.deviceName, config.adminPassword,
    config.md5Verification, config.fileLoggingEnabled);
  
  if (!generalResult.isSuccess()) {
    return generalResult;
  }
  
  // Save WiFi settings
  auto wifiResult = PreferencesManager::saveWiFiSettings(
    config.wifiSSID1, config.wifiPassword1,
    config.wifiSSID2, config.wifiPassword2,
    config.wifiSSID3, config.wifiPassword3);
  
  if (!wifiResult.isSuccess()) {
    return wifiResult;
  }
  
  // Save debug settings
  auto debugResult = PreferencesManager::saveDebugSettings(
    config.debugRAM, config.debugMeasurementCycle,
    config.debugSensor, config.debugDisplay, config.debugWebSocket);
  
  if (!debugResult.isSuccess()) {
    return debugResult;
  }
  
  // Save LED traffic light settings
  auto ledResult = PreferencesManager::saveLedTrafficSettings(
    config.ledTrafficLightMode, config.ledTrafficLightSelectedMeasurement);
  
  if (!ledResult.isSuccess()) {
    return ledResult;
  }
  
  // Save flower status sensor
  auto flowerResult = PreferencesManager::saveFlowerStatusSensor(config.flowerStatusSensor);
  
  if (!flowerResult.isSuccess()) {
    return flowerResult;
  }
  
  return PersistenceResult::success();
}
}

// Helper function to save to Preferences
ConfigPersistence::PersistenceResult ConfigPersistence::saveToPreferences(const ConfigData& config) {
  // Save general settings
  auto generalResult = PreferencesManager::saveGeneralSettings(
    config.deviceName, config.adminPassword,
    config.md5Verification, config.fileLoggingEnabled);
  
  if (!generalResult.isSuccess()) {
    return generalResult;
  }
  
  // Save WiFi settings
  auto wifiResult = PreferencesManager::saveWiFiSettings(
    config.wifiSSID1, config.wifiPassword1,
    config.wifiSSID2, config.wifiPassword2,
    config.wifiSSID3, config.wifiPassword3);
  
  if (!wifiResult.isSuccess()) {
    return wifiResult;
  }
  
  // Save debug settings
  auto debugResult = PreferencesManager::saveDebugSettings(
    config.debugRAM, config.debugMeasurementCycle,
    config.debugSensor, config.debugDisplay, config.debugWebSocket);
  
  if (!debugResult.isSuccess()) {
    return debugResult;
  }
  
  // Save LED traffic light settings
  auto ledResult = PreferencesManager::saveLedTrafficSettings(
    config.ledTrafficLightMode, config.ledTrafficLightSelectedMeasurement);
  
  if (!ledResult.isSuccess()) {
    return ledResult;
  }
  
  // Save flower status sensor
  auto flowerResult = PreferencesManager::saveFlowerStatusSensor(config.flowerStatusSensor);
  
  if (!flowerResult.isSuccess()) {
    return flowerResult;
  }
  
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::resetToDefaults(ConfigData& config) {
  // Clear Preferences and remove JSON backup files
  logger.info(F("ConfigP"), F("ResetToDefaults: Lösche alle Preferences und Dateien"));

  // Clear all Preferences namespaces
  auto clearResult = PreferencesManager::clearAll();
  if (!clearResult.isSuccess()) {
    logger.warning(F("ConfigP"), F("Fehler beim Löschen der Preferences: ") + clearResult.getMessage());
  }

  // Also remove legacy JSON files if they exist
  if (LittleFS.exists("/config.json")) {
    if (LittleFS.remove("/config.json")) {
      logger.info(F("ConfigP"), F("/config.json gelöscht"));
    } else {
      logger.warning(F("ConfigP"), F("Fehler beim Löschen von /config.json"));
    }
  }
  
  if (LittleFS.exists("/config.json.bak")) {
    LittleFS.remove("/config.json.bak");
  }

  if (LittleFS.exists("/sensors.json")) {
    if (LittleFS.remove("/sensors.json")) {
      logger.info(F("ConfigP"), F("/sensors.json gelöscht"));
    } else {
      logger.warning(F("ConfigP"), F("Fehler beim Löschen von /sensors.json"));
    }
  }
  
  if (LittleFS.exists("/sensors.json.bak")) {
    LittleFS.remove("/sensors.json.bak");
  }

  logger.info(F("ConfigP"), F("Factory Reset abgeschlossen"));
  // Return success; caller (UI) will handle reboot
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult
ConfigPersistence::saveToFileMinimal(const ConfigData& config) {
  // Save to Preferences instead of JSON
  logger.info(F("ConfigP"), F("Speichere Konfiguration in Preferences..."));
  auto result = saveToPreferences(config);
  if (result.isSuccess()) {
    logger.info(F("ConfigP"), F("Konfiguration erfolgreich in Preferences gespeichert"));
  } else {
    logger.error(F("ConfigP"), F("Fehler beim Speichern in Preferences: ") + result.getMessage());
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
