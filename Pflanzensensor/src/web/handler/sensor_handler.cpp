/**
 * @file sensor_handler.cpp
 * Fixed version with memory safety improvements
 */

#include "web/handler/sensor_handler.h"

#include <ArduinoJson.h>

#include "influxdb/influxdb.h"  // For getRebootCount()
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/helper.h"
#include "web/core/components.h"

// Maximum number of values per sensor
static constexpr size_t MAX_VALUES = 10;

RouterResult SensorHandler::onRegisterRoutes(WebRouter& router) {
  logger.info(F("SensorHandler"), F("Registering sensor routes:"));

  // Register Latest Values endpoint
  auto latestResult = router.addRoute(HTTP_GET, "/getLatestValues",
                                      [this]() { handleGetLatestValues(); });
  if (!latestResult.isSuccess()) return latestResult;

  logger.info(F("SensorHandler"), F("Sensor routes registered successfully"));
  return RouterResult::success();
}

HandlerResult SensorHandler::handleGet(
    const String& /*uri*/, const std::map<String, String>& /*query*/) {
  // Let registerRoutes handle the actual routing
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             "Use registerRoutes instead");
}

HandlerResult SensorHandler::handlePost(
    const String& /*uri*/, const std::map<String, String>& /*params*/) {
  // Let registerRoutes handle the actual routing
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             "Use registerRoutes instead");
}

void SensorHandler::handleGetLatestValues() {
  // **CRITICAL FIX 1: Memory check before starting**
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 4096) {  // Require at least 4KB free
    logger.warning(
        F("SensorHandler"),
        F("Insufficient memory for JSON response: ") + String(freeHeap));
    _server.send(503, F("application/json"),
                 F("{\"error\":\"Insufficient memory\"}"));
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

  // **FIXED: Remove invalid null check for reference**
  // References cannot be null in C++, so &_sensorManager will always be valid

  auto managerState = _sensorManager.getState();
  if (managerState != ManagerState::INITIALIZED) {
    logger.warning(F("SensorHandler"),
                   F("Sensor manager not initialized, state: ") +
                       String((int)managerState));
    sendChunk(F("},\"error\":\"Sensor manager not initialized\"}"));
    endChunkedResponse();
    return;
  }

  // **CRITICAL FIX 3: Safe sensor access with bounds checking**
  const auto& sensors = _sensorManager.getSensors();
  const size_t sensorCount = sensors.size();

  if (sensorCount == 0) {
    logger.warning(F("SensorHandler"), F("No sensors found in sensor manager"));
    sendChunk(F("},\"error\":\"No sensors available\"}"));
    endChunkedResponse();
    return;
  }

  bool firstMeasurement = true;
  size_t processedSensors = 0;

  for (size_t sensorIndex = 0; sensorIndex < sensorCount && sensorIndex < 20;
       sensorIndex++) {
    // **CRITICAL FIX 4: Memory check during loop**
    if (ESP.getFreeHeap() < 4096) {
      logger.warning(F("SensorHandler"),
                     F("Low memory during processing, stopping"));
      break;
    }

    const auto& sensor = sensors[sensorIndex];
    if (!sensor) {
      logger.warning(F("SensorHandler"),
                     F("Null sensor at index ") + String(sensorIndex));
      continue;
    }

    // Safety check: ensure sensor is properly initialized before accessing its
    // data
    if (!sensor->isInitialized()) {
      logger.warning(F("SensorHandler"),
                     F("Skipping uninitialized sensor: ") + sensor->getName());
      continue;
    }

    // **CRITICAL FIX 5: Safe string access**
    String sensorName;
    try {
      sensorName = sensor->getName();
      if (sensorName.length() == 0) {
        sensorName = F("Unknown_") + String(sensorIndex);
      }
    } catch (...) {
      logger.error(F("SensorHandler"), F("Exception getting sensor name"));
      continue;
    }

    if (!sensor->isEnabled()) {
      logger.debug(F("SensorHandler"),
                   F("Sensor ") + sensorName + F(" is disabled"));
      continue;
    }

    // **CRITICAL FIX 6: Safe measurement data access**
    MeasurementData measurementData;
    try {
      measurementData = sensor->getMeasurementData();
    } catch (...) {
      logger.error(F("SensorHandler"),
                   F("Exception getting measurement data for ") + sensorName);
      continue;
    }

    if (!measurementData.isValid() || measurementData.activeValues == 0) {
      logger.warning(F("SensorHandler"),
                     F("Invalid measurement data for sensor ") + sensorName);
      continue;
    }

    // **CRITICAL FIX 7: Bounds checking for measurement values**
    size_t safeActiveValues =
        min(measurementData.activeValues,
            static_cast<size_t>(SensorConfig::MAX_MEASUREMENTS));
    safeActiveValues = min(safeActiveValues, MAX_VALUES);

    // Output each measurement with safe bounds checking
    for (size_t i = 0; i < safeActiveValues; i++) {
      // **CRITICAL FIX 8: Array bounds protection**
      if (i >= SensorConfig::MAX_MEASUREMENTS ||
          i >= measurementData.values.size() ||
          i >= SensorConfig::MAX_MEASUREMENTS) {
        logger.warning(F("SensorHandler"),
                       F("Array bounds exceeded for sensor ") + sensorName);
        break;
      }

      // **CRITICAL FIX 9: Safe string access for field names**
      String fieldName;
      try {
        fieldName = measurementData.fieldNames[i];
        if (fieldName.length() == 0) {
          continue;  // Skip empty field names
        }

        // Sanitize field name - remove any problematic characters
        fieldName.replace("\"", "");
        fieldName.replace("\n", "");
        fieldName.replace("\r", "");

        if (fieldName.length() > 50) {  // Limit field name length
          fieldName = fieldName.substring(0, 50);
        }
      } catch (...) {
        logger.error(F("SensorHandler"), F("Exception accessing field name"));
        continue;
      }

      if (!firstMeasurement) {
        sendChunk(F(","));
      }
      firstMeasurement = false;

      // **CRITICAL FIX 10: Safe value and unit access**
      float value = 0.0f;
      String unit;
      try {
        value = measurementData.values[i];
        unit = measurementData.units[i];

        // Sanitize unit string
        unit.replace("\"", "");
        unit.replace("\n", "");
        unit.replace("\r", "");
        if (unit.length() > 10) {
          unit = unit.substring(0, 10);
        }
      } catch (...) {
        logger.error(F("SensorHandler"), F("Exception accessing value/unit"));
        continue;
      }

      // Build JSON safely
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

      // Add lastMeasurement and measurementInterval
      sendChunk(F(",\"lastMeasurement\":"));
      sendChunk(String(sensor->getMeasurementStartTime()));
      sendChunk(F(",\"measurementInterval\":"));
      sendChunk(String(sensor->getMeasurementInterval()));
      sendChunk(F(",\"status\":\""));
      sendChunk(sensor->getStatus(i));
      sendChunk(F("\""));

      // Always include absolute min/max values to ensure they are displayed
      // properly
      const auto& config = sensor->config();
      if (i < config.measurements.size()) {
        sendChunk(F(",\"absoluteMin\":"));
        sendChunk(String(config.measurements[i].absoluteMin, 2));
        sendChunk(F(",\"absoluteMax\":"));
        sendChunk(String(config.measurements[i].absoluteMax, 2));
      }

      // Add raw value for analog sensors
#if USE_ANALOG
      if (isAnalogSensor(sensor.get())) {
        AnalogSensor* analog = static_cast<AnalogSensor*>(sensor.get());
        int rawValue = analog->getLastRawValue(i);
        sendChunk(F(",\"raw\":"));
        sendChunk(String(rawValue));

        // Always include absolute raw min/max values for analog sensors to
        // ensure they are displayed properly
        const auto& config = sensor->config();
        if (i < config.measurements.size()) {
          sendChunk(F(",\"absoluteRawMin\":"));
          sendChunk(String(config.measurements[i].absoluteRawMin));
          sendChunk(F(",\"absoluteRawMax\":"));
          sendChunk(String(config.measurements[i].absoluteRawMax));
        }
      }
#endif

      sendChunk(F("}"));  // <-- This closes the measurement object
    }

    processedSensors++;

    // **CRITICAL FIX 12: Yield after each sensor**
    yield();
    ESP.wdtFeed();
  }

  sendChunk(F("}"));  // Close sensors object

  // **CRITICAL FIX 13: Safe system info with error handling**
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
    logger.error(F("SensorHandler"), F("Exception in system info"));
    sendChunk(F(",\"error\":\"System info error\"}}"));
  }

  endChunkedResponse();
}

bool SensorHandler::validateRequest() const {
  return true;  // Sensor endpoints are public
}

void SensorHandler::createSensorListSection() const {
  Component::sendChunk(_server, F("<section class='sensor-section'>"));
  Component::sendChunk(_server, F("    <h3>Sensoren</h3>"));
  Component::sendChunk(_server, F("    <div class='sensor-grid'>"));

  for (const auto& sensor : _sensorManager.getSensors()) {
    if (!sensor) continue;  // Safety check

    Component::sendChunk(_server, F("<div class='card'>"));
    Component::sendChunk(_server, F("<h3>"));
    Component::sendChunk(_server, sensor->getName());
    Component::sendChunk(_server, F("</h3>"));

    // Form
    Component::sendChunk(_server,
                         F("<form method='post' action='/admin/sensor'>\n"));
    Component::sendChunk(
        _server,
        F("    <input type='hidden' name='action' value='toggle_sensor'>\n"));
    Component::sendChunk(
        _server, F("    <input type='hidden' name='sensor_id' value='"));
    Component::sendChunk(_server, sensor->getId());
    Component::sendChunk(_server, F("'>\n"));

    // Checkbox
    Component::sendChunk(_server, F("    <div class='form-check'>\n"));
    Component::sendChunk(
        _server, F("        <input type='checkbox' class='form-check-input' "
                   "id='enabled' name='enabled'"));
    if (sensor->isEnabled()) {
      Component::sendChunk(_server, F(" checked"));
    }
    Component::sendChunk(_server, F(">\n"));
    Component::sendChunk(_server, F("        <label class='form-check-label' "
                                    "for='enabled'>Aktiviert</label>\n"));
    Component::sendChunk(_server, F("    </div>\n"));

    // Button
    Component::button(_server, "Aktualisieren", "submit", "btn btn-primary");
    Component::sendChunk(_server, F("</form>\n"));

    // Sensor Info
    Component::sendChunk(_server, F("<div class='sensor-info'>\n"));
    Component::sendChunk(_server, F("    <p>Typ: "));
    Component::sendChunk(_server, sensor->getId());
    Component::sendChunk(_server, F("</p>\n"));
    Component::sendChunk(_server, F("    <p>Letzter Wert: "));

    // Safe value access
    try {
      auto data = sensor->getMeasurementData();
      if (data.isValid() && data.activeValues > 0) {
        Component::sendChunk(_server, String(data.values[0], 2));
      } else {
        Component::sendChunk(_server, F("N/A"));
      }
    } catch (...) {
      Component::sendChunk(_server, F("Error"));
    }

    Component::sendChunk(_server, F("</p>\n"));
    Component::sendChunk(_server, F("    <p>Status: "));
    Component::sendChunk(_server, sensor->isEnabled() ? F("OK") : F("Fehler"));
    Component::sendChunk(_server, F("</p>\n"));
    Component::sendChunk(_server, F("</div>\n"));

    Component::sendChunk(_server, F("</div>"));  // End card
  }

  Component::sendChunk(_server, F("    </div>\n"));  // End sensor-grid
  Component::sendChunk(_server, F("</section>\n"));
}
