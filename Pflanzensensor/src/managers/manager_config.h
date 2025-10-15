/**
 * @file manager_config.h
 * @brief Main configuration manager - coordinates all config subsystems
 */

#ifndef MANAGER_CONFIG_H
#define MANAGER_CONFIG_H

#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>

#include "../configs/config_pflanzensensor.h"
#include "../utils/critical_section.h"
#include "../utils/result_types.h"
#include "../web/handler/web_ota_handler.h"
#include "manager_config_debug.h"
#include "manager_config_notifier.h"
#include "manager_config_persistence.h"
#include "manager_config_sensor_tracker.h"
#include "manager_config_types.h"
#include "manager_config_validator.h"
#include "manager_config_web_handler.h"

// Forward declarations
class ResourceManager;
class SensorManager;
class WebOTAHandler;

/**
 * @class ConfigManager
 * @brief Main configuration manager - coordinates all config subsystems
 *
 * This class serves as the main interface for configuration management,
 * coordinating various specialized subsystems for validation, persistence,
 * web handling, notifications, debug settings, and sensor tracking.
 */
class ConfigManager {
 public:
  using ConfigResult = TypedResult<ConfigError, void>;

  /**
   * @brief Get the singleton instance of ConfigManager
   * @return Reference to the ConfigManager instance
   */
  static ConfigManager& getInstance();

  // Core operations
  /**
   * @brief Load configuration from persistent storage
   * @return Result of the load operation
   */
  ConfigResult loadConfig();

  /**
   * @brief Save the current configuration to persistent storage
   * @return Result of the save operation
   */
  ConfigResult saveConfig();

  /**
   * @brief Reset configuration to default values
   * @return Result of the reset operation
   */
  ConfigResult resetToDefaults();

  // Web interface
  /**
   * @brief Update configuration from web server
   * @param server Reference to the ESP8266 web server
   * @return Result of the update operation
   */
  ConfigResult updateFromWeb(ESP8266WebServer& server);

  // Main configuration setters
  /**
   * @brief Set the admin password
   * @param password New admin password
   * @return Result of the set operation
   */
  ConfigResult setAdminPassword(const String& password);

  /**
   * @brief Set the MD5 verification status
   * @param enabled True to enable MD5 verification, false to disable
   * @return Result of the set operation
   */
  ConfigResult setMD5Verification(bool enabled);

  /**
   * @brief Set the Collectd status
   * @param enabled True to enable Collectd, false to disable
   * @return Result of the set operation
   */
  ConfigResult setCollectdEnabled(bool enabled);

  /**
   * @brief Set the file logging status
   * @param enabled True to enable file logging, false to disable
   * @return Result of the set operation
   */
  ConfigResult setFileLoggingEnabled(bool enabled);

  /**
   * @brief Set the Collectd single measurement sending status
   * @param enable True to enable sending single measurements, false to disable
   * @return Result of the set operation
   */
  ConfigResult setCollectdSendSingleMeasurement(bool enable);

  /**
   * @brief Set a configuration value by key
   * @param key The configuration key to set
   * @param value The value to set
   * @return Result of the set operation
   */
  ConfigResult setConfigValue(const char* key, const char* value);

  // Main configuration getters
  /**
   * @brief Get the current admin password
   * @return The current admin password string
   */
  inline String getAdminPassword() const { return m_configData.adminPassword; }

  /**
   * @brief Check if MD5 verification is enabled
   * @return True if MD5 verification is enabled, false otherwise
   */
  inline bool isMD5Verification() const { return m_configData.md5Verification; }

  /**
   * @brief Check if Collectd is enabled
   * @return True if Collectd is enabled, false otherwise
   */
  inline bool isCollectdEnabled() const { return m_configData.collectdEnabled; }

  /**
   * @brief Check if file logging is enabled
   * @return True if file logging is enabled, false otherwise
   */
  inline bool isFileLoggingEnabled() const {
    return m_configData.fileLoggingEnabled;
  }

  /**
   * @brief Check if a firmware upgrade is scheduled
   * @return True if a firmware upgrade is pending
   */
  bool getDoFirmwareUpgrade();

  /**
   * @brief Set the firmware upgrade status
   * @param enable True to enable firmware upgrade, false to disable
   * @return Result of the set operation
   */
  ConfigResult setDoFirmwareUpgrade(bool enable);

  /**
   * @brief Check if a filesystem update is pending
   * @return True if a filesystem update is pending, false otherwise
   */
  bool isFileSystemUpdatePending() const;

  /**
   * @brief Check if a firmware update is pending
   * @return True if a firmware update is pending, false otherwise
   */
  bool isFirmwareUpdatePending() const;

  /**
   * @brief Set or clear flags for pending updates
   * @param fileSystem True to mark filesystem update as pending
   * @param firmware True to mark firmware update as pending
   * @return Result of the update operation
   */
  ConfigResult setUpdateFlags(bool fileSystem, bool firmware);

  /**
   * @brief Set the filesystem update pending status
   * @param pending True to mark the filesystem update as pending, false
   * otherwise
   */
  void setFileSystemUpdatePending(bool pending);

  // Logging
  /**
   * @brief Set the log level
   * @param level The log level to set
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setLogLevel(const String& level);

  /**
   * @brief Get the log level
   * @return The current log level
   */
  String getLogLevel() const;

  // Subsystem access
  /**
   * @brief Get access to debug configuration
   * @return Reference to DebugConfig instance
   */
  inline DebugConfig& getDebugConfig() { return m_debugConfig; }

  /**
   * @brief Get access to debug configuration (const)
   * @return Const reference to DebugConfig instance
   */
  inline const DebugConfig& getDebugConfig() const { return m_debugConfig; }

  /**
   * @brief Get access to sensor error tracker
   * @return Reference to SensorErrorTracker instance
   */
  inline SensorErrorTracker& getSensorErrorTracker() {
    return m_sensorErrorTracker;
  }

  /**
   * @brief Get access to sensor error tracker (const)
   * @return Const reference to SensorErrorTracker instance
   */
  inline const SensorErrorTracker& getSensorErrorTracker() const {
    return m_sensorErrorTracker;
  }

  // Convenience methods for backward compatibility
  /**
   * @brief Check if RAM debugging is enabled
   * @return True if RAM debugging is enabled, false otherwise
   */
  inline bool isDebugRAM() const { return m_debugConfig.isRAMDebugEnabled(); }

  /**
   * @brief Check if measurement cycle debugging is enabled
   * @return True if measurement cycle debugging is enabled, false otherwise
   */
  bool isDebugMeasurementCycle() const {
    return m_debugConfig.isMeasurementCycleDebugEnabled();
  }

  /**
   * @brief Check if sensor debugging is enabled
   * @return True if sensor debugging is enabled, false otherwise
   */
  bool isDebugSensor() const { return m_debugConfig.isSensorDebugEnabled(); }

  /**
   * @brief Check if display debugging is enabled
   * @return True if display debugging is enabled, false otherwise
   */
  bool isDebugDisplay() const { return m_debugConfig.isDisplayDebugEnabled(); }

  /**
   * @brief Check if WebSocket debugging is enabled
   * @return True if WebSocket debugging is enabled, false otherwise
   */
  bool isDebugWebSocket() const {
    return m_debugConfig.isWebSocketDebugEnabled();
  }

  /**
   * @brief Set the RAM debugging status
   * @param enabled True to enable RAM debugging, false to disable
   * @return Result of the set operation
   */
  ConfigResult setDebugRAM(bool enabled);

  /**
   * @brief Set the measurement cycle debugging status
   * @param enabled True to enable measurement cycle debugging, false to disable
   * @return Result of the set operation
   */
  ConfigResult setDebugMeasurementCycle(bool enabled);

  /**
   * @brief Set the sensor debugging status
   * @param enabled True to enable sensor debugging, false to disable
   * @return Result of the set operation
   */
  ConfigResult setDebugSensor(bool enabled);

  /**
   * @brief Set the display debugging status
   * @param enabled True to enable display debugging, false to disable
   * @return Result of the set operation
   */
  ConfigResult setDebugDisplay(bool enabled);

  /**
   * @brief Set the WebSocket debugging status
   * @param enabled True to enable WebSocket debugging, false to disable
   * @return Result of the set operation
   */
  ConfigResult setDebugWebSocket(bool enabled);

  /**
   * @brief Get the device name (user-configurable)
   * @return String Device name
   */
  inline String getDeviceName() const { return m_configData.deviceName; }

  /**
   * @brief Set the device name (user-configurable)
   * @param name New device name
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setDeviceName(const String& name);

  // Notification system
  /**
   * @brief Add a callback to be invoked on configuration changes
   * @param callback The callback function to be added
   */
  void addChangeCallback(ConfigNotifier::ChangeCallback callback);

  // Dependencies
  /**
   * @brief Set the SensorManager instance
   * @param manager Pointer to the SensorManager instance
   */
  inline void setSensorManager(SensorManager* manager) {
    m_sensorManager = manager;
  }

  /**
   * @brief Get WiFi SSID 1
   */
  inline String getWiFiSSID1() const { return m_configData.wifiSSID1; }
  /**
   * @brief Set WiFi SSID 1
   */
  ConfigResult setWiFiSSID1(const String& ssid) {
    m_configData.wifiSSID1 = ssid;
    return saveConfig();
  }
  /**
   * @brief Get WiFi Password 1
   */
  inline String getWiFiPassword1() const { return m_configData.wifiPassword1; }
  /**
   * @brief Set WiFi Password 1
   */
  ConfigResult setWiFiPassword1(const String& pwd) {
    m_configData.wifiPassword1 = pwd;
    return saveConfig();
  }
  /**
   * @brief Get WiFi SSID 2
   */
  inline String getWiFiSSID2() const { return m_configData.wifiSSID2; }
  /**
   * @brief Set WiFi SSID 2
   */
  ConfigResult setWiFiSSID2(const String& ssid) {
    m_configData.wifiSSID2 = ssid;
    return saveConfig();
  }
  /**
   * @brief Get WiFi Password 2
   */
  inline String getWiFiPassword2() const { return m_configData.wifiPassword2; }
  /**
   * @brief Set WiFi Password 2
   */
  ConfigResult setWiFiPassword2(const String& pwd) {
    m_configData.wifiPassword2 = pwd;
    return saveConfig();
  }
  /**
   * @brief Get WiFi SSID 3
   */
  inline String getWiFiSSID3() const { return m_configData.wifiSSID3; }
  /**
   * @brief Set WiFi SSID 3
   */
  ConfigResult setWiFiSSID3(const String& ssid) {
    m_configData.wifiSSID3 = ssid;
    return saveConfig();
  }
  /**
   * @brief Get WiFi Password 3
   */
  inline String getWiFiPassword3() const { return m_configData.wifiPassword3; }
  /**
   * @brief Set WiFi Password 3
   */
  ConfigResult setWiFiPassword3(const String& pwd) {
    m_configData.wifiPassword3 = pwd;
    return saveConfig();
  }

#if USE_MAIL
  // Mail/SMTP configuration
  /**
   * @brief Check if mail functionality is enabled
   * @return True if mail is enabled, false otherwise
   */
  inline bool isMailEnabled() const {
    return m_configData.mailEnabled;
  }

  /**
   * @brief Get SMTP host
   * @return SMTP server host string
   */
  inline String getSmtpHost() const {
    return m_configData.smtpHost;
  }

  /**
   * @brief Get SMTP port
   * @return SMTP server port number
   */
  inline uint16_t getSmtpPort() const {
    return m_configData.smtpPort;
  }

  /**
   * @brief Get SMTP username
   * @return SMTP username/email string
   */
  inline String getSmtpUser() const {
    return m_configData.smtpUser;
  }

  /**
   * @brief Get SMTP password
   * @return SMTP password string
   */
  inline String getSmtpPassword() const {
    return m_configData.smtpPassword;
  }

  /**
   * @brief Get SMTP sender name
   * @return Sender display name string
   */
  inline String getSmtpSenderName() const {
    return m_configData.smtpSenderName;
  }

  /**
   * @brief Get SMTP sender email
   * @return Sender email address string
   */
  inline String getSmtpSenderEmail() const {
    return m_configData.smtpSenderEmail;
  }

  /**
   * @brief Get SMTP recipient email
   * @return Default recipient email string
   */
  inline String getSmtpRecipient() const {
    return m_configData.smtpRecipient;
  }

  /**
   * @brief Check if SMTP STARTTLS is enabled
   * @return True if STARTTLS is enabled, false otherwise
   */
  inline bool isSmtpEnableStartTLS() const {
    return m_configData.smtpEnableStartTLS;
  }

  /**
   * @brief Check if SMTP debug is enabled
   * @return True if SMTP debug is enabled, false otherwise
   */
  inline bool isSmtpDebug() const {
    return m_configData.smtpDebug;
  }

  /**
   * @brief Check if test mail on boot is enabled
   * @return True if test mail on boot is enabled, false otherwise
   */
  inline bool isSmtpSendTestMailOnBoot() const {
    return m_configData.smtpSendTestMailOnBoot;
  }

  /**
   * @brief Set mail functionality enabled status
   * @param enabled True to enable mail, false to disable
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setMailEnabled(bool enabled);

  /**
   * @brief Set SMTP host
   * @param host SMTP server host string
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpHost(const String& host);

  /**
   * @brief Set SMTP port
   * @param port SMTP server port number
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpPort(uint16_t port);

  /**
   * @brief Set SMTP username
   * @param user SMTP username/email string
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpUser(const String& user);

  /**
   * @brief Set SMTP password
   * @param password SMTP password string
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpPassword(const String& password);

  /**
   * @brief Set SMTP sender name
   * @param senderName Sender display name string
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpSenderName(const String& senderName);

  /**
   * @brief Set SMTP sender email
   * @param senderEmail Sender email address string
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpSenderEmail(const String& senderEmail);

  /**
   * @brief Set SMTP recipient email
   * @param recipient Default recipient email string
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpRecipient(const String& recipient);

  /**
   * @brief Set SMTP STARTTLS enabled status
   * @param enabled True to enable STARTTLS, false to disable
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpEnableStartTLS(bool enabled);

  /**
   * @brief Set SMTP debug enabled status
   * @param enabled True to enable SMTP debug, false to disable
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpDebug(bool enabled);

  /**
   * @brief Set test mail on boot enabled status
   * @param enabled True to enable test mail on boot, false to disable
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setSmtpSendTestMailOnBoot(bool enabled);

#endif // USE_MAIL

  // LED Traffic Light configuration
  /**
   * @brief Get LED traffic light mode
   * @return 0 = off, 1 = all measurements, 2 = single measurement
   */
  inline uint8_t getLedTrafficLightMode() const {
    return m_configData.ledTrafficLightMode;
  }

  /**
   * @brief Set LED traffic light mode
   * @param mode 0 = off, 1 = all measurements, 2 = single measurement
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setLedTrafficLightMode(uint8_t mode);

  /**
   * @brief Get LED traffic light selected measurement for mode 2
   * @return Selected measurement ID string, empty if no measurement selected
   */
  inline String getLedTrafficLightSelectedMeasurement() const {
    return m_configData.ledTrafficLightSelectedMeasurement;
  }

  /**
   * @brief Set LED traffic light selected measurement for mode 2
   * @param measurementId Measurement ID string to select
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setLedTrafficLightSelectedMeasurement(
      const String& measurementId);

  // Flower Status configuration (for startpage display)
  /**
   * @brief Get the sensor that controls the flower face status
   * @return Sensor ID string (format: "sensorId_measurementIndex")
   */
  inline String getFlowerStatusSensor() const {
    return m_configData.flowerStatusSensor.isEmpty()
           ? String("ANALOG_1")  // Default to Bodenfeuchte
           : m_configData.flowerStatusSensor;
  }

  /**
   * @brief Set the sensor that controls the flower face status
   * @param sensorId Sensor ID string (format: "sensorId_measurementIndex")
   * @return ConfigResult indicating success or failure
   */
  ConfigResult setFlowerStatusSensor(const String& sensorId);

 private:
  ConfigManager();
  ~ConfigManager() = default;
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  ConfigData m_configData;
  ConfigWebHandler m_webHandler;
  ConfigNotifier m_notifier;
  DebugConfig m_debugConfig;
  SensorErrorTracker m_sensorErrorTracker;

  SensorManager* m_sensorManager = nullptr;
  bool m_configLoaded = false;

  /**
   * @brief Notify listeners of a configuration change
   * @param key The key of the changed configuration
   * @param value The new value of the configuration
   * @param updateSensors Whether to update sensor settings
   */
  void notifyConfigChange(const String& key, const String& value,
                          bool updateSensors = true);

  /**
   * @brief Validate and save configuration
   * @return Result of the validate and save operation
   */
  ConfigResult validateAndSave();

  /**
   * @brief Sync subsystem data with main config data
   */
  void syncSubsystemData();
};

// Global instances
extern std::unique_ptr<WebOTAHandler> otaHandler;
extern std::unique_ptr<ResourceManager> resourceManager;
#define ConfigMgr ConfigManager::getInstance()

#endif
