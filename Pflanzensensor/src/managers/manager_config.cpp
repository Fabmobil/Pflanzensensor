/**
 * @file manager_config.cpp
 * @brief Implementation of the simplified ConfigManager class
 */

#include "manager_config.h"

#include "../logger/logger.h"
#include "../managers/manager_config_persistence.h"
#include "../managers/manager_resource.h"
#include "../managers/manager_sensor.h"
#include "../managers/manager_sensor_persistence.h"
#include "../utils/critical_section.h"
#include "../web/handler/admin_handler.h"
#include "../web/handler/web_ota_handler.h"
#include "managers/manager_config_preferences.h"

ConfigManager::ConfigManager()
    : m_webHandler(*this), m_debugConfig(m_notifier), m_sensorErrorTracker(m_notifier) {}

ConfigManager& ConfigManager::getInstance() {
  static ConfigManager instance;
  return instance;
}

ConfigManager::ConfigResult ConfigManager::loadConfig() {
  ScopedLock lock;

  // Load main configuration
  auto result = ConfigPersistence::load(m_configData);
  if (!result.isSuccess()) {
    return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                              result.getMessage());
  }

  // Sync subsystem data
  syncSubsystemData();

  notifyConfigChange("config", "loaded", false);
  m_configLoaded = true;

  // Apply logging settings
  logger.enableFileLogging(m_configData.fileLoggingEnabled);

  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::saveConfig() {
  ScopedLock lock;

  // Sync data from subsystems back to main config
  m_debugConfig.saveToConfigData(m_configData);

  // Validate before saving
  auto validationResult = ConfigValidator::validateConfigData(m_configData);
  if (!validationResult.isSuccess()) {
    logger.error(F("ConfigM"),
                 F("Konfigurationsvalidierung fehlgeschlagen: ") + validationResult.getMessage());
    return ConfigResult::fail(validationResult.error().value_or(ConfigError::UNKNOWN_ERROR),
                              validationResult.getMessage());
  }

  // Save main configuration
  auto result = ConfigPersistence::save(m_configData);
  if (!result.isSuccess()) {
    return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                              result.getMessage());
  }

  // Save sensor configuration
  auto sensorResult = SensorPersistence::save();
  if (!sensorResult.isSuccess()) {
    logger.warning(F("ConfigM"), F("Speichern der Sensorkonfiguration fehlgeschlagen: ") +
                                     sensorResult.getMessage());
    // Continue even if sensor config save fails
  }

  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::resetToDefaults() {
  ScopedLock lock;

  auto result = ConfigPersistence::resetToDefaults(m_configData);
  if (!result.isSuccess()) {
    return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                              result.getMessage());
  }

  // Sync subsystem data
  syncSubsystemData();

  // Ensure in-memory config reflects compile-time defaults for items that
  // should be reset immediately (device name, etc.). We intentionally do
  // NOT persist these values here to avoid re-creating a config file from
  // in-memory values after the persistence layer deleted it. The Admin
  // handler will perform the reboot after rendering a confirmation page.
  m_configData.deviceName = String(DEVICE_NAME);

  notifyConfigChange("config", "reset", true);
  // Do not call saveConfig() here — that could re-write the deleted
  // config.json with current in-memory values. The caller (web UI) will
  // trigger a reboot so the device boots with empty storage and will use
  // compile-time defaults on next load.
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::updateFromWeb(ESP8266WebServer& server) {
  ScopedLock lock;
  return m_webHandler.updateFromWebRequest(server);
}

ConfigManager::ConfigResult ConfigManager::setAdminPassword(const String& password) {
  ScopedLock lock;

  auto validation = ConfigValidator::validatePassword(password);
  if (!validation.isSuccess()) {
    return ConfigResult::fail(validation.error().value_or(ConfigError::UNKNOWN_ERROR),
                              validation.getMessage());
  }

  if (m_configData.adminPassword != password) {
    m_configData.adminPassword = password;
    auto saveResult = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "admin_pwd", password);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                                saveResult.getMessage());
    }
    notifyConfigChange("admin_password", "updated", true);
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setMD5Verification(bool enabled) {
  ScopedLock lock;
  return updateBoolConfig(m_configData.md5Verification, enabled, 
                         [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "md5_verify", val); },
                         "md5_verification", true);
}

ConfigManager::ConfigResult ConfigManager::setCollectdEnabled(bool enabled) {
  ScopedLock lock;
  return updateBoolConfig(m_configData.collectdEnabled, enabled,
                         [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "collectd_en", val); },
                         "collectd_enabled", true);
}

ConfigManager::ConfigResult ConfigManager::setFileLoggingEnabled(bool enabled) {
  ScopedLock lock;
  return updateBoolConfig(m_configData.fileLoggingEnabled, enabled,
                         [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "file_log", val); },
                         "file_logging_enabled", true);
}

ConfigManager::ConfigResult ConfigManager::setUpdateFlags(bool fileSystem, bool firmware) {
  ScopedLock lock;

  // Only one update type can be active at a time
  if (fileSystem && firmware) {
    return ConfigResult::fail(ConfigError::VALIDATION_ERROR,
                              F("Es kann jeweils nur ein Update-Typ aktiv sein"));
  }

  logger.info(F("ConfigM"), F("Setze Update-Flags - Dateisystem: ") + String(fileSystem) +
                                F(", Firmware: ") + String(firmware));

  // If setting filesystem update flag, backup Preferences to file BEFORE reboot
  if (fileSystem) {
    logger.info(F("ConfigM"), F("Sichere Preferences vor Dateisystem-Update..."));
    if (!ConfigPersistence::backupPreferencesToFile()) {
      logger.warning(F("ConfigM"), F("Preferences-Sicherung fehlgeschlagen - Fortsetzen trotzdem"));
    } else {
      logger.info(F("ConfigM"), F("Preferences erfolgreich in Datei gesichert"));
    }
  }

  ConfigPersistence::writeUpdateFlagsToFile(fileSystem, firmware);

  notifyConfigChange("update_flags", "fs:" + String(fileSystem) + ",fw:" + String(firmware), true);

  return ConfigResult::success();
}

bool ConfigManager::isFileSystemUpdatePending() const {
  bool fs, fw;
  ConfigPersistence::readUpdateFlagsFromFile(fs, fw);
  return fs;
}

bool ConfigManager::isFirmwareUpdatePending() const {
  bool fs, fw;
  ConfigPersistence::readUpdateFlagsFromFile(fs, fw);
  return fw;
}

bool ConfigManager::getDoFirmwareUpgrade() {
  bool fs, fw;
  ConfigPersistence::readUpdateFlagsFromFile(fs, fw);
  // Update mode is active if either filesystem or firmware update is pending
  return fs || fw;
}

ConfigManager::ConfigResult ConfigManager::setDoFirmwareUpgrade(bool enable) {
  ScopedLock lock;
  bool fs, fw;
  ConfigPersistence::readUpdateFlagsFromFile(fs, fw);

  if (enable) {
    // If enabling update mode, set firmware update pending (default choice)
    ConfigPersistence::writeUpdateFlagsToFile(false, true);
    notifyConfigChange("do_firmware_upgrade", "true", true);
  } else {
    // If disabling update mode, clear all update flags
    ConfigPersistence::writeUpdateFlagsToFile(false, false);
    notifyConfigChange("do_firmware_upgrade", "false", true);
  }

  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setCollectdSendSingleMeasurement(bool enable) {
  ScopedLock lock;
  notifyConfigChange("collectd_single_measurement", enable ? "true" : "false", true);
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setLogLevel(const String& level) {
  ScopedLock lock;

  auto validation = ConfigValidator::validateLogLevel(level);
  if (!validation.isSuccess()) {
    return ConfigResult::fail(validation.error().value_or(ConfigError::UNKNOWN_ERROR),
                              validation.getMessage());
  }

  logger.setLogLevel(Logger::stringToLogLevel(level));
  
  // Persist to Preferences
  auto result = PreferencesManager::updateStringValue(PreferencesNamespaces::LOG, "level", level);
  if (!result.isSuccess()) {
    logger.error(F("ConfigM"), F("Failed to persist log_level: ") + result.getMessage());
    return ConfigResult::fail(ConfigError::SAVE_FAILED, result.getMessage());
  }
  
  notifyConfigChange("log_level", level, true);

  return ConfigResult::success();
}

String ConfigManager::getLogLevel() const { return logger.logLevelToString(logger.getLogLevel()); }

// Note: Debug setters moved to end of file with DRY helpers

ConfigManager::ConfigResult ConfigManager::setConfigValue(const char* key, const char* value) {
  ScopedLock lock;
  String keyStr(key);
  String valueStr(value);

  // Handle different configuration keys
  if (keyStr == "admin_password") {
    if (m_configData.adminPassword != valueStr) {
      auto result = setAdminPassword(valueStr);
      if (!result.isSuccess()) {
        return result;
      }
    }
  } else if (keyStr == "md5_verification") {
    bool newValue = (valueStr == "true" || valueStr == "1");
    if (m_configData.md5Verification != newValue) {
      auto result = setMD5Verification(newValue);
      if (!result.isSuccess()) {
        return result;
      }
    }
  } else if (keyStr == "collectd_enabled") {
    bool newValue = (valueStr == "true" || valueStr == "1");
    if (m_configData.collectdEnabled != newValue) {
      auto result = setCollectdEnabled(newValue);
      if (!result.isSuccess()) {
        return result;
      }
    }
  } else if (keyStr == "file_logging_enabled") {
    bool newValue = (valueStr == "true" || valueStr == "1");
    if (m_configData.fileLoggingEnabled != newValue) {
      auto result = setFileLoggingEnabled(newValue);
      if (!result.isSuccess()) {
        return result;
      }
    }
  } else if (keyStr.startsWith("debug_")) {
    // Handle debug flags
    bool newValue = (valueStr == "true" || valueStr == "1");
    if (keyStr == "debug_ram") {
      auto result = setDebugRAM(newValue);
      if (!result.isSuccess())
        return result;
    } else if (keyStr == "debug_measurement_cycle") {
      auto result = setDebugMeasurementCycle(newValue);
      if (!result.isSuccess())
        return result;
    } else if (keyStr == "debug_sensor") {
      auto result = setDebugSensor(newValue);
      if (!result.isSuccess())
        return result;
    } else if (keyStr == "debug_display") {
      auto result = setDebugDisplay(newValue);
      if (!result.isSuccess())
        return result;
    } else if (keyStr == "debug_websocket") {
      auto result = setDebugWebSocket(newValue);
      if (!result.isSuccess())
        return result;
    }
  } else if (keyStr == "log_level") {
    String currentLevel = getLogLevel();
    if (currentLevel != valueStr) {
      auto result = setLogLevel(valueStr);
      if (!result.isSuccess()) {
        return result;
      }
    }
  } else {
    return ConfigResult::fail(ConfigError::VALIDATION_ERROR,
                              F("Unknown configuration key: ") + keyStr);
  }

  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setConfigValue(const String& namespaceName,
                                                          const String& key, const String& value,
                                                          ConfigValueType type) {
  ScopedLock lock;

  logger.debug(F("ConfigM"), String(F("setConfigValue: namespace=")) + namespaceName + F(", key=") +
                                 key + F(", value=") + value);

  // Handle general namespace - route through existing setters for validation
  if (namespaceName == "general") {
    if (key == "device_name") {
      auto result = setDeviceName(value);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: device_name = ")) + value);
      }
      return result;
    } else if (key == "admin_pwd") {
      auto result = setAdminPassword(value);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), F("Einstellung geändert: admin_pwd = ***"));
      }
      return result;
    } else if (key == "md5_verify") {
      bool enabled = (value == "true" || value == "1");
      auto result = setMD5Verification(enabled);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: md5_verify = ")) +
                                      (enabled ? F("true") : F("false")));
      }
      return result;
    } else if (key == "file_log") {
      bool enabled = (value == "true" || value == "1");
      auto result = setFileLoggingEnabled(enabled);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: file_log = ")) +
                                      (enabled ? F("true") : F("false")));
      }
      return result;
    } else if (key == "collectd_enabled") {
      bool enabled = (value == "true" || value == "1");
      auto result = setCollectdEnabled(enabled);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: collectd_enabled = ")) +
                                      (enabled ? F("true") : F("false")));
      }
      return result;
    } else if (key == "flower_sens") {
      auto result = setFlowerStatusSensor(value);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: flower_sens = ")) + value);
      }
      return result;
    }
  }

  // Handle WiFi namespace
  else if (namespaceName == "wifi") {
    Preferences prefs;
    if (!prefs.begin(PreferencesNamespaces::WIFI, false)) {
      return ConfigResult::fail(ConfigError::FILE_ERROR, F("Failed to open WiFi namespace"));
    }

    bool success = false;
    String displayValue = value;
    if (key == "ssid1") {
      success = PreferencesManager::putString(prefs, "ssid1", value);
      if (success)
        setWiFiSSID1(value);
    } else if (key == "pwd1") {
      success = PreferencesManager::putString(prefs, "pwd1", value);
      if (success)
        setWiFiPassword1(value);
      displayValue = "***";
    } else if (key == "ssid2") {
      success = PreferencesManager::putString(prefs, "ssid2", value);
      if (success)
        setWiFiSSID2(value);
    } else if (key == "pwd2") {
      success = PreferencesManager::putString(prefs, "pwd2", value);
      if (success)
        setWiFiPassword2(value);
      displayValue = "***";
    } else if (key == "ssid3") {
      success = PreferencesManager::putString(prefs, "ssid3", value);
      if (success)
        setWiFiSSID3(value);
    } else if (key == "pwd3") {
      success = PreferencesManager::putString(prefs, "pwd3", value);
      if (success)
        setWiFiPassword3(value);
      displayValue = "***";
    }

    prefs.end();
    if (!success) {
      return ConfigResult::fail(ConfigError::SAVE_FAILED, F("Failed to save WiFi setting"));
    }
    logger.info(F("ConfigM"), String(F("Einstellung geändert: ")) + key + F(" = ") + displayValue);
    return ConfigResult::success();
  }

  // Handle display namespace
  else if (namespaceName == "display") {
    Preferences prefs;
    if (!prefs.begin(PreferencesNamespaces::DISP, false)) {
      return ConfigResult::fail(ConfigError::FILE_ERROR, F("Failed to open display namespace"));
    }

    bool success = false;
    String displayValue = value;
    if (key == "show_ip") {
      bool enabled = (value == "true" || value == "1");
      auto result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_ip", enabled);
      success = result.isSuccess();
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "show_clock") {
      bool enabled = (value == "true" || value == "1");
      auto result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_clock", enabled);
      success = result.isSuccess();
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "show_flower") {
      bool enabled = (value == "true" || value == "1");
      auto result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_flower", enabled);
      success = result.isSuccess();
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "show_fabmobil") {
      bool enabled = (value == "true" || value == "1");
      auto result = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_fabmobil", enabled);
      success = result.isSuccess();
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "screen_dur") {
      unsigned int duration = value.toInt();
      auto result = PreferencesManager::updateUIntValue(PreferencesNamespaces::DISP, "screen_dur", duration);
      success = result.isSuccess();
    } else if (key == "clock_fmt") {
      auto result = PreferencesManager::updateStringValue(PreferencesNamespaces::DISP, "clock_fmt", value);
      success = result.isSuccess();
    }

    prefs.end();
    if (!success) {
      return ConfigResult::fail(ConfigError::SAVE_FAILED, F("Failed to save display setting"));
    }
    logger.info(F("ConfigM"), String(F("Einstellung geändert: ")) + key + F(" = ") + displayValue);
    notifyConfigChange(key, value, false);
    return ConfigResult::success();
  }

  // Handle debug namespace
  else if (namespaceName == "debug") {
    bool enabled = (value == "true" || value == "1");
    ConfigResult result = ConfigResult::success();
    if (key == "ram") {
      result = setDebugRAM(enabled);
    } else if (key == "meas_cycle") {
      result = setDebugMeasurementCycle(enabled);
    } else if (key == "sensor") {
      result = setDebugSensor(enabled);
    } else if (key == "display") {
      result = setDebugDisplay(enabled);
    } else if (key == "websocket") {
      result = setDebugWebSocket(enabled);
    }
    if (result.isSuccess()) {
      logger.info(F("ConfigM"), String(F("Einstellung geändert: ")) + key + F(" = ") +
                                    (enabled ? F("true") : F("false")));
    }
    return result;
  }

  // Handle log namespace
  else if (namespaceName == "log") {
    ConfigResult result = ConfigResult::success();
    if (key == "level") {
      result = setLogLevel(value);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: log_level = ")) + value);
      }
      return result;
    } else if (key == "file_enabled") {
      bool enabled = (value == "true" || value == "1");
      result = setFileLoggingEnabled(enabled);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: file_enabled = ")) +
                                      (enabled ? F("true") : F("false")));
      }
      return result;
    }
  }

  // Handle LED traffic light namespace
  else if (namespaceName == "led_traf") {
    ConfigResult result = ConfigResult::success();
    if (key == "mode") {
      uint8_t mode = value.toInt();
      result = setLedTrafficLightMode(mode);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: led_mode = ")) + String(mode));
      }
      return result;
    } else if (key == "sel_meas") {
      result = setLedTrafficLightSelectedMeasurement(value);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: led_sel_meas = ")) + value);
      }
      return result;
    }
  }

  // Handle sensor namespaces (format: s_SENSORID)
  else if (namespaceName.startsWith("s_")) {
    Preferences prefs;
    if (!prefs.begin(namespaceName.c_str(), false)) {
      return ConfigResult::fail(ConfigError::FILE_ERROR,
                                F("Failed to open sensor namespace: ") + namespaceName);
    }

    bool success = false;
    String displayValue = value;

    // Write the value based on type
    switch (type) {
    case ConfigValueType::BOOL: {
      bool boolValue = (value == "true" || value == "1");
      success = PreferencesManager::putBool(prefs, key.c_str(), boolValue);
      displayValue = boolValue ? F("true") : F("false");
      break;
    }
    case ConfigValueType::INT: {
      int intValue = value.toInt();
      success = PreferencesManager::putInt(prefs, key.c_str(), intValue);
      break;
    }
    case ConfigValueType::UINT: {
      unsigned int uintValue = value.toInt();
      success = PreferencesManager::putUInt(prefs, key.c_str(), uintValue);
      break;
    }
    case ConfigValueType::FLOAT: {
      float floatValue = value.toFloat();
      success = PreferencesManager::putFloat(prefs, key.c_str(), floatValue);
      break;
    }
    case ConfigValueType::STRING: {
      success = PreferencesManager::putString(prefs, key.c_str(), value);
      break;
    }
    }

    prefs.end();

    if (!success) {
      return ConfigResult::fail(ConfigError::SAVE_FAILED,
                                F("Failed to save sensor setting: ") + key);
    }

    logger.info(F("ConfigM"), String(F("Einstellung geändert: ")) + namespaceName + F(".") + key +
                                  F(" = ") + displayValue);
    notifyConfigChange(key, value, true);
    return ConfigResult::success();
  }

  return ConfigResult::fail(ConfigError::VALIDATION_ERROR,
                            F("Unknown namespace or key: ") + namespaceName + F(".") + key);
}



void ConfigManager::addChangeCallback(ConfigNotifier::ChangeCallback callback) {
  m_notifier.addChangeCallback(callback);
}

void ConfigManager::notifyConfigChange(const String& key, const String& value, bool updateSensors) {

  // Delegate to notifier
  logger.debug(F("ConfigM"), String(F("Notifying config change for key: ")) + key +
                                 F(" (updateSensors=") + String(updateSensors) + F(")"));
  m_notifier.notifyChange(key, value, updateSensors);
}

// ====== Generic DRY Helper Methods ======

ConfigManager::ConfigResult ConfigManager::updateBoolConfig(
    bool& currentValue, bool newValue, BoolUpdateFunc updateFunc,
    const String& notifyKey, bool updateSensors) {
  
  if (currentValue != newValue) {
    currentValue = newValue;
    
    // Persist atomically to Preferences
    auto saveResult = updateFunc(newValue);
    if (!saveResult.isSuccess()) {
      logger.error(F("ConfigM"), String(F("Failed to persist ")) + notifyKey + F(": ") + saveResult.getMessage());
      return ConfigResult::fail(ConfigError::SAVE_FAILED, saveResult.getMessage());
    }
    
    notifyConfigChange(notifyKey, newValue ? "true" : "false", updateSensors);
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::updateStringConfig(
    String& currentValue, const String& newValue, StringUpdateFunc updateFunc,
    const String& notifyKey, bool updateSensors) {
  
  if (currentValue != newValue) {
    currentValue = newValue;
    
    // Persist atomically to Preferences
    auto saveResult = updateFunc(newValue);
    if (!saveResult.isSuccess()) {
      logger.error(F("ConfigM"), String(F("Failed to persist ")) + notifyKey + F(": ") + saveResult.getMessage());
      return ConfigResult::fail(ConfigError::SAVE_FAILED, saveResult.getMessage());
    }
    
    notifyConfigChange(notifyKey, newValue, updateSensors);
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::updateUInt8Config(
    uint8_t& currentValue, uint8_t newValue, UInt8UpdateFunc updateFunc,
    const String& notifyKey, bool updateSensors) {
  
  if (currentValue != newValue) {
    currentValue = newValue;
    
    // Persist atomically to Preferences
    auto saveResult = updateFunc(newValue);
    if (!saveResult.isSuccess()) {
      logger.error(F("ConfigM"), String(F("Failed to persist ")) + notifyKey + F(": ") + saveResult.getMessage());
      return ConfigResult::fail(ConfigError::SAVE_FAILED, saveResult.getMessage());
    }
    
    notifyConfigChange(notifyKey, String(newValue), updateSensors);
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::updateDebugConfig(
    bool enabled, DebugSetFunc debugSetFunc, BoolUpdateFunc updateFunc) {
  
  auto result = (m_debugConfig.*debugSetFunc)(enabled);
  if (!result.isSuccess()) {
    return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                             result.getMessage());
  }
  
  // Persist atomically to Preferences
  auto saveResult = updateFunc(enabled);
  if (!saveResult.isSuccess()) {
    return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                             saveResult.getMessage());
  }
  
  return ConfigResult::success();
}

void ConfigManager::syncSubsystemData() {
  // Load data into subsystems
  m_debugConfig.loadFromConfigData(m_configData);
}

// ====== Simplified Setters Using DRY Helpers ======

ConfigManager::ConfigResult ConfigManager::setDebugRAM(bool enabled) {
  return updateDebugConfig(enabled, &DebugConfig::setRAMDebug, 
                          [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "ram", val); });
}

ConfigManager::ConfigResult ConfigManager::setDebugMeasurementCycle(bool enabled) {
  return updateDebugConfig(enabled, &DebugConfig::setMeasurementCycleDebug,
                          [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "meas_cycle", val); });
}

ConfigManager::ConfigResult ConfigManager::setDebugSensor(bool enabled) {
  return updateDebugConfig(enabled, &DebugConfig::setSensorDebug,
                          [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "sensor", val); });
}

ConfigManager::ConfigResult ConfigManager::setDebugDisplay(bool enabled) {
  return updateDebugConfig(enabled, &DebugConfig::setDisplayDebug,
                          [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "display", val); });
}

ConfigManager::ConfigResult ConfigManager::setDebugWebSocket(bool enabled) {
  return updateDebugConfig(enabled, &DebugConfig::setWebSocketDebug,
                          [](bool val) { return PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "websocket", val); });
}

ConfigManager::ConfigResult ConfigManager::setDeviceName(const String& name) {
  return updateStringConfig(m_configData.deviceName, name,
                           [](const String& val) { return PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "device_name", val); },
                           "device_name", false);
}

ConfigManager::ConfigResult ConfigManager::setFlowerStatusSensor(const String& sensorId) {
  return updateStringConfig(m_configData.flowerStatusSensor, sensorId,
                           [](const String& val) { return PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "flower_sens", val); },
                           "flower_status_sensor", false);
}

ConfigManager::ConfigResult ConfigManager::setLedTrafficLightMode(uint8_t mode) {
  return updateUInt8Config(m_configData.ledTrafficLightMode, mode,
                          [](uint8_t val) { return PreferencesManager::updateUInt8Value(PreferencesNamespaces::LED_TRAFFIC, "mode", val); },
                          "led_traffic_light_mode", false);
}

ConfigManager::ConfigResult ConfigManager::setLedTrafficLightSelectedMeasurement(const String& measurementId) {
  return updateStringConfig(m_configData.ledTrafficLightSelectedMeasurement, measurementId,
                           [](const String& val) { return PreferencesManager::updateStringValue(PreferencesNamespaces::LED_TRAFFIC, "sel_meas", val); },
                           "led_traffic_light_selected_measurement", false);
}

// ====== Simplified Setters Using DRY Helpers (defined at end of file) ======
