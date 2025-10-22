/**
 * @file admin_sensor_handler_config.cpp
 * @brief Sensor configuration update and retrieval handlers
 */

#include <map>

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"
#include "sensors/sensor_analog.h"
#include "utils/helper.h"

void AdminSensorHandler::handleSingleSensorUpdate() {
  if (!requireAjaxRequest()) return;
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }
  if (!_server.hasArg("sensor_id")) {
    sendJsonResponse(400,
                     F("{\"success\":false,\"error\":\"sensor_id fehlt\"}"));
    return;
  }
  String id = _server.arg("sensor_id");
  logger.debug(F("AdminSensorHandler"),
               F("handleSingleSensorUpdate: empfangene sensor_id = ") + id);
  // Log all POST arguments
  for (int i = 0; i < _server.args(); ++i) {
    logger.debug(F("AdminSensorHandler"), F("POST arg: ") + _server.argName(i) +
                                              F(" = ") + _server.arg(i));
  }
  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }
  Sensor* sensor = _sensorManager.getSensor(id);
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
  bool changesOccurred = false;
  // Update name(s)
  for (size_t i = 0;
       i < config.activeMeasurements && i < config.measurements.size(); ++i) {
    String nameArg = "name_" + id + "_" + String(i);
    if (_server.hasArg(nameArg)) {
      String newName = _server.arg(nameArg);
      if (newName != config.measurements[i].name) {
    logger.info(F("AdminSensorHandler"),
          F("Ändere Name für ") + id + F("[") + String(i) +
            F("] von '") + config.measurements[i].name +
            F("' zu '") + newName + F("'"));
        config.measurements[i].name = newName;
        changesOccurred = true;
      }
    }
    // Parse combined thresholds field if present
    String thresholdsArg = "thresholds_" + id + "_" + String(i);
    if (_server.hasArg(thresholdsArg)) {
      String csv = _server.arg(thresholdsArg);
      float vals[4] = {0, 0, 0, 0};
      int n = sscanf(csv.c_str(), "%f,%f,%f,%f", &vals[0], &vals[1], &vals[2],
                     &vals[3]);
        if (n == 4) {
        auto& limits = config.measurements[i].limits;
        if (limits.yellowLow != vals[0] || limits.greenLow != vals[1] ||
            limits.greenHigh != vals[2] || limits.yellowHigh != vals[3]) {
          logger.info(F("AdminSensorHandler"),
                      F("Ändere Schwellenwerte für ") + id + F("[") + String(i) +
                          F("] von ") + String(limits.yellowLow) + "," +
                          String(limits.greenLow) + "," +
                          String(limits.greenHigh) + "," +
                          String(limits.yellowHigh) + F(" zu ") +
                          String(vals[0]) + "," + String(vals[1]) + "," +
                          String(vals[2]) + "," + String(vals[3]));
          limits.yellowLow = vals[0];
          limits.greenLow = vals[1];
          limits.greenHigh = vals[2];
          limits.yellowHigh = vals[3];
          changesOccurred = true;
        }
      }
    }
    // Min/max update
#if USE_ANALOG
    if (isAnalogSensor(sensor)) {
      AnalogSensor* analog = static_cast<AnalogSensor*>(sensor);
      String minArg = "min_" + id + "_" + String(i);
      String maxArg = "max_" + id + "_" + String(i);
      String invertedArg = "inverted_" + id + "_" + String(i);
      if (_server.hasArg(minArg)) {
        float newMin = _server.arg(minArg).toFloat();
        if (newMin != analog->getMinValue(i)) {
          analog->setMinValue(i, newMin);
          changesOccurred = true;
        }
      }
      if (_server.hasArg(maxArg)) {
        float newMax = _server.arg(maxArg).toFloat();
        if (newMax != analog->getMaxValue(i)) {
          analog->setMaxValue(i, newMax);
          changesOccurred = true;
        }
      }
      if (_server.hasArg(invertedArg)) {
        bool newInverted = _server.hasArg(invertedArg);
        if (newInverted != config.measurements[i].inverted) {
          config.measurements[i].inverted = newInverted;
          changesOccurred = true;
        }
      }
    }
#endif
    // Thresholds (legacy/individual fields, fallback)
    if (processThresholds(sensor, i)) {
      logger.info(F("AdminSensorHandler"),
                  F("Schwellenwerte geändert f\u00fcr ") + id + F("[") + String(i) + F("]"));
      changesOccurred = true;
    }
    // Persist thresholds
    Thresholds th;
    th.yellowLow = config.measurements[i].limits.yellowLow;
    th.greenLow = config.measurements[i].limits.greenLow;
    th.greenHigh = config.measurements[i].limits.greenHigh;
    th.yellowHigh = config.measurements[i].limits.yellowHigh;
    auto& limits = sensor->mutableConfig().measurements[i].limits;
    limits.yellowLow = th.yellowLow;
    limits.greenLow = th.greenLow;
    limits.greenHigh = th.greenHigh;
    limits.yellowHigh = th.yellowHigh;
  }
  // Enabled state
  bool newEnabled = _server.hasArg("enabled_" + id);
  if (newEnabled != sensor->isEnabled()) {
    logger.info(F("AdminSensorHandler"),
                F("Aktivierungszustand f\u00fcr ") + id + F(" von ") +
                    (sensor->isEnabled() ? F("aktiv") : F("inaktiv")) + F(" nach ") +
                    (newEnabled ? F("aktiv") : F("inaktiv")));
    sensor->setEnabled(newEnabled);
    changesOccurred = true;
  }
  // Save config if changed
  if (changesOccurred) {
    // Use atomic sensor updates instead of full config save
    auto saveResult = SensorPersistence::saveToFileMinimal();
    logger.info(F("AdminSensorHandler"),
                F("SensorPersistence::saveToFile() called, result: ") +
                    saveResult.getMessage());
    if (!saveResult.isSuccess()) {
      sendJsonResponse(
          500,
          F("{\"success\":false,\"error\":\"Fehler beim Speichern der Sensor-Konfiguration\"}"));
      return;
    }
  } else {
    logger.info(F("AdminSensorHandler"),
                F("Keine Änderungen f\u00fcr Sensor ") + id);
  }
  sendJsonResponse(200, F("{\"success\":true}"));
}

void AdminSensorHandler::handleMeasurementName() {
  if (!requireAjaxRequest()) return;
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentication required\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("name")) {
    sendJsonResponse(
        400,
        F("{\"success\":false,\"error\":\"Missing required parameters\"}"));
    return;
  }

  String id = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  String newName = _server.arg("name");

  logger.debug(F("AdminSensorHandler"), F("handleMeasurementName: sensor_id=") +
                                            id + F(", measurement_index=") +
                                            String(measurementIndex) +
                                            F(", name='") + newName + F("'"));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor manager not healthy\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(id);
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

  if (measurementIndex >= sensor->config().activeMeasurements) {
    sendJsonResponse(
        400, F("{\"success\":false,\"error\":\"Invalid measurement index\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (config.measurements[measurementIndex].name != newName) {
    logger.info(F("AdminSensorHandler"),
                F("Changing name for ") + id + F("[") +
                    String(measurementIndex) + F("] from '") +
                    config.measurements[measurementIndex].name + F("' to '") +
                    newName + F("'"));

    config.measurements[measurementIndex].name = newName;

    // Save the change
    auto saveResult = SensorPersistence::saveToFileMinimal();
    if (!saveResult.isSuccess()) {
      sendJsonResponse(
          500,
          F("{\"success\":false,\"error\":\"Failed to save sensor config\"}"));
      return;
    }

    logger.info(F("AdminSensorHandler"), F("Successfully updated name for ") +
                                             id + F("[") +
                                             String(measurementIndex) + F("]"));
  }

  sendJsonResponse(200, F("{\"success\":true}"));
}

void AdminSensorHandler::handleGetSensorConfigJson() {
  if (!validateRequest()) {
    sendJsonResponse(
        401, F("{\"success\":false,\"error\":\"Authentication required\"}"));
    return;
  }
  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Sensor manager not healthy\"}"));
    return;
  }
  beginChunkedResponse(F("application/json"));
  sendChunk(F("{\"success\":true,\"sensors\":{"));
  bool firstSensor = true;
  const auto& sensors = _sensorManager.getSensors();
  for (const auto& sensor : sensors) {
    if (!sensor || !sensor->isInitialized()) continue;
    if (!firstSensor) sendChunk(F(","));
    firstSensor = false;
    String id = sensor->getId();
    const SensorConfig& config = sensor->config();
#if USE_ANALOG
    bool analog = isAnalogSensor(sensor.get());
#endif
    char buf[256];
    snprintf(buf, sizeof(buf),
             "\"%s\":{\"id\":\"%s\",\"interval\":%lu,\"measurements\":[",
             id.c_str(), id.c_str(), config.measurementInterval);
    sendChunk(buf);
    size_t nRows = config.activeMeasurements < config.measurements.size()
                       ? config.activeMeasurements
                       : config.measurements.size();
    for (size_t i = 0; i < nRows; ++i) {
      if (i > 0) sendChunk(F(","));
      const auto& meas = config.measurements[i];
      Thresholds th;
      // Instead, access and set thresholds via the sensor's measurement config:
      // sensor->getConfig().measurements[i].limits
      th.yellowLow = meas.limits.yellowLow;
      th.greenLow = meas.limits.greenLow;
      th.greenHigh = meas.limits.greenHigh;
      th.yellowHigh = meas.limits.yellowHigh;
      snprintf(buf, sizeof(buf),
               "{\"name\":\"%s\",\"fieldName\":\"%s\",\"unit\":\"%s\","
               "\"enabled\":%s,\"thresholds\":{\"yellowLow\":%.2f,\"greenLow\":"
               "%.2f,\"greenHigh\":%.2f,\"yellowHigh\":%.2f}",
               meas.name.c_str(), meas.fieldName.c_str(), meas.unit.c_str(),
               meas.enabled ? "true" : "false", th.yellowLow, th.greenLow,
               th.greenHigh, th.yellowHigh);
      sendChunk(buf);

      // Always include absolute min/max values to ensure they are displayed
      // properly
      snprintf(buf, sizeof(buf), ",\"absoluteMin\":%.2f", meas.absoluteMin);
      sendChunk(buf);
      snprintf(buf, sizeof(buf), ",\"absoluteMax\":%.2f", meas.absoluteMax);
      sendChunk(buf);

#if USE_ANALOG
      if (analog) {
        // Always include absolute raw min/max values for analog sensors to
        // ensure they are displayed properly

        snprintf(buf, sizeof(buf), ",\"absoluteRawMin\":%d",
                 meas.absoluteRawMin);
        sendChunk(buf);
        snprintf(buf, sizeof(buf), ",\"absoluteRawMax\":%d",
                 meas.absoluteRawMax);
        sendChunk(buf);

        snprintf(buf, sizeof(buf), ",\"minmax\":{\"min\":%.2f,\"max\":%.2f}",
                 static_cast<AnalogSensor*>(sensor.get())->getMinValue(i),
                 static_cast<AnalogSensor*>(sensor.get())->getMaxValue(i));
        sendChunk(buf);
        snprintf(buf, sizeof(buf), ",\"inverted\":%s",
                 config.measurements[i].inverted ? "true" : "false");
        sendChunk(buf);
        // Autocalibration flag (persisted)
        snprintf(buf, sizeof(buf), ",\"calibrationMode\":%s",
                 config.measurements[i].calibrationMode ? "true" : "false");
        sendChunk(buf);
        // Note: autocal now persists into the calculation limits (min/max).
        // The historical extremum storage (absoluteRawMin/Max) remains
        // dedicated to measured extremes and is not modified by autocal.
      }
#endif
      sendChunk(F("}"));
    }
    sendChunk(F("]"));  // End measurements array
    sendChunk(F("}"));  // End sensor object
  }
  sendChunk(F("}}"));  // End sensors and root
  endChunkedResponse();
}
