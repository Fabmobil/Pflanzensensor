/**
 * @file admin_sensor_handler_reset.cpp
 * @brief Reset operations for sensor data (absolute min/max values)
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"
#include "sensors/sensor_analog.h"
#include "utils/helper.h"

void AdminSensorHandler::handleResetAbsoluteMinMax() {
  if (!requireAjaxRequest()) return;
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index")) {
    sendJsonResponse(
        400,
        F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();

  logger.debug(F("AdminSensorHandler"),
               F("handleResetAbsoluteMinMax: sensor=") + sensorId +
                   F(", measurement=") + String(measurementIndex));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404,
                     F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  // Reset absolute min/max values
  config.measurements[measurementIndex].absoluteMin = INFINITY;
  config.measurements[measurementIndex].absoluteMax = -INFINITY;

  // Use atomic update to reset absolute min/max values
  auto result = SensorPersistence::updateAbsoluteMinMax(
      sensorId, measurementIndex, INFINITY, -INFINITY);
  if (!result.isSuccess()) {
    logger.error(F("AdminSensorHandler"),
                 F("Fehler beim Zurücksetzen von absoluten min/max Werten: ") + result.getMessage());
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Zurücksetzen der absoluten min/max Werte\"}"));
    return;
  }

  // Reload the sensor configuration from the JSON file to ensure in-memory
  // values are updated
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("AdminSensorHandler"),
                 F("Reloading sensor configuration after reset"));
  }

  auto reloadResult = SensorPersistence::loadFromFile();
  if (!reloadResult.isSuccess()) {
    logger.warning(F("AdminSensorHandler"),
                   F("Fehler beim Nachladen der Sensor-Konfiguration nach dem Zurücksetzen: ") +
                       reloadResult.getMessage());
  } else {
      if (ConfigMgr.isDebugSensor()) {
      logger.debug(F("AdminSensorHandler"),
                   F("Sensor-Konfiguration nach dem Zurücksetzen erfolgreich neu geladen"));
    }
  }

  logger.info(F("AdminSensorHandler"), F("Absolute min/max zurückgesetzt für ") +
                                           sensorId + F("[") +
                                           String(measurementIndex) + F("]"));

  sendJsonResponse(200, F("{\"success\":true}"));
}

void AdminSensorHandler::handleResetAbsoluteRawMinMax() {
  if (!requireAjaxRequest()) return;
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index")) {
    sendJsonResponse(
        400,
        F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();

  logger.debug(F("AdminSensorHandler"),
               F("handleResetAbsoluteRawMinMax: sensor=") + sensorId +
                   F(", measurement=") + String(measurementIndex));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404,
                     F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
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
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  // Reset absolute raw min/max values
  config.measurements[measurementIndex].absoluteRawMin = INT_MAX;
  config.measurements[measurementIndex].absoluteRawMax = INT_MIN;

  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("AdminSensorHandler"),
                 F("Zurücksetzen der absoluten Roh-Min/Max-Werte für Sensor ") +
                     sensorId + F(" Messung ") + String(measurementIndex));
  }

  // Use atomic update to reset absolute raw min/max values
  auto result = SensorPersistence::updateAnalogRawMinMax(
      sensorId, measurementIndex, INT_MAX, INT_MIN);
  if (!result.isSuccess()) {
    logger.error(
        F("AdminSensorHandler"),
        F("Fehler beim Zurücksetzen der Roh-Min/Max-Werte: ") + result.getMessage());
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Zurücksetzen der Roh-Min/Max-Werte\"}"));
    return;
  }

  // No need to reload configs anymore - we now have a single source of truth
  if (ConfigMgr.isDebugSensor()) {
    logger.debug(F("AdminSensorHandler"), F("Zurücksetzen abgeschlossen für Sensor ") +
                                              sensorId + F(" Messung ") +
                                              String(measurementIndex));
  }

  logger.info(F("AdminSensorHandler"), F("Absolute Roh-Min/Max zurückgesetzt für ") +
                                           sensorId + F("[") +
                                           String(measurementIndex) + F("]"));

  sendJsonResponse(200, F("{\"success\":true}"));
}

// Autocal reset implementation removed — use reset_absolute_raw_minmax or
// reset_absolute_minmax endpoints to achieve equivalent effects.
