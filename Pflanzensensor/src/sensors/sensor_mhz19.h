/**
 * @file sensor_mhz19.h
 * @brief MH-Z19 sensor class for CO2 measurement
 */

#ifndef SENSOR_MHZ19_H
#define SENSOR_MHZ19_H

#include "sensors/sensors.h"

#if USE_MHZ19

/**
 * @brief Configuration structure for MH-Z19 CO2 sensor
 *
 * Contains all configuration parameters needed for MH-Z19 sensor operation
 */
struct MHZ19Config : public SensorConfig {
  uint8_t pwmPin;            ///< PWM input pin for reading CO2 values
  unsigned long warmupTime;  ///< Warmup time in seconds before valid readings

  /**
   * @brief Default constructor for MHZ19Config
   * @details Sets id to 'MHZ19'.
   */
  MHZ19Config() {
    name = F("MHZ19");
    id = F("MHZ19");         // Unified ID
    activeMeasurements = 1;  // CO2 only
    if (measurementInterval == 0)
      measurementInterval = MHZ19_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = MHZ19_MINIMUM_DELAY;
    pwmPin = MHZ19_PWM_PIN;
    warmupTime = MHZ19_WARMUP_TIME;  // Initial warmup time from config
  }
};

/**
 * @brief MH-Z19 CO2 sensor class implementation
 *
 * Implements the interface for reading CO2 concentration measurements
 * from an MH-Z19 sensor using PWM signal processing.
 */
class MHZ19Sensor : public Sensor {
 public:
  /**
   * @brief Construct a new MHZ19 Sensor object
   * @param config Configuration parameters for the sensor
   */
  explicit MHZ19Sensor(const MHZ19Config& config,
                       class SensorManager* sensorManager);

  /**
   * @brief Initialize the sensor
   * @return SensorResult indicating success or failure with error details
   * @return false if initialization failed
   * @override
   */
  SensorResult init() override;

  /**
   * @brief Start a new measurement cycle
   * @return SensorResult indicating success or failure with error details
   * @return false if measurement failed to start
   * @override
   */
  SensorResult startMeasurement() override;

  /**
   * @brief Continue an ongoing measurement
   * @return SensorResult indicating success, failure, or measurement completion
   * @return false if measurement needs more time
   * @override
   */
  SensorResult continueMeasurement() override;

  /**
   * @brief Clean up sensor resources
   * @override
   */
  void deinitialize() override;

  /**
   * @brief Validate a sensor reading
   * @param value The value to validate
   * @return true if value is within valid range
   * @return false if value is invalid
   * @override
   */
  bool isValidValue(float value) const override {
    return !isnan(value) && value >= MHZ19_MIN && value <= MHZ19_MAX;
  }

  /**
   * @brief Validate a sensor reading for a specific measurement index
   * @param value The value to validate
   * @param measurementIndex Index of the measurement
   * @return true if value is valid
   * @return false if value is invalid
   * @override
   */
  bool isValidValue(float value, size_t measurementIndex) const override {
    return isValidValue(value);
  }

  /**
   * @brief Get sensor warmup requirements
   * @param warmupTime Output parameter for required warmup time
   * @return true as this sensor requires warmup
   * @override
   */
  bool requiresWarmup(unsigned long& warmupTime) const override {
    warmupTime =
        m_mhz19Config.warmupTime * 1000UL;  // Convert seconds to milliseconds
    return true;
  }

  /**
   * @brief Check if sensor needs initial warmup
   * @return true as this sensor requires initial warmup
   * @override
   */
  bool isInitialWarmupSensor() const override { return true; }

  /**
   * @brief Get shared hardware information
   * @return SharedHardwareInfo containing sensor type and pin information
   * @override
   */
  SharedHardwareInfo getSharedHardwareInfo() const override {
    return SharedHardwareInfo(SensorType::MHZ19, m_mhz19Config.pwmPin,
                              m_mhz19Config.minimumDelay);
  }

  /**
   * @brief Destroy the MHZ19 Sensor object
   * @override
   */
  ~MHZ19Sensor() override;

  /**
   * @brief Fetch a single sample for the MHZ19 sensor (CO2 concentration)
   * @param value Reference to store the sample
   * @param index Measurement index (should be 0)
   * @return true if successful, false if hardware error
   */
  bool fetchSample(float& value, size_t index) override;

 protected:
  /**
   * @brief Returns the number of measurements for MHZ19Sensor (1: CO2
   * concentration)
   */
  size_t getNumMeasurements() const override { return 1; }

 private:
  static constexpr size_t REQUIRED_SAMPLES = 3;
  static constexpr unsigned long PWM_CYCLE = 1004;  // ms for one PWM cycle

  // Hardware configuration
  const MHZ19Config m_mhz19Config;  // Store the full MHZ19 config

  // Measurement state
  struct MeasurementState {
    unsigned long lastHardwareAccess{0};
    unsigned long operationStartTime{0};
    bool readInProgress{false};
  } m_state;

  // Helper methods
  const MHZ19Config& mhz19Config() const { return m_mhz19Config; }

  bool validateReading(float value) const;
  float calculatePPM(unsigned long th, unsigned long tl) const;
};

#endif  // USE_MHZ19
#endif  // SENSOR_MHZ19_H
