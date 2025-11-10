// sensor_types.h
/**
 * @file sensor_types.h
 * @brief Defines fundamental types and structures for sensor operations
 */
#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <Arduino.h>

#include <array>
#include <vector>

#include "sensor_config.h" // Ensure configuration macros are defined first
#include "sensors/sensor_autocalibration.h"

class Sensor;

/**
 * @brief Available sensor types
 */
enum class SensorType {
  DHT,
  DS18B20,
  SDS011,
  MHZ19,
  HX711,
  BMP280,
  ANALOG,
  SERIAL_RECEIVER, ///< Serial data receiver for external Arduino devices
  UNKNOWN
};

/**
 * @brief Measurement error codes
 */
enum class MeasurementError {
  NONE,
  INITIALIZATION_FAILED,
  WARMUP_TIMEOUT,
  INVALID_READING,
  COMMUNICATION_ERROR,
  HARDWARE_ERROR,
  MEMORY_ERROR
};

/**
 * @brief Sensor threshold limits
 */
struct SensorLimits {
  float yellowLow;  ///< Lower warning threshold
  float greenLow;   ///< Lower normal threshold
  float greenHigh;  ///< Upper normal threshold
  float yellowHigh; ///< Upper warning threshold

  SensorLimits() : yellowLow(0.0f), greenLow(0.0f), greenHigh(0.0f), yellowHigh(0.0f) {}
};

/**
 * @brief Structure for measurement configuration
 */
struct MeasurementConfig {
  String name;                 ///< Human readable measurement name
  String fieldName;            ///< Field name for database
  String unit;                 ///< Unit of measurement
  mutable SensorLimits limits; ///< Thresholds for this measurement
  bool enabled{true};          ///< Whether this measurement is active
  unsigned long retryDelay;    ///< Delay before retry on failure (ms)
  uint8_t maxRetries;          ///< Maximum number of retries
  /**
   * @brief Minimum value for analog measurement (if applicable)
   */
  float minValue{0.0f};
  /**
   * @brief Maximum value for analog measurement (if applicable)
   */
  float maxValue{0.0f};
  /**
   * @brief Whether to invert the scale for analog measurements (if applicable)
   */
  bool inverted{false};
  /**
   * @brief Absolute minimum value ever measured for this measurement
   */
  float absoluteMin{INFINITY};
  /**
   * @brief Absolute maximum value ever measured for this measurement
   */
  float absoluteMax{-INFINITY};
  /**
   * @brief Absolute minimum raw value ever measured for analog sensors
   */
  int absoluteRawMin{INT_MAX};
  /**
   * @brief Absolute maximum raw value ever measured for analog sensors
   */
  int absoluteRawMax{INT_MIN};
  /**
   * @brief Letzter gemessener Rohwert (nur für Analogsensoren)
   */
  int lastRawValue{-1};

  /**
   * @brief Whether automatic calibration (AutoCal) is enabled for this
   * measurement (analog sensors only).
   */
  bool calibrationMode{false};

  /**
   * @brief Autocalibration half-life in seconds
   * @details The time it should take for the autocal EMA to move 50% from
   * an old value towards a new value. User-selectable via sensors.json. If
   * zero the default of 1 day (86400s) is used.
   */
  uint32_t autocalHalfLifeSeconds{86400};

  /**
   * @brief Autocalibration state (only meaningful when calibrationMode==true)
   */
  AutoCal autocal;

  /**
   * @brief Letzter gemessener Wert (generisch für alle Sensortypen)
   */
  float lastValue{NAN};

  MeasurementConfig()
      : enabled(true),
        retryDelay(1000),
        maxRetries(3),
        minValue(0.0f),
        maxValue(0.0f),
        absoluteMin(INFINITY),
        absoluteMax(-INFINITY),
        lastValue(NAN)
  // DO NOT initialize absoluteRawMin/absoluteRawMax here
  // Let JSON loading handle these values to prevent constructor defaults
  // from overriding loaded configuration
  {
    // Initialize autocal to sensible defaults for 10-bit ADC
    autocal.min_value = 0;
    autocal.max_value = 1023;
    autocal.last_update_time = 0;
  }
};

/**
 * @brief Base configuration structure for all sensors
 */
struct SensorConfig {
  static constexpr size_t MAX_MEASUREMENTS = 8;
  static constexpr size_t FIELD_NAME_LEN = 24;
  static constexpr size_t UNIT_LEN = 8;
  static constexpr size_t ERROR_MSG_LEN = 64;

  uint8_t pin{0};                       ///< Hardware pin number for the sensor
  String id;                            ///< Unique identifier for the sensor
  String name;                          ///< Human-readable name of the sensor
  bool enabled{true};                   ///< Whether the sensor is active
  unsigned long measurementInterval{0}; ///< Time between measurements in milliseconds
  unsigned long minimumDelay{0};        ///< Minimum delay between readings
  unsigned long warmupTime{0};          ///< Time needed for sensor to warm up
  bool requiresWarmup{false};           ///< Whether sensor needs warmup period
  int measurementErrorCount{0};         ///< Count of consecutive measurement errors
  /**
   * @brief Persistent error flag for this sensor (replaces sensorErrorFlags
   * map)
   */
  bool hasPersistentError{false}; ///< True if this sensor has a persistent error
  size_t activeMeasurements{1};   ///< Number of active measurements
  std::array<MeasurementConfig, MAX_MEASUREMENTS>
      measurements; ///< Array of measurement configurations
  bool deinitializeAfterMeasurement{
      MEASUREMENT_DEINITIALIZE_SENSORS}; ///< Whether to deinitialize after
                                         ///< measuring

  /**
   * @brief Virtual destructor for proper cleanup in derived classes
   */
  virtual ~SensorConfig() = default;

  /**
   * @brief Default constructor
   * @details Initializes with first measurement enabled
   */
  SensorConfig() { measurements[0].enabled = true; }
};

/**
 * @brief Measurement data structure
 */
struct MeasurementData {
  std::array<float, SensorConfig::MAX_MEASUREMENTS> values; ///< Measurement values
  char fieldNames[SensorConfig::MAX_MEASUREMENTS][SensorConfig::FIELD_NAME_LEN];
  char units[SensorConfig::MAX_MEASUREMENTS][SensorConfig::UNIT_LEN];
  size_t activeValues{0};                             ///< Number of active values
  MeasurementError lastError{MeasurementError::NONE}; ///< Last error that occurred
  char errorMessage[SensorConfig::ERROR_MSG_LEN];     ///< Detailed error message
  bool valid{true}; ///< True if this struct is valid and owned by a sensor

  /**
   * @brief Default constructor
   * @details Initializes all values to 0.0 and sets valid=true
   */
  MeasurementData() : valid(true) {
    values.fill(0.0f);
    for (size_t i = 0; i < SensorConfig::MAX_MEASUREMENTS; ++i) {
      fieldNames[i][0] = '\0';
      units[i][0] = '\0';
    }
    errorMessage[0] = '\0';
  }

  /**
   * @brief Checks if the measurement data is valid
   * @return true if arrays are sized correctly, valid==true, and activeValues
   * is reasonable
   */
  bool isValid() const {
    if (!valid) {
      logger.error(F("MeasurementData"), F("isValid failed: valid=0"));
      return false;
    }
    if (activeValues > SensorConfig::MAX_MEASUREMENTS) {
      logger.error(F("MeasurementData"),
                   F("isValid failed: activeValues > MAX_MEASUREMENTS: ") + String(activeValues));
      return false;
    }
    return true;
  }

  /**
   * @brief Invalidate this struct (mark as not owned)
   */
  inline void invalidate() { valid = false; }

  /**
   * @brief Sets error information for the measurement
   * @param error The type of error that occurred
   * @param message Optional detailed error message
   */
  void setError(MeasurementError error, const String& message = "") {
    lastError = error;
    errorMessage[0] = '\0'; // Clear previous error message
    if (message.length() < SensorConfig::ERROR_MSG_LEN) {
      strncpy(errorMessage, message.c_str(), SensorConfig::ERROR_MSG_LEN - 1);
      errorMessage[SensorConfig::ERROR_MSG_LEN - 1] = '\0'; // Ensure null termination
    } else {
      strncpy(errorMessage, message.c_str(), SensorConfig::ERROR_MSG_LEN - 1);
      errorMessage[SensorConfig::ERROR_MSG_LEN - 1] = '\0'; // Ensure null termination
    }
  }
};

/**
 * @brief Structure for shared hardware management
 */
struct SharedHardwareInfo {
  SensorType type;        ///< Type of the sensor hardware
  uint8_t pin;            ///< Hardware pin (if applicable)
  unsigned long minDelay; ///< Minimum delay between readings
  bool exclusive{false};  ///< Whether device needs exclusive access

  SharedHardwareInfo(SensorType t, uint8_t p, unsigned long d, bool e = false)
      : type(t), pin(p), minDelay(d), exclusive(e) {}
};

#endif // SENSOR_TYPES_H
