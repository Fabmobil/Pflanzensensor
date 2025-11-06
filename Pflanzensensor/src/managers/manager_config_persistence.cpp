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

  // Load WiFi settings from separate namespaces
  Preferences wifi1Prefs;
  if (wifi1Prefs.begin(PreferencesNamespaces::WIFI1, true)) {
    config.wifiSSID1 = PreferencesManager::getString(wifi1Prefs, "ssid", "");
    config.wifiPassword1 = PreferencesManager::getString(wifi1Prefs, "pwd", "");
    wifi1Prefs.end();
  }

  Preferences wifi2Prefs;
  if (wifi2Prefs.begin(PreferencesNamespaces::WIFI2, true)) {
    config.wifiSSID2 = PreferencesManager::getString(wifi2Prefs, "ssid", "");
    config.wifiPassword2 = PreferencesManager::getString(wifi2Prefs, "pwd", "");
    wifi2Prefs.end();
  }

  Preferences wifi3Prefs;
  if (wifi3Prefs.begin(PreferencesNamespaces::WIFI3, true)) {
    config.wifiSSID3 = PreferencesManager::getString(wifi3Prefs, "ssid", "");
    config.wifiPassword3 = PreferencesManager::getString(wifi3Prefs, "pwd", "");
    wifi3Prefs.end();
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

bool ConfigPersistence::savePreferencesToFlash() {
  logger.info(F("ConfigP"), F("Sichere Preferences in Flash..."));
  
  // First, backup to JSON file (reuse existing function)
  if (!backupPreferencesToFile()) {
    logger.error(F("ConfigP"), F("Konnte JSON-Backup nicht erstellen"));
    return false;
  }
  
  // Read the JSON file
  File f = LittleFS.open("/prefs_backup.json", "r");
  if (!f) {
    logger.error(F("ConfigP"), F("Konnte Backup-Datei nicht öffnen"));
    return false;
  }
  
  String jsonData = f.readString();
  f.close();
  
  // Delete the file from LittleFS (will be wiped anyway)
  LittleFS.remove("/prefs_backup.json");
  
  // Save to flash using FlashPersistence
  #include "../utils/flash_persistence.h"
  auto result = FlashPersistence::saveToFlash(jsonData);
  if (!result.isSuccess()) {
    logger.error(F("ConfigP"), F("Flash-Speicherung fehlgeschlagen: ") + result.getMessage());
    return false;
  }
  
  logger.info(F("ConfigP"), F("Preferences erfolgreich in Flash gesichert"));
  return true;
}

bool ConfigPersistence::restorePreferencesFromFlash() {
  logger.info(F("ConfigP"), F("Stelle Preferences aus Flash wieder her..."));
  
  // Load JSON from flash
  #include "../utils/flash_persistence.h"
  String jsonData;
  auto result = FlashPersistence::loadFromFlash(jsonData);
  if (!result.isSuccess()) {
    logger.error(F("ConfigP"), F("Flash-Lesen fehlgeschlagen: ") + result.getMessage());
    return false;
  }
  
  // Write to temporary file for restoration
  File f = LittleFS.open("/prefs_backup.json", "w");
  if (!f) {
    logger.error(F("ConfigP"), F("Konnte temporäre Datei nicht erstellen"));
    return false;
  }
  
  f.print(jsonData);
  f.close();
  
  // Restore using existing function
  bool success = restorePreferencesFromFile();
  
  // Clean up
  LittleFS.remove("/prefs_backup.json");
  
  // Clear flash storage
  FlashPersistence::clearFlash();
  
  if (success) {
    logger.info(F("ConfigP"), F("Preferences erfolgreich aus Flash wiederhergestellt"));
  } else {
    logger.error(F("ConfigP"), F("Wiederherstellen aus Flash fehlgeschlagen"));
  }
  
  return success;
}

bool ConfigPersistence::backupPreferencesToFile() {
  logger.info(F("ConfigP"), F("Sichere Preferences in Datei..."));
  
  // Create JSON document for backup (allocate enough space)
  DynamicJsonDocument doc(8192);
  Preferences prefs;
  
  // Backup general namespace
  if (prefs.begin(PreferencesNamespaces::GENERAL, true)) {
    JsonObject general = doc.createNestedObject("general");
    general["device_name"] = prefs.getString("device_name", "Pflanzensensor");
    general["admin_pwd"] = prefs.getString("admin_pwd", "admin");
    general["md5_verify"] = prefs.getBool("md5_verify", true);
    general["collectd_en"] = prefs.getBool("collectd_en", false);
    general["file_log"] = prefs.getBool("file_log", false);
    general["flower_sens"] = prefs.getString("flower_sens", "");
    prefs.end();
  }
  
  // Backup WiFi namespaces (3 separate namespaces)
  JsonObject wifi = doc.createNestedObject("wifi");
  if (prefs.begin(PreferencesNamespaces::WIFI1, true)) {
    wifi["ssid1"] = prefs.getString("ssid", "");
    wifi["pwd1"] = prefs.getString("pwd", "");
    prefs.end();
  }
  if (prefs.begin(PreferencesNamespaces::WIFI2, true)) {
    wifi["ssid2"] = prefs.getString("ssid", "");
    wifi["pwd2"] = prefs.getString("pwd", "");
    prefs.end();
  }
  if (prefs.begin(PreferencesNamespaces::WIFI3, true)) {
    wifi["ssid3"] = prefs.getString("ssid", "");
    wifi["pwd3"] = prefs.getString("pwd", "");
    prefs.end();
  }
  
  // Backup display namespace
  if (prefs.begin(PreferencesNamespaces::DISP, true)) {
    JsonObject disp = doc.createNestedObject("display");
    disp["show_ip"] = prefs.getBool("show_ip", true);
    disp["show_clock"] = prefs.getBool("show_clock", true);
    disp["show_flower"] = prefs.getBool("show_flower", true);
    disp["show_fabmobil"] = prefs.getBool("show_fabmobil", true);
    disp["screen_dur"] = prefs.getUInt("screen_dur", 5);
    disp["clock_fmt"] = prefs.getString("clock_fmt", "24h");
    prefs.end();
  }
  
  // Backup debug namespace
  if (prefs.begin(PreferencesNamespaces::DEBUG, true)) {
    JsonObject debug = doc.createNestedObject("debug");
    debug["ram"] = prefs.getBool("ram", false);
    debug["meas_cycle"] = prefs.getBool("meas_cycle", false);
    debug["sensor"] = prefs.getBool("sensor", false);
    debug["display"] = prefs.getBool("display", false);
    debug["websocket"] = prefs.getBool("websocket", false);
    prefs.end();
  }
  
  // Backup log namespace
  if (prefs.begin(PreferencesNamespaces::LOG, true)) {
    JsonObject log = doc.createNestedObject("log");
    log["level"] = prefs.getUChar("level", 3);
    log["file_enabled"] = prefs.getBool("file_enabled", false);
    prefs.end();
  }
  
  // Backup LED traffic namespace
  if (prefs.begin(PreferencesNamespaces::LED_TRAFFIC, true)) {
    JsonObject led = doc.createNestedObject("led_traffic");
    led["mode"] = prefs.getUChar("mode", 0);
    led["sel_meas"] = prefs.getString("sel_meas", "");
    prefs.end();
  }
  
  // Backup sensor namespaces  
  const char* sensorIds[] = {"ANALOG", "DHT"};
  JsonArray sensors = doc.createNestedArray("sensors");
  
  for (const char* sensorId : sensorIds) {
    String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
    if (prefs.begin(ns.c_str(), true)) {
      if (prefs.getBool("initialized", false)) {
        JsonObject sensor = sensors.createNestedObject();
        sensor["id"] = sensorId;
        sensor["name"] = prefs.getString("name", "");
        sensor["meas_int"] = prefs.getUInt("meas_int", 30000);
        sensor["has_err"] = prefs.getBool("has_err", false);
        
        // Backup measurements (max 8 for ANALOG, 2 for DHT)
        uint8_t maxMeas = (String(sensorId) == "ANALOG") ? 8 : 2;
        JsonArray measurements = sensor.createNestedArray("measurements");
        
        for (uint8_t i = 0; i < maxMeas; i++) {
          String prefix = "m" + String(i) + "_";
          String nameKey = prefix + "nm";
          
          if (prefs.isKey(nameKey.c_str())) {
            JsonObject meas = measurements.createNestedObject();
            meas["idx"] = i;
            meas["en"] = prefs.getBool((prefix + "en").c_str(), true);
            meas["nm"] = prefs.getString((prefix + "nm").c_str(), "");
            meas["fn"] = prefs.getString((prefix + "fn").c_str(), "");
            meas["un"] = prefs.getString((prefix + "un").c_str(), "");
            meas["min"] = prefs.getFloat((prefix + "min").c_str(), 0.0);
            meas["max"] = prefs.getFloat((prefix + "max").c_str(), 100.0);
            meas["yl"] = prefs.getFloat((prefix + "yl").c_str(), 0.0);
            meas["gl"] = prefs.getFloat((prefix + "gl").c_str(), 0.0);
            meas["gh"] = prefs.getFloat((prefix + "gh").c_str(), 100.0);
            meas["yh"] = prefs.getFloat((prefix + "yh").c_str(), 100.0);
            meas["inv"] = prefs.getBool((prefix + "inv").c_str(), false);
            meas["cal"] = prefs.getBool((prefix + "cal").c_str(), false);
            meas["acd"] = prefs.getUInt((prefix + "acd").c_str(), 0);
            meas["rmin"] = prefs.getInt((prefix + "rmin").c_str(), 0);
            meas["rmax"] = prefs.getInt((prefix + "rmax").c_str(), 1023);
          }
        }
      }
      prefs.end();
    }
  }
  
  // Write to file
  File f = LittleFS.open("/prefs_backup.json", "w");
  if (!f) {
    logger.error(F("ConfigP"), F("Konnte Backup-Datei nicht erstellen"));
    return false;
  }
  
  if (serializeJson(doc, f) == 0) {
    logger.error(F("ConfigP"), F("Fehler beim Schreiben der Backup-Datei"));
    f.close();
    return false;
  }
  
  f.close();
  logger.info(F("ConfigP"), F("Preferences erfolgreich in /prefs_backup.json gesichert"));
  return true;
}

bool ConfigPersistence::restorePreferencesFromFile() {
  logger.info(F("ConfigP"), F("Stelle Preferences aus Datei wieder her..."));
  
  File f = LittleFS.open("/prefs_backup.json", "r");
  if (!f) {
    logger.warning(F("ConfigP"), F("Keine Backup-Datei gefunden"));
    return false;
  }
  
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, f);
  f.close();
  
  if (error) {
    logger.error(F("ConfigP"), F("Fehler beim Lesen der Backup-Datei: ") + String(error.c_str()));
    return false;
  }
  
  Preferences prefs;
  
  // Restore general namespace
  if (doc.containsKey("general")) {
    JsonObject general = doc["general"];
    if (prefs.begin(PreferencesNamespaces::GENERAL, false)) {
      if (general.containsKey("device_name")) prefs.putString("device_name", general["device_name"].as<String>().c_str());
      if (general.containsKey("admin_pwd")) prefs.putString("admin_pwd", general["admin_pwd"].as<String>().c_str());
      if (general.containsKey("md5_verify")) prefs.putBool("md5_verify", general["md5_verify"]);
      if (general.containsKey("collectd_en")) prefs.putBool("collectd_en", general["collectd_en"]);
      if (general.containsKey("file_log")) prefs.putBool("file_log", general["file_log"]);
      if (general.containsKey("flower_sens")) prefs.putString("flower_sens", general["flower_sens"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
  }
  
  // Restore WiFi namespaces (3 separate namespaces)
  if (doc.containsKey("wifi")) {
    JsonObject wifi = doc["wifi"];
    
    // Restore WiFi 1
    if (prefs.begin(PreferencesNamespaces::WIFI1, false)) {
      if (wifi.containsKey("ssid1")) prefs.putString("ssid", wifi["ssid1"].as<String>().c_str());
      if (wifi.containsKey("pwd1")) prefs.putString("pwd", wifi["pwd1"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
    
    // Restore WiFi 2
    if (prefs.begin(PreferencesNamespaces::WIFI2, false)) {
      if (wifi.containsKey("ssid2")) prefs.putString("ssid", wifi["ssid2"].as<String>().c_str());
      if (wifi.containsKey("pwd2")) prefs.putString("pwd", wifi["pwd2"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
    
    // Restore WiFi 3
    if (prefs.begin(PreferencesNamespaces::WIFI3, false)) {
      if (wifi.containsKey("ssid3")) prefs.putString("ssid", wifi["ssid3"].as<String>().c_str());
      if (wifi.containsKey("pwd3")) prefs.putString("pwd", wifi["pwd3"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
  }
  
  // Restore display namespace
  if (doc.containsKey("display")) {
    JsonObject disp = doc["display"];
    if (prefs.begin(PreferencesNamespaces::DISP, false)) {
      if (disp.containsKey("show_ip")) prefs.putBool("show_ip", disp["show_ip"]);
      if (disp.containsKey("show_clock")) prefs.putBool("show_clock", disp["show_clock"]);
      if (disp.containsKey("show_flower")) prefs.putBool("show_flower", disp["show_flower"]);
      if (disp.containsKey("show_fabmobil")) prefs.putBool("show_fabmobil", disp["show_fabmobil"]);
      if (disp.containsKey("screen_dur")) prefs.putUInt("screen_dur", disp["screen_dur"]);
      if (disp.containsKey("clock_fmt")) prefs.putString("clock_fmt", disp["clock_fmt"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
  }
  
  // Restore debug namespace
  if (doc.containsKey("debug")) {
    JsonObject debug = doc["debug"];
    if (prefs.begin(PreferencesNamespaces::DEBUG, false)) {
      if (debug.containsKey("ram")) prefs.putBool("ram", debug["ram"]);
      if (debug.containsKey("meas_cycle")) prefs.putBool("meas_cycle", debug["meas_cycle"]);
      if (debug.containsKey("sensor")) prefs.putBool("sensor", debug["sensor"]);
      if (debug.containsKey("display")) prefs.putBool("display", debug["display"]);
      if (debug.containsKey("websocket")) prefs.putBool("websocket", debug["websocket"]);
      prefs.putBool("initialized", true);
      prefs.end();
    }
  }
  
  // Restore log namespace
  if (doc.containsKey("log")) {
    JsonObject log = doc["log"];
    if (prefs.begin(PreferencesNamespaces::LOG, false)) {
      if (log.containsKey("level")) prefs.putUChar("level", log["level"]);
      if (log.containsKey("file_enabled")) prefs.putBool("file_enabled", log["file_enabled"]);
      prefs.putBool("initialized", true);
      prefs.end();
    }
  }
  
  // Restore LED traffic namespace
  if (doc.containsKey("led_traffic")) {
    JsonObject led = doc["led_traffic"];
    if (prefs.begin(PreferencesNamespaces::LED_TRAFFIC, false)) {
      if (led.containsKey("mode")) prefs.putUChar("mode", led["mode"]);
      if (led.containsKey("sel_meas")) prefs.putString("sel_meas", led["sel_meas"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
  }
  
  // Restore sensor namespaces
  if (doc.containsKey("sensors")) {
    JsonArray sensors = doc["sensors"];
    for (JsonObject sensor : sensors) {
      String sensorId = sensor["id"].as<String>();
      String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
      
      if (prefs.begin(ns.c_str(), false)) {
        if (sensor.containsKey("name")) prefs.putString("name", sensor["name"].as<String>().c_str());
        if (sensor.containsKey("meas_int")) prefs.putUInt("meas_int", sensor["meas_int"]);
        if (sensor.containsKey("has_err")) prefs.putBool("has_err", sensor["has_err"]);
        prefs.putBool("initialized", true);
        
        // Restore measurements
        if (sensor.containsKey("measurements")) {
          JsonArray measurements = sensor["measurements"];
          for (JsonObject meas : measurements) {
            uint8_t idx = meas["idx"];
            String prefix = "m" + String(idx) + "_";
            
            if (meas.containsKey("en")) prefs.putBool((prefix + "en").c_str(), meas["en"]);
            if (meas.containsKey("nm")) prefs.putString((prefix + "nm").c_str(), meas["nm"].as<String>().c_str());
            if (meas.containsKey("fn")) prefs.putString((prefix + "fn").c_str(), meas["fn"].as<String>().c_str());
            if (meas.containsKey("un")) prefs.putString((prefix + "un").c_str(), meas["un"].as<String>().c_str());
            if (meas.containsKey("min")) prefs.putFloat((prefix + "min").c_str(), meas["min"]);
            if (meas.containsKey("max")) prefs.putFloat((prefix + "max").c_str(), meas["max"]);
            if (meas.containsKey("yl")) prefs.putFloat((prefix + "yl").c_str(), meas["yl"]);
            if (meas.containsKey("gl")) prefs.putFloat((prefix + "gl").c_str(), meas["gl"]);
            if (meas.containsKey("gh")) prefs.putFloat((prefix + "gh").c_str(), meas["gh"]);
            if (meas.containsKey("yh")) prefs.putFloat((prefix + "yh").c_str(), meas["yh"]);
            if (meas.containsKey("inv")) prefs.putBool((prefix + "inv").c_str(), meas["inv"]);
            if (meas.containsKey("cal")) prefs.putBool((prefix + "cal").c_str(), meas["cal"]);
            if (meas.containsKey("acd")) prefs.putUInt((prefix + "acd").c_str(), meas["acd"]);
            if (meas.containsKey("rmin")) prefs.putInt((prefix + "rmin").c_str(), meas["rmin"]);
            if (meas.containsKey("rmax")) prefs.putInt((prefix + "rmax").c_str(), meas["rmax"]);
          }
        }
        
        prefs.end();
      }
    }
  }
  
  // Don't delete backup file here - it will be wiped during filesystem update anyway
  // If we delete it now, we lose the backup before it can be used for RAM backup
  
  logger.info(F("ConfigP"), F("Preferences erfolgreich wiederhergestellt"));
  return true;
}
