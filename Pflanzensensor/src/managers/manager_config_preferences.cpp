/**
 * @file manager_config_preferences.cpp
 * @brief Implementation of PreferencesManager class
 */

#include "managers/manager_config_preferences.h"
#include "../configs/config_pflanzensensor.h"
#include "../logger/logger.h"

// Helper functions for type-safe access
String PreferencesManager::getString(Preferences& prefs, const char* key,
                                     const String& defaultValue) {
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

// Convenience getters that accept a namespace key
String PreferencesManager::getString(const char* namespaceKey, const char* key,
                                     const String& defaultValue) {
  Preferences prefs;
  if (!prefs.begin(namespaceKey, true)) {
    // If cannot open, return default
    return defaultValue;
  }
  String val = getString(prefs, key, defaultValue);
  prefs.end();
  return val;
}

bool PreferencesManager::getBool(const char* namespaceKey, const char* key, bool defaultValue) {
  Preferences prefs;
  if (!prefs.begin(namespaceKey, true)) {
    return defaultValue;
  }
  bool val = getBool(prefs, key, defaultValue);
  prefs.end();
  return val;
}

uint32_t PreferencesManager::getUInt(const char* namespaceKey, const char* key,
                                     uint32_t defaultValue) {
  Preferences prefs;
  if (!prefs.begin(namespaceKey, true)) {
    return defaultValue;
  }
  uint32_t val = getUInt(prefs, key, defaultValue);
  prefs.end();
  return val;
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

  // Initialize WiFi 1 namespace
  if (!prefs.begin(PreferencesNamespaces::WIFI1, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi1-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open WiFi1 namespace");
  }
  putBool(prefs, "initialized", true);
  putString(prefs, "ssid", String(WIFI_SSID_1));
  putString(prefs, "pwd", String(WIFI_PASSWORD_1));
  prefs.end();
  logger.info(F("PrefMgr"), F("WiFi1-Namespace initialisiert"));

  // Initialize WiFi 2 namespace
  if (!prefs.begin(PreferencesNamespaces::WIFI2, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi2-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open WiFi2 namespace");
  }
  putBool(prefs, "initialized", true);
  putString(prefs, "ssid", String(WIFI_SSID_2));
  putString(prefs, "pwd", String(WIFI_PASSWORD_2));
  prefs.end();
  logger.info(F("PrefMgr"), F("WiFi2-Namespace initialisiert"));

  // Initialize WiFi 3 namespace
  if (!prefs.begin(PreferencesNamespaces::WIFI3, false)) {
    logger.error(F("PrefMgr"), F("Fehler beim Öffnen des WiFi3-Namespace"));
    return PrefResult::fail(ConfigError::FILE_ERROR, "Cannot open WiFi3 namespace");
  }
  putBool(prefs, "initialized", true);
  putString(prefs, "ssid", String(WIFI_SSID_3));
  putString(prefs, "pwd", String(WIFI_PASSWORD_3));
  prefs.end();
  logger.info(F("PrefMgr"), F("WiFi3-Namespace initialisiert"));

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
  putBool(prefs, "show_qr", false);
  putUInt(prefs, "screen_dur", DISPLAY_DEFAULT_TIME * 1000);
  putString(prefs, "clock_fmt", "24h");
  putString(prefs, "sensor_disp", ""); // Sensor display settings (empty = all sensors shown)

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
    if (!result.isSuccess())
      return result;
  } else {
    logger.info(F("PrefMgr"), F("General-Namespace bereits vorhanden"));
  }

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
  const char* namespaces[] = {PreferencesNamespaces::GENERAL,     PreferencesNamespaces::WIFI1,
                              PreferencesNamespaces::WIFI2,       PreferencesNamespaces::WIFI3,
                              PreferencesNamespaces::DISP,        PreferencesNamespaces::LOG,
                              PreferencesNamespaces::LED_TRAFFIC, PreferencesNamespaces::DEBUG};

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

// ====== Atomic Update Functions (DRY) ======

// WiFi credentials require special handling (validates index and updates two keys)
PreferencesManager::PrefResult PreferencesManager::updateWiFiCredentials(uint8_t setIndex,
                                                                         const String& ssid,
                                                                         const String& password) {
  if (setIndex < 1 || setIndex > 3) {
    return PrefResult::fail(ConfigError::VALIDATION_ERROR, "Invalid WiFi set index (must be 1-3)");
  }

  // Determine which WiFi namespace to use
  const char* wifiNamespace = nullptr;
  if (setIndex == 1) {
    wifiNamespace = PreferencesNamespaces::WIFI1;
  } else if (setIndex == 2) {
    wifiNamespace = PreferencesNamespaces::WIFI2;
  } else {
    wifiNamespace = PreferencesNamespaces::WIFI3;
  }

  Preferences prefs;
  if (!prefs.begin(wifiNamespace, false)) {
    logger.error(F("PrefMgr"),
                 F("Fehler beim Öffnen des WiFi-Namespace: ") + String(wifiNamespace));
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Cannot open WiFi namespace");
  }

  if (!putString(prefs, "ssid", ssid) || !putString(prefs, "pwd", password)) {
    prefs.end();
    return PrefResult::fail(ConfigError::SAVE_FAILED, "Failed to save WiFi credentials");
  }

  prefs.end();
  return PrefResult::success();
}

// ========== DRY Generic Update Helpers ==========

PreferencesManager::PrefResult PreferencesManager::updateBoolValue(const char* namespaceKey,
                                                                   const char* key, bool value) {
  Preferences prefs;
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
  Preferences prefs;
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
  Preferences prefs;
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
  Preferences prefs;
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
