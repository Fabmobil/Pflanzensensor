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
#include "../web/handler/admin_handler.h"
#include "../web/handler/web_ota_handler.h"

ConfigManager::ConfigManager()
    : m_webHandler(*this),
      m_debugConfig(m_notifier),
      m_sensorErrorTracker(m_notifier) {}

ConfigManager& ConfigManager::getInstance() {
  static ConfigManager instance;
  return instance;
}

ConfigManager::ConfigResult ConfigManager::loadConfig() {
  ScopedLock lock;

  // Load main configuration
  auto result = ConfigPersistence::loadFromFile(m_configData);
  if (!result.isSuccess()) {
    return ConfigResult::fail(
        result.error().value_or(ConfigError::UNKNOWN_ERROR),
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
    logger.error(F("ConfigM"), F("Konfigurationsvalidierung fehlgeschlagen: ") +
                                   validationResult.getMessage());
    return ConfigResult::fail(
        validationResult.error().value_or(ConfigError::UNKNOWN_ERROR),
        validationResult.getMessage());
  }

  // Save main configuration
  auto result = ConfigPersistence::saveToFileMinimal(m_configData);
  if (!result.isSuccess()) {
    return ConfigResult::fail(
        result.error().value_or(ConfigError::UNKNOWN_ERROR),
        result.getMessage());
  }

  // Save sensor configuration
  auto sensorResult = SensorPersistence::saveToFileMinimal();
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
    return ConfigResult::fail(
        result.error().value_or(ConfigError::UNKNOWN_ERROR),
        result.getMessage());
  }

  // Sync subsystem data
  syncSubsystemData();

  notifyConfigChange("config", "reset", true);
  return saveConfig();
}

ConfigManager::ConfigResult ConfigManager::updateFromWeb(
    ESP8266WebServer& server) {
  ScopedLock lock;
  return m_webHandler.updateFromWebRequest(server);
}

ConfigManager::ConfigResult ConfigManager::setAdminPassword(
    const String& password) {
  ScopedLock lock;

  auto validation = ConfigValidator::validatePassword(password);
  if (!validation.isSuccess()) {
    return ConfigResult::fail(
        validation.error().value_or(ConfigError::UNKNOWN_ERROR),
        validation.getMessage());
  }

  m_configData.adminPassword = password;
  notifyConfigChange("admin_password", "updated", true);
  return saveConfig();
}

ConfigManager::ConfigResult ConfigManager::setMD5Verification(bool enabled) {
  ScopedLock lock;
  m_configData.md5Verification = enabled;
  notifyConfigChange("md5_verification", enabled ? "true" : "false", true);
  return saveConfig();
}

ConfigManager::ConfigResult ConfigManager::setCollectdEnabled(bool enabled) {
  ScopedLock lock;
  m_configData.collectdEnabled = enabled;
  notifyConfigChange("collectd_enabled", enabled ? "true" : "false", true);
  return saveConfig();
}

ConfigManager::ConfigResult ConfigManager::setFileLoggingEnabled(bool enabled) {
  ScopedLock lock;
  m_configData.fileLoggingEnabled = enabled;
  notifyConfigChange("file_logging_enabled", enabled ? "true" : "false", true);
  return saveConfig();
}

ConfigManager::ConfigResult ConfigManager::setUpdateFlags(bool fileSystem,
                                                          bool firmware) {
  ScopedLock lock;

  // Only one update type can be active at a time
  if (fileSystem && firmware) {
    return ConfigResult::fail(ConfigError::VALIDATION_ERROR,
                              F("Es kann jeweils nur ein Update-Typ aktiv sein"));
  }

  logger.info(F("ConfigM"),
              F("Setze Update-Flags - Dateisystem: ") + String(fileSystem) +
                  F(", Firmware: ") + String(firmware));

  ConfigPersistence::writeUpdateFlagsToFile(fileSystem, firmware);

  notifyConfigChange("update_flags",
                     "fs:" + String(fileSystem) + ",fw:" + String(firmware),
                     true);

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

ConfigManager::ConfigResult ConfigManager::setCollectdSendSingleMeasurement(
    bool enable) {
  ScopedLock lock;
  notifyConfigChange("collectd_single_measurement", enable ? "true" : "false",
                     true);
  return saveConfig();
}

ConfigManager::ConfigResult ConfigManager::setLogLevel(const String& level) {
  ScopedLock lock;

  auto validation = ConfigValidator::validateLogLevel(level);
  if (!validation.isSuccess()) {
    return ConfigResult::fail(
        validation.error().value_or(ConfigError::UNKNOWN_ERROR),
        validation.getMessage());
  }

  logger.setLogLevel(Logger::stringToLogLevel(level));
  notifyConfigChange("log_level", level, true);

  auto result = saveConfig();

  return result;
}

String ConfigManager::getLogLevel() const {
  return logger.logLevelToString(logger.getLogLevel());
}

// Debug configuration convenience methods
ConfigManager::ConfigResult ConfigManager::setDebugRAM(bool enabled) {
  auto result = m_debugConfig.setRAMDebug(enabled);
  if (result.isSuccess()) {
    return saveConfig();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugMeasurementCycle(
    bool enabled) {
  auto result = m_debugConfig.setMeasurementCycleDebug(enabled);
  if (result.isSuccess()) {
    return saveConfig();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugSensor(bool enabled) {
  auto result = m_debugConfig.setSensorDebug(enabled);
  if (result.isSuccess()) {
    return saveConfig();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugDisplay(bool enabled) {
  auto result = m_debugConfig.setDisplayDebug(enabled);
  if (result.isSuccess()) {
    return saveConfig();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setDebugWebSocket(bool enabled) {
  auto result = m_debugConfig.setWebSocketDebug(enabled);
  if (result.isSuccess()) {
    return saveConfig();
  }
  return ConfigResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                            result.getMessage());
}

ConfigManager::ConfigResult ConfigManager::setConfigValue(const char* key,
                                                          const char* value) {
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
      if (!result.isSuccess()) return result;
    } else if (keyStr == "debug_measurement_cycle") {
      auto result = setDebugMeasurementCycle(newValue);
      if (!result.isSuccess()) return result;
    } else if (keyStr == "debug_sensor") {
      auto result = setDebugSensor(newValue);
      if (!result.isSuccess()) return result;
    } else if (keyStr == "debug_display") {
      auto result = setDebugDisplay(newValue);
      if (!result.isSuccess()) return result;
    } else if (keyStr == "debug_websocket") {
      auto result = setDebugWebSocket(newValue);
      if (!result.isSuccess()) return result;
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

ConfigManager::ConfigResult ConfigManager::setDeviceName(const String& name) {
  if (m_configData.deviceName != name) {
    m_configData.deviceName = name;
    return saveConfig();
  }
  return ConfigResult::success();
}

void ConfigManager::addChangeCallback(ConfigNotifier::ChangeCallback callback) {
  m_notifier.addChangeCallback(callback);
}

void ConfigManager::notifyConfigChange(const String& key, const String& value,
                                       bool updateSensors) {


  // Delegate to notifier
  logger.debug(F("ConfigM"), String(F("Notifying config change for key: ")) + key + F(" (updateSensors=") + String(updateSensors) + F(")"));
  m_notifier.notifyChange(key, value, updateSensors);
}

void ConfigManager::syncSubsystemData() {
  // Load data into subsystems
  m_debugConfig.loadFromConfigData(m_configData);
}

#if USE_MAIL
// Mail/SMTP configuration setters
ConfigManager::ConfigResult ConfigManager::setMailEnabled(bool enabled) {
  if (m_configData.mailEnabled != enabled) {
    m_configData.mailEnabled = enabled;
    notifyConfigChange("mail_enabled", enabled ? "true" : "false", true);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpHost(const String& host) {
  if (m_configData.smtpHost != host) {
    m_configData.smtpHost = host;
    notifyConfigChange("smtp_host", host, false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpPort(uint16_t port) {
  if (m_configData.smtpPort != port) {
    m_configData.smtpPort = port;
    String portStr = String(port);
    notifyConfigChange("smtp_port", portStr, false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpUser(const String& user) {
  if (m_configData.smtpUser != user) {
    m_configData.smtpUser = user;
    notifyConfigChange("smtp_user", user, false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpPassword(const String& password) {
  if (m_configData.smtpPassword != password) {
    m_configData.smtpPassword = password;
    notifyConfigChange("smtp_password", "***", false);  // Mask password in logs
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpSenderName(const String& senderName) {
  if (m_configData.smtpSenderName != senderName) {
    m_configData.smtpSenderName = senderName;
    notifyConfigChange("smtp_sender_name", senderName, false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpSenderEmail(const String& senderEmail) {
  if (m_configData.smtpSenderEmail != senderEmail) {
    m_configData.smtpSenderEmail = senderEmail;
    notifyConfigChange("smtp_sender_email", senderEmail, false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpRecipient(const String& recipient) {
  if (m_configData.smtpRecipient != recipient) {
    m_configData.smtpRecipient = recipient;
    notifyConfigChange("smtp_recipient", recipient, false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpEnableStartTLS(bool enabled) {
  if (m_configData.smtpEnableStartTLS != enabled) {
    m_configData.smtpEnableStartTLS = enabled;
    notifyConfigChange("smtp_enable_starttls", enabled ? "true" : "false", false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpDebug(bool enabled) {
  if (m_configData.smtpDebug != enabled) {
    m_configData.smtpDebug = enabled;
    notifyConfigChange("smtp_debug", enabled ? "true" : "false", false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setSmtpSendTestMailOnBoot(bool enabled) {
  if (m_configData.smtpSendTestMailOnBoot != enabled) {
    m_configData.smtpSendTestMailOnBoot = enabled;
    notifyConfigChange("smtp_send_test_mail_on_boot", enabled ? "true" : "false", false);
    return saveConfig();
  }
  return ConfigResult::success();
}
#endif // USE_MAIL

ConfigManager::ConfigResult ConfigManager::setLedTrafficLightMode(
    uint8_t mode) {
  if (m_configData.ledTrafficLightMode != mode) {
    m_configData.ledTrafficLightMode = mode;
    String modeStr = String(mode);
    notifyConfigChange("led_traffic_light_mode", modeStr, false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult
ConfigManager::setLedTrafficLightSelectedMeasurement(
    const String& measurementId) {
  if (m_configData.ledTrafficLightSelectedMeasurement != measurementId) {
    m_configData.ledTrafficLightSelectedMeasurement = measurementId;
    notifyConfigChange("led_traffic_light_selected_measurement", measurementId,
                       false);
    return saveConfig();
  }
  return ConfigResult::success();
}

ConfigManager::ConfigResult ConfigManager::setFlowerStatusSensor(
    const String& sensorId) {
  if (m_configData.flowerStatusSensor != sensorId) {
    m_configData.flowerStatusSensor = sensorId;
    notifyConfigChange("flower_status_sensor", sensorId, false);
    return saveConfig();
  }
  return ConfigResult::success();
}
