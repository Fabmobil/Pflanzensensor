/**
 * @file manager_config_preferences.cpp
 * @brief Implementation of PreferencesManager class
 */

#include "managers/manager_config_preferences.h"
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
} // namespace PreferencesNamespaces

// Helper functions for type-safe access
String PreferencesManager::getString(PreferencesEEPROM& prefs, const char* key,
                                     const String& defaultValue) {
  return prefs.getString(key, defaultValue);
}

bool PreferencesManager::getBool(PreferencesEEPROM& prefs, const char* key, bool defaultValue) {
  return prefs.getBool(key, defaultValue);
}

uint8_t PreferencesManager::getUChar(PreferencesEEPROM& prefs, const char* key,
                                     uint8_t defaultValue) {
  return prefs.getUChar(key, defaultValue);
}

uint32_t PreferencesManager::getUInt(PreferencesEEPROM& prefs, const char* key,
                                     uint32_t defaultValue) {
  return prefs.getUInt(key, defaultValue);
}

int PreferencesManager::getInt(PreferencesEEPROM& prefs, const char* key, int defaultValue) {
  return prefs.getInt(key, defaultValue);
}

float PreferencesManager::getFloat(PreferencesEEPROM& prefs, const char* key, float defaultValue) {
  return prefs.getFloat(key, defaultValue);
}

// Convenience getters that accept a namespace key
String PreferencesManager::getString(const char* namespaceKey, const char* key,
                                     const String& defaultValue) {
  PreferencesEEPROM prefs;
  if (!prefs.begin(namespaceKey, true)) {
    // If cannot open, return default
    return defaultValue;
  }
  String val = getString(prefs, key, defaultValue);
  prefs.end();
  return val;
}

bool PreferencesManager::getBool(const char* namespaceKey, const char* key, bool defaultValue) {
  PreferencesEEPROM prefs;
  if (!prefs.begin(namespaceKey, true)) {
    return defaultValue;
  }
  bool val = getBool(prefs, key, defaultValue);
  prefs.end();
  return val;
}

uint32_t PreferencesManager::getUInt(const char* namespaceKey, const char* key,
                                     uint32_t defaultValue) {
  PreferencesEEPROM prefs;
  if (!prefs.begin(namespaceKey, true)) {
    return defaultValue;
  }
  uint32_t val = getUInt(prefs, key, defaultValue);
  prefs.end();
  return val;
}

bool PreferencesManager::putString(PreferencesEEPROM& prefs, const char* key, const String& value) {
  return prefs.putString(key, value) > 0;
}

bool PreferencesManager::putBool(PreferencesEEPROM& prefs, const char* key, bool value) {
  return prefs.putBool(key, value) > 0;
}

bool PreferencesManager::putUChar(PreferencesEEPROM& prefs, const char* key, uint8_t value) {
  return prefs.putUChar(key, value) > 0;
}

bool PreferencesManager::putUInt(PreferencesEEPROM& prefs, const char* key, uint32_t value) {
  return prefs.putUInt(key, value) > 0;
}

bool PreferencesManager::putInt(PreferencesEEPROM& prefs, const char* key, int value) {
  return prefs.putInt(key, value) > 0;
}

bool PreferencesManager::putFloat(PreferencesEEPROM& prefs, const char* key, float value) {
  return prefs.putFloat(key, value) > 0;
}

// Check if namespace exists
bool PreferencesManager::namespaceExists(const char* namespaceName) {
  PreferencesEEPROM prefs;
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
  PreferencesEEPROM prefs;
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

// Initialize WiFi namespaces with defaults - now uses separate namespace for each WiFi set
PreferencesManager::PrefResult PreferencesManager::initWiFiNamespace() {
  // Initialize WiFi1
  PreferencesEEPROM prefs1;
  if (!prefs1.begin(PreferencesNamespaces::WIFI1, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi1-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open WiFi1 namespace");
  }
  putBool(prefs1, "initialized", true);
  putString(prefs1, "ssid", String(WIFI_SSID_1));
  putString(prefs1, "pwd", String(WIFI_PASSWORD_1));
  prefs1.end();
  logger.info(F("PrefMgr"), F("WiFi1-Namespace initialisiert"));

  // Initialize WiFi2
  PreferencesEEPROM prefs2;
  if (!prefs2.begin(PreferencesNamespaces::WIFI2, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi2-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open WiFi2 namespace");
  }
  putBool(prefs2, "initialized", true);
  putString(prefs2, "ssid", String(WIFI_SSID_2));
  putString(prefs2, "pwd", String(WIFI_PASSWORD_2));
  prefs2.end();
  logger.info(F("PrefMgr"), F("WiFi2-Namespace initialisiert"));

  // Initialize WiFi3
  PreferencesEEPROM prefs3;
  if (!prefs3.begin(PreferencesNamespaces::WIFI3, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi3-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open WiFi3 namespace");
  }
  putBool(prefs3, "initialized", true);
  putString(prefs3, "ssid", String(WIFI_SSID_3));
  putString(prefs3, "pwd", String(WIFI_PASSWORD_3));
  prefs3.end();
  logger.info(F("PrefMgr"), F("WiFi3-Namespace initialisiert"));

  return PrefResult::success();
}

// Initialize Display namespace with defaults
PreferencesManager::PrefResult PreferencesManager::initDisplayNamespace() {
  PreferencesEEPROM prefs;
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
  PreferencesEEPROM prefs;
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
  PreferencesEEPROM prefs;
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
  PreferencesEEPROM prefs;
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
    if (!result.isSuccess())
      return result;
  } else {
    logger.info(F("PrefMgr"), F("General-Namespace bereits vorhanden"));
  }

  // Check and initialize WiFi namespaces (wifi1, wifi2, wifi3)
  if (!namespaceExists(PreferencesNamespaces::WIFI1) || 
      !namespaceExists(PreferencesNamespaces::WIFI2) || 
      !namespaceExists(PreferencesNamespaces::WIFI3)) {
    auto result = initWiFiNamespace();
    if (!result.isSuccess())
      return result;
  } else {
    logger.info(F("PrefMgr"), F("WiFi-Namespaces bereits vorhanden"));
  }

  if (!namespaceExists(PreferencesNamespaces::DISP)) {
    auto result = initDisplayNamespace();
    if (!result.isSuccess())
      return result;
  } else {
    logger.info(F("PrefMgr"), F("Display-Namespace bereits vorhanden"));
  }

  if (!namespaceExists(PreferencesNamespaces::LOG)) {
    auto result = initLogNamespace();
    if (!result.isSuccess())
      return result;
  } else {
    logger.info(F("PrefMgr"), F("Log-Namespace bereits vorhanden"));
  }

  if (!namespaceExists(PreferencesNamespaces::LED_TRAFFIC)) {
    auto result = initLedTrafficNamespace();
    if (!result.isSuccess())
      return result;
  } else {
    logger.info(F("PrefMgr"), F("LED-Traffic-Namespace bereits vorhanden"));
  }

  if (!namespaceExists(PreferencesNamespaces::DEBUG)) {
    auto result = initDebugNamespace();
    if (!result.isSuccess())
      return result;
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
  const char* namespaces[] = {PreferencesNamespaces::GENERAL,     PreferencesNamespaces::WIFI,
                              PreferencesNamespaces::DISP,        PreferencesNamespaces::LOG,
                              PreferencesNamespaces::LED_TRAFFIC, PreferencesNamespaces::DEBUG};

  for (const char* ns : namespaces) {
    PreferencesEEPROM prefs;
    if (prefs.begin(ns, false)) {
      prefs.clear();
      prefs.end();
      logger.info(F("PrefMgr"), String(F("Namespace gelöscht: ")) + String(ns));
    }
  }

  logger.info(F("PrefMgr"), F("Factory Reset abgeschlossen"));
  return PrefResult::success();
}

// Sensor settings
// Sensor settings
PreferencesManager::PrefResult
PreferencesManager::saveSensorSettings(const String& sensorId, const String& name,
                                       unsigned long measurementInterval, bool hasPersistentError) {

  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  PreferencesEEPROM prefs;
  if (!prefs.begin(ns.c_str(), false)) {
    logger.error(F("PrefMgr"),
                 String(F("Fehler beim Speichern der Sensor-Einstellungen für ")) + sensorId);
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

PreferencesManager::PrefResult
PreferencesManager::loadSensorSettings(const String& sensorId, String& name,
                                       unsigned long& measurementInterval,
                                       bool& hasPersistentError) {

  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  PreferencesEEPROM prefs;
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
    const String& sensorId, uint8_t measurementIndex, bool enabled, const String& name,
    const String& fieldName, const String& unit, float minValue, float maxValue, float yellowLow,
    float greenLow, float greenHigh, float yellowHigh, bool inverted, bool calibrationMode,
    uint32_t autocalDuration, int absoluteRawMin, int absoluteRawMax) {

  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  PreferencesEEPROM prefs;
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
    const String& sensorId, uint8_t measurementIndex, bool& enabled, String& name,
    String& fieldName, String& unit, float& minValue, float& maxValue, float& yellowLow,
    float& greenLow, float& greenHigh, float& yellowHigh, bool& inverted, bool& calibrationMode,
    uint32_t& autocalDuration, int& absoluteRawMin, int& absoluteRawMax) {

  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  PreferencesEEPROM prefs;
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
  PreferencesEEPROM prefs;
  if (prefs.begin(ns.c_str(), false)) {
    prefs.clear();
    prefs.end();
    logger.info(F("PrefMgr"), String(F("Sensor-Namespace gelöscht: ")) + sensorId);
  }
  return PrefResult::success();
}

// ====== Atomic Update Functions (DRY) ======

// WiFi credentials require special handling (validates index and updates two keys)
PreferencesManager::PrefResult PreferencesManager::updateWiFiCredentials(uint8_t setIndex,
                                                                         const String& ssid,
                                                                         const String& password) {
  if (setIndex < 1 || setIndex > 3) {
    return PrefResult::fail(ConfigError::VALIDATION_ERROR, "Invalid WiFi set index (must be 1-3)");
  }

  // Use separate namespace for each WiFi set
  const char* wifiNamespace = nullptr;
  if (setIndex == 1) wifiNamespace = PreferencesNamespaces::WIFI1;
  else if (setIndex == 2) wifiNamespace = PreferencesNamespaces::WIFI2;
  else if (setIndex == 3) wifiNamespace = PreferencesNamespaces::WIFI3;

  PreferencesEEPROM prefs;
  if (!prefs.begin(wifiNamespace, false)) {
    logger.error(F("PrefMgr"), String(F("Fehler beim Öffnen des WiFi-Namespace: ")) + wifiNamespace);
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open WiFi namespace");
  }

  // Use simple keys "ssid" and "pwd" since each WiFi set has its own namespace
  if (!putString(prefs, "ssid", ssid) || !putString(prefs, "pwd", password)) {
    prefs.end();
    logger.error(F("PrefMgr"), String(F("Failed to save WiFi credentials to ")) + wifiNamespace);
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save WiFi credentials");
  }

  prefs.end();
  logger.debug(F("PrefMgr"), String(F("WiFi credentials saved to ")) + wifiNamespace);
  return PrefResult::success();
}

// ========== DRY Generic Update Helpers ==========

PreferencesManager::PrefResult PreferencesManager::updateBoolValue(const char* namespaceKey,
                                                                   const char* key, bool value) {
  PreferencesEEPROM prefs;
  if (!prefs.begin(namespaceKey, false)) {
    logger.error(F("PrefMgr"), String(F("Fehler beim Öffnen des Namespace: ")) + namespaceKey);
    return PrefResult::fail(ConfigError::SAVE_FAILED,
                            String("Cannot open namespace: ") + namespaceKey);
  }

  if (!putBool(prefs, key, value)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, String("Failed to save ") + key);
  }

  prefs.end();
  return PrefResult::success();
}

PreferencesManager::PrefResult PreferencesManager::updateStringValue(const char* namespaceKey,
                                                                     const char* key,
                                                                     const String& value) {
  PreferencesEEPROM prefs;
  if (!prefs.begin(namespaceKey, false)) {
    logger.error(F("PrefMgr"), String(F("Fehler beim Öffnen des Namespace: ")) + namespaceKey);
    return PrefResult::fail(ConfigError::SAVE_FAILED,
                            String("Cannot open namespace: ") + namespaceKey);
  }

  if (!putString(prefs, key, value)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, String("Failed to save ") + key);
  }

  prefs.end();
  return PrefResult::success();
}

PreferencesManager::PrefResult
PreferencesManager::updateUInt8Value(const char* namespaceKey, const char* key, uint8_t value) {
  PreferencesEEPROM prefs;
  if (!prefs.begin(namespaceKey, false)) {
    logger.error(F("PrefMgr"), String(F("Fehler beim Öffnen des Namespace: ")) + namespaceKey);
    return PrefResult::fail(ConfigError::SAVE_FAILED,
                            String("Cannot open namespace: ") + namespaceKey);
  }

  if (!putUChar(prefs, key, value)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, String("Failed to save ") + key);
  }

  prefs.end();
  return PrefResult::success();
}

PreferencesManager::PrefResult
PreferencesManager::updateUIntValue(const char* namespaceKey, const char* key, unsigned int value) {
  PreferencesEEPROM prefs;
  if (!prefs.begin(namespaceKey, false)) {
    logger.error(F("PrefMgr"), String(F("Fehler beim Öffnen des Namespace: ")) + namespaceKey);
    return PrefResult::fail(ConfigError::SAVE_FAILED,
                            String("Cannot open namespace: ") + namespaceKey);
  }

  if (!putUInt(prefs, key, value)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, String("Failed to save ") + key);
  }

  prefs.end();
  return PrefResult::success();
}
