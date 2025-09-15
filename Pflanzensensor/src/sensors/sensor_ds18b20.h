/**
 * @file sensor_ds18b20.h
 * @brief DS18B20 sensor class for temperature measurement
 */

#ifndef SENSOR_DS18B20_H
#define SENSOR_DS18B20_H

#include "configs/config.h"
#include "sensors/sensors.h"

#if USE_DS18B20
#include <DallasTemperature.h>
#include <OneWire.h>

/**
 * @brief Configuration structure for DS18B20 temperature sensors
 *
 * Contains configuration parameters for one or more DS18B20 sensors
 * connected to the same OneWire bus.
 */
struct DS18B20Config : public SensorConfig {
  /** @brief OneWire bus pin number */
  uint8_t oneWireBus;
  /** @brief Number of DS18B20 sensors on the bus */
  size_t sensorCount;

  /**
   * @brief Default constructor for DS18B20Config
   * @details Sets id to 'DS18B20'.
   */
  DS18B20Config()
      : oneWireBus(ONE_WIRE_BUS), sensorCount(DS18B20_SENSOR_COUNT) {
    name = F("DS18B20");
    id = F("DS18B20");  // Unified ID
    activeMeasurements = sensorCount;
    if (measurementInterval == 0)
      measurementInterval = DS18B20_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = DS18B20_MINIMUM_DELAY;
  }
};

/**
 * @brief Class for managing DS18B20 temperature sensors
 *
 * Handles initialization, measurement, and reading of DS18B20 temperature
 * sensors connected via OneWire bus.
 */
class DS18B20Sensor : public Sensor {
 public:
  /**
   * @brief Construct a new DS18B20Sensor object
   * @param config Configuration parameters for the sensor
   */
  explicit DS18B20Sensor(const DS18B20Config& config,
                         class SensorManager* sensorManager);

  /**
   * @brief Check if a system restart has been requested
   * @return true if a restart is requested, false otherwise
   */
  bool isRestartRequested() const { return m_state.restartRequested; }

  /**
   * @brief Initialize the sensor hardware
   * @return SensorResult indicating success or failure with error details
   * @override
   */
  SensorResult init() override;

  /**
   * @brief Begin a new measurement cycle
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
   * @brief Clean up and deinitialize the sensor
   * @override
   */
  void deinitialize() override;

  /**
   * @brief Check if a temperature value is within valid range
   * @param value Temperature value to check
   * @return true if value is valid, false otherwise
   * @override
   */
  bool isValidValue(float value) const override {
    return !isnan(value) && value >= -55.0f && value <= 125.0f;
  }

  /**
   * @brief Check if a temperature value is valid for a specific sensor
   * @param value Temperature value to check
   * @param measurementIndex Index of the sensor
   * @return true if value is valid, false otherwise
   * @override
   */
  bool isValidValue(float value, size_t measurementIndex) const override {
    return isValidValue(value);
  }

  /**
   * @brief Get shared hardware information for this sensor
   * @return SharedHardwareInfo structure with sensor details
   * @override
   */
  SharedHardwareInfo getSharedHardwareInfo() const override {
    return SharedHardwareInfo(SensorType::DS18B20, ds18b20Config().oneWireBus,
                              ds18b20Config().minimumDelay);
  }

  /**
   * @brief Log sensor-specific debug details
   */
  void logDebugDetails() const override;

  /**
   * @brief Destroy the DS18B20Sensor object
   */
  ~DS18B20Sensor() override;

  /**
   * @brief Fetch a single sample for a given DS18B20 sensor
   * @param value Reference to store the sample
   * @param index Sensor index
   * @return true if successful, false if hardware error
   */
  bool fetchSample(float& value, size_t index) override;

  /**
   * @brief Nonblocking measurement cycle for DS18B20 (overrides base)
   * @return SensorResult indicating success, pending, or error
   */
  SensorResult performMeasurementCycle() override;

 protected:
  /**
   * @brief Returns the number of measurements for DS18B20Sensor (number of
   * active sensors)
   */
  size_t getNumMeasurements() const override;

 private:
  static constexpr unsigned long MAX_CONVERSION_TIME = 750;  // ms
  static constexpr uint8_t MAX_INIT_RETRIES = 5;
  static constexpr unsigned long INIT_RETRY_DELAY = 5000;  // 5 seconds

  // Hardware configuration
  const uint8_t m_oneWireBus;
  const size_t m_sensorCount;

  // Hardware interface
  std::unique_ptr<OneWire> m_oneWire;
  std::unique_ptr<DallasTemperature> m_sensors;

  // Measurement state
  struct MeasurementState {
    std::vector<float> readings;
    unsigned long lastHardwareAccess{0};
    unsigned long operationStartTime{0};
    bool readInProgress{false};
    bool conversionRequested{false};
    std::vector<float> lastValidReadings;
    std::vector<uint8_t> consecutiveInvalidCount;

    // Init retry tracking
    uint8_t initRetryCount{0};
    bool hasRestarted{false};
    unsigned long lastInitRetryTime{0};
    bool restartRequested{false};

    void reset(size_t sensorCount) {
      readings.clear();
      readings.resize(sensorCount, 0.0f);
      lastValidReadings.resize(sensorCount, 0.0f);
      consecutiveInvalidCount.resize(sensorCount, 0);
      readInProgress = false;
      conversionRequested = false;
      lastHardwareAccess = 0;
      operationStartTime = 0;
      // Don't reset retry counters here as they need to persist across
      // measurement cycles
      restartRequested = false;
    }
  } m_state;

  // Per-cycle state for nonblocking measurement
  size_t cycleMeasurementIndex = 0;
  unsigned long cycleConversionStart = 0;
  bool cycleConversionInProgress = false;

  // Helper methods
  const DS18B20Config& ds18b20Config() const {
    return static_cast<const DS18B20Config&>(config());
  }

  bool validateReading(float value) const;
  bool canAccessHardware() const;
  bool requestTemperatures();
};

#endif  // USE_DS18B20
#endif  // SENSOR_DS18B20_H
