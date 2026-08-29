/**
 * @file admin_sensor_handler_thresholds.cpp
 * @brief Implementation of threshold management functionality
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"

bool AdminSensorHandler::processThresholds(Sensor* sensor, size_t measurementIdx) {
  if (!sensor)
    return false;
  String id = sensor->getId();
  SensorConfig& config = sensor->mutableConfig();
  if (measurementIdx >= config.activeMeasurements || measurementIdx >= config.measurements.size())
    return false;
  const auto& currentLimits = config.measurements[measurementIdx].limits;
  SensorLimits newLimits = currentLimits;
  bool updated = false;
  float newValue;
  // Check each threshold value
  if (updateThreshold(id + "_" + String(measurementIdx), "yellowLow", currentLimits.yellowLow,
                      newValue)) {
    newLimits.yellowLow = newValue;
    updated = true;
  }
  if (updateThreshold(id + "_" + String(measurementIdx), "greenLow", currentLimits.greenLow,
                      newValue)) {
    newLimits.greenLow = newValue;
    updated = true;
  }
  if (updateThreshold(id + "_" + String(measurementIdx), "greenHigh", currentLimits.greenHigh,
                      newValue)) {
    newLimits.greenHigh = newValue;
    updated = true;
  }
  if (updateThreshold(id + "_" + String(measurementIdx), "yellowHigh", currentLimits.yellowHigh,
                      newValue)) {
    newLimits.yellowHigh = newValue;
    updated = true;
  }
  if (updated) {
    config.measurements[measurementIdx].limits = newLimits;
    LOG_INFO(F("AdminSensorHandler"), String(F("Schwellenwerte aktualisiert f\u00fcr ")) + id +
                                          String(F("[")) + String(measurementIdx) +
                                          String(F("]: ")) + String(newLimits.yellowLow, 2) +
                                          String(F(", ")) + String(newLimits.greenLow, 2) +
                                          String(F(", ")) + String(newLimits.greenHigh, 2) +
                                          String(F(", ")) + String(newLimits.yellowHigh, 2));
  }
  return updated;
}

bool AdminSensorHandler::updateThreshold(const String& baseId, const String& thresholdName,
                                         const float& currentValue, float& newValue) {
  String argName = baseId + "_" + thresholdName;
  if (_server.hasArg(argName)) {
    newValue = _server.arg(argName).toFloat();
    if (newValue != currentValue) {
      return true;
    }
  }
  return false;
}

void AdminSensorHandler::handleThresholds() {
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("thresholds")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  String thresholdsCsv = _server.arg("thresholds");

  // Debug: print all incoming arguments
  LOG_DEBUG(F("AdminSensorHandler"), String(F("handleThresholds: sensor=")) + sensorId +
                                         String(F(", measurement=")) + String(measurementIndex) +
                                         String(F(", thresholds=")) + thresholdsCsv);
  LOG_DEBUG(F("AdminSensorHandler"), String(F("sensorId length: ")) + String(sensorId.length()));
  LOG_DEBUG(F("AdminSensorHandler"),
            String(F("thresholdsCsv length: ")) + String(thresholdsCsv.length()));

  // Print all POST args for full context
  for (int i = 0; i < _server.args(); ++i) {
    LOG_DEBUG(F("AdminSensorHandler"),
              String(F("POST arg: ")) + _server.argName(i) + String(F(" = ")) + _server.arg(i));
  }

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(500,
                     F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    LOG_ERROR(F("AdminSensorHandler"), String(F("Sensor nicht gefunden: ")) + sensorId);
    sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    LOG_ERROR(F("AdminSensorHandler"), String(F("Sensor nicht initialisiert: ")) + sensorId);
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    LOG_ERROR(F("AdminSensorHandler"),
              String(F("Ungültiger Messindex: ")) + String(measurementIndex));
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  // Parse CSV thresholds: yellowLow,greenLow,greenHigh,yellowHigh
  float thresholds[4] = {0, 0, 0, 0};
  int n = sscanf(thresholdsCsv.c_str(), "%f,%f,%f,%f", &thresholds[0], &thresholds[1],
                 &thresholds[2], &thresholds[3]);

  // Debug: print parsed threshold values
  LOG_DEBUG(F("AdminSensorHandler"), String(F("Parsed thresholds: n=")) + String(n) +
                                         String(F(", values=")) + String(thresholds[0], 2) +
                                         String(F(",")) + String(thresholds[1], 2) +
                                         String(F(",")) + String(thresholds[2], 2) +
                                         String(F(",")) + String(thresholds[3], 2));

  if (n != 4) {
    LOG_ERROR(F("AdminSensorHandler"),
              String(F("Ungültiges Schwellenwert-Format: ")) + thresholdsCsv);
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiges Schwellenwert-Format\"}"));
    return;
  }

  // Validate threshold order: yellowLow <= greenLow <= greenHigh <= yellowHigh
  if (thresholds[0] > thresholds[1] || thresholds[1] > thresholds[2] ||
      thresholds[2] > thresholds[3]) {
    LOG_ERROR(F("AdminSensorHandler"),
              String(F("Ungültige Reihenfolge der Schwellenwerte: ")) + String(thresholds[0], 2) +
                  String(F(",")) + String(thresholds[1], 2) + String(F(",")) +
                  String(thresholds[2], 2) + String(F(",")) + String(thresholds[3], 2));
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Ungültige Reihenfolge der Schwellenwerte\"}"));
    return;
  }

  auto& limits = config.measurements[measurementIndex].limits;
  bool changed = false;

  // Debug: print limits before
  LOG_DEBUG(F("AdminSensorHandler"), String(F("Limits before: ")) + String(limits.yellowLow, 2) +
                                         String(F(",")) + String(limits.greenLow, 2) +
                                         String(F(",")) + String(limits.greenHigh, 2) +
                                         String(F(",")) + String(limits.yellowHigh, 2));

  if (limits.yellowLow != thresholds[0]) {
    limits.yellowLow = thresholds[0];
    changed = true;
  }
  if (limits.greenLow != thresholds[1]) {
    limits.greenLow = thresholds[1];
    changed = true;
  }
  if (limits.greenHigh != thresholds[2]) {
    limits.greenHigh = thresholds[2];
    changed = true;
  }
  if (limits.yellowHigh != thresholds[3]) {
    limits.yellowHigh = thresholds[3];
    changed = true;
  }

  // Debug: print limits after
  LOG_DEBUG(F("AdminSensorHandler"), String(F("Limits after: ")) + String(limits.yellowLow, 2) +
                                         String(F(",")) + String(limits.greenLow, 2) +
                                         String(F(",")) + String(limits.greenHigh, 2) +
                                         String(F(",")) + String(limits.yellowHigh, 2));

  if (changed) {
    // Persist thresholds to config manager
    Thresholds th;
    th.yellowLow = limits.yellowLow;
    th.greenLow = limits.greenLow;
    th.greenHigh = limits.greenHigh;
    th.yellowHigh = limits.yellowHigh;
    auto& limits2 = sensor->mutableConfig().measurements[measurementIndex].limits;
    limits2.yellowLow = th.yellowLow;
    limits2.greenLow = th.greenLow;
    limits2.greenHigh = th.greenHigh;
    limits2.yellowHigh = th.yellowHigh;

    // Use atomic update for thresholds
    auto result = SensorPersistence::updateSensorThresholds(sensorId, measurementIndex,
                                                            limits.yellowLow, limits.greenLow,
                                                            limits.greenHigh, limits.yellowHigh);
    if (!result.isSuccess()) {
      LOG_ERROR(F("AdminSensorHandler"),
                String(F("Fehler beim Speichern der Schwellenwerte: ")) + result.getMessage());
      sendJsonResponse(
          500, F("{\"success\":false,\"error\":\"Fehler beim Speichern der Schwellenwerte\"}"));
      return;
    }

    LOG_INFO(F("AdminSensorHandler"),
             String(F("Thresholds updated for sensor ")) + sensorId + String(F(" measurement ")) +
                 String(measurementIndex) + String(F(": ")) + String(thresholds[0], 2) +
                 String(F(",")) + String(thresholds[1], 2) + String(F(",")) +
                 String(thresholds[2], 2) + String(F(",")) + String(thresholds[3], 2));
  }

  sendJsonResponse(200, F("{\"success\":true}"));
}
