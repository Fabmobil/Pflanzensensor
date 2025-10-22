/**
 * @file sensor_measurement_state.h
 * @brief Defines the state machine and tracking for sensor measurement cycles
 * @details This file contains the core state management for sensor
 * measurements, including state transitions, timing, and error handling.
 */
#ifndef SENSOR_MEASUREMENT_STATE_H
#define SENSOR_MEASUREMENT_STATE_H

#include <Arduino.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "sensors/sensor_types.h"

/**
 * @enum MeasurementState
 * @brief Enumerates the possible states of a sensor measurement cycle
 * @details Defines the complete lifecycle of a measurement from waiting to
 * completion
 */
enum class MeasurementState {
  WAITING_FOR_DUE,   /**< Waiting for next measurement interval */
  WAITING_FOR_SLOT,  /**< Waiting for measurement slot to become available */
  WAITING_FOR_DELAY, /**< Waiting for minimum delay between operations */
  INITIALIZING,      /**< Sensor is being initialized */
  WARMUP,            /**< Sensor is warming up (if needed) */
  MEASURING,         /**< Taking measurements */
  PROCESSING,        /**< Processing measurement results */
  SENDING_INFLUX,    /**< Sending data to InfluxDB */
  DEINITIALIZING,    /**< Sensor is being deinitialized */
  ERROR              /**< Error state */
};

/**
 * @struct MeasurementStateInfo
 * @brief Structure to track the state and timing information of a measurement
 * cycle
 * @details Maintains comprehensive state information including timing, error
 * tracking, and flags for the measurement process. This structure is the core
 * of the measurement state machine.
 */
struct MeasurementStateInfo {
  MeasurementState state{MeasurementState::WAITING_FOR_DUE}; /**< Current state of the measurement
                                             cycle */
  unsigned long stateStartTime{0};   /**< Timestamp when the current state started (milliseconds) */
  unsigned long lastAttemptTime{0};  /**< Timestamp of the last slot attempt (milliseconds) */
  uint8_t errorCount{0};             /**< Number of consecutive errors encountered */
  bool needsInitialization{true};    /**< Flag indicating if sensor initialization is needed */
  bool needsWarmup{false};           /**< Flag indicating if sensor warmup period is needed */
  bool measurementStarted{false};    /**< Flag indicating if measurement process has started */
  unsigned long warmupTimeNeeded{0}; /**< Required warmup time in milliseconds */
  unsigned long warmupStartTime{0};  /**< Timestamp when warmup period started */

  // Timing information
  unsigned long lastMeasurementTime{0}; /**< Timestamp of the last successful measurement */
  unsigned long nextDueTime{0};         /**< Timestamp when the next measurement should start */
  unsigned long minimumDelayEndTime{
      0}; /**< Timestamp when the minimum delay between operations ends */
  unsigned long measurementInterval{0}; /**< Interval between measurements in milliseconds */

  // Error tracking
  String lastError;               /**< Description of the most recent error */
  unsigned long lastErrorTime{0}; /**< Timestamp when the last error occurred */
  bool fatalError{false};         /**< Flag indicating an unrecoverable error */

  /**
   * @brief Sets the current state to a new state
   * @param newState The new state to transition to
   * @param sensorName Optional sensor name for logging purposes
   * @details Handles state transition logging and updates timing information.
   *          Also manages the measurementStarted flag for the MEASURING state.
   */
  void setState(MeasurementState newState, const String& sensorName = "") {
    if (state != newState) {
      if (ConfigMgr.isDebugMeasurementCycle()) {
        String transition =
            sensorName + F(": State ") + stateToString(state) + F(" -> ") + stateToString(newState);
        logger.debug(F("MeasurementState"), transition);
      }
      state = newState;
      stateStartTime = millis();

      // Reset measurementStarted flag when transitioning to or from MEASURING
      // state
      if (newState == MeasurementState::MEASURING) {
        measurementStarted = false;
      }
    }
  }

  /**
   * @brief Determines if the measurement is due based on the current time
   * @return true if it's time for the next measurement
   * @details Compares the current time against the scheduled nextDueTime
   */
  bool isDue() const { return millis() >= nextDueTime; }

  /**
   * @brief Schedules the next measurement based on the base time and interval
   * @param baseTime The reference time for scheduling (usually current time)
   * @param interval The time between measurements in milliseconds
   * @details Updates both the last measurement time and calculates the next due
   * time
   */
  void scheduleNextMeasurement(unsigned long baseTime, unsigned long interval) {
    measurementInterval = interval;
    lastMeasurementTime = baseTime;
    nextDueTime = baseTime + interval;
  }

  /**
   * @brief Sets the minimum delay required between operations
   * @param delay The minimum delay in milliseconds
   * @details Calculates and stores the timestamp when the delay period will end
   */
  void setMinimumDelay(unsigned long delay) { minimumDelayEndTime = millis() + delay; }

  /**
   * @brief Checks if the minimum delay has elapsed
   * @return true if the minimum delay period has passed
   * @details Compares current time against the calculated end time of the delay
   */
  bool isMinimumDelayElapsed() const { return millis() >= minimumDelayEndTime; }

  /**
   * @brief Records an error and updates error tracking information
   * @param error Description of the error that occurred
   * @details Updates error count and may set fatal error flag if maximum errors
   * exceeded
   * @see MEASUREMENT_ERROR_COUNT
   */
  void recordError(const String& error) {
    lastError = error;
    lastErrorTime = millis(); /**< Update error time when recording error */
    errorCount++;
    if (errorCount >= MEASUREMENT_ERROR_COUNT) {
      fatalError = true;
    }
  }

  /**
   * @brief Resets the measurement state to its initial values
   * @details Resets all state tracking except measurementInterval.
   *          Used when restarting the measurement cycle or recovering from
   * errors.
   */
  void reset() {
    state = MeasurementState::WAITING_FOR_DUE;
    stateStartTime = millis();
    errorCount = 0;
    fatalError = false;
    lastError = "";
    needsInitialization = true;
    warmupStartTime = 0;
    lastErrorTime = 0;
    measurementStarted = false;
    // Note: measurementInterval is not reset to persist across cycles
  }

  /**
   * @brief Converts a MeasurementState enum value to its string representation
   * @param state The MeasurementState to convert
   * @return String representation of the state
   * @details Provides human-readable names for all possible measurement states
   */
  static String stateToString(MeasurementState state) {
    switch (state) {
    case MeasurementState::WAITING_FOR_DUE:
      return "WAITING_FOR_DUE";
    case MeasurementState::WAITING_FOR_SLOT:
      return "WAITING_FOR_SLOT";
    case MeasurementState::WAITING_FOR_DELAY:
      return "WAITING_FOR_DELAY";
    case MeasurementState::INITIALIZING:
      return "INITIALIZING";
    case MeasurementState::WARMUP:
      return "WARMUP";
    case MeasurementState::MEASURING:
      return "MEASURING";
    case MeasurementState::PROCESSING:
      return "PROCESSING";
    case MeasurementState::SENDING_INFLUX:
      return "SENDING_INFLUX";
    case MeasurementState::DEINITIALIZING:
      return "DEINITIALIZING";
    case MeasurementState::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
    }
  }
};

#endif // SENSOR_MEASUREMENT_STATE_H
