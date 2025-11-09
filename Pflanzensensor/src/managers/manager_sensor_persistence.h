/**
 * @file manager_sensor_persistence.h
 * @brief Sensor configuration file persistence layer
 */

#ifndef MANAGER_SENSOR_PERSISTENCE_H
#define MANAGER_SENSOR_PERSISTENCE_H

#include "../utils/result_types.h"
#include "manager_config_types.h"
#include <ArduinoJson.h>

// Forward declarations
#if USE_ANALOG
class AnalogSensor;
#endif

struct MeasurementConfig; // From sensor_types.h

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
   * @brief Update measurement name atomically
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param name New name for the measurement
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateMeasurementName(const String& sensorId, size_t measurementIndex,
                                                 const String& name);

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
   * @brief Enqueue an analog raw min/max update to be processed later in the main loop.
   * This avoids performing blocking Preferences writes from a time-critical context.
   * Updates are batched and written every 60 seconds to reduce flash wear.
   * @param sensorId Sensor ID
   * @param measurementIndex Measurement index
   * @param absoluteRawMin New minimum raw value
   * @param absoluteRawMax New maximum raw value
   */
  static void enqueueAnalogRawMinMax(const String& sensorId, size_t measurementIndex,
                                     int absoluteRawMin, int absoluteRawMax);

  /**
   * @brief Enqueue an absolute min/max update (float) to be processed later.
   * Used by all sensor types (not just analog) to batch persistence writes.
   * @param sensorId Sensor ID
   * @param measurementIndex Measurement index
   * @param absoluteMin New minimum value
   * @param absoluteMax New maximum value
   */
  static void enqueueAbsoluteMinMax(const String& sensorId, size_t measurementIndex,
                                    float absoluteMin, float absoluteMax);

  /**
   * @brief Enqueue an analog min/max/inverted update (integer) to be processed later.
   * @param sensorId Sensor ID
   * @param measurementIndex Measurement index
   * @param minValue Minimum calibrated value
   * @param maxValue Maximum calibrated value
   * @param inverted Inversion flag
   */
  static void enqueueAnalogMinMaxInteger(const String& sensorId, size_t measurementIndex,
                                         int minValue, int maxValue, bool inverted);

  /**
   * @brief Flush pending updates for a specific sensor immediately after measurement cycle.
   * This is the preferred method - each sensor flushes its own data right after measurement.
   * @param sensorId Sensor ID to flush updates for
   */
  static void flushPendingUpdatesForSensor(const String& sensorId);

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

  // ============================================================================
  // JSON-based persistence (new approach)
  // ============================================================================

  /**
   * @brief Save a single measurement configuration to JSON file
   * @param sensorId Sensor ID (e.g., "ANALOG", "DHT")
   * @param measurementIndex Measurement index (0-based)
   * @param config Measurement configuration to save
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult saveMeasurementToJson(const String& sensorId, size_t measurementIndex,
                                                 const MeasurementConfig& config);

  /**
   * @brief Load a single measurement configuration from JSON file
   * @param sensorId Sensor ID (e.g., "ANALOG", "DHT")
   * @param measurementIndex Measurement index (0-based)
   * @param config Output parameter - loaded configuration
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult loadMeasurementFromJson(const String& sensorId, size_t measurementIndex,
                                                   MeasurementConfig& config);

  /**
   * @brief Update a single measurement setting via generic field name
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param fieldName Name of the field to update (e.g., "enabled", "minValue", "name")
   * @param value JSON value to set
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateMeasurementSetting(const String& sensorId, size_t measurementIndex,
                                                    const String& fieldName,
                                                    const JsonVariant& value);

  /**
   * @brief Update multiple measurement settings at once (batch update)
   * @param sensorId Sensor ID to update
   * @param measurementIndex Measurement index to update
   * @param settings JSON object with field name/value pairs
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult updateMeasurementSettings(const String& sensorId,
                                                     size_t measurementIndex,
                                                     const JsonObject& settings);

private:
  SensorPersistence() = default;
};

#endif
