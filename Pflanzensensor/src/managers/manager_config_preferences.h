/**
 * @file manager_config_preferences.h
 * @brief Preferences-based persistence layer for configuration management
 * 
 * This module provides a centralized interface for storing and retrieving
 * configuration values using the ESP Preferences library on LittleFS.
 * It organizes settings into logical namespaces for better organization.
 */

#ifndef MANAGER_CONFIG_PREFERENCES_H
#define MANAGER_CONFIG_PREFERENCES_H

#include <Arduino.h>
#include <Preferences.h>
#include "../utils/result_types.h"

// Namespace constants
namespace PreferencesNamespaces {
  constexpr const char* GENERAL = "general";      // General settings (device name, passwords, etc.)
  constexpr const char* WIFI = "wifi";           // WiFi credentials and network settings
  constexpr const char* DISP = "display";        // Display configuration (renamed from DISPLAY to avoid Arduino.h macro conflict)
  constexpr const char* LOG = "log";             // Logging settings
  constexpr const char* LED_TRAFFIC = "led_traf"; // LED traffic light settings (max 15 chars)
  constexpr const char* DEBUG = "debug";         // Debug flags
  
  // Sensor namespaces - dynamic, format: "sensor_X" where X is sensor index
  String getSensorNamespace(const String& sensorId);
  String getSensorMeasurementKey(uint8_t measurementIndex, const char* suffix);
}

/**
 * @class PreferencesManager
 * @brief Manager class for Preferences-based configuration storage
 * 
 * This class provides helper functions for common operations like:
 * - Namespace initialization
 * - Key-value loading and saving
 * - Type-safe getters and setters
 * - Migration from JSON to Preferences
 */
class PreferencesManager {
public:
  using PrefResult = TypedResult<ConfigError, void>;
  
  /**
   * @brief Initialize all namespaces with default values if they don't exist
   * @return PrefResult indicating success or failure
   */
  static PrefResult initializeAllNamespaces();
  
  /**
   * @brief Check if a namespace exists (has been initialized)
   * @param namespaceName The namespace to check
   * @return True if the namespace exists, false otherwise
   */
  static bool namespaceExists(const char* namespaceName);
  
  /**
   * @brief Clear all preferences (factory reset)
   * @return PrefResult indicating success or failure
   */
  static PrefResult clearAll();
  
  // General settings
  static PrefResult saveGeneralSettings(const String& deviceName, 
                                       const String& adminPassword,
                                       bool md5Verification,
                                       bool fileLoggingEnabled);
  static PrefResult loadGeneralSettings(String& deviceName, 
                                       String& adminPassword,
                                       bool& md5Verification,
                                       bool& fileLoggingEnabled);
  
  // WiFi settings
  static PrefResult saveWiFiSettings(const String& ssid1, const String& pwd1,
                                     const String& ssid2, const String& pwd2,
                                     const String& ssid3, const String& pwd3);
  static PrefResult loadWiFiSettings(String& ssid1, String& pwd1,
                                     String& ssid2, String& pwd2,
                                     String& ssid3, String& pwd3);
  
  // Display settings
  static PrefResult saveDisplaySettings(bool showIpScreen, bool showClock,
                                        bool showFlowerImage, bool showFabmobilImage,
                                        unsigned long screenDuration,
                                        const String& clockFormat);
  static PrefResult loadDisplaySettings(bool& showIpScreen, bool& showClock,
                                        bool& showFlowerImage, bool& showFabmobilImage,
                                        unsigned long& screenDuration,
                                        String& clockFormat);
  
  // Log settings
  static PrefResult saveLogSettings(const String& logLevel, bool fileLogging);
  static PrefResult loadLogSettings(String& logLevel, bool& fileLogging);
  
  // LED Traffic Light settings
  static PrefResult saveLedTrafficSettings(uint8_t mode, const String& selectedMeasurement);
  static PrefResult loadLedTrafficSettings(uint8_t& mode, String& selectedMeasurement);
  
  // Debug settings
  static PrefResult saveDebugSettings(bool debugRAM, bool debugMeasurementCycle,
                                      bool debugSensor, bool debugDisplay,
                                      bool debugWebSocket);
  static PrefResult loadDebugSettings(bool& debugRAM, bool& debugMeasurementCycle,
                                      bool& debugSensor, bool& debugDisplay,
                                      bool& debugWebSocket);
  
  // Sensor settings - per sensor
  static PrefResult saveSensorSettings(const String& sensorId, 
                                       const String& name,
                                       unsigned long measurementInterval,
                                       bool hasPersistentError);
  static PrefResult loadSensorSettings(const String& sensorId,
                                       String& name,
                                       unsigned long& measurementInterval,
                                       bool& hasPersistentError);
  
  // Sensor measurement settings - per measurement
  static PrefResult saveSensorMeasurement(const String& sensorId,
                                         uint8_t measurementIndex,
                                         bool enabled,
                                         const String& name,
                                         const String& fieldName,
                                         const String& unit,
                                         float minValue, float maxValue,
                                         float yellowLow, float greenLow,
                                         float greenHigh, float yellowHigh,
                                         bool inverted,
                                         bool calibrationMode,
                                         uint32_t autocalDuration,
                                         int absoluteRawMin,
                                         int absoluteRawMax);
  static PrefResult loadSensorMeasurement(const String& sensorId,
                                         uint8_t measurementIndex,
                                         bool& enabled,
                                         String& name,
                                         String& fieldName,
                                         String& unit,
                                         float& minValue, float& maxValue,
                                         float& yellowLow, float& greenLow,
                                         float& greenHigh, float& yellowHigh,
                                         bool& inverted,
                                         bool& calibrationMode,
                                         uint32_t& autocalDuration,
                                         int& absoluteRawMin,
                                         int& absoluteRawMax);
  
  // Check if sensor namespace exists
  static bool sensorNamespaceExists(const String& sensorId);
  
  // Clear sensor namespace
  static PrefResult clearSensorNamespace(const String& sensorId);
  
  // Flower status sensor setting
  static PrefResult saveFlowerStatusSensor(const String& sensorId);
  static PrefResult loadFlowerStatusSensor(String& sensorId);
  
  // Atomic update functions (DRY - only save what changed)
  static PrefResult updateDeviceName(const String& deviceName);
  static PrefResult updateAdminPassword(const String& adminPassword);
  static PrefResult updateMD5Verification(bool enabled);
  static PrefResult updateFileLoggingEnabled(bool enabled);
  
  static PrefResult updateWiFiCredentials(uint8_t setIndex, const String& ssid, const String& password);
  
  static PrefResult updateLedTrafficMode(uint8_t mode);
  static PrefResult updateLedTrafficMeasurement(const String& measurement);
  
  static PrefResult updateDebugRAM(bool enabled);
  static PrefResult updateDebugMeasurementCycle(bool enabled);
  static PrefResult updateDebugSensor(bool enabled);
  static PrefResult updateDebugDisplay(bool enabled);
  static PrefResult updateDebugWebSocket(bool enabled);
  
  // Atomic update methods for display settings
  static PrefResult updateScreenDuration(unsigned int duration);
  static PrefResult updateClockFormat(const String& format);
  static PrefResult updateClockEnabled(bool enabled);
  static PrefResult updateIpScreenEnabled(bool enabled);
  static PrefResult updateFlowerImageEnabled(bool enabled);
  static PrefResult updateFabmobilImageEnabled(bool enabled);
  
  // Helper functions for type-safe access
  static String getString(Preferences& prefs, const char* key, const String& defaultValue = "");
  static bool getBool(Preferences& prefs, const char* key, bool defaultValue = false);
  static uint8_t getUChar(Preferences& prefs, const char* key, uint8_t defaultValue = 0);
  static uint32_t getUInt(Preferences& prefs, const char* key, uint32_t defaultValue = 0);
  static int getInt(Preferences& prefs, const char* key, int defaultValue = 0);
  static float getFloat(Preferences& prefs, const char* key, float defaultValue = 0.0f);
  
  static bool putString(Preferences& prefs, const char* key, const String& value);
  static bool putBool(Preferences& prefs, const char* key, bool value);
  static bool putUChar(Preferences& prefs, const char* key, uint8_t value);
  static bool putUInt(Preferences& prefs, const char* key, uint32_t value);
  static bool putInt(Preferences& prefs, const char* key, int value);
  static bool putFloat(Preferences& prefs, const char* key, float value);

  // Public initialization functions for individual namespaces
  static PrefResult initGeneralNamespace();
  static PrefResult initWiFiNamespace();
  static PrefResult initDisplayNamespace();
  static PrefResult initLogNamespace();
  static PrefResult initLedTrafficNamespace();
  static PrefResult initDebugNamespace();

private:
  PreferencesManager() = default;
};

#endif // MANAGER_CONFIG_PREFERENCES_H
