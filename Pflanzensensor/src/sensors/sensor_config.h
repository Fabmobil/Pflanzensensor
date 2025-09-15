/**
 * @file sensor_config.h
 * @brief Generalized configuration templates for sensors using consistent
 * naming schemes
 * @details This header provides configuration structures and macros for
 * standardizing sensor configurations across the system, supporting both
 * analog and DS18B20 temperature sensors.
 */

#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <Arduino.h>

// Check if CONFIG_FILE is defined
#ifndef CONFIG_FILE
#error "CONFIG_FILE not defined. Please specify a config file in platformio.ini"
#endif

// Include the config file specified in platformio.ini
#include CONFIG_FILE

/**
 * @brief Default configuration structure for analog sensors
 * @details Provides a standardized way to configure analog sensors with
 * calibration values, thresholds, and operational parameters.
 */
struct AnalogSensorDefaults {
  const char* name;       ///< Sensor name for identification
  const char* fieldName;  ///< Field name used for data storage/display
  int rawMin;             ///< Minimum raw analog value (typically 0)
  int rawMax;             ///< Maximum raw analog value (typically 1023)
  float yellowLow;        ///< Lower yellow warning threshold
  float greenLow;         ///< Lower green (normal) threshold
  float greenHigh;        ///< Upper green (normal) threshold
  float yellowHigh;       ///< Upper yellow warning threshold
  unsigned long
      measurementInterval;  ///< Time between measurements in milliseconds
  bool calibrationMode;     ///< Whether sensor is in calibration mode

  /**
   * @brief Constructor for AnalogSensorDefaults
   * @param n Sensor name
   * @param f Field name
   * @param rmin Minimum raw value
   * @param rmax Maximum raw value
   * @param yl Yellow low threshold
   * @param gl Green low threshold
   * @param gh Green high threshold
   * @param yh Yellow high threshold
   * @param mi Measurement interval in milliseconds
   * @param cal Calibration mode flag
   */
  AnalogSensorDefaults(const char* n = "", const char* f = "", int rmin = 0,
                       int rmax = 1023, float yl = 0.0f, float gl = 0.0f,
                       float gh = 100.0f, float yh = 100.0f,
                       unsigned long mi = 60000, bool cal = false)
      : name(n),
        fieldName(f),
        rawMin(rmin),
        rawMax(rmax),
        yellowLow(yl),
        greenLow(gl),
        greenHigh(gh),
        yellowHigh(yh),
        measurementInterval(mi),
        calibrationMode(cal) {}
};

/**
 * @brief Default configuration structure for DS18B20 sensors
 * @details Provides configuration parameters for DS18B20 temperature sensors,
 * including thresholds and measurement timing.
 */
struct DS18B20SensorDefaults {
  const char* name;                   ///< Sensor name
  const char* fieldName;              ///< Field name for measurements
  float yellowLow;                    ///< Lower yellow threshold
  float greenLow;                     ///< Lower green threshold
  float greenHigh;                    ///< Upper green threshold
  float yellowHigh;                   ///< Upper yellow threshold
  unsigned long measurementInterval;  ///< Measurement interval in milliseconds
};

/**
 * @brief Helper macro to create analog sensor defaults from index
 * @param N The index of the analog sensor (1-based)
 * @return AnalogSensorDefaults structure initialized with configuration values
 * @note Requires corresponding configuration defines (ANALOG_N_*) to be set
 * in the config file
 */
#define ANALOG_SENSOR_DEFAULT(N)                                         \
  AnalogSensorDefaults(                                                  \
      ANALOG_##N##_NAME, ANALOG_##N##_FIELD_NAME, ANALOG_##N##_MIN,      \
      ANALOG_##N##_MAX, ANALOG_##N##_YELLOW_LOW, ANALOG_##N##_GREEN_LOW, \
      ANALOG_##N##_GREEN_HIGH, ANALOG_##N##_YELLOW_HIGH,                 \
      MEASUREMENT_INTERVAL * 1000, ANALOG_##N##_CALIBRATION_MODE)

/**
 * @brief Helper macro to create DS18B20 sensor defaults from index
 * @param N The index of the DS18B20 sensor (1-based)
 * @return DS18B20SensorDefaults structure initialized with configuration values
 * @note Requires corresponding configuration defines (DS18B20_N_*) to be set
 * in the config file
 */
#define DS18B20_SENSOR_DEFAULT(N)                                            \
  {                                                                          \
    DS18B20_##N##_NAME, DS18B20_##N##_FIELD_NAME, DS18B20_##N##_YELLOW_LOW,  \
        DS18B20_##N##_GREEN_LOW, DS18B20_##N##_GREEN_HIGH,                   \
        DS18B20_##N##_YELLOW_HIGH, DS18B20_##N##_MEASUREMENT_INTERVAL * 1000 \
  }

// Ensure we have the required configuration defines
#ifndef ANALOG_SENSOR_COUNT
#define ANALOG_SENSOR_COUNT 0
#endif

#ifndef DS18B20_SENSOR_COUNT
#define DS18B20_SENSOR_COUNT 0
#endif

#if ANALOG_SENSOR_COUNT > 0
/**
 * @brief Global array of analog sensor defaults
 */
const AnalogSensorDefaults ANALOG_SENSOR_DEFAULTS[] = {ANALOG_SENSOR_DEFAULT(1)
#if ANALOG_SENSOR_COUNT > 1
                                                           ,
                                                       ANALOG_SENSOR_DEFAULT(2)
#endif
#if ANALOG_SENSOR_COUNT > 2
                                                           ,
                                                       ANALOG_SENSOR_DEFAULT(3)
#endif
#if ANALOG_SENSOR_COUNT > 3
                                                           ,
                                                       ANALOG_SENSOR_DEFAULT(4)
#endif
#if ANALOG_SENSOR_COUNT > 4
                                                           ,
                                                       ANALOG_SENSOR_DEFAULT(5)
#endif
#if ANALOG_SENSOR_COUNT > 5
                                                           ,
                                                       ANALOG_SENSOR_DEFAULT(6)
#endif
#if ANALOG_SENSOR_COUNT > 6
                                                           ,
                                                       ANALOG_SENSOR_DEFAULT(7)
#endif
#if ANALOG_SENSOR_COUNT > 7
                                                           ,
                                                       ANALOG_SENSOR_DEFAULT(8)
#endif
};
#endif

#if DS18B20_SENSOR_COUNT > 0
/**
 * @brief Global array of DS18B20 sensor defaults
 */
const DS18B20SensorDefaults DS18B20_SENSOR_DEFAULTS[] = {
    DS18B20_SENSOR_DEFAULT(1)
#if DS18B20_SENSOR_COUNT > 1
        ,
    DS18B20_SENSOR_DEFAULT(2)
#endif
#if DS18B20_SENSOR_COUNT > 2
        ,
    DS18B20_SENSOR_DEFAULT(3)
#endif
#if DS18B20_SENSOR_COUNT > 3
        ,
    DS18B20_SENSOR_DEFAULT(4)
#endif
#if DS18B20_SENSOR_COUNT > 4
        ,
    DS18B20_SENSOR_DEFAULT(5)
#endif
#if DS18B20_SENSOR_COUNT > 5
        ,
    DS18B20_SENSOR_DEFAULT(6)
#endif
#if DS18B20_SENSOR_COUNT > 6
        ,
    DS18B20_SENSOR_DEFAULT(7)
#endif
#if DS18B20_SENSOR_COUNT > 7
        ,
    DS18B20_SENSOR_DEFAULT(8)
#endif
};
#endif

/**
 * @brief Get the number of configured analog sensors
 * @return size_t Number of analog sensors in the defaults array
 * @details Returns 0 if ANALOG_SENSOR_COUNT is not defined or is 0
 */
constexpr size_t getAnalogSensorCount() {
#if ANALOG_SENSOR_COUNT > 0
  return sizeof(ANALOG_SENSOR_DEFAULTS) / sizeof(ANALOG_SENSOR_DEFAULTS[0]);
#else
  return 0;
#endif
}

/**
 * @brief Get the number of configured DS18B20 sensors
 * @return size_t Number of DS18B20 sensors in the defaults array
 * @details Returns 0 if DS18B20_SENSOR_COUNT is not defined or is 0
 */
constexpr size_t getDS18B20SensorCount() {
#if DS18B20_SENSOR_COUNT > 0
  return sizeof(DS18B20_SENSOR_DEFAULTS) / sizeof(DS18B20_SENSOR_DEFAULTS[0]);
#else
  return 0;
#endif
}

#endif  // SENSOR_CONFIG_H
