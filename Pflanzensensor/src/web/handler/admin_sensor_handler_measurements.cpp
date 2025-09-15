/**
 * @file admin_sensor_handler_measurements.cpp
 * @brief Measurement-specific operations (enable/disable, intervals)
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_sensor_persistence.h"

void AdminSensorHandler::handleMeasurementEnable() {
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentication required\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("enabled")) {
    sendJsonResponse(
        400,
        F("{\"success\":false,\"error\":\"Missing required parameters\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  bool enabled = (_server.arg("enabled") == "true");

  logger.debug(F("AdminSensorHandler"), F("handleMeasurementEnable: sensor=") +
                                            sensorId + F(", measurement=") +
                                            String(measurementIndex) +
                                            F(", enabled=") + String(enabled));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor manager not healthy\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404,
                     F("{\"success\":false,\"error\":\"Sensor not found\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Sensor not initialized\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Invalid measurement index\"}"));
    return;
  }

  if (config.measurements[measurementIndex].enabled != enabled) {
    config.measurements[measurementIndex].enabled = enabled;
    // Use atomic update for measurement enabled state
    auto result = SensorPersistence::updateMeasurementEnabled(
        sensorId, measurementIndex, enabled);
    if (!result.isSuccess()) {
      logger.error(F("AdminSensorHandler"),
                   F("Failed to update measurement enabled state: ") +
                       result.getMessage());
      sendJsonResponse(500, F("{\"success\":false,\"error\":\"Failed to save "
                              "measurement state\"}"));
      return;
    }
    logger.info(F("AdminSensorHandler"),
                F("Measurement ") + String(measurementIndex) +
                    F(" for sensor ") + sensorId + F(" ") +
                    (enabled ? F("enabled") : F("disabled")));
  }

  sendJsonResponse(200, F("{\"success\":true}"));
}

void AdminSensorHandler::handleMeasurementInterval() {
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentication required\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("interval")) {
    sendJsonResponse(
        400,
        F("{\"success\":false,\"error\":\"Missing required parameters\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  unsigned long intervalSeconds = _server.arg("interval").toInt();
  unsigned long intervalMilliseconds = intervalSeconds * 1000;

  logger.debug(F("AdminSensorHandler"),
               F("handleMeasurementInterval: sensor=") + sensorId +
                   F(", interval=") + String(intervalSeconds) + F("s"));

  // Validate interval
  if (intervalSeconds < 10 || intervalSeconds > 3600) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Invalid interval "
                            "(10-3600 seconds)\"}"));
    return;
  }

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor manager not healthy\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404,
                     F("{\"success\":false,\"error\":\"Sensor not found\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Sensor not initialized\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (config.measurementInterval != intervalMilliseconds) {
    config.measurementInterval = intervalMilliseconds;
    sensor->setMeasurementInterval(intervalMilliseconds);
    // Use atomic update for measurement interval
    auto result = SensorPersistence::updateMeasurementInterval(
        sensorId, intervalMilliseconds);
    if (!result.isSuccess()) {
      logger.error(
          F("AdminSensorHandler"),
          F("Failed to update measurement interval: ") + result.getMessage());
      sendJsonResponse(
          500, F("{\"success\":false,\"error\":\"Failed to save interval\"}"));
      return;
    }
  }

  sendJsonResponse(200, F("{\"success\":true}"));
}
