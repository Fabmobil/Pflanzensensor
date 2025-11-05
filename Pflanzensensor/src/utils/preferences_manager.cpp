/**
 * @file preferences_manager.cpp
 * @brief Implementation of PreferencesManager class
 */

#include "preferences_manager.h"
#include "../configs/config_pflanzensensor.h"
#include "../logger/logger.h"

namespace PreferencesNamespaces {
  String getSensorNamespace(const String& sensorId) {
    // Namespace max length is 15 chars, so we need to truncate sensor IDs
    String ns = "s_" + sensorId;
    if (ns.length() > 15) {
      ns = ns.substring(0, 15);
    }
    return ns;
  }
  
  String getSensorMeasurementKey(uint8_t measurementIndex, const char* suffix) {
    return "m" + String(measurementIndex) + "_" + String(suffix);
  }
}

// Helper functions for type-safe access
String PreferencesManager::getString(Preferences& prefs, const char* key, const String& defaultValue) {
  return prefs.getString(key, defaultValue);
}

bool PreferencesManager::getBool(Preferences& prefs, const char* key, bool defaultValue) {
  return prefs.getBool(key, defaultValue);
}

uint8_t PreferencesManager::getUChar(Preferences& prefs, const char* key, uint8_t defaultValue) {
  return prefs.getUChar(key, defaultValue);
}

uint32_t PreferencesManager::getUInt(Preferences& prefs, const char* key, uint32_t defaultValue) {
  return prefs.getUInt(key, defaultValue);
}

int PreferencesManager::getInt(Preferences& prefs, const char* key, int defaultValue) {
  return prefs.getInt(key, defaultValue);
}

float PreferencesManager::getFloat(Preferences& prefs, const char* key, float defaultValue) {
  return prefs.getFloat(key, defaultValue);
}

bool PreferencesManager::putString(Preferences& prefs, const char* key, const String& value) {
  return prefs.putString(key, value) > 0;
}

bool PreferencesManager::putBool(Preferences& prefs, const char* key, bool value) {
  return prefs.putBool(key, value) > 0;
}

bool PreferencesManager::putUChar(Preferences& prefs, const char* key, uint8_t value) {
  return prefs.putUChar(key, value) > 0;
}

bool PreferencesManager::putUInt(Preferences& prefs, const char* key, uint32_t value) {
  return prefs.putUInt(key, value) > 0;
}

bool PreferencesManager::putInt(Preferences& prefs, const char* key, int value) {
  return prefs.putInt(key, value) > 0;
}

bool PreferencesManager::putFloat(Preferences& prefs, const char* key, float value) {
  return prefs.putFloat(key, value) > 0;
}

// Check if namespace exists
bool PreferencesManager::namespaceExists(const char* namespaceName) {
  Preferences prefs;
  bool opened = prefs.begin(namespaceName, true); // Read-only
  if (opened) {
    // Check if there's at least one key
    bool exists = prefs.isKey("initialized");
    prefs.end();
    return exists;
  }
  return false;
}

// Initialize general namespace with defaults
PreferencesManager::PrefResult PreferencesManager::initGeneralNamespace() {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des General-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open general namespace");
  }
  
  // Mark as initialized
  putBool(prefs, "initialized", true);
  
  // Set defaults from config_example.h
  putString(prefs, "device_name", String(DEVICE_NAME));
  putString(prefs, "admin_pwd", String(ADMIN_PASSWORD));
  putBool(prefs, "md5_verify", false);
  putBool(prefs, "file_log", FILE_LOGGING_ENABLED);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("General-Namespace mit Standardwerten initialisiert"));
  return PrefResult::success();
}

// Initialize WiFi namespace with defaults
PreferencesManager::PrefResult PreferencesManager::initWiFiNamespace() {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::WIFI, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open WiFi namespace");
  }
  
  putBool(prefs, "initialized", true);
  putString(prefs, "ssid1", String(WIFI_SSID_1));
  putString(prefs, "pwd1", String(WIFI_PASSWORD_1));
  putString(prefs, "ssid2", String(WIFI_SSID_2));
  putString(prefs, "pwd2", String(WIFI_PASSWORD_2));
  putString(prefs, "ssid3", String(WIFI_SSID_3));
  putString(prefs, "pwd3", String(WIFI_PASSWORD_3));
  
  prefs.end();
  logger.info(F("PrefMgr"), F("WiFi-Namespace mit Standardwerten initialisiert"));
  return PrefResult::success();
}

// Initialize Display namespace with defaults
PreferencesManager::PrefResult PreferencesManager::initDisplayNamespace() {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Display-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open display namespace");
  }
  
  putBool(prefs, "initialized", true);
  putBool(prefs, "show_ip", true);
  putBool(prefs, "show_clock", true);
  putBool(prefs, "show_flower", true);
  putBool(prefs, "show_fabmobil", true);
  putUInt(prefs, "screen_dur", DISPLAY_DEFAULT_TIME * 1000);
  putString(prefs, "clock_fmt", "24h");
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Display-Namespace mit Standardwerten initialisiert"));
  return PrefResult::success();
}

// Initialize Log namespace with defaults
PreferencesManager::PrefResult PreferencesManager::initLogNamespace() {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LOG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Log-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open log namespace");
  }
  
  putBool(prefs, "initialized", true);
  putString(prefs, "level", String(LOG_LEVEL));
  putBool(prefs, "file_enabled", FILE_LOGGING_ENABLED);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Log-Namespace mit Standardwerten initialisiert"));
  return PrefResult::success();
}

// Initialize LED Traffic Light namespace with defaults
PreferencesManager::PrefResult PreferencesManager::initLedTrafficNamespace() {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LED_TRAFFIC, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des LED-Traffic-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open LED traffic namespace");
  }
  
  putBool(prefs, "initialized", true);
  putUChar(prefs, "mode", 2); // Default to mode 2 (single measurement)
  putString(prefs, "sel_meas", "ANALOG_1");
  
  prefs.end();
  logger.info(F("PrefMgr"), F("LED-Traffic-Namespace mit Standardwerten initialisiert"));
  return PrefResult::success();
}

// Initialize Debug namespace with defaults
PreferencesManager::PrefResult PreferencesManager::initDebugNamespace() {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Debug-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open debug namespace");
  }
  
  putBool(prefs, "initialized", true);
  putBool(prefs, "ram", DEBUG_RAM);
  putBool(prefs, "meas_cycle", DEBUG_MEASUREMENT_CYCLE);
  putBool(prefs, "sensor", DEBUG_SENSOR);
  putBool(prefs, "display", DEBUG_DISPLAY);
  putBool(prefs, "websocket", DEBUG_WEBSOCKET);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-Namespace mit Standardwerten initialisiert"));
  return PrefResult::success();
}

// Initialize all namespaces
PreferencesManager::PrefResult PreferencesManager::initializeAllNamespaces() {
  logger.info(F("PrefMgr"), F("Initialisiere Preferences-Namespaces..."));
  
  // Initialize each namespace if it doesn't exist
  if (!namespaceExists(PreferencesNamespaces::GENERAL)) {
    auto result = initGeneralNamespace();
    if (!result.isSuccess()) return result;
  } else {
    logger.info(F("PrefMgr"), F("General-Namespace bereits vorhanden"));
  }
  
  if (!namespaceExists(PreferencesNamespaces::WIFI)) {
    auto result = initWiFiNamespace();
    if (!result.isSuccess()) return result;
  } else {
    logger.info(F("PrefMgr"), F("WiFi-Namespace bereits vorhanden"));
  }
  
  if (!namespaceExists(PreferencesNamespaces::DISP)) {
    auto result = initDisplayNamespace();
    if (!result.isSuccess()) return result;
  } else {
    logger.info(F("PrefMgr"), F("Display-Namespace bereits vorhanden"));
  }
  
  if (!namespaceExists(PreferencesNamespaces::LOG)) {
    auto result = initLogNamespace();
    if (!result.isSuccess()) return result;
  } else {
    logger.info(F("PrefMgr"), F("Log-Namespace bereits vorhanden"));
  }
  
  if (!namespaceExists(PreferencesNamespaces::LED_TRAFFIC)) {
    auto result = initLedTrafficNamespace();
    if (!result.isSuccess()) return result;
  } else {
    logger.info(F("PrefMgr"), F("LED-Traffic-Namespace bereits vorhanden"));
  }
  
  if (!namespaceExists(PreferencesNamespaces::DEBUG)) {
    auto result = initDebugNamespace();
    if (!result.isSuccess()) return result;
  } else {
    logger.info(F("PrefMgr"), F("Debug-Namespace bereits vorhanden"));
  }
  
  logger.info(F("PrefMgr"), F("Alle Namespaces erfolgreich initialisiert"));
  return PrefResult::success();
}

// Clear all preferences
PreferencesManager::PrefResult PreferencesManager::clearAll() {
  logger.info(F("PrefMgr"), F("Lösche alle Preferences (Factory Reset)..."));
  
  // Clear each namespace
  const char* namespaces[] = {
    PreferencesNamespaces::GENERAL,
    PreferencesNamespaces::WIFI,
    PreferencesNamespaces::DISP,
    PreferencesNamespaces::LOG,
    PreferencesNamespaces::LED_TRAFFIC,
    PreferencesNamespaces::DEBUG
  };
  
  for (const char* ns : namespaces) {
    Preferences prefs;
    if (prefs.begin(ns, false)) {
      prefs.clear();
      prefs.end();
      logger.info(F("PrefMgr"), String(F("Namespace gelöscht: ")) + String(ns));
    }
  }
  
  logger.info(F("PrefMgr"), F("Factory Reset abgeschlossen"));
  return PrefResult::success();
}

// General settings
PreferencesManager::PrefResult PreferencesManager::saveGeneralSettings(
    const String& deviceName, const String& adminPassword,
    bool md5Verification, bool fileLoggingEnabled) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Speichern der General-Einstellungen"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open general namespace");
  }
  
  putString(prefs, "device_name", deviceName);
  putString(prefs, "admin_pwd", adminPassword);
  putBool(prefs, "md5_verify", md5Verification);
  putBool(prefs, "file_log", fileLoggingEnabled);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("General-Einstellungen gespeichert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadGeneralSettings(
    String& deviceName, String& adminPassword,
    bool& md5Verification, bool& fileLoggingEnabled) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, true)) {
    logger.warning(F("PrefMgr"), F("General-Namespace nicht gefunden, verwende Standardwerte"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "General namespace not found");
  }
  
  deviceName = getString(prefs, "device_name", DEVICE_NAME);
  adminPassword = getString(prefs, "admin_pwd", ADMIN_PASSWORD);
  md5Verification = getBool(prefs, "md5_verify", false);
  fileLoggingEnabled = getBool(prefs, "file_log", FILE_LOGGING_ENABLED);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("General-Einstellungen geladen"));
  return PrefResult::success();
}

// WiFi settings
PreferencesManager::PrefResult PreferencesManager::saveWiFiSettings(
    const String& ssid1, const String& pwd1,
    const String& ssid2, const String& pwd2,
    const String& ssid3, const String& pwd3) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::WIFI, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Speichern der WiFi-Einstellungen"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open WiFi namespace");
  }
  
  putString(prefs, "ssid1", ssid1);
  putString(prefs, "pwd1", pwd1);
  putString(prefs, "ssid2", ssid2);
  putString(prefs, "pwd2", pwd2);
  putString(prefs, "ssid3", ssid3);
  putString(prefs, "pwd3", pwd3);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("WiFi-Einstellungen gespeichert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadWiFiSettings(
    String& ssid1, String& pwd1,
    String& ssid2, String& pwd2,
    String& ssid3, String& pwd3) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::WIFI, true)) {
    logger.warning(F("PrefMgr"), F("WiFi-Namespace nicht gefunden, verwende Standardwerte"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "WiFi namespace not found");
  }
  
  ssid1 = getString(prefs, "ssid1", WIFI_SSID_1);
  pwd1 = getString(prefs, "pwd1", WIFI_PASSWORD_1);
  ssid2 = getString(prefs, "ssid2", WIFI_SSID_2);
  pwd2 = getString(prefs, "pwd2", WIFI_PASSWORD_2);
  ssid3 = getString(prefs, "ssid3", WIFI_SSID_3);
  pwd3 = getString(prefs, "pwd3", WIFI_PASSWORD_3);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("WiFi-Einstellungen geladen"));
  return PrefResult::success();
}

// Display settings
PreferencesManager::PrefResult PreferencesManager::saveDisplaySettings(
    bool showIpScreen, bool showClock, bool showFlowerImage, 
    bool showFabmobilImage, unsigned long screenDuration,
    const String& clockFormat) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Speichern der Display-Einstellungen"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open display namespace");
  }
  
  putBool(prefs, "show_ip", showIpScreen);
  putBool(prefs, "show_clock", showClock);
  putBool(prefs, "show_flower", showFlowerImage);
  putBool(prefs, "show_fabmobil", showFabmobilImage);
  putUInt(prefs, "screen_dur", screenDuration);
  putString(prefs, "clock_fmt", clockFormat);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Display-Einstellungen gespeichert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadDisplaySettings(
    bool& showIpScreen, bool& showClock, bool& showFlowerImage,
    bool& showFabmobilImage, unsigned long& screenDuration,
    String& clockFormat) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, true)) {
    logger.warning(F("PrefMgr"), F("Display-Namespace nicht gefunden, verwende Standardwerte"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Display namespace not found");
  }
  
  showIpScreen = getBool(prefs, "show_ip", true);
  showClock = getBool(prefs, "show_clock", true);
  showFlowerImage = getBool(prefs, "show_flower", true);
  showFabmobilImage = getBool(prefs, "show_fabmobil", true);
  screenDuration = getUInt(prefs, "screen_dur", DISPLAY_DEFAULT_TIME * 1000);
  clockFormat = getString(prefs, "clock_fmt", "24h");
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Display-Einstellungen geladen"));
  return PrefResult::success();
}

// Log settings
PreferencesManager::PrefResult PreferencesManager::saveLogSettings(
    const String& logLevel, bool fileLogging) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LOG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Speichern der Log-Einstellungen"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open log namespace");
  }
  
  putString(prefs, "level", logLevel);
  putBool(prefs, "file_enabled", fileLogging);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Log-Einstellungen gespeichert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadLogSettings(
    String& logLevel, bool& fileLogging) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LOG, true)) {
    logger.warning(F("PrefMgr"), F("Log-Namespace nicht gefunden, verwende Standardwerte"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Log namespace not found");
  }
  
  logLevel = getString(prefs, "level", LOG_LEVEL);
  fileLogging = getBool(prefs, "file_enabled", FILE_LOGGING_ENABLED);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Log-Einstellungen geladen"));
  return PrefResult::success();
}

// LED Traffic Light settings
PreferencesManager::PrefResult PreferencesManager::saveLedTrafficSettings(
    uint8_t mode, const String& selectedMeasurement) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LED_TRAFFIC, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Speichern der LED-Traffic-Einstellungen"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open LED traffic namespace");
  }
  
  putUChar(prefs, "mode", mode);
  putString(prefs, "sel_meas", selectedMeasurement);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("LED-Traffic-Einstellungen gespeichert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadLedTrafficSettings(
    uint8_t& mode, String& selectedMeasurement) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LED_TRAFFIC, true)) {
    logger.warning(F("PrefMgr"), F("LED-Traffic-Namespace nicht gefunden, verwende Standardwerte"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "LED traffic namespace not found");
  }
  
  mode = getUChar(prefs, "mode", 2);
  selectedMeasurement = getString(prefs, "sel_meas", "ANALOG_1");
  
  prefs.end();
  logger.info(F("PrefMgr"), F("LED-Traffic-Einstellungen geladen"));
  return PrefResult::success();
}

// Debug settings
PreferencesManager::PrefResult PreferencesManager::saveDebugSettings(
    bool debugRAM, bool debugMeasurementCycle, bool debugSensor,
    bool debugDisplay, bool debugWebSocket) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Speichern der Debug-Einstellungen"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open debug namespace");
  }
  
  putBool(prefs, "ram", debugRAM);
  putBool(prefs, "meas_cycle", debugMeasurementCycle);
  putBool(prefs, "sensor", debugSensor);
  putBool(prefs, "display", debugDisplay);
  putBool(prefs, "websocket", debugWebSocket);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-Einstellungen gespeichert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadDebugSettings(
    bool& debugRAM, bool& debugMeasurementCycle, bool& debugSensor,
    bool& debugDisplay, bool& debugWebSocket) {
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, true)) {
    logger.warning(F("PrefMgr"), F("Debug-Namespace nicht gefunden, verwende Standardwerte"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Debug namespace not found");
  }
  
  debugRAM = getBool(prefs, "ram", DEBUG_RAM);
  debugMeasurementCycle = getBool(prefs, "meas_cycle", DEBUG_MEASUREMENT_CYCLE);
  debugSensor = getBool(prefs, "sensor", DEBUG_SENSOR);
  debugDisplay = getBool(prefs, "display", DEBUG_DISPLAY);
  debugWebSocket = getBool(prefs, "websocket", DEBUG_WEBSOCKET);
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-Einstellungen geladen"));
  return PrefResult::success();
}

// Sensor settings
PreferencesManager::PrefResult PreferencesManager::saveSensorSettings(
    const String& sensorId, const String& name, unsigned long measurementInterval,
    bool hasPersistentError) {
  
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("PrefMgr"), String(F("Fehler beim Speichern der Sensor-Einstellungen für ")) + sensorId);
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }
  
  putBool(prefs, "initialized", true);
  putString(prefs, "name", name);
  putUInt(prefs, "meas_int", measurementInterval);
  putBool(prefs, "has_err", hasPersistentError);
  
  prefs.end();
  logger.info(F("PrefMgr"), String(F("Sensor-Einstellungen gespeichert für ")) + sensorId);
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadSensorSettings(
    const String& sensorId, String& name, unsigned long& measurementInterval,
    bool& hasPersistentError) {
  
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), true)) {
    logger.warning(F("PrefMgr"), String(F("Sensor-Namespace nicht gefunden für ")) + sensorId);
    return PrefResult::fail(ConfigError::FILE_ERROR, "Sensor namespace not found");
  }
  
  name = getString(prefs, "name", "");
  measurementInterval = getUInt(prefs, "meas_int", MEASUREMENT_INTERVAL * 1000);
  hasPersistentError = getBool(prefs, "has_err", false);
  
  prefs.end();
  logger.info(F("PrefMgr"), String(F("Sensor-Einstellungen geladen für ")) + sensorId);
  return PrefResult::success();
}

// Sensor measurement settings
PreferencesManager::PrefResult PreferencesManager::saveSensorMeasurement(
    const String& sensorId, uint8_t measurementIndex,
    bool enabled, const String& name, const String& fieldName, const String& unit,
    float minValue, float maxValue,
    float yellowLow, float greenLow, float greenHigh, float yellowHigh,
    bool inverted, bool calibrationMode, uint32_t autocalDuration,
    int absoluteRawMin, int absoluteRawMax) {
  
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("PrefMgr"), String(F("Fehler beim Speichern der Messwerte für ")) + sensorId);
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open sensor namespace");
  }
  
  String prefix = "m" + String(measurementIndex) + "_";
  putBool(prefs, (prefix + "en").c_str(), enabled);
  putString(prefs, (prefix + "nm").c_str(), name);
  putString(prefs, (prefix + "fn").c_str(), fieldName);
  putString(prefs, (prefix + "un").c_str(), unit);
  putFloat(prefs, (prefix + "min").c_str(), minValue);
  putFloat(prefs, (prefix + "max").c_str(), maxValue);
  putFloat(prefs, (prefix + "yl").c_str(), yellowLow);
  putFloat(prefs, (prefix + "gl").c_str(), greenLow);
  putFloat(prefs, (prefix + "gh").c_str(), greenHigh);
  putFloat(prefs, (prefix + "yh").c_str(), yellowHigh);
  putBool(prefs, (prefix + "inv").c_str(), inverted);
  putBool(prefs, (prefix + "cal").c_str(), calibrationMode);
  putUInt(prefs, (prefix + "acd").c_str(), autocalDuration);
  putInt(prefs, (prefix + "rmin").c_str(), absoluteRawMin);
  putInt(prefs, (prefix + "rmax").c_str(), absoluteRawMax);
  
  prefs.end();
  logger.info(F("PrefMgr"), String(F("Messwert-Einstellungen gespeichert für ")) + sensorId + 
              String(F(" Messung ")) + String(measurementIndex));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadSensorMeasurement(
    const String& sensorId, uint8_t measurementIndex,
    bool& enabled, String& name, String& fieldName, String& unit,
    float& minValue, float& maxValue,
    float& yellowLow, float& greenLow, float& greenHigh, float& yellowHigh,
    bool& inverted, bool& calibrationMode, uint32_t& autocalDuration,
    int& absoluteRawMin, int& absoluteRawMax) {
  
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (!prefs.begin(ns.c_str(), true)) {
    logger.warning(F("PrefMgr"), String(F("Sensor-Namespace nicht gefunden für ")) + sensorId);
    return PrefResult::fail(ConfigError::FILE_ERROR, "Sensor namespace not found");
  }
  
  String prefix = "m" + String(measurementIndex) + "_";
  enabled = getBool(prefs, (prefix + "en").c_str(), true);
  name = getString(prefs, (prefix + "nm").c_str(), "");
  fieldName = getString(prefs, (prefix + "fn").c_str(), "");
  unit = getString(prefs, (prefix + "un").c_str(), "");
  minValue = getFloat(prefs, (prefix + "min").c_str(), 0.0f);
  maxValue = getFloat(prefs, (prefix + "max").c_str(), 100.0f);
  yellowLow = getFloat(prefs, (prefix + "yl").c_str(), 10.0f);
  greenLow = getFloat(prefs, (prefix + "gl").c_str(), 20.0f);
  greenHigh = getFloat(prefs, (prefix + "gh").c_str(), 80.0f);
  yellowHigh = getFloat(prefs, (prefix + "yh").c_str(), 90.0f);
  inverted = getBool(prefs, (prefix + "inv").c_str(), false);
  calibrationMode = getBool(prefs, (prefix + "cal").c_str(), false);
  autocalDuration = getUInt(prefs, (prefix + "acd").c_str(), 86400);
  absoluteRawMin = getInt(prefs, (prefix + "rmin").c_str(), INT_MAX);
  absoluteRawMax = getInt(prefs, (prefix + "rmax").c_str(), INT_MIN);
  
  prefs.end();
  logger.info(F("PrefMgr"), String(F("Messwert-Einstellungen geladen für ")) + sensorId + 
              String(F(" Messung ")) + String(measurementIndex));
  return PrefResult::success();
}

// Check if sensor namespace exists
bool PreferencesManager::sensorNamespaceExists(const String& sensorId) {
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  return namespaceExists(ns.c_str());
}

// Clear sensor namespace
PreferencesManager::PrefResult PreferencesManager::clearSensorNamespace(const String& sensorId) {
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  Preferences prefs;
  if (prefs.begin(ns.c_str(), false)) {
    prefs.clear();
    prefs.end();
    logger.info(F("PrefMgr"), String(F("Sensor-Namespace gelöscht: ")) + sensorId);
  }
  return PrefResult::success();
}

// Flower status sensor
PreferencesManager::PrefResult PreferencesManager::saveFlowerStatusSensor(const String& sensorId) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Speichern des Flower-Status-Sensors"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open general namespace");
  }
  
  putString(prefs, "flower_sens", sensorId);
  prefs.end();
  logger.info(F("PrefMgr"), F("Flower-Status-Sensor gespeichert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::loadFlowerStatusSensor(String& sensorId) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, true)) {
    logger.warning(F("PrefMgr"), F("General-Namespace nicht gefunden"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "General namespace not found");
  }
  
  sensorId = getString(prefs, "flower_sens", "ANALOG_1");
  prefs.end();
  logger.info(F("PrefMgr"), F("Flower-Status-Sensor geladen"));
  return PrefResult::success();
}

// ====== Atomic Update Functions (DRY) ======

PreferencesManager::PrefResult PreferencesManager::updateDeviceName(const String& deviceName) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des General-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open general namespace");
  }
  
  if (!putString(prefs, "device_name", deviceName)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save device name");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Gerätename aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateAdminPassword(const String& adminPassword) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des General-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open general namespace");
  }
  
  if (!putString(prefs, "admin_pwd", adminPassword)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save admin password");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Admin-Passwort aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateMD5Verification(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des General-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open general namespace");
  }
  
  if (!putBool(prefs, "md5_verify", enabled)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save MD5 verification");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("MD5-Verifizierung aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateFileLoggingEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des General-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open general namespace");
  }
  
  if (!putBool(prefs, "file_log", enabled)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save file logging");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Datei-Logging aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateWiFiCredentials(uint8_t setIndex, const String& ssid, const String& password) {
  if (setIndex < 1 || setIndex > 3) {
    return PrefResult::fail(ConfigError::INVALID_INPUT, "Invalid WiFi set index (must be 1-3)");
  }
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::WIFI, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open WiFi namespace");
  }
  
  String ssidKey = "ssid" + String(setIndex);
  String pwdKey = "pwd" + String(setIndex);
  
  if (!putString(prefs, ssidKey.c_str(), ssid) || !putString(prefs, pwdKey.c_str(), password)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save WiFi credentials");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), String(F("WiFi-Zugangsdaten Set ")) + String(setIndex) + F(" aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateLedTrafficMode(uint8_t mode) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LED_TRAFFIC, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des LED-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open LED namespace");
  }
  
  if (!putUChar(prefs, "mode", mode)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save LED mode");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("LED-Traffic-Mode aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateLedTrafficMeasurement(const String& measurement) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LED_TRAFFIC, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des LED-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open LED namespace");
  }
  
  if (!putString(prefs, "sel_meas", measurement)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save LED measurement");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("LED-Traffic-Messung aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateDebugRAM(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Debug-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open debug namespace");
  }
  
  if (!putBool(prefs, "ram", enabled)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save debug RAM");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-RAM aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateDebugMeasurementCycle(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Debug-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open debug namespace");
  }
  
  if (!putBool(prefs, "meas_cycle", enabled)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save debug measurement cycle");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-Messzyklus aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateDebugSensor(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Debug-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open debug namespace");
  }
  
  if (!putBool(prefs, "sensor", enabled)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save debug sensor");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-Sensor aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateDebugDisplay(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Debug-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open debug namespace");
  }
  
  if (!putBool(prefs, "display", enabled)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save debug display");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-Display aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateDebugWebSocket(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des Debug-Namespace"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open debug namespace");
  }
  
  if (!putBool(prefs, "websocket", enabled)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save debug websocket");
  }
  
  prefs.end();
  logger.info(F("PrefMgr"), F("Debug-WebSocket aktualisiert"));
  return PrefResult::success();
}

// Atomic update methods for display settings
PreferencesManager::PrefResult PreferencesManager::updateScreenDuration(unsigned int duration) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen von Display-Preferences"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open display namespace");
  }

  prefs.putUInt("scr_dur", duration);
  prefs.end();

  logger.info(F("PrefMgr"), F("Screen-Duration aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateClockFormat(const String& format) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen von Display-Preferences"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open display namespace");
  }

  putString(prefs, "clk_fmt", format);
  prefs.end();

  logger.info(F("PrefMgr"), F("Clock-Format aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateClockEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen von Display-Preferences"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open display namespace");
  }

  prefs.putBool("show_clk", enabled);
  prefs.end();

  logger.info(F("PrefMgr"), F("Clock-Enabled aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateIpScreenEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen von Display-Preferences"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open display namespace");
  }

  prefs.putBool("show_ip", enabled);
  prefs.end();

  logger.info(F("PrefMgr"), F("IP-Screen-Enabled aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateFlowerImageEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen von Display-Preferences"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open display namespace");
  }

  prefs.putBool("show_flwr", enabled);
  prefs.end();

  logger.info(F("PrefMgr"), F("Flower-Image-Enabled aktualisiert"));
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateFabmobilImageEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen von Display-Preferences"));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open display namespace");
  }

  prefs.putBool("show_fab", enabled);
  prefs.end();

  logger.info(F("PrefMgr"), F("Fabmobil-Image-Enabled aktualisiert"));
  return PrefResult::success();
}
