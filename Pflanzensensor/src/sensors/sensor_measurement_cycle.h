/**
 * @file sensor_measurement_cycle.h
 * @brief Manages the measurement cycle of individual sensors
 * @details Implements a state machine that controls the complete lifecycle of a
 * sensor measurement, including initialization, measurement, data processing,
 * and cleanup.
 */
#ifndef SENSOR_MEASUREMENT_CYCLE_MANAGER_H
#define SENSOR_MEASUREMENT_CYCLE_MANAGER_H

#include <memory>

#include "influxdb/influxdb.h"
#include "managers/manager_config.h"
#include "sensor_measurement_state.h"
#include "sensors/sensor_manager_limiter.h"
#include "sensors/sensors.h"

/**
 * @class SensorMeasurementCycleManager
 * @brief Manages the complete measurement cycle for a single sensor
 * @details Implements a state machine that handles all aspects of sensor
 * measurement, including timing, initialization, measurement, data processing,
 * and error handling.
 */
class SensorMeasurementCycleManager {
 public:
  /**
   * @brief Constructor for the measurement cycle manager
   * @param sensor Pointer to the sensor to manage
   */
  explicit SensorMeasurementCycleManager(Sensor* sensor);

  /**
   * @brief Updates the measurement cycle state machine
   * @return true if the cycle is complete, false if still in progress
   * @details This is the main method that should be called regularly to
   * progress through the measurement cycle states.
   */
  bool updateMeasurementCycle();

  /**
   * @brief Resets the measurement cycle to its initial state
   */
  void reset();

  /**
   * @brief Gets the current state of the measurement cycle
   * @return Current MeasurementState
   */
  MeasurementState getCurrentState() const;

  /**
   * @brief Gets the last error message if any
   * @return Reference to the last error message string
   */
  const String& getLastError() const;

  /**
   * @brief Checks if it's time for the next measurement
   * @return true if a new measurement is due
   */
  bool isDue() const { return m_state.isDue(); }

  /**
   * @brief Forces the next measurement for this sensor ASAP
   */
  void forceImmediateMeasurement() {
    unsigned long now = millis();
    m_state.setState(MeasurementState::WAITING_FOR_DUE,
                     m_sensor ? m_sensor->getId() : "");
    m_state.nextDueTime = now;
  }

 private:
  // Timeouts and delays
  static constexpr unsigned long INIT_TIMEOUT =
      5000;  ///< Timeout for initialization (5 seconds)
  static constexpr unsigned long MEASURE_TIMEOUT =
      30000;  ///< Timeout for measurement (30 seconds)
  static constexpr unsigned long ERROR_RETRY_DELAY =
      1000;  ///< Delay before retrying after error (1 second)
  static constexpr unsigned long INIT_DELAY =
      100;  ///< Delay after initialization (100ms)
  static constexpr unsigned long WARMUP_DELAY =
      100;  ///< Delay after warmup (100ms)
  static constexpr unsigned long DEBUG_INTERVAL =
      5000;  ///< Interval between debug logs (5 seconds)
  static constexpr unsigned long SLOT_RETRY_DELAY =
      50;  ///< Delay between slot attempts (50ms, reduced from 100ms)
  static constexpr unsigned long SLOT_TIMEOUT =
      30000;  ///< Maximum slot hold time (30 seconds)

  // Member variables
  Sensor* m_sensor;              ///< Pointer to the managed sensor
  MeasurementStateInfo m_state;  ///< Current state information
  MeasurementState m_lastState;  ///< Previous state for transition tracking
  std::vector<float> m_currentResults;  ///< Current measurement results
  // **CRITICAL FIX: Remove local MeasurementData copy to prevent memory
  // corruption** We'll work directly with the sensor's MeasurementData instead
  unsigned long m_lastDebugTime{0};  ///< Last debug message timestamp
  unsigned long m_cycleStartTime{
      0};  ///< Start time of current measurement cycle
  unsigned long m_lastSlotAttemptTime{
      0};  ///< Last attempt to acquire measurement slot
  unsigned long m_slotRequestStartTime{
      0};  ///< When current slot request started

  // State handlers (defined in separate files)

  /**
   * @brief Handles the WAITING_FOR_DUE state
   * @return true if state processing is complete
   */
  bool handleWaitingForDue();

  /**
   * @brief Handles the WAITING_FOR_SLOT state
   */
  void handleWaitingForSlot();

  /**
   * @brief Handles the WAITING_FOR_DELAY state
   */
  void handleWaitingForDelay();

  /**
   * @brief Handles the WARMUP state
   */
  void handleWarmup();

  /**
   * @brief Handles the MEASURING state
   */
  void handleMeasuring();

  // Initialization handlers (defined in
  // sensor_measurement_cycle_initialization.cpp)

  /**
   * @brief Handles the INITIALIZING state
   */
  void handleInitializing();

  // Data processing handlers (defined in
  // sensor_measurement_cycle_data_processing.cpp)

  /**
   * @brief Handles the PROCESSING state
   */
  void handleProcessing();

  /**
   * @brief Handles the SENDING_INFLUX state
   */
  void handleSendingInflux();

  /**
   * @brief Handles the DEINITIALIZING state
   */
  void handleDeinitializing();

  /**
   * @brief Logs the measurement results
   */
  void logMeasurementResults();

  // Error handling (defined in sensor_measurement_cycle_error_handling.cpp)

  /**
   * @brief Handles the ERROR state
   */
  void handleError();

  /**
   * @brief Handles unknown states
   */
  void handleUnknownState();

  /**
   * @brief Handles errors that occur during state processing
   * @param error Description of the error
   */
  void handleStateError(const String& error);

  /**
   * @brief Handles C++ exceptions during processing
   * @param e The caught exception
   */
  void handleException(const std::exception& e);

  /**
   * @brief Deactivates the sensor after fatal errors
   */
  void deactivateSensor();

  /**
   * @brief Checks if the slot request has timed out
   * @return true if the slot request has exceeded the timeout
   */
  bool checkSlotTimeout();

  /**
   * @brief Initiates a new slot request
   */
  void startSlotRequest();
};

#endif  // SENSOR_MEASUREMENT_CYCLE_MANAGER_H
