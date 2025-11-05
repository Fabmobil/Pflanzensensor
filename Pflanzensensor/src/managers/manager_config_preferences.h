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

#include "../utils/result_types.h"
#include <Arduino.h>
#include <Preferences.h>

// Namespace constants
namespace PreferencesNamespaces {
constexpr const char* GENERAL = "general"; // General settings (device name, passwords, etc.)
constexpr const char* WIFI = "wifi";       // WiFi credentials and network settings
constexpr const char* DISP =
    "display"; // Display configuration (renamed from DISPLAY to avoid Arduino.h macro conflict)
constexpr const char* LOG = "log";              // Logging settings
constexpr const char* LED_TRAFFIC = "led_traf"; // LED traffic light settings (max 15 chars)
constexpr const char* DEBUG = "debug";          // Debug flags

// Sensor namespaces - dynamic, format: "sensor_X" where X is sensor index
String getSensorNamespace(const String& sensorId);
String getSensorMeasurementKey(uint8_t measurementIndex, const char* suffix);
} // namespace PreferencesNamespaces

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

  // Sensor settings - per sensor
  static PrefResult saveSensorSettings(const String& sensorId, const String& name,
                                       unsigned long measurementInterval, bool hasPersistentError);
  static PrefResult loadSensorSettings(const String& sensorId, String& name,
                                       unsigned long& measurementInterval,
                                       bool& hasPersistentError);

  // Sensor measurement settings - per measurement
  static PrefResult saveSensorMeasurement(const String& sensorId, uint8_t measurementIndex,
                                          bool enabled, const String& name, const String& fieldName,
                                          const String& unit, float minValue, float maxValue,
                                          float yellowLow, float greenLow, float greenHigh,
                                          float yellowHigh, bool inverted, bool calibrationMode,
                                          uint32_t autocalDuration, int absoluteRawMin,
                                          int absoluteRawMax);
  static PrefResult loadSensorMeasurement(const String& sensorId, uint8_t measurementIndex,
                                          bool& enabled, String& name, String& fieldName,
                                          String& unit, float& minValue, float& maxValue,
                                          float& yellowLow, float& greenLow, float& greenHigh,
                                          float& yellowHigh, bool& inverted, bool& calibrationMode,
                                          uint32_t& autocalDuration, int& absoluteRawMin,
                                          int& absoluteRawMax);

  // Check if sensor namespace exists
  static bool sensorNamespaceExists(const String& sensorId);

  // Clear sensor namespace
  static PrefResult clearSensorNamespace(const String& sensorId);

  // Specialized WiFi update (validates index + updates 2 keys atomically)
  static PrefResult updateWiFiCredentials(uint8_t setIndex, const String& ssid,
                                          const String& password);

  // Helper functions for type-safe access
  static String getString(Preferences& prefs, const char* key, const String& defaultValue = "");
  static bool getBool(Preferences& prefs, const char* key, bool defaultValue = false);
  static uint8_t getUChar(Preferences& prefs, const char* key, uint8_t defaultValue = 0);
  static uint32_t getUInt(Preferences& prefs, const char* key, uint32_t defaultValue = 0);
  static int getInt(Preferences& prefs, const char* key, int defaultValue = 0);
  static float getFloat(Preferences& prefs, const char* key, float defaultValue = 0.0f);

  // Convenience getters that accept a namespace key (open/close Preferences internally)
  static String getString(const char* namespaceKey, const char* key,
                          const String& defaultValue = "");
  static bool getBool(const char* namespaceKey, const char* key, bool defaultValue = false);
  static uint32_t getUInt(const char* namespaceKey, const char* key, uint32_t defaultValue = 0);

  static bool putString(Preferences& prefs, const char* key, const String& value);
  static bool putBool(Preferences& prefs, const char* key, bool value);
  static bool putUChar(Preferences& prefs, const char* key, uint8_t value);
  static bool putUInt(Preferences& prefs, const char* key, uint32_t value);
  static bool putInt(Preferences& prefs, const char* key, int value);
  static bool putFloat(Preferences& prefs, const char* key, float value);

  // DRY Generic update helpers - use these directly instead of wrapper methods
  static PrefResult updateBoolValue(const char* namespaceKey, const char* key, bool value);
  static PrefResult updateStringValue(const char* namespaceKey, const char* key,
                                      const String& value);
  static PrefResult updateUInt8Value(const char* namespaceKey, const char* key, uint8_t value);
  static PrefResult updateUIntValue(const char* namespaceKey, const char* key, unsigned int value);

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
