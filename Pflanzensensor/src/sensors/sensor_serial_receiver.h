/**
 * @file sensor_serial_receiver.h
 * @brief Serial receiver sensor for receiving data from external Arduino
 * devices
 * @details Receives JSON data over SoftwareSerial from external Arduino devices
 *          without WiFi capability. Uses simple request-response protocol.
 */

#ifndef SENSOR_SERIAL_RECEIVER_H
#define SENSOR_SERIAL_RECEIVER_H

#include <Arduino.h>

#include <memory>

// Include config for pin definitions and feature flags
#include "configs/config.h"

#if USE_SERIAL_RECEIVER
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#endif
#include "sensors/sensor_config.h"
#include "sensors/sensor_measurement_types.h"
#include "sensors/sensors.h"

/**
 * @struct SerialReceiverConfig
 * @brief Configuration for serial receiver sensor
 */
struct SerialReceiverConfig : public SensorConfig {
  unsigned long baudRate;  ///< Baud rate for serial communication
  unsigned long timeout;   ///< Timeout for serial operations in milliseconds
  String requestCommand;   ///< Command to send to request data (e.g., "GET")

  SerialReceiverConfig()
      : baudRate(9600), timeout(4000), requestCommand("GET") {}

  void configureMeasurements();
};

/**
 * @struct SerialReceiverData
 * @brief Data structure for received serial data
 */
struct SerialReceiverData {
  float lPerMin;                ///< Liters per minute
  float absoluteCounts;         ///< Absolute counts
  float sumLPerMin;             ///< Sum of liters per minute
  float lPerMin24h;             ///< Liters per minute in last 24 hours
  unsigned long arduinoMillis;  ///< Arduino millis value
  unsigned long uptime;         ///< Uptime in seconds
  float lPerHour;               ///< Liters per hour

  SerialReceiverData()
      : lPerMin(0),
        absoluteCounts(0),
        sumLPerMin(0),
        lPerMin24h(0),
        arduinoMillis(0),
        uptime(0),
        lPerHour(0) {}
};

/**
 * @class SerialReceiverSensor
 * @brief Sensor for receiving data over serial from external Arduino devices
 */
class SerialReceiverSensor : public Sensor {
 public:
  /**
   * @brief Constructor
   * @param config Configuration for the sensor
   * @param sensorManager Pointer to the sensor manager
   */
  explicit SerialReceiverSensor(const SerialReceiverConfig& config,
                                class SensorManager* sensorManager);

  /**
   * @brief Destructor
   */
  ~SerialReceiverSensor() override = default;

  // Override required methods from base Sensor class
  SensorResult init() override;
  SensorResult startMeasurement() override;
  SensorResult continueMeasurement() override;
  void deinitialize() override;
  bool isValidValue(float value) const override;
  bool isValidValue(float value, size_t measurementIndex) const override;

  /**
   * @brief Get shared hardware information
   * @return SharedHardwareInfo with sensor type and pin information
   */
  SharedHardwareInfo getSharedHardwareInfo() const override {
#if USE_SERIAL_RECEIVER
    return SharedHardwareInfo(SensorType::SERIAL_RECEIVER,
                              SERIAL_RECEIVER_RX_PIN, config_.minimumDelay);
#else
    return SharedHardwareInfo(SensorType::SERIAL_RECEIVER, 0, config_.minimumDelay);
#endif
  }

  /**
   * @brief Get the number of measurements for this sensor
   * @return Number of measurements (7 for serial receiver)
   */
  size_t getNumMeasurements() const override {
    return config_.activeMeasurements;
  }

  /**
   * @brief Check if sensor should be deinitialized after measurement
   * @return false for serial receiver (maintains connection)
   */
  bool shouldDeinitializeAfterMeasurement() const override { return false; }

  /**
   * @brief Fetch a sample value for a specific measurement
   * @param value Output parameter for the fetched value
   * @param index Index of the measurement to fetch
   * @return true if successful, false otherwise
   */
  bool fetchSample(float& value, size_t index) override;

 private:
  SerialReceiverConfig config_;
  // Serial interface is only available when feature flag is enabled
#if USE_SERIAL_RECEIVER
  std::unique_ptr<SoftwareSerial> serial_;
#else
  // Placeholder to keep class layout when feature disabled
  void* serial_ = nullptr;
#endif
  SerialReceiverData lastData_;
  bool dataValid_;

  /**
   * @brief Send request for data
   * @return true if request sent successfully
   */
  bool sendRequest();

  /**
   * @brief Read response from Arduino
   * @param response Output parameter for the received response
   * @return true if response received successfully
   */
  bool readResponse(String& response);

  /**
   * @brief Request a specific measurement value from Arduino
   * @param measurementIndex Index of the measurement to request (0-6)
   * @return true if request successful
   */
  bool requestMeasurement(size_t measurementIndex);

  /**
   * @brief Parse single measurement value from Arduino response
   * @param response Response string from Arduino
   * @param value Output parameter for the parsed value
   * @return true if parsing successful
   */
  bool parseMeasurementValue(const String& response, float& value);

  /**
   * @brief Check if external device is connected and responding
   * @return true if device responds to ping
   */
  bool isDeviceConnected() const;
};

#endif  // SENSOR_SERIAL_RECEIVER_H
