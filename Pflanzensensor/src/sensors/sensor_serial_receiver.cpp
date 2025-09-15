/**
 * @file sensor_serial_receiver.cpp
 * @brief Implementation of serial receiver sensor
 */

#include "sensors/sensor_serial_receiver.h"

#include "configs/config.h"
#if USE_SERIAL_RECEIVER
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#endif

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
  logger.debug(F("SerialReceiver"), F("Initialisiere SerialReceiverSensor"));

  try {
#if USE_SERIAL_RECEIVER
    serial_ = std::make_unique<SoftwareSerial>(SERIAL_RECEIVER_RX_PIN,
                                               SERIAL_RECEIVER_TX_PIN);
    serial_->begin(config_.baudRate);

    // Don't test communication during init - just set up the hardware
    // Communication will be tested during the first measurement
  logger.info(F("SerialReceiver"), F("Hardware erfolgreich initialisiert"));

    // Mark sensor as initialized so measurement cycle manager knows it's ready
    m_initialized = true;

    return SensorResult::success();
#else
    // Serial receiver not enabled, return error
  logger.error(F("SerialReceiver"), F("Serial-Empfänger in Konfiguration nicht aktiviert"));
  return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                F("Serial-Empfänger nicht aktiviert"));
#endif
  } catch (const std::exception& e) {
  logger.error(F("SerialReceiver"),
         F("Ausnahme während der Initialisierung: ") + String(e.what()));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              String(e.what()));
  }
}

SensorResult SerialReceiverSensor::startMeasurement() {
  // Reduced logging - only log errors

  if (!m_initialized) {
  logger.error(F("SerialReceiver"), F("Serielle Schnittstelle nicht initialisiert"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                F("Serielle Schnittstelle nicht initialisiert"));
  }

  return SensorResult::success();
}

SensorResult SerialReceiverSensor::continueMeasurement() {
  // No bulk communication needed, just return success
  return SensorResult::success();
}

void SerialReceiverSensor::deinitialize() {
  logger.debug(F("SerialReceiver"), F("Deinitialisiere SerialReceiverSensor"));
  #if USE_SERIAL_RECEIVER
  if (serial_) {
    serial_->end();
    serial_.reset();
  }
  #endif
}

bool SerialReceiverSensor::isValidValue(float value) const {
  // Basic validation - check if value is not NaN or infinite
  return !isnan(value) && !isinf(value);
}

bool SerialReceiverSensor::isValidValue(float value,
                                        size_t measurementIndex) const {
  if (!isValidValue(value)) {
  logger.info(F("SerialReceiver"),
        F("Wertvalidierung fehlgeschlagen: NaN oder unendlich für Index ") +
          String(measurementIndex) + F(" value=") + String(value));
    return false;
  }

  // Check against configured min/max values
  if (measurementIndex < config_.activeMeasurements) {
    bool isValid = value >= config_.measurements[measurementIndex].minValue &&
                   value <= config_.measurements[measurementIndex].maxValue;

    if (!isValid) {
      logger.info(F("SerialReceiver"),
                  F("Wert ") + String(value) + F(" für Index ") +
                      String(measurementIndex) + F(" außerhalb des Bereichs [") +
                      String(config_.measurements[measurementIndex].minValue) +
                      F(", ") +
                      String(config_.measurements[measurementIndex].maxValue) +
                      F("]"));
    } else {
      logger.info(F("SerialReceiver"),
                  F("Wert ") + String(value) + F(" für Index ") +
                      String(measurementIndex) + F(" ist VALIDE"));
    }

    return isValid;
  }

  logger.info(F("SerialReceiver"),
              F("Ungültiger Messindex: ") + String(measurementIndex));
  return false;
}

bool SerialReceiverSensor::fetchSample(float& value, size_t index) {
  if (index >= config_.activeMeasurements) {
    logger.error(F("SerialReceiver"),
                 F("Ungültiger Messindex: ") + String(index));
    return false;
  }

  // Request the specific measurement from Arduino
#if USE_SERIAL_RECEIVER
  if (!requestMeasurement(index)) {
    logger.error(F("SerialReceiver"),
                 F("Anforderung des Messindex fehlgeschlagen: ") + String(index));
    return false;
  }

  // Read the response
  String response;
  if (!readResponse(response)) {
  logger.error(
    F("SerialReceiver"),
    F("Konnte Antwort für Messindex nicht lesen: ") + String(index));
    return false;
  }

  // Parse the single value
  if (!parseMeasurementValue(response, value)) {
  logger.error(
    F("SerialReceiver"),
    F("Konnte Wert für Messindex nicht parsen: ") + String(index));
    return false;
  }

  // Reduced logging - only log errors
  return true;
#else
  // Feature disabled: indicate no data
  value = NAN;
  return false;
#endif
}

bool SerialReceiverSensor::requestMeasurement(size_t measurementIndex) {
  #if !USE_SERIAL_RECEIVER
  (void)measurementIndex;
  return false;
  #else
  if (!serial_) {
    logger.error(F("SerialReceiver"), F("Serielle Schnittstelle nicht initialisiert"));
    return false;
  }

  // Clear any pending data first
  int clearCount = 0;
  while (serial_->available()) {
    serial_->read();
    clearCount++;
  }
  // Reduced logging - only log errors

  delay(50);  // Small delay before sending

  // Send the specific measurement request (e.g., "GET:0" for first measurement)
  String command =
      String(config_.requestCommand) + ":" + String(measurementIndex);
  serial_->println(command);

  // Reduced logging - only log errors
  return true;
  #endif
}

bool SerialReceiverSensor::readResponse(String& response) {
  #if !USE_SERIAL_RECEIVER
  (void)response;
  return false;
  #else
  if (!serial_) {
    logger.error(F("SerialReceiver"), F("Serial not initialized"));
    return false;
  }

  response = "";
  unsigned long startTime = millis();
  int bytesReceived = 0;

  logger.debug(F("SerialReceiver"), F("Warte auf Antwort (Timeout: ") +
                                        String(config_.timeout) + F("ms)"));

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
      // No data available, small delay
      delay(10);
    }
  }

  // Reduced logging - only log errors
  return response.length() > 0;
  #endif
}

bool SerialReceiverSensor::isDeviceConnected() const {
  #if !USE_SERIAL_RECEIVER
  return false;
  #else
  if (!serial_) {
    return false;
  }

  // Try to send a ping and see if we get any response
  // This is a lightweight check that doesn't require full JSON parsing
  serial_->println("PING");

  unsigned long startTime = millis();
  while (millis() - startTime < 500) {  // Short timeout for ping
    if (serial_->available()) {
      // Clear the response
      while (serial_->available()) {
        serial_->read();
      }
      return true;
    }
    delay(1);
  }

  return false;
  #endif
}

bool SerialReceiverSensor::parseMeasurementValue(const String& response,
                                                 float& value) {
  // Trim whitespace
  String trimmed = response;
  trimmed.trim();

  if (trimmed.length() == 0) {
    logger.warning(F("SerialReceiver"), F("Empty response"));
    return false;
  }

  // Try to parse as float
  char* endptr;
  float parsedValue = strtof(trimmed.c_str(), &endptr);

  // Check if parsing was successful (endptr should point to end of string)
  if (endptr == trimmed.c_str() || *endptr != '\0') {
    logger.warning(F("SerialReceiver"),
                   F("Failed to parse value: '") + trimmed + F("'"));
    return false;
  }

  // Check for NaN or infinite values
  if (isnan(parsedValue) || isinf(parsedValue)) {
    logger.warning(F("SerialReceiver"),
                   F("Invalid value (NaN/inf): ") + String(parsedValue));
    return false;
  }

  value = parsedValue;
  // Reduced logging - only log errors
  return true;
}
