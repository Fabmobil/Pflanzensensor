/**
 * @file sensor_measurement.h
 * @brief Sensor measurement state machine implementation
 * @details Provides comprehensive measurement handling including:
 *          - State management
 *          - Error handling
 *          - Timeout monitoring
 *          - Logging
 *          - Data transmission
 */

#ifndef SENSOR_MEASUREMENT_H
#define SENSOR_MEASUREMENT_H

#include <memory>

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "sensors/sensors.h"

/**
 * @class SensorMeasurement
 * @brief Manages the measurement lifecycle of a sensor
 * @details Implements a state machine that handles:
 *          - Measurement initialization
 *          - State transitions
 *          - Error recovery
 *          - Data collection
 *          - Result processing
 */
class SensorMeasurement {
public:
  /**
   * @brief Constructor for sensor measurement handler
   * @param sensor Pointer to sensor instance to manage
   * @details Initializes measurement handler with:
   *          - Sensor reference
   *          - State tracking
   *          - Timing management
   *          - Error counters
   */
  explicit SensorMeasurement(Sensor* sensor)
      : m_sensor(sensor),
        m_retryCount(0),
        m_measurementStartTime(0),
        m_lastStateChange(0),
        m_stateDebugPrinted(false) {}

private:
  Sensor* m_sensor;                                     ///< Managed sensor instance
  uint8_t m_retryCount;                                 ///< Current retry attempt count
  unsigned long m_measurementStartTime;                 ///< Start time of current measurement
  unsigned long m_lastStateChange;                      ///< Timestamp of last state change
  MeasurementState m_lastState{MeasurementState::IDLE}; ///< Previous state
  bool m_stateDebugPrinted;                             ///< State change logging flag

  /// Maximum number of retry attempts
  static constexpr uint8_t MAX_RETRIES = 3;
  /// Timeout duration for state transitions (30 seconds)
  static constexpr unsigned long STATE_TIMEOUT = 30000;
};

#endif // SENSOR_MEASUREMENT_H
