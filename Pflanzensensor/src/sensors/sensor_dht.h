/**
 * @file sensor_dht.h
 * @brief DHT sensor class for temperature and humidity
 */

#ifndef SENSOR_DHT_H
#define SENSOR_DHT_H

#include <memory>
#include <vector>

#include "configs/config.h"
#include "managers/manager_config.h"
#include "sensors/sensors.h"

/**
 * @brief Configuration structure for DHT sensor
 */
struct DHTConfig : public SensorConfig {
  uint8_t pin;   ///< Digital pin for DHT sensor
  uint8_t type;  ///< DHT sensor type (DHT11, DHT22, etc.)

  /**
   * @brief Default constructor for DHTConfig
   * @details Sets pin to DHT_PIN and type to 0 (must be set in cpp). Sets id to
   * 'DHT'.
   */
  DHTConfig() : pin(DHT_PIN), type(0) {
    name = F("DHT");
    id = F("DHT");  // Unified ID
    activeMeasurements = 2;
    if (measurementInterval == 0)
      measurementInterval = DHT_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = DHT_MINIMUM_DELAY;
  }
};

/**
 * @class DHTSensor
 * @brief DHT temperature and humidity sensor implementation
 * @details Handles reading from DHT11 and DHT22 sensors, including
 * temperature and humidity measurements with validation and error handling.
 */
class DHTSensor : public Sensor {
 public:
  /**
   * @brief Constructor
   * @param config Sensor configuration
   */
  explicit DHTSensor(const DHTConfig& config,
                     class SensorManager* sensorManager);

  /**
   * @brief Destructor
   */
  ~DHTSensor() override;

  // Core sensor interface implementation
  /**
   * @brief Initializes the DHT sensor
   * @return SensorResult indicating success or failure with error details
   */
  SensorResult init() override;

  /**
   * @brief Validates a measurement value
   * @param value Value to validate
   * @return true if value is valid
   */
  bool isValidValue(float value) const override {
    return !isnan(value) && value > -100.0f && value < 200.0f;
  }

  /**
   * @brief Validates a measurement value for a specific measurement
   * @param value Value to validate
   * @param measurementIndex Index of the measurement (0=temperature,
   * 1=humidity)
   * @return true if value is valid
   */
  bool isValidValue(float value, size_t measurementIndex) const override;

  /**
   * @brief Deinitializes the sensor
   */
  void deinitialize() override;

  /**
   * @brief Checks if sensor requires warmup
   * @param[out] warmupTime Required warmup time in milliseconds
   * @return true if warmup is required
   */
  bool requiresWarmup(unsigned long& warmupTime) const override;

  /**
   * @brief Checks if sensor needs warmup before each measurement
   * @return true if measurement warmup is needed
   */
  bool isMeasurementWarmupSensor() const override { return true; }

  /**
   * @brief Gets shared hardware information
   * @return SharedHardwareInfo containing hardware details
   */
  SharedHardwareInfo getSharedHardwareInfo() const override;

  /**
   * @brief Fetch a single sample for a given measurement index (0=temp,
   * 1=humidity)
   * @param value Reference to store the sample
   * @param index Measurement index
   * @return true if successful, false if hardware error
   */
  bool fetchSample(float& value, size_t index) override;

  /**
   * @brief Starts a new measurement for the DHT sensor
   * @return SensorResult indicating success or failure
   */
  SensorResult startMeasurement() override;

  /**
   * @brief Continues an in-progress measurement for the DHT sensor
   * @return SensorResult indicating success, failure, or measurement completion
   */
  SensorResult continueMeasurement() override;

  /**
   * @brief Log sensor-specific debug details
   */
  void logDebugDetails() const override;

 protected:
  /**
   * @brief Returns the number of measurements for DHTSensor (2: temperature and
   * humidity)
   */
  size_t getNumMeasurements() const override { return 2; }

 private:
  // DHTConfig m_config;  ///< DHT-specific configuration - removed, using
  // manager's config instead DHTesp m_dhtesp; ///< DHTesp sensor instance
  uint8_t m_pin;   ///< GPIO pin number
  uint8_t m_type;  ///< DHT sensor type (DHT11, DHT22)

  // Measurement state
  struct MeasurementState {
    bool readInProgress{false};           ///< Whether a read is in progress
    unsigned long operationStartTime{0};  ///< When the operation started
    bool readingTemperature{true};        ///< Whether reading temperature
    unsigned long lastHardwareAccess{0};  ///< Last hardware access time
  } m_state;

  static constexpr uint8_t REQUIRED_SAMPLES = 3;  ///< Number of samples to take
  static constexpr unsigned long HARDWARE_ACCESS_DELAY_MS =
      1000;  ///< Delay between hardware accesses (reduced from 2000ms)

  /**
   * @brief Validates temperature reading
   * @param value Temperature value to validate
   * @return true if temperature is valid
   */
  bool isValidTemperature(float value) const {
    return !isnan(value) && value >= -40.0f && value <= 80.0f;
  }

  /**
   * @brief Validates humidity reading
   * @param value Humidity value to validate
   * @return true if humidity is valid
   */
  bool isValidHumidity(float value) const {
    return !isnan(value) && value >= 0.0f && value <= 100.0f;
  }

  /**
   * @brief Checks if hardware can be accessed
   * @return true if hardware can be accessed
   */
  bool canAccessHardware() const;
};

#endif  // SENSOR_DHT_H
