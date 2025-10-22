/**
 * @file sensor_analog.h
 * @brief Analog sensor class for analog value measurement
 */

#ifndef SENSOR_ANALOG_H
#define SENSOR_ANALOG_H

#include "sensor_analog_multiplexer.h"
#include "sensors/sensor_autocalibration.h"
#include "sensors/sensors.h"

// Forward declarations
class SensorPersistence;

#if USE_ANALOG

/**
 * @brief Configuration structure for analog sensors
 *
 * Holds all configuration parameters needed for analog sensor operation
 * including pin assignments, multiplexer settings, and calibration values.
 */
struct AnalogConfig : public SensorConfig {
  uint8_t pin;                ///< The analog input pin to read from
  bool useMultiplexer;        ///< Whether to use multiplexer for multiple inputs
  unsigned long minimumDelay; ///< Minimum delay between readings

  /**
   * @brief Default constructor for AnalogConfig
   * @details Sets id to 'ANALOG'.
   */
  AnalogConfig()
      : SensorConfig(),
        pin(ANALOG_PIN),
        useMultiplexer(USE_MULTIPLEXER),
        minimumDelay(ANALOG_MINIMUM_DELAY) {
    name = F("Analog Sensor");
    id = F("ANALOG"); // Unified ID
    activeMeasurements = ANALOG_SENSOR_COUNT;
    if (measurementInterval == 0)
      measurementInterval = ANALOG_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = ANALOG_MINIMUM_DELAY;

    // Set min/max for each measurement in MeasurementConfig
    for (size_t i = 0; i < activeMeasurements && i < measurements.size(); ++i) {
      measurements[i].minValue = /* set from macro or default */ 0.0f;
      measurements[i].maxValue = /* set from macro or default */ 0.0f;

      // DO NOT initialize raw min/max values here - let JSON loading handle it
      // This prevents constructor values from overriding loaded JSON values
    }
  }
};

/**
 * @brief Class handling analog sensor operations
 *
 * Manages analog sensor initialization, measurement, and data processing
 * with support for multiplexed inputs and configurable sampling.
 */
class AnalogSensor : public Sensor {
public:
  /**
   * @brief Constructs an analog sensor instance
   * @param config The configuration for this analog sensor
   * @param sensorManager Reference to the sensor manager
   */
  explicit AnalogSensor(const AnalogConfig& config, class SensorManager* sensorManager);

  /**
   * @brief Initializes the analog sensor hardware
   * @return SensorResult indicating success or failure with error details
   * @override
   */
  SensorResult init() override;

  /**
   * @brief Begins a new measurement cycle
   * @return SensorResult indicating success or failure with error details
   * @override
   */
  SensorResult startMeasurement() override;

  /**
   * @brief Continues an in-progress measurement
   * @return SensorResult indicating success, failure, or measurement completion
   * @override
   */
  SensorResult continueMeasurement() override;

  /**
   * @brief Cleans up and releases hardware resources
   * @override
   */
  void deinitialize() override;

  /**
   * @brief Validates a measurement value
   * @param value The value to validate
   * @return true if value is valid, false otherwise
   * @override
   */
  bool isValidValue(float value) const override {
    return !isnan(value) && value >= 0.0f && value <= 100.0f;
  }

  /**
   * @brief Validates a measurement value for a specific index
   * @param value The value to validate
   * @param measurementIndex The index of the measurement
   * @return true if value is valid, false otherwise
   * @override
   */
  bool isValidValue(float value, size_t measurementIndex) const override {
    return isValidValue(value);
  }

  /**
   * @brief Gets information about shared hardware resources
   * @return SharedHardwareInfo structure with resource details
   * @override
   */
  SharedHardwareInfo getSharedHardwareInfo() const override {
    return SharedHardwareInfo(SensorType::ANALOG, m_analogConfig.pin, m_analogConfig.minimumDelay);
  }

  /**
   * @brief Destructor
   * @override
   */
  ~AnalogSensor() override;

  /**
   * @brief Fetch a single sample for a given analog channel
   * @param value Reference to store the sample
   * @param index Channel index
   * @return true if successful, false if hardware error
   *
   * @note For inverted sensors (inverted=true), the mapping is flipped:
   *       high raw values result in low percentages and vice versa.
   *       This is useful for sensors like soil moisture where high
   *       resistance (high raw value) means low moisture (low percentage).
   */
  bool fetchSample(float& value, size_t index) override;

  /**
   * @brief Log sensor-specific debug details
   */
  void logDebugDetails() const override;

  /**
   * @brief Get the minimum value for a given channel
   * @param idx Channel index
   * @return Minimum value
   */
  inline float getMinValue(size_t idx) const {
    if (idx < m_analogConfig.measurements.size()) {
      const auto& m = m_analogConfig.measurements[idx];
      // Use runtime autocal if either the sensor's runtime copy or the
      // persistent config indicates calibrationMode is active. This
      // prevents transient races between persistence and runtime state.
      bool cfgCal = false;
      if (idx < this->config().measurements.size())
        cfgCal = this->config().measurements[idx].calibrationMode;
      if (m.calibrationMode || cfgCal)
        return static_cast<float>(m.autocal.min_value);
      return m.minValue;
    }
    return 0.0f;
  }
  /**
   * @brief Set the minimum value for a given channel
   * @param idx Channel index
   * @param v Minimum value to set
   */
  inline void setMinValue(size_t idx, float v) {
    if (idx < m_analogConfig.measurements.size())
      m_analogConfig.measurements[idx].minValue = v;
  }
  /**
   * @brief Get the maximum value for a given channel
   * @param idx Channel index
   * @return Maximum value
   */
  inline float getMaxValue(size_t idx) const {
    if (idx < m_analogConfig.measurements.size()) {
      const auto& m = m_analogConfig.measurements[idx];
      bool cfgCal = false;
      if (idx < this->config().measurements.size())
        cfgCal = this->config().measurements[idx].calibrationMode;
      if (m.calibrationMode || cfgCal)
        return static_cast<float>(m.autocal.max_value);
      return m.maxValue;
    }
    return 0.0f;
  }
  /**
   * @brief Set the maximum value for a given channel
   * @param idx Channel index
   * @param v Maximum value to set
   */
  inline void setMaxValue(size_t idx, float v) {
    if (idx < m_analogConfig.measurements.size())
      m_analogConfig.measurements[idx].maxValue = v;
  }
  /**
   * @brief Get the last raw ADC value for a given channel
   * @param idx Channel index
   * @return Last raw ADC value, or -1 if unavailable
   */
  inline int getLastRawValue(size_t idx) const {
    return (idx < m_lastRawValues.size()) ? m_lastRawValues[idx] : -1;
  }

  /**
   * @brief Set the absolute raw minimum value for a given channel
   * @param idx Channel index
   * @param rawMin Absolute raw minimum value to set
   */
  inline void setAbsoluteRawMin(size_t idx, int rawMin) {
    if (idx < m_analogConfig.measurements.size())
      m_analogConfig.measurements[idx].absoluteRawMin = rawMin;
  }

  /**
   * @brief Set the absolute raw maximum value for a given channel
   * @param idx Channel index
   * @param rawMax Absolute raw maximum value to set
   */
  inline void setAbsoluteRawMax(size_t idx, int rawMax) {
    if (idx < m_analogConfig.measurements.size())
      m_analogConfig.measurements[idx].absoluteRawMax = rawMax;
  }

  /**
   * @brief Set the autocalibration state for a given channel
   * @param idx Channel index
   * @param cal AutoCal struct to set
   */
  inline void setAutoCalibration(size_t idx, const AutoCal& cal) {
    if (idx < m_analogConfig.measurements.size())
      m_analogConfig.measurements[idx].autocal = cal;
  }

  /**
   * @brief Set or clear the autocalibration runtime flag for a channel
   * @details This updates the sensor's internal runtime copy of the
   * measurement configuration so measurement-time logic (like clamping)
   * observes the calibration mode immediately.
   */
  inline void setCalibrationMode(size_t idx, bool enabled) {
    if (idx < m_analogConfig.measurements.size())
      m_analogConfig.measurements[idx].calibrationMode = enabled;
  }

  /**
   * @brief Get the autocalibration state for a given channel
   */
  inline AutoCal getAutoCalibration(size_t idx) const {
    if (idx < m_analogConfig.measurements.size())
      return m_analogConfig.measurements[idx].autocal;
    return AutoCal();
  }

  /**
   * @brief Get autocal min/max quickly (uint16_t)
   */
  inline uint16_t getAutoCalMin(size_t idx) const {
    if (idx < m_analogConfig.measurements.size())
      return m_analogConfig.measurements[idx].autocal.min_value;
    return 0;
  }
  inline uint16_t getAutoCalMax(size_t idx) const {
    if (idx < m_analogConfig.measurements.size())
      return m_analogConfig.measurements[idx].autocal.max_value;
    return 1023;
  }

protected:
  /**
   * @brief Returns the number of measurements for this analog sensor (clamped
   * to MAX_MEASUREMENTS)
   */
  size_t getNumMeasurements() const override {
    return std::min(m_analogConfig.activeMeasurements,
                    static_cast<size_t>(SensorConfig::MAX_MEASUREMENTS));
  }

private:
  // Store our own copy of the config
  AnalogConfig m_analogConfig;

  // Hardware interface
#if USE_MULTIPLEXER
  std::unique_ptr<Multiplexer> m_multiplexer;
#endif

  // Per-channel minimum values for analog sensors (for validation/UI)
  std::vector<float> m_minValues;
  /**
   * @brief Per-channel maximum values for analog sensors (for validation/UI)
   */
  std::vector<float> m_maxValues;
  // Store last raw ADC value per channel
  std::vector<int> m_lastRawValues;

  // Only keep DRY-compliant helpers
  bool validateReading(int reading, size_t measurementIndex) const;

  /**
   * @brief Maps raw analog values to percentage values
   * @param rawValue The raw ADC reading
   * @param measurementIndex The index of the measurement configuration
   * @return Mapped percentage value (0-100%)
   *
   * @note For inverted sensors, the mapping is flipped so that high raw values
   *       result in low percentages. This is useful for sensors like soil
   *       moisture where high resistance (high raw value) means low moisture.
   */
  float mapAnalogValue(int rawValue, size_t measurementIndex) const;

  bool canAccessHardware() const;
};

#endif // USE_ANALOG
#endif // SENSOR_ANALOG_H
