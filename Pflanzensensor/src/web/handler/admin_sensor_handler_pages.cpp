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
  if (!validateRequest()) return;
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin", "sensors", "admin_sensors"};
  renderPage(
      F("Sensor Konfiguration"), "admin",
      [this]() {
        sendChunk(F("<div class='container'>"));
        bool changesOccurred = false;
    // Speicher-Logging vor dem Update
    logger.info(F("AdminSensorHandler"),
          F("Speicher vor Update: freier Heap = ") +
            String(ESP.getFreeHeap()) +
            F(" bytes, Fragmentierung = ") +
            String(ESP.getHeapFragmentation()) + F("%"));
        auto result = ResourceMgr.executeCritical(
            "Sensor Config Update", [&]() -> ResourceResult {
              // Speicher-Logging innerhalb des kritischen Abschnitts
              logger.info(F("AdminSensorHandler"),
                          F("Speicher im kritischen Abschnitt: freier Heap = ") +
                              String(ESP.getFreeHeap()) +
                              F(" bytes, Fragmentierung = ") +
                              String(ESP.getHeapFragmentation()) + F("%"));
              if (!_sensorManager.isHealthy()) {
                return ResourceResult::fail(ResourceError::INVALID_STATE,
                                            F("Sensor-Manager nicht betriebsbereit"));
              }
              const auto& sensors = _sensorManager.getSensors();
              for (auto& sensor : sensors) {
                if (!sensor) continue;
                if (!sensor->isInitialized()) continue;
                String id = sensor->getId();
                SensorConfig& config = sensor->mutableConfig();
                for (size_t i = 0; i < config.activeMeasurements &&
                                   i < config.measurements.size();
                     ++i) {
                  // Name update
                  String nameArg = "name_" + id + "_" + String(i);
                  if (_server.hasArg(nameArg)) {
                    String newName = _server.arg(nameArg);
                    if (newName != config.measurements[i].name) {
                      config.measurements[i].name = newName;
            logger.info(F("AdminSensorHandler"),
                  F("Name aktualisiert für ") + id + F("[") +
                    String(i) + F("]: ") + newName);
                      sendChunk(F("<li>Sensor "));
                      sendChunk(id);
                      sendChunk(F(" Messwert "));
                      sendChunk(String(i));
                      sendChunk(F(": Name geändert zu '"));
                      sendChunk(newName);
                      sendChunk(F("'</li>"));
                      changesOccurred = true;
                    }
                  }
                  // Thresholds
                  if (processThresholds(sensor.get(), i)) {
                    sendChunk(F("<li>Sensor "));
                    sendChunk(id);
                    sendChunk(F(" Messwert "));
                    sendChunk(String(i));
                    sendChunk(F(": Schwellwerte aktualisiert</li>"));
                    changesOccurred = true;
                  }
          // Min/max update
#if USE_ANALOG
                  if (isAnalogSensor(sensor.get())) {
                    AnalogSensor* analog =
                        static_cast<AnalogSensor*>(sensor.get());
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
                  // Persist thresholds
                }
                // Process enabled state (sensor-wide)
                bool newEnabled = _server.hasArg("enabled_" + id);
                if (newEnabled != sensor->isEnabled()) {
                  sensor->setEnabled(newEnabled);
                  logger.info(
                      F("AdminSensorHandler"),
                      F("Aktivierungszustand für ") + id + F(": ") +
                          (newEnabled ? F("aktiviert") : F("deaktiviert")));
                  sendChunk(F("<li>Sensor "));
                  sendChunk(id);
                  sendChunk(newEnabled ? F(": aktiviert</li>")
                                       : F(": deaktiviert</li>"));
                  changesOccurred = true;
                }
              }
              // Save sensor config if changes occurred
              if (changesOccurred) {
                auto saveResult = SensorPersistence::saveToFileMinimal();
                logger.info(
                    F("AdminSensorHandler"),
                    F("SensorPersistence::saveToFile() called, result: ") +
                        saveResult.getMessage());
                if (!saveResult.isSuccess()) {
                  return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR,
                                              F("Fehler beim Speichern der Sensor-Konfiguration: ") +
                                                  saveResult.getMessage());
                }
              }
              // Speicher-Logging nach dem Update
              logger.info(F("AdminSensorHandler"),
                          F("Speicher nach Update: freier Heap = ") +
                              String(ESP.getFreeHeap()) +
                              F(" bytes, Fragmentierung = ") +
                              String(ESP.getHeapFragmentation()) + F("%"));
              return ResourceResult::success();
            });
    // Speicher-Logging nach Handler
    logger.info(F("AdminSensorHandler"),
          F("Speicher nach Handler: freier Heap = ") +
            String(ESP.getFreeHeap()) +
            F(" bytes, Fragmentierung = ") +
            String(ESP.getHeapFragmentation()) + F("%"));
        if (!result.isSuccess()) {
          sendChunk(F("<h2>Fehler</h2><p>Fehler beim Speichern: "));
          sendChunk(result.getMessage());
          sendChunk(F("</p>"));
        } else if (changesOccurred) {
          sendChunk(F("<h2>Änderungen gespeichert</h2>"));
        } else {
          sendChunk(F("<h2>Keine Änderungen</h2>"));
          sendChunk(F("<p>Es wurden keine Änderungen vorgenommen.</p>"));
        }
        sendChunk(
            F("<br><a href='/admin/sensors' class='button button-primary'>"
              "Zurück zur Sensorkonfiguration</a>"));
        sendChunk(F("</div>"));
      },
      css, js);
}

void AdminSensorHandler::handleTriggerMeasurement() {
  if (!_server.hasArg("sensor_id")) {
    logger.error(F("AdminSensorHandler"),
                 F("Messung auslösen: fehlende Sensor-ID"));
    sendError(400, F("Fehlende Sensor-ID"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  String measurementIndexStr = _server.arg("measurement_index");
  logger.debug(F("AdminSensorHandler"),
               "Triggering measurement for sensor: " + sensorId +
                   (measurementIndexStr.length() > 0
                        ? " measurement: " + measurementIndexStr
                        : ""));

  if (!_sensorManager.isHealthy()) {
    logger.error(F("AdminSensorHandler"),
                 F("Messung auslösen: Sensor-Manager nicht betriebsbereit"));
    sendError(500, F("Sensor-Manager nicht betriebsbereit"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    logger.error(F("AdminSensorHandler"),
                 F("Messung auslösen: Sensor nicht gefunden: ") + sensorId);
    sendError(404, F("Sensor nicht gefunden"));
    return;
  }

  // New: Force immediate measurement via cycle manager
  if (_sensorManager.forceImmediateMeasurement(sensorId)) {
  logger.info(F("AdminSensorHandler"),
        F("Manuelle Messung geplant f\u00fcr Sensor: ") + sensorId +
          (measurementIndexStr.length() > 0
             ? F(" Messung: ") + measurementIndexStr
             : F("")));
  sendJsonResponse(
    200, F("{\"success\":true,\"message\":\"Messung geplant\"}"));
    return;
  } else {
  logger.error(
    F("AdminSensorHandler"),
    F("Fehler beim Planen einer manuellen Messung f\u00fcr Sensor: ") + sensorId);
  sendJsonResponse(
    500,
    F("{\"success\":false,\"error\":\"Fehler beim Planen der Messung\"}"));
    return;
  }
}
