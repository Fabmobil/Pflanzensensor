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

  // Load debug flags - use runtime values, fallback to compile-time defaults
  // only if not present
  config.debugRAM = doc.containsKey("debug_ram") ? doc["debug_ram"] : DEBUG_RAM;
  config.debugMeasurementCycle = doc.containsKey("debug_measurement_cycle")
                                     ? doc["debug_measurement_cycle"]
                                     : DEBUG_MEASUREMENT_CYCLE;
  config.debugSensor = doc.containsKey("debug_sensor") ? doc["debug_sensor"] : DEBUG_SENSOR;
  config.debugDisplay = doc.containsKey("debug_display") ? doc["debug_display"] : DEBUG_DISPLAY;
  config.debugWebSocket =
      doc.containsKey("debug_websocket") ? doc["debug_websocket"] : DEBUG_WEBSOCKET;

  // LED Traffic Light settings
  // Default to mode 2 (single measurement) as initial default for new devices
  config.ledTrafficLightMode = doc.containsKey("led_traffic_light_mode")
                                   ? doc["led_traffic_light_mode"]
                                   : 2; // Default to mode 2 (single measurement)
  // If the selected measurement is missing or empty, default to ANALOG_1 so
  // the UI shows a meaningful selection on first boot.
  config.ledTrafficLightSelectedMeasurement =
      (doc.containsKey("led_traffic_light_selected_measurement") &&
       doc["led_traffic_light_selected_measurement"].as<String>() != "")
          ? doc["led_traffic_light_selected_measurement"].as<String>()
          : String("ANALOG_1"); // Default to ANALOG_1

  // Flower Status settings
  // If the key is missing or the value is an empty string, treat it as missing
  // and use the compile-time default. This ensures devices with an empty
  // value in an existing config.json still get the intended default and
  // the value is written back during migration.
  config.flowerStatusSensor =
      (doc.containsKey("flower_status_sensor") && doc["flower_status_sensor"].as<String>() != "")
          ? doc["flower_status_sensor"].as<String>()
          : String("ANALOG_1"); // Default to ANALOG_1 (Bodenfeuchte)

  // --- Migration: if keys are missing in an existing config.json, add them
  // using compile-time defaults so devices upgraded from older firmware still
  // get the new settings present in the file. This is a best-effort write.
  bool modified = false;
  if (!doc.containsKey("md5_verification")) {
    doc["md5_verification"] = config.md5Verification;
    modified = true;
  }
  if (!doc.containsKey("collectd_enabled")) {
    doc["collectd_enabled"] = config.collectdEnabled;
    modified = true;
  }
  if (!doc.containsKey("file_logging_enabled")) {
    doc["file_logging_enabled"] = config.fileLoggingEnabled;
    modified = true;
  }
  if (!doc.containsKey("device_name")) {
    doc["device_name"] = config.deviceName;
    modified = true;
  }
  if (!doc.containsKey("debug_ram")) {
    doc["debug_ram"] = config.debugRAM;
    modified = true;
  }
  if (!doc.containsKey("debug_measurement_cycle")) {
    doc["debug_measurement_cycle"] = config.debugMeasurementCycle;
    modified = true;
  }
  if (!doc.containsKey("debug_sensor")) {
    doc["debug_sensor"] = config.debugSensor;
    modified = true;
  }
  if (!doc.containsKey("debug_display")) {
    doc["debug_display"] = config.debugDisplay;
    modified = true;
  }
  if (!doc.containsKey("debug_websocket")) {
    doc["debug_websocket"] = config.debugWebSocket;
    modified = true;
  }
  if (!doc.containsKey("wifi_ssid_1")) {
    doc["wifi_ssid_1"] = config.wifiSSID1;
    modified = true;
  }
  if (!doc.containsKey("wifi_password_1")) {
    doc["wifi_password_1"] = config.wifiPassword1;
    modified = true;
  }
  // Ensure migration also fills in defaults when key is missing or empty
  if (!doc.containsKey("led_traffic_light_mode")) {
    doc["led_traffic_light_mode"] = config.ledTrafficLightMode;
    modified = true;
  }
  if (!doc.containsKey("led_traffic_light_selected_measurement") ||
      doc["led_traffic_light_selected_measurement"].as<String>() == "") {
    doc["led_traffic_light_selected_measurement"] = config.ledTrafficLightSelectedMeasurement;
    modified = true;
  }
  // If the key is missing or present but empty, add the compile-time/default
  // value to the file so upgrades populate the new setting consistently.
  if (!doc.containsKey("flower_status_sensor") || doc["flower_status_sensor"].as<String>() == "") {
    doc["flower_status_sensor"] = config.flowerStatusSensor;
    modified = true;
  }

  if (modified) {
    String err;
    if (PersistenceUtils::writeJsonFile("/config.json", doc, err)) {
      logger.info(F("ConfigP"), F("Konfigurationsdatei mit fehlenden Compile‑Defaults ergänzt"));
    } else {
      logger.warning(F("ConfigP"), F("Fehler beim Migrieren neuer Config‑Keys: ") + err);
    }
  }

  logger.logMemoryStats(F("ConfigP_load_after"));
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::resetToDefaults(ConfigData& config) {
  // Simplified reset: remove stored config and sensors files and reboot.
  logger.info(F("ConfigP"),
              F("ResetToDefaults requested: deleting /config.json and /sensors.json"));

  // Attempt to remove config and sensors files; ignore errors but log them
  if (LittleFS.exists("/config.json")) {
    if (LittleFS.remove("/config.json")) {
      logger.info(F("ConfigP"), F("/config.json deleted"));
    } else {
      logger.warning(F("ConfigP"), F("Failed to delete /config.json"));
    }
  } else {
    logger.info(F("ConfigP"), F("/config.json not present"));
  }

  if (LittleFS.exists("/sensors.json")) {
    if (LittleFS.remove("/sensors.json")) {
      logger.info(F("ConfigP"), F("/sensors.json deleted"));
    } else {
      logger.warning(F("ConfigP"), F("Failed to delete /sensors.json"));
    }
  } else {
    logger.info(F("ConfigP"), F("/sensors.json not present"));
  }

  // Done: files removed. Return success; caller (UI) will handle reboot so the
  // admin page can be rendered and the user sees confirmation.
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult
ConfigPersistence::saveToFileMinimal(const ConfigData& config) {
  StaticJsonDocument<512> doc;
  doc[F("admin_password")] = config.adminPassword;
  doc[F("md5_verification")] = config.md5Verification;
  doc[F("collectd_enabled")] = config.collectdEnabled;
  doc[F("file_logging_enabled")] = config.fileLoggingEnabled;
  doc[F("device_name")] = config.deviceName;
  doc[F("debug_ram")] = config.debugRAM;
  doc[F("debug_measurement_cycle")] = config.debugMeasurementCycle;
  doc[F("debug_sensor")] = config.debugSensor;
  doc[F("debug_display")] = config.debugDisplay;
  doc[F("debug_websocket")] = config.debugWebSocket;
  doc[F("wifi_ssid_1")] = config.wifiSSID1;
  doc[F("wifi_password_1")] = config.wifiPassword1;
  doc[F("wifi_ssid_2")] = config.wifiSSID2;
  doc[F("wifi_password_2")] = config.wifiPassword2;
  doc[F("wifi_ssid_3")] = config.wifiSSID3;
  doc[F("wifi_password_3")] = config.wifiPassword3;
  doc[F("led_traffic_light_mode")] = config.ledTrafficLightMode;
  doc[F("led_traffic_light_selected_measurement")] = config.ledTrafficLightSelectedMeasurement;
  doc[F("flower_status_sensor")] = config.flowerStatusSensor;

#if USE_MAIL
  saveMailConfigToJson(doc, config);
#endif

  String errorMsg;
  if (!PersistenceUtils::writeJsonFile("/config.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Log key debug flags and other safe-to-log settings for auditing
  logger.info(F("ConfigP"),
              String(F("Konfiguration gespeichert: debug_ram=")) +
                  (config.debugRAM ? F("true") : F("false")) + F(", debug_measurement_cycle=") +
                  (config.debugMeasurementCycle ? F("true") : F("false")) + F(", debug_sensor=") +
                  (config.debugSensor ? F("true") : F("false")) + F(", debug_display=") +
                  (config.debugDisplay ? F("true") : F("false")) + F(", debug_websocket=") +
                  (config.debugWebSocket ? F("true") : F("false")));

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
