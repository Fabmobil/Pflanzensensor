/**
 * @file sensor_count.h
 * @brief Compile-time sensor counting and measurement calculations
 * @details Provides static methods for calculating various sensor-related
 * counts at compile time, including enabled sensors, measurement counts, and
 *          memory requirements.
 */
#ifndef SENSOR_COUNT_H
#define SENSOR_COUNT_H

#include "configs/config.h"

/**
 * @class SensorCounter
 * @brief Static utility class for compile-time sensor calculations
 * @details Provides methods to calculate various sensor-related counts
 *          at compile time based on the configuration settings.
 *          All methods are constexpr to ensure compile-time evaluation.
 */
class SensorCounter {
 public:
  /**
   * @brief Calculate total number of enabled sensors
   * @return Total count of enabled sensors across all types
   * @details Sums up the count of enabled sensors based on configuration:
   *          - DHT: 1 if enabled
   *          - DS18B20: Count from configuration
   *          - SDS011: 1 if enabled
   *          - MHZ19: 1 if enabled
   *          - Analog: Count from configuration
   */
  static constexpr size_t getEnabledSensorCount() {
    size_t count =
        (USE_DHT ? 1 : 0);
#if USE_ANALOG
    count += ANALOG_SENSOR_COUNT;
#endif
    return count;
  }

  /**
   * @brief Get number of measurements provided by DHT sensor
   * @return Number of DHT measurements (2 if enabled, 0 if disabled)
   * @details DHT sensors always provide 2 measurements when enabled:
   *          - Temperature
   *          - Humidity
   */
  static constexpr size_t getDHTMeasurementCount() { return USE_DHT ? 2 : 0; }


  /**
   * @brief Calculate total number of measurements across all sensors
   * @return Total count of measurements from all enabled sensors
   * @details Sums up measurements from all sensor types:
   *          - DHT: 2 measurements if enabled
   *          - DS18B20: 1 measurement per sensor
   *          - SDS011: 2 measurements if enabled
   *          - MHZ19: 1 measurement if enabled
   *          - Analog: 1 measurement per sensor
   */
  static constexpr size_t getTotalMeasurementCount() {
    size_t count = getDHTMeasurementCount();
#if USE_ANALOG
    count += ANALOG_SENSOR_COUNT;
#endif
    return count;
  }

  /**
   * @brief Calculate maximum memory needed for collected measurements
   * @return Maximum number of measurement values that need to be stored
   * @details Returns the total measurement count, accounting for sensors
   *          that produce multiple values. This is used for memory allocation
   *          in measurement storage systems.
   */
  static constexpr size_t getMaxCollectedMeasurements() {
    return getTotalMeasurementCount();
  }

  /**
   * @brief Get maximum number of simultaneous measurements possible
   * @return Maximum number of sensors that could measure simultaneously
   * @details Returns the total number of enabled sensors, as this represents
   *          the maximum number of simultaneous measurement operations that
   *          could occur. Used for resource allocation in measurement systems.
   */
  static constexpr size_t getMaxSimultaneousMeasurements() {
    return getEnabledSensorCount();
  }
};

#endif  // SENSOR_COUNT_H
