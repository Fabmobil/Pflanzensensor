/**
 * @file sensors.h
 * @brief Base Sensor class and interface for all sensor types
 */
#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

#include <memory>
#include <vector>

#include "logger/logger.h"
#include "sensor_config.h"  // Ensure configuration macros are defined first
#include "sensor_measurement_state.h"
#include "sensor_types.h"
#include "utils/result_types.h"

// Forward declaration
class SensorManager;

// --- Threshold utility declarations ---
struct ThresholdDefaults {
  float yellowLow;
  float greenLow;
  float greenHigh;
  float yellowHigh;
};

Thresholds getOrInitThresholds(const String& key,
                               const ThresholdDefaults& macroDefaults);

/**
 * @struct SensorMeasurementState
 * @brief Generic state for sensor measurement cycles (used by base Sensor)
 */
struct SensorMeasurementState {
  bool readInProgress = false;  ///< True if a measurement is in progress
  unsigned long operationStartTime = 0;     ///< When the measurement started
  size_t sampleCount = 0;                   ///< Number of samples collected
  std::vector<std::vector<float>> samples;  ///< [measurement][sample]
  unsigned long lastSampleTime =
      0;  ///< Timestamp of last sample (for nonblocking delay)
  size_t measurementIndex = 0;      ///< Current measurement index in cycle
  size_t sampleIndex = 0;           ///< Current sample index for measurement
  bool measurementStarted = false;  ///< True if measurement cycle started
  // Extend as needed for derived sensors
};

/**
 * @class Sensor
 * @brief Base class for all sensor implementations
 * @details Abstract base class that defines the interface and common
 * functionality for all sensor types in the system. Handles sensor lifecycle,
 * measurement management, and error handling.
 */
class Sensor {
 public:
 protected:
  class SensorManager* m_sensorManager;  ///< Reference to sensor manager
  String m_id;                           ///< Local copy of sensor ID
  SensorConfig m_tempConfig;  ///< Sensor configuration (stored locally)
  bool m_enabled{false};      ///< Whether the sensor is enabled
  bool m_initialized{false};  ///< Whether the sensor is initialized
  uint8_t m_errorCount{0};    ///< Count of consecutive errors
  unsigned long m_measurementInterval{0};  ///< Time between measurements
  std::unique_ptr<MeasurementData>
      m_lastMeasurementData;  ///< Pointer to measurement data
  std::vector<String>
      m_statuses;  ///< Current sensor status for each measurement
  MeasurementStateInfo m_stateInfo;    ///< Current state information
  bool m_isInWarmup{false};            ///< Whether sensor is warming up
  unsigned long m_warmupStartTime{0};  ///< When warmup started
  unsigned long m_warmupTime{0};       ///< Required warmup duration
  bool m_measurementDataValid{
      false};  ///< True if m_lastMeasurementData is valid and owned

  static constexpr uint8_t MAX_RETRIES =
      3;  ///< Maximum number of retry attempts
  static constexpr uint8_t MAX_INVALID_READINGS =
      3;  ///< Maximum consecutive invalid readings
  static constexpr unsigned long RETRY_DELAY_MS =
      1000;  ///< Delay between retries in ms

  // State tracking for error handling
  struct ErrorState {
    uint8_t errorCount{0};    ///< Number of hard errors
    uint8_t invalidCount{0};  ///< Number of consecutive invalid readings
    unsigned long lastInvalidTime{0};  ///< Timestamp of last invalid reading
    bool inRetryDelay{false};          ///< Whether we're in retry delay period
  } m_errorState;

  /**
   * @brief Initializes a measurement configuration
   * @param index Index of the measurement to initialize
   * @param name Human-readable name of the measurement
   * @param fieldName Field name for data storage
   * @param unit Unit of measurement
   * @param yellowLow Lower warning threshold
   * @param greenLow Lower normal threshold
   * @param greenHigh Upper normal threshold
   * @param yellowHigh Upper warning threshold
   */
  void initMeasurement(size_t index, const String& name,
                       const String& fieldName, const String& unit,
                       float yellowLow, float greenLow, float greenHigh,
                       float yellowHigh);

  /**
   * @brief Handle invalid readings with non-blocking retry delay
   * @param value The invalid value for logging
   * @return SensorResult indicating whether to retry or if error limit reached
   */
  SensorResult handleInvalidReading(float value);

  /**
   * @brief Check if we should wait before retrying
   * @return true if still in retry delay period
   */
  bool isInRetryDelay() const {
    return m_errorState.inRetryDelay &&
           (millis() - m_errorState.lastInvalidTime < RETRY_DELAY_MS);
  }

  /**
   * @brief Reset invalid reading counter
   */
  void resetInvalidCount() {
    m_errorState.invalidCount = 0;
    m_errorState.inRetryDelay = false;
  }

 public:
  /**
   * @brief Constructor for Sensor
   * @param config The sensor configuration
   * @param sensorManager Pointer to the sensor manager (can be null during
   * construction)
   */
  explicit Sensor(const SensorConfig& config,
                  class SensorManager* sensorManager);

  /**
   * @brief Virtual destructor
   */
  virtual ~Sensor();

  // Core initialization and measurement methods
  /**
   * @brief Initializes the sensor hardware
   * @return SensorResult indicating success or failure with error details
   */
  virtual SensorResult init() = 0;

  /**
   * @brief Alternative initialization method
   * @return SensorResult indicating success or failure with error details
   */
  virtual SensorResult initialize() { return init(); }

  /**
   * @brief Starts a new measurement
   * @return SensorResult indicating success or failure with error details
   */
  virtual SensorResult startMeasurement() = 0;

  /**
   * @brief Continues an in-progress measurement
   * @return SensorResult indicating success, failure, or measurement completion
   */
  virtual SensorResult continueMeasurement() = 0;

  /**
   * @brief Validates a measurement value
   * @param value Value to validate
   * @return true if value is valid
   */
  virtual bool isValidValue(float value) const = 0;

  /**
   * @brief Validates a measurement value for a specific measurement
   * @param value Value to validate
   * @param measurementIndex Index of the measurement
   * @return true if value is valid
   */
  virtual bool isValidValue(float value, size_t measurementIndex) const = 0;

  /**
   * @brief Validates the memory state of the sensor
   * @return SensorResult indicating success or failure with error details
   */
  SensorResult validateMemoryState() const;

  /**
   * @brief Resets the sensor's memory state for recovery
   * @return SensorResult indicating success or failure
   * @details Attempts to recover from memory corruption by reinitializing
   *          the measurement data structure
   */
  SensorResult resetMemoryState();

  /**
   * @brief Deinitializes the sensor
   */
  virtual void deinitialize();

  /**
   * @brief Handles sensor errors
   */
  virtual void handleSensorError();

  // State management
  /**
   * @brief Stops the sensor
   */
  void stop();

  /**
   * @brief Resets the measurement state
   */
  void resetMeasurementState();

  /**
   * @brief Forces the next measurement to occur immediately
   */
  void forceNextMeasurement();

  /**
   * @brief Checks if a measurement is due
   * @return true if measurement is due
   */
  virtual bool isDueMeasurement() const;

  // Warmup support
  /**
   * @brief Checks if sensor requires warmup
   * @param[out] warmupTime Required warmup time in milliseconds
   * @return true if warmup is required
   */
  virtual bool requiresWarmup(unsigned long& warmupTime) const;

  /**
   * @brief Checks if sensor needs initial warmup
   * @return true if initial warmup is needed
   */
  virtual bool isInitialWarmupSensor() const { return false; }

  /**
   * @brief Checks if sensor needs warmup before each measurement
   * @return true if measurement warmup is needed
   */
  virtual bool isMeasurementWarmupSensor() const { return false; }

  /**
   * @brief Starts the warmup process
   * @return SensorResult indicating success or failure with error details
   */
  virtual SensorResult startWarmup();

  /**
   * @brief Checks if warmup is complete
   * @return true if warmup is complete
   */
  virtual bool isWarmupComplete() const;

  /**
   * @brief Handler called when warmup completes
   */
  virtual void handleWarmupComplete() {}

  /**
   * @brief Checks if sensor should be deinitialized after measurement
   * @return true if deinitialization is needed
   */
  virtual bool shouldDeinitializeAfterMeasurement() const;

  // Status and state access
  /**
   * @brief Checks if sensor is initialized
   * @return true if initialized
   */
  inline bool isInitialized() const { return m_initialized; }

  /**
   * @brief Checks if sensor is enabled
   * @return true if enabled
   */
  inline bool isEnabled() const { return m_enabled; }

  /**
   * @brief Gets current sensor status for a specific measurement
   * @param measurementIndex Index of the measurement (defaults to 0 for
   * backward compatibility)
   * @return Status string for the specified measurement
   */
  const String& getStatus(size_t measurementIndex = 0) const;

  /**
   * @brief Updates sensor status based on measurement values and thresholds
   * @param measurementIndex Index of the measurement to update status for
   * @details Calculates and updates the sensor status based on the current
   *          measurement value and the sensor's threshold limits
   */
  void updateStatus(size_t measurementIndex = 0);

  /**
   * @brief Determines sensor status based on measurement value and thresholds
   * @param value The current measurement value
   * @param limits The sensor threshold limits
   * @param isOneSided True if sensor uses one-sided limits (like PM sensors),
   * false for two-sided
   * @return String representing the status: "green", "yellow", or "red"
   * @details Compares the measurement value against the sensor's threshold
   * limits to determine if the sensor is in a normal (green), warning (yellow),
   *          or critical (red) state.
   */
  static String determineSensorStatus(float value, const SensorLimits& limits,
                                      bool isOneSided = false);

  /**
   * @brief Gets error count
   * @return Number of consecutive errors
   */
  inline uint8_t getErrorCount() const { return m_errorCount; }

  /**
   * @brief Gets current measurement state
   * @return Current MeasurementState
   */
  inline MeasurementState getState() const { return m_stateInfo.state; }

  /**
   * @brief Gets sensor ID
   * @return Sensor ID string
   */
  const String& getId() const;

  /**
   * @brief Gets sensor name
   * @return Sensor name string
   */
  const String& getName() const;

  /**
   * @brief Gets measurement interval
   * @return Interval in milliseconds
   */
  inline unsigned long getMeasurementInterval() const {
    return m_measurementInterval;
  }

  /**
   * @brief Gets last measurement data
   * @return Reference to last measurement data, or static invalid if deinit
   */
  const MeasurementData& getMeasurementData() const {
    if (!m_measurementDataValid || !m_lastMeasurementData) {
      logger.error(getName(), F(": getMeasurementData() called after deinit!"));
      static MeasurementData invalidData;
      invalidData.invalidate();
      return invalidData;
    }
    return *m_lastMeasurementData;
  }

  /**
   * @brief Get the sensor configuration (const)
   * @return Reference to the sensor configuration
   */
  const SensorConfig& config() const;

  /**
   * @brief Get the sensor configuration (mutable)
   * @return Reference to the sensor configuration
   */
  SensorConfig& mutableConfig();

  /**
   * @brief Gets name of specific measurement
   * @param index Measurement index
   * @return Measurement name string
   */
  const String& getMeasurementName(size_t index) const;

  /**
   * @brief Gets start time of current measurement
   * @return Start time in milliseconds
   */
  unsigned long getMeasurementStartTime() const;

  /**
   * @brief Update the measurement data
   * @param data New measurement data to store
   */
  void updateMeasurementData(const MeasurementData& data);

  /**
   * @brief Returns the averaged results for each measurement channel
   * @return Vector of averaged values (one per channel)
   */
  virtual std::vector<float> getAveragedResults() const;

  // Status manipulation
  /**
   * @brief Sets sensor enabled state
   * @param enabled New enabled state
   */
  inline void setEnabled(bool enabled) { m_enabled = enabled; }

  /**
   * @brief Sets measurement interval
   * @param interval New interval in milliseconds
   */
  inline void setMeasurementInterval(unsigned long interval) {
    m_measurementInterval = interval;
  }

  /**
   * @brief Resets error count to zero
   */
  inline void resetErrorCount() { m_errorCount = 0; }

  bool shouldRetry() const;

  /**
   * @brief Sets sensor name
   * @param name New sensor name
   */
  void setName(const String& name);

  /**
   * @brief Gets shared hardware information
   * @return SharedHardwareInfo structure
   */
  virtual SharedHardwareInfo getSharedHardwareInfo() const;

  /**
   * @brief Generic measurement loop (template method)
   * @return SensorResult indicating success or failure
   * @details Handles sample collection, error handling, and averaging. Calls
   * fetchSample() for hardware access.
   */
  virtual SensorResult performMeasurementCycle();

  /**
   * @brief Updates last measurement timestamp
   */
  void updateLastMeasurementTime();

 protected:
  /**
   * @brief Log a debug message if sensor debug is enabled
   * @param msg The message to log
   */
  inline void logDebug(const String& msg) const {
    if (ConfigMgr.isDebugSensor()) {
      logger.debug(getName(), msg);
    }
  }

  /**
   * @brief Log sensor-specific debug details (override in derived classes)
   */
  virtual void logDebugDetails() const {}

  /**
   * @brief Returns the number of measurements for this sensor (override in
   * derived if needed)
   * @return Number of measurements (e.g., 2 for DHT: temp+humidity)
   */
  virtual size_t getNumMeasurements() const;

  /**
   * @brief Sets new measurement state
   * @param newState State to transition to
   */
  void setState(MeasurementState newState);

  /**
   * @brief Helper to clear and shrink a std::vector (frees memory)
   * @tparam T Vector element type
   * @param vec Reference to the vector to clear and shrink
   */
  template <typename T>
  static void clearAndShrink(std::vector<T>& vec) {
    vec.clear();
    vec.shrink_to_fit();
  }

  /**
   * @brief Fetch a single sample for a given measurement index (to be
   * implemented by derived)
   * @param value Reference to store the sample
   * @param index Measurement index (e.g., 0=temp, 1=humidity)
   * @return true if successful, false if hardware error
   */
  virtual bool fetchSample(float& value, size_t index) = 0;

  /**
   * @brief Computes the average of the collected samples for each channel
   * @return Vector of averaged values (one per channel)
   */
  std::vector<float> averageSamples() const;

  SensorMeasurementState m_state;  ///< Generic measurement state
};

#endif  // SENSORS_H
