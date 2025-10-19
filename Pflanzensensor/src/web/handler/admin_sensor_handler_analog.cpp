/**
 * @file admin_sensor_handler_analog.cpp
 * @brief Analog sensor specific operations (min/max, inversion)
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_sensor_persistence.h"
#include "sensors/sensor_analog.h"
#include "utils/helper.h"

void AdminSensorHandler::handleAnalogInverted() {
#if USE_ANALOG
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("inverted")) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  bool inverted = _server.arg("inverted") == "true";

  logger.debug(F("AdminSensorHandler"),
               F("handleAnalogInverted: sensor=") + sensorId +
                   F(", measurement=") + String(measurementIndex) +
                   F(", inverted=") + String(inverted));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  if (!isAnalogSensor(sensor)) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Sensor ist nicht analog\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    logger.error(F("AdminSensorHandler"),
                 F("Ungültiger Messindex: ") + String(measurementIndex) +
                     F(", max erlaubt: ") +
                     String(config.measurements.size() - 1));
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  // If nothing changed, return OK (use config's inverted flag)
  if (config.measurements[measurementIndex].inverted == inverted) {
    logger.debug(F("AdminSensorHandler"),
                 F("Keine Änderungen für invertierten Zustand festgestellt"));
    sendJsonResponse(200, F("{\"success\":true}"));
    return;
  }

  // Update in-memory config and persist
  config.measurements[measurementIndex].inverted = inverted;
  auto result = SensorPersistence::updateAnalogMinMax(
      sensorId, measurementIndex, config.measurements[measurementIndex].minValue,
      config.measurements[measurementIndex].maxValue, config.measurements[measurementIndex].inverted);
  if (!result.isSuccess()) {
    logger.error(
        F("AdminSensorHandler"),
        F("Fehler beim Aktualisieren des invertierten Zustands: ") + result.getMessage());
    sendJsonResponse(
        500,
        F("{\"success\":false,\"error\":\"Fehler beim Speichern des invertierten Zustands\"}"));
    return;
  }

  logger.info(F("AdminSensorHandler"),
              F("Analog invertiert für ") + sensorId + F("[") +
                  String(measurementIndex) + F("]: invertiert=") +
                  String(inverted));

  sendJsonResponse(200, F("{\"success\":true}"));
#else
  sendJsonResponse(
      400, F("{\"success\":false,\"error\":\"Analog sensors not enabled\"}"));
#endif
}

void AdminSensorHandler::handleAnalogMinMax() {
#if USE_ANALOG
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("min") || !_server.hasArg("max")) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  float newMin = _server.arg("min").toFloat();
  float newMax = _server.arg("max").toFloat();

  logger.debug(F("AdminSensorHandler"),
               F("handleAnalogMinMax: sensor=") + sensorId +
                   F(", measurement=") + String(measurementIndex) +
                   F(", min=") + String(newMin) + F(", max=") + String(newMax));

  // Debug: print all incoming arguments
  for (int i = 0; i < _server.args(); ++i) {
    logger.debug(F("AdminSensorHandler"), F("POST arg: ") + _server.argName(i) +
                                              F(" = ") + _server.arg(i));
  }

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  if (!isAnalogSensor(sensor)) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Sensor ist nicht analog\"}"));
    return;
  }

  AnalogSensor* analog = static_cast<AnalogSensor*>(sensor);
  // Use the config to check measurement count instead of protected method
  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    logger.error(F("AdminSensorHandler"),
                 F("Ungültiger Messindex: ") + String(measurementIndex) +
                     F(", max erlaubt: ") +
                     String(config.measurements.size() - 1));
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  bool changed = false;
  if (newMin != analog->getMinValue(measurementIndex)) {
    analog->setMinValue(measurementIndex, newMin);
    changed = true;
  }
  if (newMax != analog->getMaxValue(measurementIndex)) {
    analog->setMaxValue(measurementIndex, newMax);
    changed = true;
  }
  if (changed) {
    logger.debug(F("AdminSensorHandler"),
                 F("Analog-Min/Max geändert, Konfiguration wird aktualisiert und persistiert"));
    // Persist min/max in config
    config.measurements[measurementIndex].minValue =
        analog->getMinValue(measurementIndex);
    config.measurements[measurementIndex].maxValue =
        analog->getMaxValue(measurementIndex);
    // Use atomic update for analog min/max values
    auto result = SensorPersistence::updateAnalogMinMax(
        sensorId, measurementIndex,
        config.measurements[measurementIndex].minValue,
        config.measurements[measurementIndex].maxValue,
        config.measurements[measurementIndex].inverted);
    if (!result.isSuccess()) {
      logger.error(
          F("AdminSensorHandler"),
          F("Fehler beim Aktualisieren von Analog-Min/Max: ") + result.getMessage());
      sendJsonResponse(
          500,
          F("{\"success\":false,\"error\":\"Fehler beim Speichern der Min/Max-Werte\"}"));
      return;
    }
    logger.debug(F("AdminSensorHandler"),
                 F("Erfolgreich Analog-Min/Max aktualisiert für ") + sensorId +
                     F("[") + String(measurementIndex) + F("]: min=") +
                     String(config.measurements[measurementIndex].minValue) +
                     F(", max=") +
                     String(config.measurements[measurementIndex].maxValue));
  } else {
    logger.debug(F("AdminSensorHandler"),
                 F("Keine Änderungen für Analog-Min/Max-Werte festgestellt"));
  }

  sendJsonResponse(200, F("{\"success\":true}"));
#else
  sendJsonResponse(
      400, F("{\"success\":false,\"error\":\"Analog sensors not enabled\"}"));
#endif
}
