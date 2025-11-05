/**
 * @file manager_config_types.h
 * @brief Common types and structures for configuration management
 */

#ifndef MANAGER_CONFIG_TYPES_H
#define MANAGER_CONFIG_TYPES_H

#include <Arduino.h>

#include <map>

#include "../configs/config_pflanzensensor.h"
#include "../utils/result_types.h"

// Add ConfigError to string conversion function
inline String errorTypeToString(ConfigError error) {
  switch (error) {
  case ConfigError::SUCCESS:
    return F("Success");
  case ConfigError::VALIDATION_ERROR:
    return F("Validation Error");
  case ConfigError::FILE_ERROR:
    return F("File Error");
  case ConfigError::PARSE_ERROR:
    return F("Parse Error");
  case ConfigError::SAVE_FAILED:
    return F("Save Failed");
  case ConfigError::UNKNOWN_ERROR:
  default:
    return F("Unknown Config Error");
  }
}

/**
 * @brief Enum for configuration value types
 */
enum class ConfigValueType {
  BOOL,   ///< Boolean value
  INT,    ///< Integer value
  UINT,   ///< Unsigned integer value
  FLOAT,  ///< Float value
  STRING  ///< String value
};

/**
 * @brief Struct for storing threshold values for a measurement
 */
struct Thresholds {
  float yellowLow;
  float greenLow;
  float greenHigh;
  float yellowHigh;
};

// ConfigError is defined in ../utils/result_types.h
// We don't redefine it here to avoid conflicts

/**
 * @brief Main configuration data structure (general settings only)
 * @note Update flags are now stored in a separate file, not in this struct.
 */
struct ConfigData {
  String adminPassword;
  bool md5Verification;
  bool collectdEnabled;
  bool fileLoggingEnabled;
  // Removed: bool doFirmwareUpgrade;
  // Removed: bool fileSystemUpdatePending;
  // Removed: bool firmwareUpdatePending;

  // Debug flags
  bool debugRAM;
  bool debugMeasurementCycle;
  bool debugSensor;
  bool debugDisplay;
  bool debugWebSocket;

  /**
   * @brief Device name (user-configurable)
   */
  String deviceName;

  /**
   * @brief WiFi credentials (up to 3 sets)
   */
  String wifiSSID1;     ///< Primary WiFi SSID
  String wifiPassword1; ///< Primary WiFi password
  String wifiSSID2;     ///< Secondary WiFi SSID
  String wifiPassword2; ///< Secondary WiFi password
  String wifiSSID3;     ///< Tertiary WiFi SSID
  String wifiPassword3; ///< Tertiary WiFi password

  // LED Traffic Light settings
  uint8_t ledTrafficLightMode;               ///< 0 = off, 1 = all measurements, 2 = single
                                             ///< measurement
  String ledTrafficLightSelectedMeasurement; ///< Selected measurement
                                             ///< identifier (format:
                                             ///< "sensorId_measurementIndex",
                                             ///< e.g., "analog_0")

  // Flower Status settings (for startpage)
  String flowerStatusSensor; ///< Sensor that controls the flower face status
                             ///< (format: "sensorId_measurementIndex",
                             ///< default: "ANALOG_1" for Bodenfeuchte)
};

/**
 * @brief Sensor configuration data structure
 */
struct SensorConfigData {
  // This will be populated from the sensors.json file
  // The actual sensor data is managed by the SensorManager
  // This structure is mainly for persistence operations
};

#endif
