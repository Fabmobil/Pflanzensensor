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
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"

bool ConfigPersistence::configFileExists() {
  return LittleFS.exists("/config.json");
}

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

ConfigPersistence::PersistenceResult ConfigPersistence::loadFromFile(
    ConfigData& config) {
  logger.logMemoryStats(F("ConfigP_load_before"));
  String errorMsg;
  StaticJsonDocument<512> doc;
  doc.clear();

  if (!PersistenceUtils::fileExists("/config.json")) {
    // Datei existiert nicht: Standardwerte verwenden, nicht als Fehler behandeln
    logger.info(F("ConfigP"), F("Konfigurationsdatei nicht gefunden, verwende Standardwerte."));
    auto result = resetToDefaults(config);
    logger.logMemoryStats(F("ConfigP_load_after"));
    return result;
  }

  if (!PersistenceUtils::readJsonFile("/config.json", doc, errorMsg)) {
    logger.error(F("ConfigP"), F("Konfiguration konnte nicht geladen werden: ") + errorMsg);
    logger.logMemoryStats(F("ConfigP_load_after"));
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Only log file contents if there is a parse error (handled in utility)
  // No info log for successful load

  // Load main configuration values
  config.adminPassword = doc["admin_password"] | INITIAL_ADMIN_PASSWORD;
  config.md5Verification = doc["md5_verification"] | false;
  config.collectdEnabled = doc["collectd_enabled"] | USE_INFLUXDB;
  config.fileLoggingEnabled =
      doc["file_logging_enabled"] | FILE_LOGGING_ENABLED;
  config.deviceName = doc["device_name"] | String(DEVICE_NAME);

  // Load WiFi credentials
  config.wifiSSID1 = doc["wifi_ssid_1"] | "";
  config.wifiPassword1 = doc["wifi_password_1"] | "";
  config.wifiSSID2 = doc["wifi_ssid_2"] | "";
  config.wifiPassword2 = doc["wifi_password_2"] | "";
  config.wifiSSID3 = doc["wifi_ssid_3"] | "";
  config.wifiPassword3 = doc["wifi_password_3"] | "";

  // Load debug flags - use runtime values, fallback to compile-time defaults
  // only if not present
  config.debugRAM = doc.containsKey("debug_ram") ? doc["debug_ram"] : DEBUG_RAM;
  config.debugMeasurementCycle = doc.containsKey("debug_measurement_cycle")
                                     ? doc["debug_measurement_cycle"]
                                     : DEBUG_MEASUREMENT_CYCLE;
  config.debugSensor =
      doc.containsKey("debug_sensor") ? doc["debug_sensor"] : DEBUG_SENSOR;
  config.debugDisplay =
      doc.containsKey("debug_display") ? doc["debug_display"] : DEBUG_DISPLAY;
  config.debugWebSocket = doc.containsKey("debug_websocket")
                              ? doc["debug_websocket"]
                              : DEBUG_WEBSOCKET;

  // LED Traffic Light settings
  config.ledTrafficLightMode = doc.containsKey("led_traffic_light_mode")
                                   ? doc["led_traffic_light_mode"]
                                   : 1;  // Default to mode 1 (all measurements)
  config.ledTrafficLightSelectedMeasurement =
      doc.containsKey("led_traffic_light_selected_measurement")
          ? doc["led_traffic_light_selected_measurement"].as<String>()
          : "";  // Default to empty (no measurement selected)

  logger.logMemoryStats(F("ConfigP_load_after"));
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::resetToDefaults(
    ConfigData& config) {
  logger.logMemoryStats(F("ConfigP_reset_before"));
  StaticJsonDocument<512> doc;
  doc.clear();

  config.adminPassword = INITIAL_ADMIN_PASSWORD;
  config.md5Verification = false;
  config.collectdEnabled = USE_INFLUXDB;
  config.fileLoggingEnabled = FILE_LOGGING_ENABLED;
  config.deviceName = String(DEVICE_NAME);

  // Initialize debug flags to defaults
  config.debugRAM = DEBUG_RAM;
  config.debugMeasurementCycle = DEBUG_MEASUREMENT_CYCLE;
  config.debugSensor = DEBUG_SENSOR;
  config.debugDisplay = DEBUG_DISPLAY;
  config.debugWebSocket = DEBUG_WEBSOCKET;

  // LED Traffic Light settings - default to mode 1
  config.ledTrafficLightMode = 1;
  config.ledTrafficLightSelectedMeasurement = "";

  doc["admin_password"] = config.adminPassword;
  doc["md5_verification"] = config.md5Verification;
  doc["collectd_enabled"] = config.collectdEnabled;
  doc["file_logging_enabled"] = config.fileLoggingEnabled;
  doc["device_name"] = config.deviceName;
  doc["debug_ram"] = config.debugRAM;
  doc["debug_measurement_cycle"] = config.debugMeasurementCycle;
  doc["debug_sensor"] = config.debugSensor;
  doc["debug_display"] = config.debugDisplay;
  doc["debug_websocket"] = config.debugWebSocket;
  doc["wifi_ssid_1"] = config.wifiSSID1;
  doc["wifi_password_1"] = config.wifiPassword1;
  doc["wifi_ssid_2"] = config.wifiSSID2;
  doc["wifi_password_2"] = config.wifiPassword2;
  doc["wifi_ssid_3"] = config.wifiSSID3;
  doc["wifi_password_3"] = config.wifiPassword3;
  doc["led_traffic_light_mode"] = config.ledTrafficLightMode;
  doc["led_traffic_light_selected_measurement"] =
      config.ledTrafficLightSelectedMeasurement;

  String errorMsg;
  if (PersistenceUtils::writeJsonFile("/config.json", doc, errorMsg)) {
    logger.info(F("ConfigP"), F("Minimale Konfigurationsdatei erstellt"));
  } else {
    logger.error(F("ConfigP"),
                 F("Erstellen der initialen Konfigurationsdatei fehlgeschlagen: ") + errorMsg);
  }

  logger.logMemoryStats(F("ConfigP_reset_after"));
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::saveToFileMinimal(
    const ConfigData& config) {
  StaticJsonDocument<512> doc;
  doc["admin_password"] = config.adminPassword;
  doc["md5_verification"] = config.md5Verification;
  doc["collectd_enabled"] = config.collectdEnabled;
  doc["file_logging_enabled"] = config.fileLoggingEnabled;
  doc["device_name"] = config.deviceName;
  doc["debug_ram"] = config.debugRAM;
  doc["debug_measurement_cycle"] = config.debugMeasurementCycle;
  doc["debug_sensor"] = config.debugSensor;
  doc["debug_display"] = config.debugDisplay;
  doc["debug_websocket"] = config.debugWebSocket;
  doc["wifi_ssid_1"] = config.wifiSSID1;
  doc["wifi_password_1"] = config.wifiPassword1;
  doc["wifi_ssid_2"] = config.wifiSSID2;
  doc["wifi_password_2"] = config.wifiPassword2;
  doc["wifi_ssid_3"] = config.wifiSSID3;
  doc["wifi_password_3"] = config.wifiPassword3;
  doc["led_traffic_light_mode"] = config.ledTrafficLightMode;
  doc["led_traffic_light_selected_measurement"] =
      config.ledTrafficLightSelectedMeasurement;
  // Remove unused variable 'errorMsg'
  if (!PersistenceUtils::writeJsonFile("/config.json", doc, *(new String()))) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR);
  }
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
