/**
 * @file manager_sensor_persistence.h
 * @brief Sensor configuration file persistence layer
 */

#ifndef MANAGER_SENSOR_PERSISTENCE_H
#define MANAGER_SENSOR_PERSISTENCE_H

#include "../utils/result_types.h"
#include "manager_config_types.h"

// Forward declarations
#if USE_ANALOG
class AnalogSensor;
#endif

class SensorPersistence {
public:
  using PersistenceResult = TypedResult<ConfigError, void>;

  /**
   * @brief Load sensor configuration from Preferences
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult load();

  /**
   * @brief Save sensor configuration to Preferences
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult save();

  /**
   * @brief Update a specific sensor threshold atomically
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param yellowLow Yellow low threshold value
   * @param greenLow Green low threshold value
   * @param greenHigh Green high threshold value
   * @param yellowHigh Yellow high threshold value
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateSensorThresholds(const String& sensorId, size_t measurementIndex,
                                                  float yellowLow, float greenLow, float greenHigh,
                                                  float yellowHigh);

  /**
   * @brief Update analog sensor min/max values atomically
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param minValue Minimum value
   * @param maxValue Maximum value
   * @param inverted Whether the sensor is inverted
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAnalogMinMax(const String& sensorId, size_t measurementIndex,
                                              float minValue, float maxValue, bool inverted);

  // Variant that expects integer min/max values (for use by autocal) to
  // make callers explicit about rounding semantics.
  static PersistenceResult updateAnalogMinMaxInteger(const String& sensorId,
                                                     size_t measurementIndex, int minValue,
                                                     int maxValue, bool inverted);

  // Variant that updates integer min/max but does NOT trigger a full
  // reload of the configuration file. Useful for autocal paths that
  // persist frequently and must avoid transient reloads that interfere
  // with runtime state.
  static PersistenceResult updateAnalogMinMaxIntegerNoReload(const String& sensorId,
                                                             size_t measurementIndex, int minValue,
                                                             int maxValue, bool inverted);

  /**
   * @brief Update sensor measurement interval atomically
   * @param sensorId Sensor ID to update
   * @param interval Measurement interval in milliseconds
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateMeasurementInterval(const String& sensorId,
                                                     unsigned long interval);

  /**
   * @brief Update sensor measurement enabled state atomically
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param enabled Whether the measurement is enabled
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateMeasurementEnabled(const String& sensorId, size_t measurementIndex,
                                                    bool enabled);

  /**
   * @brief Update absolute min/max values atomically
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param absoluteMin Absolute minimum value
   * @param absoluteMax Absolute maximum value
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAbsoluteMinMax(const String& sensorId, size_t measurementIndex,
                                                float absoluteMin, float absoluteMax);

  /**
   * @brief Update analog sensor raw min/max values atomically
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param absoluteRawMin Absolute minimum raw value
   * @param absoluteRawMax Absolute maximum raw value
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAnalogRawMinMax(const String& sensorId, size_t measurementIndex,
                                                 int absoluteRawMin, int absoluteRawMax);

  /**
   * @brief Update analog sensor calibration mode flag atomically
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param enabled New calibration mode flag
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAnalogCalibrationMode(const String& sensorId,
                                                       size_t measurementIndex, bool enabled);

  /**
   * @brief Update autocal half-life duration (seconds) for an analog measurement
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param halfLifeSeconds New half-life in seconds
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAutocalDuration(const String& sensorId, size_t measurementIndex,
                                                 uint32_t halfLifeSeconds);

  /**
   * @brief Check if sensor configuration exists in Preferences
   * @return True if sensor config exists, false otherwise
   */
  static bool configExists();

  /**
   * @brief Get estimated sensor configuration size in Preferences
   * @return Estimated size in bytes
   */
  static size_t getConfigSize();

private:
  SensorPersistence() = default;

  /**
   * @brief Internal save method
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult saveInternal();

  /**
   * @brief Internal method for updating sensor thresholds
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param yellowLow Yellow low threshold value
   * @param greenLow Green low threshold value
   * @param greenHigh Green high threshold value
   * @param yellowHigh Yellow high threshold value
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateSensorThresholdsInternal(const String& sensorId,
                                                          size_t measurementIndex, float yellowLow,
                                                          float greenLow, float greenHigh,
                                                          float yellowHigh);

  /**
   * @brief Internal method for updating analog min/max values
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param minValue Minimum value
   * @param maxValue Maximum value
   * @param inverted Whether the sensor is inverted
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAnalogMinMaxInternal(const String& sensorId,
                                                      size_t measurementIndex, float minValue,
                                                      float maxValue, bool inverted);

  /**
   * @brief Internal method for updating measurement interval
   * @param sensorId Sensor ID to update
   * @param interval Measurement interval in milliseconds
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateMeasurementIntervalInternal(const String& sensorId,
                                                             unsigned long interval);

  /**
   * @brief Internal method for updating measurement enabled state
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param enabled Whether the measurement is enabled
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateMeasurementEnabledInternal(const String& sensorId,
                                                            size_t measurementIndex, bool enabled);

  /**
   * @brief Internal method for updating absolute min/max values
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param absoluteMin Absolute minimum value
   * @param absoluteMax Absolute maximum value
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAbsoluteMinMaxInternal(const String& sensorId,
                                                        size_t measurementIndex, float absoluteMin,
                                                        float absoluteMax);

  /**
   * @brief Internal method for updating analog raw min/max values
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param absoluteRawMin Absolute minimum raw value
   * @param absoluteRawMax Absolute maximum raw value
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateAnalogRawMinMaxInternal(const String& sensorId,
                                                         size_t measurementIndex,
                                                         int absoluteRawMin, int absoluteRawMax);
};

#endif
