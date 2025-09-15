/**
 * @file sensor_hx711.h
 * @brief HX711 sensor class for weight measurement
 */

#ifndef SENSOR_HX711_H
#define SENSOR_HX711_H

#if USE_HX711

#include <HX711.h>

#include "sensors/sensors.h"

/**
 * @brief Configuration structure for HX711 weight sensor
 *
 * Contains configuration parameters for HX711 sensor operation
 */
struct HX711Config : public SensorConfig {
  uint8_t doutPin;  ///< Data output pin
  uint8_t sckPin;   ///< Clock pin

  /**
   * @brief Default constructor for HX711Config
   * @details Sets id to 'HX711'.
   */
  HX711Config() {
    name = F("HX711");
    id = F("HX711");         // Unified ID
    activeMeasurements = 1;  // Weight only
    if (measurementInterval == 0)
      measurementInterval = HX711_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = HX711_MINIMUM_DELAY;
    doutPin = HX711_DOUT_PIN;
    sckPin = HX711_SCK_PIN;
  }
};

/**
 * @brief Class for managing HX711 weight sensor operations
 *
 * Handles initialization, measurement, and reading of HX711 load cell amplifier
 */
class HX711Sensor : public Sensor {
 public:
  /**
   * @brief Construct a new HX711Sensor object
   * @param config Configuration for the sensor
   */
  explicit HX711Sensor(const HX711Config& config,
                       class SensorManager* sensorManager);

  /**
   * @brief Initialize the sensor hardware
   * @return SensorResult indicating success or failure with error details
   * @override
   */
  SensorResult init() override;

  /**
   * @brief Start a new measurement
   * @return SensorResult indicating success or failure with error details
   * @override
   */
  SensorResult startMeasurement() override;

  /**
   * @brief Continue an in-progress measurement
   * @return SensorResult indicating success, failure, or measurement completion
   * @override
   */
  SensorResult continueMeasurement() override;

  /**
   * @brief Clean up sensor resources
   * @override
   */
  void deinitialize() override;

  /**
   * @brief Validate a measurement value
   * @param value Value to validate
   * @return true if value is valid
   * @override
   */
  bool isValidValue(float value) const override {
    return !isnan(value) && value >= 0.0f;
  }

  /**
   * @brief Validate a measurement value for a specific index
   * @param value Value to validate
   * @param measurementIndex Index of the measurement
   * @return true if value is valid
   * @override
   */
  bool isValidValue(float value, size_t measurementIndex) const override {
    return isValidValue(value);
  }

  /**
   * @brief Get shared hardware information
   * @return SharedHardwareInfo structure with sensor details
   * @override
   */
  SharedHardwareInfo getSharedHardwareInfo() const override {
    return SharedHardwareInfo(SensorType::HX711, hx711Config().doutPin,
                              hx711Config().minimumDelay);
  }

  /**
   * @brief Destructor
   */
  ~HX711Sensor() override;

  /**
   * @brief Fetch a single sample for the HX711 sensor (weight)
   * @param value Reference to store the sample
   * @param index Measurement index (should be 0)
   * @return true if successful, false if hardware error
   */
  bool fetchSample(float& value, size_t index) override;

 protected:
  /**
   * @brief Returns the number of measurements for HX711Sensor (1: weight)
   */
  size_t getNumMeasurements() const override { return 1; }

 private:
  static constexpr size_t REQUIRED_SAMPLES =
      3;  ///< Number of samples required for a valid reading

  // Hardware configuration
  const HX711Config& hx711Config() const {
    return static_cast<const HX711Config&>(config());
  }

  std::unique_ptr<HX711> m_scale;  ///< HX711 sensor instance

  // Measurement state
  struct MeasurementState {
    unsigned long lastHardwareAccess{0};
    unsigned long operationStartTime{0};
    bool readInProgress{false};

    void reset() { readInProgress = false; }
  } m_state;

  // Helper methods
  bool validateReading(float value) const;
  bool canAccessHardware() const;
};
#endif  // USE_HX/!!
#endif  // SENSOR_HX711_H
