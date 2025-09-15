/**
 * @file sensor_bmp280.h
 * @brief BMP280 sensor class for pressure and temperature measurement
 */

#ifndef SENSOR_BMP280_H
#define SENSOR_BMP280_H

#include "configs/config.h"
#include "sensors/sensors.h"

#if USE_BMP280
#include <Adafruit_BMP280.h>

/**
 * @brief Configuration structure for BMP280 sensor
 * @details Contains all configuration parameters needed for BMP280 sensor
 * operation
 */
struct BMP280Config : public SensorConfig {
  uint8_t sckPin;  ///< SCK pin for I2C communication
  uint8_t sdiPin;  ///< SDI pin for I2C communication

  /**
   * @brief Default constructor for BMP280Config
   * @details Sets id to 'BMP280'.
   */
  BMP280Config() : sckPin(BMP280_SCK_PIN), sdiPin(BMP280_SDI_PIN) {
    name = F("BMP280");
    id = F("BMP280");  // Unified ID
    activeMeasurements = 2;
    if (measurementInterval == 0)
      measurementInterval = BMP280_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = BMP280_MINIMUM_DELAY;
  }
};

/**
 * @brief Class for managing BMP280 temperature and pressure sensor
 * @details Handles initialization, measurement, and reading of BMP280 sensor
 * data
 */
class BMP280Sensor : public Sensor {
 public:
  /**
   * @brief Construct a new BMP280Sensor object
   * @param config Configuration for the sensor
   */
  explicit BMP280Sensor(const BMP280Config& config,
                        class SensorManager* sensorManager);

  /**
   * @brief Initialize the sensor
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
   * @brief Continue an ongoing measurement
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
  bool isValidValue(float value) const override { return !isnan(value); }

  /**
   * @brief Validate a measurement value for a specific measurement
   * @param value Value to validate
   * @param measurementIndex Index of measurement (0=temp, 1=pressure)
   * @return true if value is valid
   * @override
   */
  bool isValidValue(float value, size_t measurementIndex) const override;

  /**
   * @brief Get shared hardware information
   * @return SharedHardwareInfo structure with sensor details
   * @override
   */
  SharedHardwareInfo getSharedHardwareInfo() const override;

  /**
   * @brief Destructor
   */
  ~BMP280Sensor() override;

  /**
   * @brief Fetch a single sample for a given BMP280 measurement (0=temp,
   * 1=pressure)
   * @param value Reference to store the sample
   * @param index Measurement index
   * @return true if successful, false if hardware error
   */
  bool fetchSample(float& value, size_t index) override;

 protected:
  /**
   * @brief Returns the number of measurements for BMP280Sensor (2: temperature
   * and pressure)
   */
  size_t getNumMeasurements() const override { return 2; }

 private:
  static constexpr size_t REQUIRED_SAMPLES = 3;
  static constexpr uint8_t BMP280_I2C_ADDRESS = 0x76;  // Default I2C address

  // Hardware configuration
  const BMP280Config m_bmp280Config;

  // Hardware interface
  std::unique_ptr<Adafruit_BMP280> m_bmp280;

  // Measurement state
  struct MeasurementState {
    unsigned long lastHardwareAccess{0};
    unsigned long operationStartTime{0};
    bool readInProgress{false};

    void reset() {
      readInProgress = false;
      lastHardwareAccess = 0;
      operationStartTime = 0;
    }
  } m_state;

  // Helper methods
  const BMP280Config& bmp280Config() const {
    return static_cast<const BMP280Config&>(config());
  }
  bool canAccessHardware() const;
};

#endif  // USE_BMP280
#endif  // SENSOR_BMP280_H
