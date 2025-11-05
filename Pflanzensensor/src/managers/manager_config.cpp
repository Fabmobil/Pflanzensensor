/**
 * @file manager_config.cpp
 * @brief Implementation of the simplified ConfigManager class
 */

#include "manager_config.h"

#include "../logger/logger.h"
#include "../managers/manager_resource.h"
#include "../managers/manager_sensor.h"
#include "../managers/manager_sensor_persistence.h"
#include "../utils/critical_section.h"
#include "managers/manager_config_preferences.h"
#include "../web/handler/admin_handler.h"
#include "../web/handler/web_ota_handler.h"

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
    auto saveResult = PreferencesManager::updateAdminPassword(password);
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
  if (m_configData.md5Verification != enabled) {
    m_configData.md5Verification = enabled;
    auto saveResult = PreferencesManager::updateMD5Verification(enabled);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    notifyConfigChange("md5_verification", enabled ? "true" : "false", true);
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setCollectdEnabled(bool enabled) {
  ScopedLock lock;
  m_configData.collectdEnabled = enabled;
  notifyConfigChange("collectd_enabled", enabled ? "true" : "false", true);
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setFileLoggingEnabled(bool enabled) {
  ScopedLock lock;
  if (m_configData.fileLoggingEnabled != enabled) {
    m_configData.fileLoggingEnabled = enabled;
    auto saveResult = PreferencesManager::updateFileLoggingEnabled(enabled);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    notifyConfigChange("file_logging_enabled", enabled ? "true" : "false", true);
  }
  return ConfigResult::success();
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
  notifyConfigChange("log_level", level, true);

  // Persist intentionally left to the caller to batch writes
  return ConfigResult::success();
}

String ConfigManager::getLogLevel() const { return logger.logLevelToString(logger.getLogLevel()); }

// Debug configuration convenience methods
ConfigManager::ConfigResult ConfigManager::setDebugRAM(bool enabled) {
  auto result = m_debugConfig.setRAMDebug(enabled);
  if (result.isSuccess()) {
    // Save atomically to Preferences
    auto saveResult = PreferencesManager::updateDebugRAM(enabled);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    return ConfigResult::success();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugMeasurementCycle(bool enabled) {
  auto result = m_debugConfig.setMeasurementCycleDebug(enabled);
  if (result.isSuccess()) {
    auto saveResult = PreferencesManager::updateDebugMeasurementCycle(enabled);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    return ConfigResult::success();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugSensor(bool enabled) {
  auto result = m_debugConfig.setSensorDebug(enabled);
  if (result.isSuccess()) {
    auto saveResult = PreferencesManager::updateDebugSensor(enabled);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    return ConfigResult::success();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugDisplay(bool enabled) {
  auto result = m_debugConfig.setDisplayDebug(enabled);
  if (result.isSuccess()) {
    auto saveResult = PreferencesManager::updateDebugDisplay(enabled);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    return ConfigResult::success();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugWebSocket(bool enabled) {
  auto result = m_debugConfig.setWebSocketDebug(enabled);
  if (result.isSuccess()) {
    auto saveResult = PreferencesManager::updateDebugWebSocket(enabled);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    return ConfigResult::success();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

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
                                                          const String& key, 
                                                          const String& value, 
                                                          ConfigValueType type) {
  ScopedLock lock;
  
  logger.debug(F("ConfigM"), String(F("setConfigValue: namespace=")) + namespaceName + 
               F(", key=") + key + F(", value=") + value);
  
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
        logger.info(F("ConfigM"), String(F("Einstellung geändert: md5_verify = ")) + (enabled ? F("true") : F("false")));
      }
      return result;
    } else if (key == "file_log") {
      bool enabled = (value == "true" || value == "1");
      auto result = setFileLoggingEnabled(enabled);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: file_log = ")) + (enabled ? F("true") : F("false")));
      }
      return result;
    } else if (key == "collectd_enabled") {
      bool enabled = (value == "true" || value == "1");
      auto result = setCollectdEnabled(enabled);
      if (result.isSuccess()) {
        logger.info(F("ConfigM"), String(F("Einstellung geändert: collectd_enabled = ")) + (enabled ? F("true") : F("false")));
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
      if (success) setWiFiSSID1(value);
    } else if (key == "pwd1") {
      success = PreferencesManager::putString(prefs, "pwd1", value);
      if (success) setWiFiPassword1(value);
      displayValue = "***";
    } else if (key == "ssid2") {
      success = PreferencesManager::putString(prefs, "ssid2", value);
      if (success) setWiFiSSID2(value);
    } else if (key == "pwd2") {
      success = PreferencesManager::putString(prefs, "pwd2", value);
      if (success) setWiFiPassword2(value);
      displayValue = "***";
    } else if (key == "ssid3") {
      success = PreferencesManager::putString(prefs, "ssid3", value);
      if (success) setWiFiSSID3(value);
    } else if (key == "pwd3") {
      success = PreferencesManager::putString(prefs, "pwd3", value);
      if (success) setWiFiPassword3(value);
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
      success = PreferencesManager::putBool(prefs, "show_ip", enabled);
      auto result = PreferencesManager::updateIpScreenEnabled(enabled);
      if (!result.isSuccess()) success = false;
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "show_clock") {
      bool enabled = (value == "true" || value == "1");
      success = PreferencesManager::putBool(prefs, "show_clock", enabled);
      auto result = PreferencesManager::updateClockEnabled(enabled);
      if (!result.isSuccess()) success = false;
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "show_flower") {
      bool enabled = (value == "true" || value == "1");
      success = PreferencesManager::putBool(prefs, "show_flower", enabled);
      auto result = PreferencesManager::updateFlowerImageEnabled(enabled);
      if (!result.isSuccess()) success = false;
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "show_fabmobil") {
      bool enabled = (value == "true" || value == "1");
      success = PreferencesManager::putBool(prefs, "show_fabmobil", enabled);
      auto result = PreferencesManager::updateFabmobilImageEnabled(enabled);
      if (!result.isSuccess()) success = false;
      displayValue = enabled ? F("true") : F("false");
    } else if (key == "screen_dur") {
      unsigned int duration = value.toInt();
      success = PreferencesManager::putUInt(prefs, "screen_dur", duration);
      auto result = PreferencesManager::updateScreenDuration(duration);
      if (!result.isSuccess()) success = false;
    } else if (key == "clock_fmt") {
      success = PreferencesManager::putString(prefs, "clock_fmt", value);
      auto result = PreferencesManager::updateClockFormat(value);
      if (!result.isSuccess()) success = false;
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
    ConfigResult result;
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
      logger.info(F("ConfigM"), String(F("Einstellung geändert: ")) + key + F(" = ") + (enabled ? F("true") : F("false")));
    }
    return result;
  }
  
  // Handle log namespace
  else if (namespaceName == "log") {
    ConfigResult result;
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
        logger.info(F("ConfigM"), String(F("Einstellung geändert: file_enabled = ")) + (enabled ? F("true") : F("false")));
      }
      return result;
    }
  }
  
  // Handle LED traffic light namespace
  else if (namespaceName == "led_traf") {
    ConfigResult result;
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
    
    logger.info(F("ConfigM"), String(F("Einstellung geändert: ")) + namespaceName + F(".") + key + F(" = ") + displayValue);
    notifyConfigChange(key, value, true);
    return ConfigResult::success();
  }
  
  return ConfigResult::fail(ConfigError::VALIDATION_ERROR, 
                           F("Unknown namespace or key: ") + namespaceName + F(".") + key);
}

ConfigManager::ConfigResult ConfigManager::setDeviceName(const String& name) {
  if (m_configData.deviceName != name) {
    m_configData.deviceName = name;
    notifyConfigChange("device_name", name, false);
    return ConfigResult::success();
  }
  return ConfigResult::success();
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

void ConfigManager::syncSubsystemData() {
  // Load data into subsystems
  m_debugConfig.loadFromConfigData(m_configData);
}

ConfigManager::ConfigResult ConfigManager::setLedTrafficLightMode(uint8_t mode) {
  if (m_configData.ledTrafficLightMode != mode) {
    m_configData.ledTrafficLightMode = mode;
    auto saveResult = PreferencesManager::updateLedTrafficMode(mode);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    String modeStr = String(mode);
    notifyConfigChange("led_traffic_light_mode", modeStr, false);
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult
ConfigManager::setLedTrafficLightSelectedMeasurement(const String& measurementId) {
  if (m_configData.ledTrafficLightSelectedMeasurement != measurementId) {
    m_configData.ledTrafficLightSelectedMeasurement = measurementId;
    auto saveResult = PreferencesManager::updateLedTrafficMeasurement(measurementId);
    if (!saveResult.isSuccess()) {
      return ConfigResult::fail(saveResult.error().value_or(ConfigError::SAVE_FAILED),
                               saveResult.getMessage());
    }
    notifyConfigChange("led_traffic_light_selected_measurement", measurementId, false);
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setFlowerStatusSensor(const String& sensorId) {
  if (m_configData.flowerStatusSensor != sensorId) {
    m_configData.flowerStatusSensor = sensorId;
    notifyConfigChange("flower_status_sensor", sensorId, false);
    return ConfigResult::success();
  }
  return ConfigResult::success();
}
