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
#include "managers/manager_config_preferences.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"
#include "managers/manager_sensor_persistence.h"
// Flash persistence is used to store prefs across FS updates
#include "../utils/flash_persistence.h"

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
  // initial memory stats suppressed to reduce verbose boot output

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
    config.fileLoggingEnabled =
        PreferencesManager::getBool(generalPrefs, "file_log", FILE_LOGGING_ENABLED);
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
    config.ledTrafficLightSelectedMeasurement =
        PreferencesManager::getString(ledPrefs, "sel_meas", "");
    ledPrefs.end();
  }

  // Load flower status sensor directly
  Preferences flowerPrefs;
  if (flowerPrefs.begin(PreferencesNamespaces::GENERAL, true)) {
    config.flowerStatusSensor =
        PreferencesManager::getString(flowerPrefs, "flower_sens", "ANALOG_1");
    flowerPrefs.end();
  }

  logger.info(F("ConfigP"), F("Konfiguration erfolgreich aus Preferences geladen"));

  // final memory stats suppressed
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

  // Also remove per-measurement JSON files stored under /config
  // These files are named like: /config/sensor_<ID>_<index>.json
  logger.info(F("ConfigP"), F("Lsche Messungs-JSON-Dateien in /config (falls vorhanden)..."));
  {
    Dir dir = LittleFS.openDir("/config");
    while (dir.next()) {
      String filename = dir.fileName();
      if (filename.startsWith("sensor_") && filename.endsWith(".json")) {
        String path = String("/config/") + filename;
        if (LittleFS.remove(path)) {
          logger.info(F("ConfigP"), String(F("Gelöscht: ")) + path);
        } else {
          logger.warning(F("ConfigP"), String(F("Konnte Datei nicht löschen: ")) + path);
        }
      }
    }
  }

  logger.info(F("ConfigP"), F("Factory Reset abgeschlossen"));
  // Return success; caller (UI) will handle reboot
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::save(const ConfigData& config) {
  // Save to Preferences using atomic update functions
  logger.info(F("ConfigP"), F("Speichere Konfiguration in Preferences..."));

  // Save general settings using atomic updates
  auto result = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "device_name",
                                                      config.deviceName);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "admin_pwd",
                                                 config.adminPassword);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "md5_verify",
                                               config.md5Verification);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "file_log",
                                               config.fileLoggingEnabled);
  if (!result.isSuccess())
    return result;

  // Save WiFi settings using specialized method
  result = PreferencesManager::updateWiFiCredentials(1, config.wifiSSID1, config.wifiPassword1);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateWiFiCredentials(2, config.wifiSSID2, config.wifiPassword2);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateWiFiCredentials(3, config.wifiSSID3, config.wifiPassword3);
  if (!result.isSuccess())
    return result;

  // Save debug settings using atomic updates
  result =
      PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "ram", config.debugRAM);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "meas_cycle",
                                               config.debugMeasurementCycle);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "sensor",
                                               config.debugSensor);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "display",
                                               config.debugDisplay);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "websocket",
                                               config.debugWebSocket);
  if (!result.isSuccess())
    return result;

  // Save LED traffic light settings using atomic updates
  result = PreferencesManager::updateUInt8Value(PreferencesNamespaces::LED_TRAFFIC, "mode",
                                                config.ledTrafficLightMode);
  if (!result.isSuccess())
    return result;

  result = PreferencesManager::updateStringValue(PreferencesNamespaces::LED_TRAFFIC, "sel_meas",
                                                 config.ledTrafficLightSelectedMeasurement);
  if (!result.isSuccess())
    return result;

  // Save flower status sensor using atomic update
  result = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "flower_sens",
                                                 config.flowerStatusSensor);
  if (!result.isSuccess())
    return result;

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
    disp["show_qr"] = prefs.getBool("show_qr", false);
    disp["screen_dur"] = prefs.getUInt("screen_dur", 5);
    disp["clock_fmt"] = prefs.getString("clock_fmt", "24h");
    disp["sensor_disp"] = prefs.getString("sensor_disp", "");
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
    // Log level is stored as string (e.g., "DEBUG", "INFO", "WARNING", "ERROR")
    String logLevelStr = prefs.getString("level", "INFO");
    log["level"] = logLevelStr;
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

  // Backup sensor measurements from JSON files (new JSON-based persistence)
  const char* sensorIds[] = {"ANALOG", "DHT"};
  JsonArray sensors = doc.createNestedArray("sensors");

  // Get sensor manager to access measurementInterval
  extern std::unique_ptr<SensorManager> sensorManager;

  for (const char* sensorId : sensorIds) {
    yield(); // Watchdog reset before processing each sensor type

    bool isAnalog = (String(sensorId) == "ANALOG");
    uint8_t maxMeas = isAnalog ? 8 : 2;

    // Get measurement interval from sensor (only once per sensor, not per measurement)
    unsigned long measurementInterval = MEASUREMENT_INTERVAL * 1000; // Default fallback
    if (sensorManager) {
      const auto& sensors_list = sensorManager->getSensors();
      for (const auto& sensorPtr : sensors_list) {
        if (sensorPtr && sensorPtr->config().id == sensorId) {
          measurementInterval = sensorPtr->config().measurementInterval;
          break;
        }
      }
    }

    // Create sensor group with id and measInt at top level
    JsonObject sensorGroup = sensors.createNestedObject();
    sensorGroup["id"] = sensorId;
    sensorGroup["measInt"] = measurementInterval;
    JsonArray measurements = sensorGroup.createNestedArray("measurements");

    for (uint8_t i = 0; i < maxMeas; i++) {
      yield(); // Watchdog reset for each measurement

      MeasurementConfig config;
      auto result = SensorPersistence::loadMeasurementFromJson(sensorId, i, config);

      if (result.isSuccess()) {
        JsonObject meas = measurements.createNestedObject();
        meas["idx"] = i;
        meas["en"] = config.enabled;
        meas["nm"] = config.name;
        meas["fn"] = config.fieldName;
        meas["un"] = config.unit;
        meas["min"] = config.minValue;
        meas["max"] = config.maxValue;

        JsonObject thresh = meas.createNestedObject("thresh");
        thresh["yl"] = config.limits.yellowLow;
        thresh["gl"] = config.limits.greenLow;
        thresh["gh"] = config.limits.greenHigh;
        thresh["yh"] = config.limits.yellowHigh;

        // Analog-spezifische Felder
        if (isAnalog) {
          meas["inv"] = config.inverted;
          meas["cal"] = config.calibrationMode;
          meas["ahl"] = config.autocalHalfLifeSeconds;
          meas["rmin"] = config.absoluteRawMin;
          meas["rmax"] = config.absoluteRawMax;

          // Handle infinity values - use null for JSON compatibility
          if (isinf(config.absoluteMin)) {
            meas["amin"] = serialized("null");
          } else {
            meas["amin"] = config.absoluteMin;
          }

          if (isinf(config.absoluteMax)) {
            meas["amax"] = serialized("null");
          } else {
            meas["amax"] = config.absoluteMax;
          }
          // include stored last raw value (may be -1 if unknown)
          meas["lastRawValue"] = config.lastRawValue;
        }
        // include stored last measured value for all sensors; use null for NaN
        if (isnan(config.lastValue)) {
          meas["lastValue"] = serialized("null");
        } else {
          meas["lastValue"] = config.lastValue;
        }
      }
    }
  }

  yield(); // Watchdog reset before file operations

  // Write to file with pretty formatting
  File f = LittleFS.open("/prefs_backup.json", "w");
  if (!f) {
    logger.error(F("ConfigP"), F("Konnte Backup-Datei nicht erstellen"));
    return false;
  }

  yield(); // Watchdog reset before serialization

  if (serializeJsonPretty(doc, f) == 0) {
    logger.error(F("ConfigP"), F("Fehler beim Schreiben der Backup-Datei"));
    f.close();
    return false;
  }

  f.close();
  logger.info(F("ConfigP"), F("Preferences erfolgreich in /prefs_backup.json gesichert"));
  return true;
}

bool ConfigPersistence::restorePreferencesFromJson(const DynamicJsonDocument& doc) {
  unsigned long startTime = millis();
  logger.debug(F("ConfigP"), F("Starte Wiederherstellung der Preferences..."));

  // Restore general namespace
  unsigned long stepStart = millis();
  if (doc.containsKey("general")) {
    JsonObjectConst general = doc["general"].as<JsonObjectConst>();
    Preferences prefs;
    if (prefs.begin(PreferencesNamespaces::GENERAL, false)) {
      prefs.clear(); // Alte Daten löschen
      if (general.containsKey("device_name"))
        prefs.putString("device_name", general["device_name"].as<String>().c_str());
      if (general.containsKey("admin_pwd"))
        prefs.putString("admin_pwd", general["admin_pwd"].as<String>().c_str());
      if (general.containsKey("md5_verify"))
        prefs.putBool("md5_verify", general["md5_verify"]);
      if (general.containsKey("collectd_en"))
        prefs.putBool("collectd_en", general["collectd_en"]);
      if (general.containsKey("file_log"))
        prefs.putBool("file_log", general["file_log"]);
      if (general.containsKey("flower_sens"))
        prefs.putString("flower_sens", general["flower_sens"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
    yield(); // Watchdog reset
    logger.debug(F("ConfigP"), String(F("General-Namespace wiederhergestellt (")) +
                                   String(millis() - stepStart) + F(" ms)"));
  }

  // Restore WiFi namespaces (3 separate namespaces)
  stepStart = millis();
  if (doc.containsKey("wifi")) {
    JsonObjectConst wifi = doc["wifi"].as<JsonObjectConst>();

    // Restore WiFi 1
    {
      Preferences prefs;
      if (prefs.begin(PreferencesNamespaces::WIFI1, false)) {
        prefs.clear(); // Alte Daten löschen
        if (wifi.containsKey("ssid1"))
          prefs.putString("ssid", wifi["ssid1"].as<String>().c_str());
        if (wifi.containsKey("pwd1"))
          prefs.putString("pwd", wifi["pwd1"].as<String>().c_str());
        prefs.putBool("initialized", true);
        prefs.end();
      }
      yield(); // Watchdog reset
    }

    // Restore WiFi 2
    {
      Preferences prefs;
      if (prefs.begin(PreferencesNamespaces::WIFI2, false)) {
        prefs.clear(); // Alte Daten löschen
        if (wifi.containsKey("ssid2"))
          prefs.putString("ssid", wifi["ssid2"].as<String>().c_str());
        if (wifi.containsKey("pwd2"))
          prefs.putString("pwd", wifi["pwd2"].as<String>().c_str());
        prefs.putBool("initialized", true);
        prefs.end();
      }
      yield(); // Watchdog reset
    }

    // Restore WiFi 3
    {
      Preferences prefs;
      if (prefs.begin(PreferencesNamespaces::WIFI3, false)) {
        prefs.clear(); // Alte Daten löschen
        if (wifi.containsKey("ssid3"))
          prefs.putString("ssid", wifi["ssid3"].as<String>().c_str());
        if (wifi.containsKey("pwd3"))
          prefs.putString("pwd", wifi["pwd3"].as<String>().c_str());
        prefs.putBool("initialized", true);
        prefs.end();
      }
      yield(); // Watchdog reset
    }
    logger.debug(F("ConfigP"), String(F("WiFi-Namespaces wiederhergestellt (")) +
                                   String(millis() - stepStart) + F(" ms)"));
  }

  // Restore display namespace
  stepStart = millis();
  if (doc.containsKey("display")) {
    JsonObjectConst disp = doc["display"].as<JsonObjectConst>();
    Preferences prefs;
    if (prefs.begin(PreferencesNamespaces::DISP, false)) {
      prefs.clear(); // Alte Daten löschen
      if (disp.containsKey("show_ip"))
        prefs.putBool("show_ip", disp["show_ip"]);
      if (disp.containsKey("show_clock"))
        prefs.putBool("show_clock", disp["show_clock"]);
      if (disp.containsKey("show_flower"))
        prefs.putBool("show_flower", disp["show_flower"]);
      if (disp.containsKey("show_fabmobil"))
        prefs.putBool("show_fabmobil", disp["show_fabmobil"]);
      if (disp.containsKey("show_qr"))
        prefs.putBool("show_qr", disp["show_qr"]);
      if (disp.containsKey("screen_dur"))
        prefs.putUInt("screen_dur", disp["screen_dur"]);
      if (disp.containsKey("clock_fmt"))
        prefs.putString("clock_fmt", disp["clock_fmt"].as<String>().c_str());
      if (disp.containsKey("sensor_disp"))
        prefs.putString("sensor_disp", disp["sensor_disp"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
    yield(); // Watchdog reset
    logger.debug(F("ConfigP"), String(F("Display-Namespace wiederhergestellt (")) +
                                   String(millis() - stepStart) + F(" ms)"));
  }

  // Restore debug namespace
  stepStart = millis();
  if (doc.containsKey("debug")) {
    JsonObjectConst debug = doc["debug"].as<JsonObjectConst>();
    Preferences prefs;
    if (prefs.begin(PreferencesNamespaces::DEBUG, false)) {
      prefs.clear(); // Alte Daten löschen
      if (debug.containsKey("ram"))
        prefs.putBool("ram", debug["ram"]);
      if (debug.containsKey("meas_cycle"))
        prefs.putBool("meas_cycle", debug["meas_cycle"]);
      if (debug.containsKey("sensor"))
        prefs.putBool("sensor", debug["sensor"]);
      if (debug.containsKey("display"))
        prefs.putBool("display", debug["display"]);
      if (debug.containsKey("websocket"))
        prefs.putBool("websocket", debug["websocket"]);
      prefs.putBool("initialized", true);
      prefs.end();
    }
    yield(); // Watchdog reset
    logger.debug(F("ConfigP"), String(F("Debug-Namespace wiederhergestellt (")) +
                                   String(millis() - stepStart) + F(" ms)"));
  }

  // Restore log namespace
  stepStart = millis();
  if (doc.containsKey("log")) {
    JsonObjectConst log = doc["log"].as<JsonObjectConst>();
    Preferences prefs;
    if (prefs.begin(PreferencesNamespaces::LOG, false)) {
      prefs.clear(); // Alte Daten löschen
      if (log.containsKey("level")) {
        // Log level is stored as string (e.g., "DEBUG", "INFO", "WARNING", "ERROR")
        String levelStr = log["level"].as<String>();
        prefs.putString("level", levelStr.c_str());
      }
      if (log.containsKey("file_enabled"))
        prefs.putBool("file_enabled", log["file_enabled"]);
      prefs.putBool("initialized", true);
      prefs.end();
    }
    yield(); // Watchdog reset
    logger.debug(F("ConfigP"), String(F("Log-Namespace wiederhergestellt (")) +
                                   String(millis() - stepStart) + F(" ms)"));
  }

  // Restore LED traffic namespace
  stepStart = millis();
  if (doc.containsKey("led_traffic")) {
    JsonObjectConst led = doc["led_traffic"].as<JsonObjectConst>();
    Preferences prefs;
    if (prefs.begin(PreferencesNamespaces::LED_TRAFFIC, false)) {
      prefs.clear(); // Alte Daten löschen
      if (led.containsKey("mode"))
        prefs.putUChar("mode", led["mode"]);
      if (led.containsKey("sel_meas"))
        prefs.putString("sel_meas", led["sel_meas"].as<String>().c_str());
      prefs.putBool("initialized", true);
      prefs.end();
    }
    yield(); // Watchdog reset
    logger.debug(F("ConfigP"), String(F("LED-Traffic-Namespace wiederhergestellt (")) +
                                   String(millis() - stepStart) + F(" ms)"));
  }

  // Restore sensor measurements to JSON files (new JSON-based persistence)
  stepStart = millis();
  if (doc.containsKey("sensors")) {
    JsonArrayConst sensors = doc["sensors"].as<JsonArrayConst>();
    logger.debug(F("ConfigP"), String(F("Beginne Wiederherstellung von ")) +
                                   String(sensors.size()) + F(" Sensor-Gruppen..."));

    // Track measurement intervals per sensor
    std::map<String, unsigned long> sensorIntervals;

    // NEW STRUCTURE: sensors array contains sensor groups with measurements sub-array
    for (JsonObjectConst sensorGroup : sensors) {
      String sensorId = sensorGroup["id"] | "";

      // Extract measurement interval at sensor group level
      if (sensorGroup.containsKey("measInt")) {
        unsigned long interval = sensorGroup["measInt"] | (MEASUREMENT_INTERVAL * 1000);
        sensorIntervals[sensorId] = interval;
      }

      // Check if new structure (with measurements array) or old structure (backward compatibility)
      if (sensorGroup.containsKey("measurements")) {
        // NEW STRUCTURE: iterate over measurements array
        JsonArrayConst measurements = sensorGroup["measurements"].as<JsonArrayConst>();

        for (JsonObjectConst meas : measurements) {
          size_t idx = meas["idx"] | 0;

          MeasurementConfig config;
          config.enabled = meas["en"] | true;
          config.name = meas["nm"] | String("");
          config.fieldName = meas["fn"] | String("");
          // Restore last measured value if present
          config.lastValue = meas.containsKey("lastValue") ? (float)meas["lastValue"] : NAN;
          config.unit = meas["un"] | String("");
          config.minValue = meas["min"] | 0.0f;
          config.maxValue = meas["max"] | 100.0f;

          JsonObjectConst thresh = meas["thresh"];
          config.limits.yellowLow = thresh["yl"] | 0.0f;
          config.limits.greenLow = thresh["gl"] | 0.0f;
          config.limits.greenHigh = thresh["gh"] | 100.0f;
          config.limits.yellowHigh = thresh["yh"] | 100.0f;

          // Analog-spezifische Felder
          if (sensorId == "ANALOG") {
            config.inverted = meas["inv"] | false;
            config.calibrationMode = meas["cal"] | false;
            config.autocalHalfLifeSeconds = meas["ahl"] | 0;
            // Restore last raw value if present
            config.lastRawValue = meas.containsKey("lastRawValue") ? (int)meas["lastRawValue"] : -1;
            config.absoluteRawMin = meas["rmin"] | 0;
            config.absoluteRawMax = meas["rmax"] | 1023;

            // Handle null values for infinity
            if (meas["amin"].isNull()) {
              config.absoluteMin = INFINITY;
            } else {
              config.absoluteMin = meas["amin"] | INFINITY;
            }

            if (meas["amax"].isNull()) {
              config.absoluteMax = -INFINITY;
            } else {
              config.absoluteMax = meas["amax"] | -INFINITY;
            }
          }

          // Schreibe Messung in JSON-Datei
          auto result = SensorPersistence::saveMeasurementToJson(sensorId, idx, config);
          if (!result.isSuccess()) {
            logger.warning(F("ConfigP"), F("Fehler beim Wiederherstellen von ") + sensorId +
                                             F("[") + String(idx) + F("]: ") + result.getMessage());
          }

          yield(); // Watchdog reset
        }
      } else {
        // OLD STRUCTURE (backward compatibility): sensor group IS the measurement
        size_t idx = sensorGroup["idx"] | 0;

        MeasurementConfig config;
        config.enabled = sensorGroup["en"] | true;
        config.name = sensorGroup["nm"] | String("");
        config.fieldName = sensorGroup["fn"] | String("");
        // Restore last measured value if present (old structure)
        config.lastValue =
            sensorGroup.containsKey("lastValue") ? (float)sensorGroup["lastValue"] : NAN;
        config.unit = sensorGroup["un"] | String("");
        config.minValue = sensorGroup["min"] | 0.0f;
        config.maxValue = sensorGroup["max"] | 100.0f;

        JsonObjectConst thresh = sensorGroup["thresh"];
        config.limits.yellowLow = thresh["yl"] | 0.0f;
        config.limits.greenLow = thresh["gl"] | 0.0f;
        config.limits.greenHigh = thresh["gh"] | 100.0f;
        config.limits.yellowHigh = thresh["yh"] | 100.0f;

        // Analog-spezifische Felder
        if (sensorId == "ANALOG") {
          config.inverted = sensorGroup["inv"] | false;
          config.calibrationMode = sensorGroup["cal"] | false;
          config.autocalHalfLifeSeconds = sensorGroup["ahl"] | 0;
          // Restore last raw value if present (old structure)
          config.lastRawValue =
              sensorGroup.containsKey("lastRawValue") ? (int)sensorGroup["lastRawValue"] : -1;
          config.absoluteRawMin = sensorGroup["rmin"] | 0;
          config.absoluteRawMax = sensorGroup["rmax"] | 1023;

          // Handle null values for infinity
          if (sensorGroup["amin"].isNull()) {
            config.absoluteMin = INFINITY;
          } else {
            config.absoluteMin = sensorGroup["amin"] | INFINITY;
          }

          if (sensorGroup["amax"].isNull()) {
            config.absoluteMax = -INFINITY;
          } else {
            config.absoluteMax = sensorGroup["amax"] | -INFINITY;
          }
        }

        // Schreibe Messung in JSON-Datei
        auto result = SensorPersistence::saveMeasurementToJson(sensorId, idx, config);
        if (!result.isSuccess()) {
          logger.warning(F("ConfigP"), F("Fehler beim Wiederherstellen von ") + sensorId + F("[") +
                                           String(idx) + F("]: ") + result.getMessage());
        }

        yield(); // Watchdog reset
      }
    }

    // Apply measurement intervals to sensors
    extern std::unique_ptr<SensorManager> sensorManager;
    if (sensorManager && !sensorIntervals.empty()) {
      // Load settings.json to update intervals persistently
      const char* settingsPath = "/config/settings.json";
      DynamicJsonDocument settingsDoc(4096);

      {
        File settingsFile = LittleFS.open(settingsPath, "r");
        if (settingsFile) {
          DeserializationError error = deserializeJson(settingsDoc, settingsFile);
          settingsFile.close();
          if (error != DeserializationError::Ok) {
            logger.warning(F("ConfigP"), F("Konnte settings.json nicht parsen, erstelle neue"));
          }
        }
      }

      // Ensure sensors object exists in settings.json
      if (!settingsDoc.containsKey("sensors")) {
        settingsDoc.createNestedObject("sensors");
      }
      JsonObject settingsSensors = settingsDoc["sensors"];

      for (const auto& pair : sensorIntervals) {
        const String& sensorId = pair.first;
        unsigned long interval = pair.second;

        // Find sensor and update interval in memory
        const auto& sensors_list = sensorManager->getSensors();
        for (const auto& sensorPtr : sensors_list) {
          if (sensorPtr && sensorPtr->config().id == sensorId) {
            sensorPtr->mutableConfig().measurementInterval = interval;
            logger.debug(F("ConfigP"), String(F("Messintervall für ")) + sensorId + F(" auf ") +
                                           String(interval) + F("ms gesetzt"));
            break;
          }
        }

        // Also save to settings.json for persistence across reboots
        if (!settingsSensors.containsKey(sensorId)) {
          settingsSensors.createNestedObject(sensorId);
        }
        settingsSensors[sensorId]["interval"] = interval;
      }

      // Save updated settings.json
      {
        File settingsFile = LittleFS.open(settingsPath, "w");
        if (settingsFile) {
          serializeJson(settingsDoc, settingsFile);
          settingsFile.close();
          logger.debug(F("ConfigP"), F("Messintervalle in settings.json gespeichert"));
        } else {
          logger.warning(F("ConfigP"), F("Konnte settings.json nicht für Schreibzugriff öffnen"));
        }
      }
    }

    logger.debug(F("ConfigP"), String(F("Sensor-Messungen wiederhergestellt (")) +
                                   String(millis() - stepStart) + F(" ms)"));
  }

  // Backup file cleaned up by caller (flash restore or config upload handler)

  logger.info(F("ConfigP"), String(F("Preferences erfolgreich wiederhergestellt (Gesamtdauer: ")) +
                                String(millis() - startTime) + F(" ms)"));
  return true;
}
