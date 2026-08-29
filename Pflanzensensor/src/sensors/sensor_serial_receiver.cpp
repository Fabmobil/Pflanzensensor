/**
 * @file sensor_serial_receiver.cpp
 * @brief Implementation of serial receiver sensor
 */

#include "sensors/sensor_serial_receiver.h"

#include <ArduinoJson.h>
#include <SoftwareSerial.h>

#include "logger/logger.h"
#include "sensors/sensor_types.h"
#include "utils/result_types.h"

extern Logger logger;

SerialReceiverSensor::SerialReceiverSensor(const SerialReceiverConfig& config,
                                           class SensorManager* sensorManager)
    : Sensor(config, sensorManager), config_(config), dataValid_(false) {}

void SerialReceiverConfig::configureMeasurements() {
  // Configure 7 measurements for water flow data
  activeMeasurements = 7;

  // Configure each measurement
  for (size_t i = 0; i < 7; ++i) {
    measurements[i].enabled = true;
  }

  // Flow Rate (l/min)
  measurements[0].name = F("Flow Rate");
  measurements[0].fieldName = F("l_per_min");
  measurements[0].unit = F("l/min");
  measurements[0].minValue = 0.0f;
  measurements[0].maxValue = 1000.0f;
  measurements[0].limits.yellowLow = 0.0f;
  measurements[0].limits.greenLow = 0.1f;
  measurements[0].limits.greenHigh = 100.0f;
  measurements[0].limits.yellowHigh = 500.0f;

  // Absolute Counts
  measurements[1].name = F("Absolute Counts");
  measurements[1].fieldName = F("absolute_counts");
  measurements[1].unit = F("counts");
  measurements[1].minValue = 0.0f;
  measurements[1].maxValue = 999999.0f;
  measurements[1].limits.yellowLow = 0.0f;
  measurements[1].limits.greenLow = 1.0f;
  measurements[1].limits.greenHigh = 999999.0f;
  measurements[1].limits.yellowHigh = 999999.0f;

  // Sum Flow Rate
  measurements[2].name = F("Sum Flow Rate");
  measurements[2].fieldName = F("sum_l_per_min");
  measurements[2].unit = F("l/min");
  measurements[2].minValue = 0.0f;
  measurements[2].maxValue = 999999.0f;
  measurements[2].limits.yellowLow = 0.0f;
  measurements[2].limits.greenLow = 0.1f;
  measurements[2].limits.greenHigh = 999999.0f;
  measurements[2].limits.yellowHigh = 999999.0f;

  // 24h Flow Rate
  measurements[3].name = F("24h Flow Rate");
  measurements[3].fieldName = F("l_per_min_24h");
  measurements[3].unit = F("l/min");
  measurements[3].minValue = 0.0f;
  measurements[3].maxValue = 1000.0f;
  measurements[3].limits.yellowLow = 0.0f;
  measurements[3].limits.greenLow = 0.1f;
  measurements[3].limits.greenHigh = 100.0f;
  measurements[3].limits.yellowHigh = 500.0f;

  // Arduino Millis
  measurements[4].name = F("Arduino Time");
  measurements[4].fieldName = F("arduino_millis");
  measurements[4].unit = F("ms");
  measurements[4].minValue = 0.0f;
  measurements[4].maxValue = 4294967295.0f;
  measurements[4].limits.yellowLow = 0.0f;
  measurements[4].limits.greenLow = 0.0f;
  measurements[4].limits.greenHigh = 4294967295.0f;
  measurements[4].limits.yellowHigh = 4294967295.0f;

  // Uptime
  measurements[5].name = F("Uptime");
  measurements[5].fieldName = F("uptime");
  measurements[5].unit = F("s");
  measurements[5].minValue = 0.0f;
  measurements[5].maxValue = 31536000.0f;
  measurements[5].limits.yellowLow = 0.0f;
  measurements[5].limits.greenLow = 0.0f;
  measurements[5].limits.greenHigh = 31536000.0f;
  measurements[5].limits.yellowHigh = 31536000.0f;

  // Liters per Hour
  measurements[6].name = F("Liters per Hour");
  measurements[6].fieldName = F("l_per_hour");
  measurements[6].unit = F("l/h");
  measurements[6].minValue = 0.0f;
  measurements[6].maxValue = 60000.0f;
  measurements[6].limits.yellowLow = 0.0f;
  measurements[6].limits.greenLow = 0.1f;
  measurements[6].limits.greenHigh = 6000.0f;
  measurements[6].limits.yellowHigh = 30000.0f;
}

SensorResult SerialReceiverSensor::init() {
  LOG_DEBUG(F("SerialReceiver"), F("Initializing SerialReceiverSensor"));

#if USE_SERIAL_RECEIVER
  serial_ = std::make_unique<SoftwareSerial>(SERIAL_RECEIVER_RX_PIN, SERIAL_RECEIVER_TX_PIN);
  serial_->begin(config_.baudRate);

  LOG_INFO(F("SerialReceiver"), String(F("Initialized on RX=")) + String(SERIAL_RECEIVER_RX_PIN));
  m_initialized = true;
  return SensorResult::success();
#else
  LOG_ERROR(F("SerialReceiver"), F("Serial receiver not enabled in configuration"));
  return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Serial receiver not enabled"));
#endif
}

SensorResult SerialReceiverSensor::startMeasurement() {
  // Reduced logging - only log errors

  if (!m_initialized) {
    LOG_ERROR(F("SerialReceiver"), F("Serial not initialized"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Serial not initialized"));
  }

  return SensorResult::success();
}

SensorResult SerialReceiverSensor::continueMeasurement() {
  // No bulk communication needed, just return success
  return SensorResult::success();
}

void SerialReceiverSensor::deinitialize() {
  LOG_DEBUG(F("SerialReceiver"), F("Deinitializing SerialReceiverSensor"));
  if (serial_) {
    serial_->end();
    serial_.reset();
  }
}

bool SerialReceiverSensor::isValidValue(float value) const {
  // Basic validation - check if value is not NaN or infinite
  return !isnan(value) && !isinf(value);
}

bool SerialReceiverSensor::isValidValue(float value, size_t measurementIndex) const {
  if (!isValidValue(value)) {
    LOG_ERROR(F("SerialReceiver"),
              String(F("Value NaN/inf for index ")) + String(measurementIndex));
    return false;
  }

  // Check against configured min/max values
  if (measurementIndex < config_.activeMeasurements) {
    bool isValid = value >= config_.measurements[measurementIndex].minValue &&
                   value <= config_.measurements[measurementIndex].maxValue;

    if (!isValid) {
      LOG_ERROR(F("SerialReceiver"),
                String(F("Value ")) + String(value) + String(F(" for index ")) +
                    String(measurementIndex) + String(F(" outside range [")) +
                    String(config_.measurements[measurementIndex].minValue) + String(F(", ")) +
                    String(config_.measurements[measurementIndex].maxValue) + F("]"));
    }

    return isValid;
  }

  LOG_ERROR(F("SerialReceiver"),
            String(F("Invalid measurement index: ")) + String(measurementIndex));
  return false;
}

bool SerialReceiverSensor::fetchSample(float& value, size_t index) {
  if (index >= config_.activeMeasurements) {
    LOG_ERROR(F("SerialReceiver"), String(F("Invalid measurement index: ")) + String(index));
    return false;
  }

  // Request the specific measurement from Arduino
  if (!requestMeasurement(index)) {
    LOG_ERROR(F("SerialReceiver"),
              String(F("Failed to request measurement index: ")) + String(index));
    return false;
  }

  // Read the response
  String response;
  if (!readResponse(response)) {
    LOG_ERROR(F("SerialReceiver"),
              String(F("Failed to read response for measurement index: ")) + String(index));
    return false;
  }

  // Parse the single value
  if (!parseMeasurementValue(response, value)) {
    LOG_ERROR(F("SerialReceiver"),
              String(F("Failed to parse value for measurement index: ")) + String(index));
    return false;
  }

  // Reduced logging - only log errors
  return true;
}

bool SerialReceiverSensor::requestMeasurement(size_t measurementIndex) {
  if (!serial_) {
    LOG_ERROR(F("SerialReceiver"), F("Serial not initialized"));
    return false;
  }

  // Puffer leeren
  while (serial_->available())
    serial_->read();

  // Messanfrage senden (z.B. "GET:0")
  serial_->print(config_.requestCommand);
  serial_->print(':');
  serial_->println(measurementIndex);
  return true;
}

bool SerialReceiverSensor::readResponse(String& response) {
  if (!serial_) {
    LOG_ERROR(F("SerialReceiver"), F("Serial not initialized"));
    return false;
  }

  response = "";
  unsigned long startTime = millis();
  int bytesReceived = 0;

  LOG_DEBUG(F("SerialReceiver"),
            String(F("Waiting for response (timeout: ")) + String(config_.timeout) + F("ms)"));

  // Read complete response with timeout - read until newline
  while (millis() - startTime < config_.timeout) {
    if (serial_->available()) {
      char c = serial_->read();
      bytesReceived++;

      // Stop at newline (end of response)
      if (c == '\n' || c == '\r') {
        break;
      }

      // Collect all characters
      response += c;
    } else {
      yield(); // CPU freigeben ohne delay()
    }
  }

  // Reduced logging - only log errors
  return response.length() > 0;
}

bool SerialReceiverSensor::isDeviceConnected() const {
  if (!serial_) {
    return false;
  }

  // Try to send a ping and see if we get any response
  // This is a lightweight check that doesn't require full JSON parsing
  serial_->println("PING");

  unsigned long startTime = millis();
  while (millis() - startTime < 500) { // Short timeout for ping
    if (serial_->available()) {
      // Clear the response
      while (serial_->available()) {
        serial_->read();
      }
      return true;
    }
    yield();
  }

  return false;
}

bool SerialReceiverSensor::parseMeasurementValue(const String& response, float& value) {
  // Trim whitespace
  String trimmed = response;
  trimmed.trim();

  if (trimmed.length() == 0) {
    LOG_WARN(F("SerialReceiver"), F("Empty response"));
    return false;
  }

  // Try to parse as float
  char* endptr;
  float parsedValue = strtof(trimmed.c_str(), &endptr);

  // Check if parsing was successful (endptr should point to end of string)
  if (endptr == trimmed.c_str() || *endptr != '\0') {
    LOG_WARN(F("SerialReceiver"), String(F("Failed to parse value: '")) + trimmed + F("'"));
    return false;
  }

  // Check for NaN or infinite values
  if (isnan(parsedValue) || isinf(parsedValue)) {
    LOG_WARN(F("SerialReceiver"), String(F("Invalid value (NaN/inf): ")) + String(parsedValue));
    return false;
  }

  value = parsedValue;
  // Reduced logging - only log errors
  return true;
}
