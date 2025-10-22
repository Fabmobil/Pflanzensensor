/**
 * @file admin_sensor_handler_pages.cpp
 * @brief Implementation of page handlers for sensor configuration
 */

#include <map>

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor_persistence.h"
#include "sensors/sensor_analog.h"
#include "utils/helper.h"

void AdminSensorHandler::handleSensorUpdate() {
  if (!requireAjaxRequest())
    return;

  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  String changes;
  bool changesOccurred = false;

  auto result = ResourceMgr.executeCritical("Sensor Config Update", [&]() -> ResourceResult {
    if (!_sensorManager.isHealthy()) {
      return ResourceResult::fail(ResourceError::INVALID_STATE,
                                  F("Sensor-Manager nicht betriebsbereit"));
    }
    const auto& sensors = _sensorManager.getSensors();
    for (auto& sensor : sensors) {
      if (!sensor)
        continue;
      if (!sensor->isInitialized())
        continue;
      String id = sensor->getId();
      SensorConfig& config = sensor->mutableConfig();
      for (size_t i = 0; i < config.activeMeasurements && i < config.measurements.size(); ++i) {
        // Name update
        String nameArg = "name_" + id + "_" + String(i);
        if (_server.hasArg(nameArg)) {
          String newName = _server.arg(nameArg);
          if (newName != config.measurements[i].name) {
            config.measurements[i].name = newName;
            logger.info(F("AdminSensorHandler"),
                        F("Name aktualisiert für ") + id + F("[") + String(i) + F("]: ") + newName);
            changes += "<li>Sensor " + id + " Messwert " + String(i) + ": Name geändert zu '" +
                       newName + "'</li>";
            changesOccurred = true;
          }
        }
        // Thresholds
        if (processThresholds(sensor.get(), i)) {
          changes +=
              "<li>Sensor " + id + " Messwert " + String(i) + ": Schwellwerte aktualisiert</li>";
          changesOccurred = true;
        }
#if USE_ANALOG
        if (isAnalogSensor(sensor.get())) {
          AnalogSensor* analog = static_cast<AnalogSensor*>(sensor.get());
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
      }
      // Process enabled state (sensor-wide)
      bool newEnabled = _server.hasArg("enabled_" + id);
      if (newEnabled != sensor->isEnabled()) {
        sensor->setEnabled(newEnabled);
        logger.info(F("AdminSensorHandler"), F("Aktivierungszustand für ") + id + F(": ") +
                                                 (newEnabled ? F("aktiviert") : F("deaktiviert")));
        changes += String("<li>Sensor ") + id +
                   (newEnabled ? F(": aktiviert</li>") : F(": deaktiviert</li>"));
        changesOccurred = true;
      }
    }
    if (changesOccurred) {
      auto saveResult = SensorPersistence::saveToFileMinimal();
      logger.info(F("AdminSensorHandler"),
                  F("SensorPersistence::saveToFile() called, result: ") + saveResult.getMessage());
      if (!saveResult.isSuccess()) {
        return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR,
                                    F("Fehler beim Speichern der Sensor-Konfiguration: ") +
                                        saveResult.getMessage());
      }
    }
    return ResourceResult::success();
  });

  if (!result.isSuccess()) {
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Fehler beim Speichern der Sensor-Konfiguration\"}"));
    return;
  }

  // Build JSON response with changes as HTML list (UI will display in toast)
  String resp;
  resp.reserve(256 + changes.length());
  resp += "{\"success\":true,\"changes\":\"";
  // Escape double quotes and backslashes inside changes
  for (size_t i = 0; i < changes.length(); ++i) {
    char c = changes.charAt(i);
    if (c == '\\')
      resp += "\\\\";
    else if (c == '"')
      resp += "\\\"";
    else if (c == '\n')
      resp += "\\n";
    else if (c == '\r')
      resp += "\\r";
    else
      resp += c;
  }
  resp += "\"}";
  sendJsonResponse(200, resp.c_str());
}

void AdminSensorHandler::handleTriggerMeasurement() {
  if (!requireAjaxRequest())
    return;

  if (!_server.hasArg("sensor_id")) {
    logger.error(F("AdminSensorHandler"), F("Messung auslösen: fehlende Sensor-ID"));
    this->sendError(400, F("Fehlende Sensor-ID"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  String measurementIndexStr = _server.arg("measurement_index");
  logger.debug(
      F("AdminSensorHandler"),
      "Triggering measurement for sensor: " + sensorId +
          (measurementIndexStr.length() > 0 ? " measurement: " + measurementIndexStr : ""));

  if (!_sensorManager.isHealthy()) {
    logger.error(F("AdminSensorHandler"),
                 F("Messung auslösen: Sensor-Manager nicht betriebsbereit"));
    this->sendError(500, F("Sensor-Manager nicht betriebsbereit"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    logger.error(F("AdminSensorHandler"),
                 F("Messung auslösen: Sensor nicht gefunden: ") + sensorId);
    this->sendError(404, F("Sensor nicht gefunden"));
    return;
  }

  // New: Force immediate measurement via cycle manager
  if (_sensorManager.forceImmediateMeasurement(sensorId)) {
    logger.info(
        F("AdminSensorHandler"),
        F("Manuelle Messung geplant f\u00fcr Sensor: ") + sensorId +
            (measurementIndexStr.length() > 0 ? F(" Messung: ") + measurementIndexStr : F("")));
    sendJsonResponse(200, F("{\"success\":true,\"message\":\"Messung geplant\"}"));
    return;
  } else {
    logger.error(F("AdminSensorHandler"),
                 F("Fehler beim Planen einer manuellen Messung f\u00fcr Sensor: ") + sensorId);
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Planen der Messung\"}"));
    return;
  }
}

void AdminSensorHandler::handleFlowerStatusUpdate(const std::map<String, String>& params) {
  if (!validateRequest())
    return;

  logger.info(F("AdminSensorHandler"), F("handleFlowerStatusUpdate() aufgerufen"));

  // Get sensor parameter from form
  auto sensorIt = params.find("sensor");
  if (sensorIt == params.end() || sensorIt->second.length() == 0) {
    logger.warning(F("AdminSensorHandler"),
                   F("Flower Status Update: Kein Sensor-Parameter erhalten"));
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Kein Sensor angegeben\"}"));
    return;
  }

  String selectedSensor = sensorIt->second;
  logger.info(F("AdminSensorHandler"), F("Ausgewählter Flower Status Sensor: ") + selectedSensor);

  // Update configuration
  auto result = ConfigMgr.setFlowerStatusSensor(selectedSensor);

  if (result.isSuccess()) {
    logger.info(F("AdminSensorHandler"),
                F("Flower Status Sensor erfolgreich gespeichert: ") + selectedSensor);
    sendJsonResponse(200, F("{\"success\":true}"));
  } else {
    logger.error(F("AdminSensorHandler"),
                 F("Fehler beim Speichern des Flower Status Sensors: ") + result.getMessage());
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Speichern\"}"));
  }
}
