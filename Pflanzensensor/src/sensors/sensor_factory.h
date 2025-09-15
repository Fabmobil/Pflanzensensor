/**
 * @file sensor_factory.h
 * @brief Factory class for creating and initializing different sensor types
 * @details Provides a centralized factory for creating and managing all
 * supported sensor types in the system. Handles conditional compilation based
 * on sensor support flags.
 */

#ifndef SENSOR_FACTORY_H
#define SENSOR_FACTORY_H

#include <memory>
#include <vector>

#include "configs/config.h"
#include "logger/logger.h"
#include "utils/result_types.h"
#if USE_ANALOG
#include "sensors/sensor_analog.h"
#endif
#if USE_DHT
#include "sensors/sensor_dht.h"
#endif
#if USE_DS18B20
#include "sensors/sensor_ds18b20.h"
#endif
#if USE_SDS011
#include "sensors/sensor_sds011.h"
#endif
#if USE_MHZ19
#include "sensors/sensor_mhz19.h"
#endif
#if USE_HX711
#include "sensors/sensor_hx711.h"
#endif
#if USE_BMP280
#include "sensors/sensor_bmp280.h"
#endif
#if USE_SERIAL_RECEIVER
#include "sensors/sensor_serial_receiver.h"
#endif
#include "sensors/sensors.h"

/**
 * @class SensorFactory
 * @brief Factory class responsible for creating and initializing different
 * types of sensors
 * @details Implements the factory pattern for sensor creation and
 * initialization. Handles all supported sensor types and manages their
 * lifecycle from creation through initialization. Supports conditional
 * compilation for different sensor types.
 */
class SensorFactory {
 public:
  /** @typedef SensorResult
   *  @brief Type alias for sensor operation results
   */
  using SensorResult = TypedResult<SensorError, void>;

  /**
   * @brief Creates all configured sensors based on the system configuration
   * @param sensors Vector to store the created sensor instances
   * @return SensorResult indicating success, partial success, or failure
   * @details Iterates through all configured sensor types and creates instances
   *          based on the configuration. Handles initialization and validation
   *          of each sensor. Supports partial success where some sensors may
   *          fail while others succeed.
   * @see createDHTSensors()
   * @see createDS18B20Sensors()
   * @see createAnalogSensors()
   * @see createMHZ19Sensors()
   * @see createSDS011Sensors()
   */
  static SensorResult createAllSensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      class SensorManager* sensorManager);

 private:
  /**
   * @brief Creates and initializes DHT temperature/humidity sensors
   * @param sensors Vector to store the created DHT sensors
   * @return SensorResult indicating success or failure
   * @details Creates DHT sensors based on configuration if USE_DHT is defined.
   *          Handles both DHT11 and DHT22 sensor types.
   */
  static SensorResult createDHTSensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Creates and initializes DS18B20 temperature sensors
   * @param sensors Vector to store the created DS18B20 sensors
   * @return SensorResult indicating success or failure
   * @details Creates DS18B20 sensors if USE_DS18B20 is defined.
   *          Supports multiple sensors on the same bus using unique addresses.
   */
  static SensorResult createDS18B20Sensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Creates and initializes analog sensors
   * @param sensors Vector to store the created analog sensors
   * @return SensorResult indicating success or failure
   * @details Creates analog sensors if USE_ANALOG is defined.
   *          Supports various analog sensor types with configurable scaling
   *          and calibration.
   */
  static SensorResult createAnalogSensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Creates and initializes MH-Z19 CO2 sensors
   * @param sensors Vector to store the created MH-Z19 sensors
   * @return SensorResult indicating success or failure
   * @details Creates MH-Z19 CO2 sensors if USE_MHZ19 is defined.
   *          Handles serial communication and auto-calibration features.
   */
  static SensorResult createMHZ19Sensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Creates and initializes SDS011 particulate matter sensors
   * @param sensors Vector to store the created SDS011 sensors
   * @return SensorResult indicating success or failure
   * @details Creates SDS011 PM2.5/PM10 sensors if USE_SDS011 is defined.
   *          Manages serial communication and sleep mode for the sensors.
   */
  static SensorResult createSDS011Sensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Creates and initializes HX711 weight sensors
   * @param sensors Vector to store the created HX711 sensors
   * @return SensorResult indicating success or failure
   */
  static SensorResult createHX711Sensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Creates and initializes BMP280 air temperature and pressure sensors
   * @param sensors Vector to store the created HX711 sensors
   * @return SensorResult indicating success or failure
   */
  static SensorResult createBMP280Sensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Creates and initializes serial receiver sensors
   * @param sensors Vector to store the created serial receiver sensors
   * @return SensorResult indicating success or failure
   * @details Creates serial receiver sensors if USE_SERIAL_RECEIVER is
   * defined. Handles serial communication and JSON parsing for external Arduino
   * data.
   */
  static SensorResult createSerialReceiverSensors(
      std::vector<std::unique_ptr<Sensor>>& sensors,
      SensorManager* sensorManager);

  /**
   * @brief Initializes a single sensor instance
   * @param sensor Reference to the sensor pointer to initialize
   * @return SensorResult indicating success or failure
   * @details Handles the complete initialization sequence for a sensor,
   *          including validation, hardware initialization, and initial
   * testing.
   * @see validateSensorConfig()
   */
  static SensorResult initializeSensor(std::unique_ptr<Sensor>& sensor);

  /**
   * @brief Validates the configuration of a sensor
   * @param sensor Pointer to the sensor to validate
   * @return true if the configuration is valid, false otherwise
   * @details Checks all required configuration parameters and ensures
   *          they are within valid ranges. Logs any validation errors.
   */
  static bool validateSensorConfig(const Sensor* sensor);

  /**
   * @brief Logs the current status of a sensor
   * @param phase Description of the current operation phase
   * @param sensor Pointer to the sensor being processed
   * @details Provides detailed logging of sensor operations for debugging
   *          and monitoring purposes.
   */
  static void logSensorStatus(const String& phase, const Sensor* sensor);

  // Prevent instantiation
  SensorFactory() = delete;  ///< Default constructor disabled
  SensorFactory(const SensorFactory&) = delete;  ///< Copy constructor disabled
  SensorFactory& operator=(const SensorFactory&) =
      delete;  ///< Assignment operator disabled
};

#endif  // SENSOR_FACTORY_H
