/**
 * @file sensor_handler.cpp
 * Fixed version with memory safety improvements
 */

#include "web/handler/sensor_handler.h"

#include <ArduinoJson.h>

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/helper.h"
#include "web/core/components.h"

// Maximum number of values per sensor
static constexpr size_t MAX_VALUES = 10;

RouterResult SensorHandler::onRegisterRoutes(WebRouter& router) {
  logger.info(F("SensorHandler"), F("Sensor-Routen werden registriert:"));

  // Register Latest Values endpoint
  auto latestResult = router.addRoute(HTTP_GET, "/getLatestValues",
                                      [this]() { handleGetLatestValues(); });
  if (!latestResult.isSuccess()) return latestResult;

  logger.info(F("SensorHandler"), F("Sensor-Routen erfolgreich registriert"));
  return RouterResult::success();
}

HandlerResult SensorHandler::handleGet(
    const String& /*uri*/, const std::map<String, String>& /*query*/) {
  // Let registerRoutes handle the actual routing
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             "Bitte verwenden Sie registerRoutes");
}

HandlerResult SensorHandler::handlePost(
    const String& /*uri*/, const std::map<String, String>& /*params*/) {
  // Let registerRoutes handle the actual routing
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             "Bitte verwenden Sie registerRoutes");
}

void SensorHandler::handleGetLatestValues() {
  // **CRITICAL FIX 1: Memory check before starting**
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 4096) {  // Require at least 4KB free
    logger.warning(
        F("SensorHandler"),
        F("Nicht genügend Speicher für JSON-Antwort: ") + String(freeHeap));
    _server.send(503, F("application/json"),
                 F("{\"error\":\"Nicht genügend Speicher\"}"));
    return;
  }

  beginChunkedResponse(F("application/json"));

  // Send basic info first
  sendChunk(F("{\"currentTime\":"));
  sendChunk(String(millis()));
  sendChunk(F(",\"deviceName\":\""));
  sendChunk(ConfigMgr.getDeviceName());
  sendChunk(F("\",\"flowerStatusSensor\":\""));
  sendChunk(ConfigMgr.getFlowerStatusSensor());
  sendChunk(F("\",\"ip\":\""));
  sendChunk(WiFi.localIP().toString());
  sendChunk(F("\",\"sensors\":{"));

  auto managerState = _sensorManager.getState();
  if (managerState != ManagerState::INITIALIZED) {
    logger.warning(F("SensorHandler"),
                   F("Sensormanager nicht initialisiert, Status: ") +
                       String((int)managerState));
    sendChunk(F("},\"error\":\"Sensormanager nicht initialisiert\"}"));
    endChunkedResponse();
    return;
  }

  const auto& sensors = _sensorManager.getSensors();
  const size_t sensorCount = sensors.size();

  if (sensorCount == 0) {
    logger.warning(F("SensorHandler"), F("Keine Sensoren im Sensormanager gefunden"));
    sendChunk(F("},\"error\":\"Keine Sensoren verfügbar\"}"));
    endChunkedResponse();
    return;
  }

  bool firstMeasurement = true;
  size_t processedSensors = 0;

  for (size_t sensorIndex = 0; sensorIndex < sensorCount && sensorIndex < 20;
       sensorIndex++) {
    if (ESP.getFreeHeap() < 4096) {
      logger.warning(F("SensorHandler"),
                     F("Wenig Speicher während der Verarbeitung, Abbruch"));
      break;
    }

    const auto& sensor = sensors[sensorIndex];
    if (!sensor) {
      logger.warning(F("SensorHandler"),
                     F("Null-Sensor an Index ") + String(sensorIndex));
      continue;
    }

    if (!sensor->isInitialized()) {
      logger.warning(F("SensorHandler"),
                     F("Überspringe nicht initialisierten Sensor: ") + sensor->getName());
      continue;
    }

    String sensorName;
    try {
      sensorName = sensor->getName();
      if (sensorName.length() == 0) {
        sensorName = F("Unbekannt_") + String(sensorIndex);
      }
    } catch (...) {
      logger.error(F("SensorHandler"), F("Fehler beim Abrufen des Sensornamens"));
      continue;
    }

    if (!sensor->isEnabled()) {
      logger.debug(F("SensorHandler"),
                   F("Sensor ") + sensorName + F(" ist deaktiviert"));
      continue;
    }

    MeasurementData measurementData;
    try {
      measurementData = sensor->getMeasurementData();
    } catch (...) {
      logger.error(F("SensorHandler"),
                   F("Fehler beim Abrufen der Messdaten für ") + sensorName);
      continue;
    }

    if (!measurementData.isValid() || measurementData.activeValues == 0) {
      logger.warning(F("SensorHandler"),
                     F("Ungültige Messdaten für Sensor ") + sensorName);
      continue;
    }

    size_t safeActiveValues =
        min(measurementData.activeValues,
            static_cast<size_t>(SensorConfig::MAX_MEASUREMENTS));
    safeActiveValues = min(safeActiveValues, MAX_VALUES);

    for (size_t i = 0; i < safeActiveValues; i++) {
      if (i >= SensorConfig::MAX_MEASUREMENTS ||
          i >= measurementData.values.size() ||
          i >= SensorConfig::MAX_MEASUREMENTS) {
        logger.warning(F("SensorHandler"),
                       F("Array-Grenzen überschritten für Sensor ") + sensorName);
        break;
      }

      String fieldName;
      try {
        fieldName = measurementData.fieldNames[i];
        if (fieldName.length() == 0) {
          continue;
        }

        fieldName.replace("\"", "");
        fieldName.replace("\n", "");
        fieldName.replace("\r", "");

        if (fieldName.length() > 50) {
          fieldName = fieldName.substring(0, 50);
        }
      } catch (...) {
        logger.error(F("SensorHandler"), F("Fehler beim Zugriff auf Feldnamen"));
        continue;
      }

      if (!firstMeasurement) {
        sendChunk(F(","));
      }
      firstMeasurement = false;

      float value = 0.0f;
      String unit;
      try {
        value = measurementData.values[i];
        unit = measurementData.units[i];

        unit.replace("\"", "");
        unit.replace("\n", "");
        unit.replace("\r", "");
        if (unit.length() > 10) {
          unit = unit.substring(0, 10);
        }
      } catch (...) {
        logger.error(F("SensorHandler"), F("Fehler beim Zugriff auf Wert/Einheit"));
        continue;
      }

      String fieldKey = sensor->getId() + "_" + String(i);
      sendChunk(F("\""));
      sendChunk(fieldKey);
      sendChunk(F("\":{\"value\":"));

      if (!isnan(value) && isfinite(value)) {
        sendChunk(String(value, 2));
      } else {
        sendChunk(F("null"));
      }

      sendChunk(F(",\"unit\":\""));
      sendChunk(unit);
      sendChunk(F("\""));

      sendChunk(F(",\"lastMeasurement\":"));
      sendChunk(String(sensor->getMeasurementStartTime()));
      sendChunk(F(",\"measurementInterval\":"));
      sendChunk(String(sensor->getMeasurementInterval()));
      sendChunk(F(",\"status\":\""));
      sendChunk(sensor->getStatus(i));
      sendChunk(F("\""));

      const auto& config = sensor->config();
      if (i < config.measurements.size()) {
        sendChunk(F(",\"absoluteMin\":"));
        sendChunk(String(config.measurements[i].absoluteMin, 2));
        sendChunk(F(",\"absoluteMax\":"));
        sendChunk(String(config.measurements[i].absoluteMax, 2));
      }

#if USE_ANALOG
      if (isAnalogSensor(sensor.get())) {
        AnalogSensor* analog = static_cast<AnalogSensor*>(sensor.get());
        int rawValue = analog->getLastRawValue(i);
        sendChunk(F(",\"raw\":"));
        sendChunk(String(rawValue));

        const auto& config = sensor->config();
        if (i < config.measurements.size()) {
          // If historical raw extrema are still the sentinel values, and
          // autocalibration is active, present the active calculation
          // limits as a UI-friendly fallback so the admin page shows
          // values instead of "--". This does NOT overwrite persisted
          // historical extrema on disk.
          int effectiveRawMin = config.measurements[i].absoluteRawMin;
          int effectiveRawMax = config.measurements[i].absoluteRawMax;
          if ((effectiveRawMin == INT_MAX || effectiveRawMax == INT_MIN) && config.measurements[i].calibrationMode) {
            if (analog) {
              float calcMin = analog->getMinValue(i);
              float calcMax = analog->getMaxValue(i);
              if (effectiveRawMin == INT_MAX) effectiveRawMin = static_cast<int>(roundf(calcMin));
              if (effectiveRawMax == INT_MIN) effectiveRawMax = static_cast<int>(roundf(calcMax));
            }
          }
          sendChunk(F(",\"absoluteRawMin\":"));
          sendChunk(String(effectiveRawMin));
          sendChunk(F(",\"absoluteRawMax\":"));
          sendChunk(String(effectiveRawMax));
           sendChunk(F(",\"calibrationMode\":"));
           sendChunk(config.measurements[i].calibrationMode ? F("true") : F("false"));
           // Also include the active calculation limits (min/max) used for
           // mapping so the admin UI can reflect autocal changes in real time.
           AnalogSensor* analogPtr = static_cast<AnalogSensor*>(sensor.get());
           if (analogPtr) {
             float calcMin = analogPtr->getMinValue(i);
             float calcMax = analogPtr->getMaxValue(i);
             sendChunk(F(",\"minmax\":{"));
             sendChunk(F("\"min\":"));
             sendChunk(String(calcMin));
             sendChunk(F(",\"max\":"));
             sendChunk(String(calcMax));
             sendChunk(F("}"));
           }
           // Note: autocalization now persists into the calculation limits
           // (min/max). The historical extremum storage (absoluteRawMin/Max)
           // remains untouched by autocal and reflects measured history only.
        }
      }
#endif

      sendChunk(F("}"));
    }

    processedSensors++;

    yield();
    ESP.wdtFeed();
  }

  sendChunk(F("}"));

  try {
    sendChunk(F(",\"system\":{\"freeHeap\":"));
    sendChunk(String(ESP.getFreeHeap()));
    sendChunk(F(",\"heapFragmentation\":"));
    sendChunk(String(ESP.getHeapFragmentation()));
    sendChunk(F(",\"rebootCount\":"));
    sendChunk(String(Helper::getRebootCount()));
    sendChunk(F(",\"version\":\""));
    sendChunk(VERSION);
    sendChunk(F("\",\"buildDate\":\""));
    sendChunk(F(__DATE__));
    sendChunk(F("\",\"processedSensors\":"));
    sendChunk(String(processedSensors));
    sendChunk(F("}}"));
  } catch (...) {
    logger.error(F("SensorHandler"), F("Fehler beim Systeminfo-Zugriff"));
    sendChunk(F(",\"error\":\"Systeminfo-Fehler\"}}"));
  }

  endChunkedResponse();
}

bool SensorHandler::validateRequest() const {
  return true;  // Sensorendpunkte sind öffentlich
}

void SensorHandler::createSensorListSection() const {
  Component::sendChunk(_server, F("<section>"));
  Component::sendChunk(_server, F("    <h3>Sensoren</h3>"));
  Component::sendChunk(_server, F("    <div>"));

  for (const auto& sensor : _sensorManager.getSensors()) {
    if (!sensor) continue;

    Component::sendChunk(_server, F("<div class='card'>"));
    Component::sendChunk(_server, F("<h3>"));
    Component::sendChunk(_server, sensor->getName());
    Component::sendChunk(_server, F("</h3>"));

    Component::sendChunk(_server,
                         F("<form method='post' action='/admin/sensor'>\n"));
    Component::sendChunk(
        _server,
        F("    <input type='hidden' name='action' value='toggle_sensor'>\n"));
    Component::sendChunk(
        _server, F("    <input type='hidden' name='sensor_id' value='"));
    Component::sendChunk(_server, sensor->getId());
    Component::sendChunk(_server, F("'>\n"));

    Component::sendChunk(_server, F("    <div>\n"));
    Component::sendChunk(
        _server, F("        <input type='checkbox' "
                   "id='enabled' name='enabled'"));
    if (sensor->isEnabled()) {
      Component::sendChunk(_server, F(" checked"));
    }
    Component::sendChunk(_server, F(">\n"));
    Component::sendChunk(_server, F("        <label "
                                    "for='enabled'>Aktiviert</label>\n"));
    Component::sendChunk(_server, F("    </div>\n"));

    Component::button(_server, "Aktualisieren", "submit", "btn btn-primary");
    Component::sendChunk(_server, F("</form>\n"));

    Component::sendChunk(_server, F("<div>\n"));
    Component::sendChunk(_server, F("    <p>Typ: "));
    Component::sendChunk(_server, sensor->getId());
    Component::sendChunk(_server, F("</p>\n"));
    Component::sendChunk(_server, F("    <p>Letzter Wert: "));

    try {
      auto data = sensor->getMeasurementData();
      if (data.isValid() && data.activeValues > 0) {
        Component::sendChunk(_server, String(data.values[0], 2));
      } else {
        Component::sendChunk(_server, F("N/A"));
      }
    } catch (...) {
      Component::sendChunk(_server, F("Fehler"));
    }

    Component::sendChunk(_server, F("</p>\n"));
    Component::sendChunk(_server, F("    <p>Status: "));
    Component::sendChunk(_server, sensor->isEnabled() ? F("OK") : F("Fehler"));
    Component::sendChunk(_server, F("</p>\n"));
    Component::sendChunk(_server, F("</div>\n"));

    Component::sendChunk(_server, F("</div>"));
  }

  Component::sendChunk(_server, F("    </div>\n"));
  Component::sendChunk(_server, F("</section>\n"));
}
