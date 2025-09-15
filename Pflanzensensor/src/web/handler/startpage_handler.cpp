/**
 * @file startpage_handler.cpp
 * @brief Implementation of main sensor dashboard handler
 */

#include "web/handler/startpage_handler.h"

#include <ArduinoJson.h>
#include <time.h>

#include <cmath>

#include "managers/manager_config.h"
#include "utils/helper.h"

// Declare external global sensor manager
extern std::unique_ptr<SensorManager> sensorManager;

HandlerResult StartpageHandler::handleGet(
    const String& uri, const std::map<String, String>& query) {
  if (uri == "/") {
    handleRoot();
    return HandlerResult::success();
  }
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unbekannter Endpunkt");
}

HandlerResult StartpageHandler::handlePost(
    const String& uri, const std::map<String, String>& params) {
  return HandlerResult::fail(HandlerError::NOT_FOUND,
                             "Keine POST-Endpunkte verfügbar");
}

void StartpageHandler::handleRoot() {
  logger.debug(F("StartpageHandler"), F("Handling root request"));
  _cleaned = false;
  std::vector<String> css = {"start"};
  std::vector<String> js = {"sensors"};

  renderPage(
      ConfigMgr.getDeviceName(), "start",
      [this]() {
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>"));
        sendChunk(ConfigMgr.getDeviceName());
        sendChunk(F("</h2>"));
        sendChunk(F("</div>"));
        generateAndSendSensorGrid();
        generateAndSendInfoContainer();
      },
      css, js);

    logger.debug(F("StartpageHandler"), F("Startseite erfolgreich gesendet"));
}

void StartpageHandler::generateAndSendSensorGrid() {
  sendChunk(F("<div class='sensor-grid'>"));

  if (sensorManager) {
    for (const auto& sensor : sensorManager->getSensors()) {
      if (!sensor || !sensor->isEnabled()) continue;

      // Check if sensor has any valid data to display
      bool hasValidData = false;
      String statusStr = "unknown";

      if (sensor->isInitialized()) {
        auto measurementData = sensor->getMeasurementData();
        if (measurementData.activeValues > 0) {
          hasValidData = true;
          statusStr = sensor->getStatus(0);  // Get status for first measurement
        }
      }

      // If sensor is not initialized but has configuration, try to show it
      // anyway
      if (!hasValidData) {
        const SensorConfig& config = sensor->config();
        if (config.activeMeasurements > 0) {
          hasValidData = true;
          statusStr = "error";  // Mark as error state
        }
      }

      if (!hasValidData) {
        logger.debug(F("StartpageHandler"),
                     F("Skipping sensor with no data: ") + sensor->getName());
        continue;
      }

      // Generate a box for each active measurement
      const SensorConfig& config = sensor->config();
      for (size_t i = 0; i < config.activeMeasurements; i++) {
        if (!config.measurements[i].enabled) continue;

        String measurementName = config.measurements[i].name;
        if (measurementName.length() == 0) {
          measurementName = sensor->getMeasurementName(i);
        }

        String unit = "";
        float value = 0.0f;

        if (sensor->isInitialized()) {
          auto measurementData = sensor->getMeasurementData();
          if (i < measurementData.activeValues) {
            value = measurementData.values[i];
            unit = measurementData.units[i];
            statusStr = sensor->getStatus(i);
          }
        }

        // If no unit from measurement data, try to get it from config
        if (unit.length() == 0) {
          unit = config.measurements[i].unit;
        }

        generateSensorBox(sensor.get(), value, measurementName, unit,
                          statusStr.c_str(), i);
        yield();
      }
    }
  }
  sendChunk(F("</div>"));
}

void StartpageHandler::generateSensorBox(const Sensor* sensor, float value,
                                         const String& name, const String& unit,
                                         const char* status,
                                         size_t measurementIndex) {
  // Safety check: ensure sensor exists
  if (!sensor) {
    logger.warning(F("StartpageHandler"),
                   F("Attempted to generate sensor box for null sensor"));
    return;
  }

  // Get current sensor status for this measurement
  String currentStatus = status ? String(status) : "unknown";
  const char* statusStr = currentStatus.c_str();

  // Start sensor box with status class
  sendChunk(F("<div class='sensor-box card status-"));
  sendChunk(statusStr);  // This adds the status class for coloring
  sendChunk(F("' data-sensor='"));
  sendChunk(sensor->getId());
  sendChunk(F("_"));
  sendChunk(String(measurementIndex));
  sendChunk(F("' data-unit='"));
  sendChunk(unit);
  sendChunk(F("'><h3 class='sensor-title'>"));
  sendChunk(name);
  sendChunk(F("</h3>"));

  // Value container with measurement button
  sendChunk(F("<div class='value-container'>"));
  sendChunk(F("<div class='value'>"));

  // Display value if valid, otherwise show status
  if (sensor->isInitialized() && !isnan(value) && isfinite(value)) {
    char valueStr[10];
    dtostrf(value, 1, 1, valueStr);
    sendChunk(valueStr);
    if (unit.length() > 0) {
      sendChunk(F(" "));
      sendChunk(unit);
    }
  } else {
    // Show error or status message instead of invalid value
    if (strcmp(statusStr, "error") == 0) {
      sendChunk(F("Fehler"));
    } else if (strcmp(statusStr, "unknown") == 0) {
      sendChunk(F("Unbekannt"));
    } else {
      sendChunk(F("Keine Daten"));
    }
  }
  sendChunk(F("</div>"));

  // Add measure button
  sendChunk(
      F("<button class='button button-primary measure-button' data-sensor='"));
  sendChunk(sensor->getId());
  sendChunk(F("'>Messen!</button>"));
  sendChunk(F("</div>"));

  // Add min/max values display - always try to show these from config
  const SensorConfig& config = sensor->config();
  if (measurementIndex < config.activeMeasurements) {
    const MeasurementConfig& measurement =
        config.measurements[measurementIndex];

    // Only show min/max if they have valid values (not INFINITY/-INFINITY)
    if (measurement.absoluteMin != INFINITY ||
        measurement.absoluteMax != -INFINITY) {
      sendChunk(F("<div class='min-max-container'>"));

      // Min value
      if (measurement.absoluteMin != INFINITY) {
        sendChunk(F("<div class='min-max-item'>"));
        sendChunk(F("<span class='min-max-label'>Min:</span> "));
        char minStr[10];
        dtostrf(measurement.absoluteMin, 1, 1, minStr);
        sendChunk(minStr);
        if (unit.length() > 0) {
          sendChunk(F(" "));
          sendChunk(unit);
        }
        sendChunk(F("</div>"));
      }

      // Max value
      if (measurement.absoluteMax != -INFINITY) {
        sendChunk(F("<div class='min-max-item'>"));
        sendChunk(F("<span class='min-max-label'>Max:</span> "));
        char maxStr[10];
        dtostrf(measurement.absoluteMax, 1, 1, maxStr);
        sendChunk(maxStr);
        if (unit.length() > 0) {
          sendChunk(F(" "));
          sendChunk(unit);
        }
        sendChunk(F("</div>"));
      }

      sendChunk(F("</div>"));
    }
  }

  // Add status indicator and last measurement info
  sendChunk(F("<div class='details'>"));
  sendChunk(F("<div class='status-indicator status-"));
  sendChunk(statusStr);
  sendChunk(F("'>Status: "));
  sendChunk(translateStatus(statusStr));

  // Add measurement time info if sensor is initialized
  if (sensor->isInitialized()) {
    unsigned long lastMeasurement = sensor->getMeasurementStartTime();
    uint32_t interval = sensor->getMeasurementInterval();
    unsigned long currentTime = millis();

    if (lastMeasurement > 0) {
      unsigned long timeSinceLastMeasurement = currentTime - lastMeasurement;
      sendChunk(F(", "));
      sendChunk(String(timeSinceLastMeasurement / 1000));  // Convert to seconds
      sendChunk(F("s/"));
      sendChunk(String(interval / 1000));  // Convert interval to seconds
      sendChunk(F("s"));
    } else {
      sendChunk(F(", Keine Messung"));
    }
  } else {
    sendChunk(F(", Sensor nicht initialisiert"));
  }
  sendChunk(F("</div></div>"));

  // Add last measurement time with data attributes for JS updating
  if (sensor->isInitialized()) {
    unsigned long lastMeasurement = sensor->getMeasurementStartTime();
    uint32_t interval = sensor->getMeasurementInterval();
    unsigned long currentTime = millis();

    sendChunk(F("<div class='last-measurement' data-time='"));
    sendChunk(String(lastMeasurement));
    sendChunk(F("' data-interval='"));
    sendChunk(String(interval));
    sendChunk(F("' data-server-time='"));
    sendChunk(String(currentTime));
    sendChunk(F("'></div>"));
  } else {
    sendChunk(
        F("<div class='last-measurement' data-time='0' data-interval='0' "
          "data-server-time='0'></div>"));
  }

  sendChunk(F("</div>"));
}

void StartpageHandler::generateAndSendInfoContainer() {
  sendChunk(F("<div class='info-container'>"));

  // Add date/time info
  sendChunk(F("<div class='info-box card'><div class='info-item'>📅 "));
  sendChunk(Helper::getFormattedDate());
  sendChunk(F(" "));
  sendChunk(Helper::getFormattedTime());
  sendChunk(F("</div></div>"));

  // Add uptime info
  sendChunk(
      F("<div class='info-box card'><div class='info-item'>⏲️ Uptime: "));
  sendChunk(Helper::getFormattedUptime());
  sendChunk(F("</div></div>"));

  // Add SSID info
  sendChunk(F("<div class='info-box card'><div class='info-item'>🌐 SSID: "));
  sendChunk(WiFi.SSID());
  sendChunk(F("</div></div>"));

  // Add IP info
  sendChunk(F("<div class='info-box card'><div class='info-item'>💻 IP: "));
  sendChunk(WiFi.localIP().toString());
  sendChunk(F("</div></div>"));

  // Add WiFi signal strength
  sendChunk(
      F("<div class='info-box card'><div class='info-item'>📶 WiFi "
        "Signalstärke: "));
  sendChunk(String(WiFi.RSSI()));
  sendChunk(F(" dBm</div></div>"));

  // Add free heap memory info
  sendChunk(
      F("<div class='info-box card'><div class='info-item' id='free-heap'>🧮 "
        "Freier HEAP: "));
  sendChunk(String(ESP.getFreeHeap()));
  sendChunk(F(" bytes</div></div>"));

  // Add heap fragmentation info
  sendChunk(
      F("<div class='info-box card'><div class='info-item' "
        "id='heap-fragmentation'>📊 Heap Fragmentierung: "));
  sendChunk(String(ESP.getHeapFragmentation()));
  sendChunk(F("%</div></div>"));

  // Add number of restarts info
  sendChunk(
      F("<div class='info-box card'><div class='info-item' id='reboot-count'>🔄 "
        "Restarts: "));
  sendChunk(String(Helper::getRebootCount()));
  sendChunk(F("</div></div>"));

  // Add WiFi setup form when in AP mode
  renderWiFiSetupForm();

  sendChunk(F("</div>"));
}

const char* StartpageHandler::translateStatus(const char* status) const {
  if (strcmp(status, "green") == 0) return "OK";
  if (strcmp(status, "yellow") == 0) return "Warnung";
  if (strcmp(status, "red") == 0) return "Kritisch";
  if (strcmp(status, "error") == 0) return "Fehler";
  if (strcmp(status, "warmup") == 0) return "Aufwärmen";
  if (strcmp(status, "unknown") == 0) return "Unbekannt";
  return status;
}

StartpageHandler::~StartpageHandler() = default;

RouterResult StartpageHandler::onRegisterRoutes(WebRouter& router) {
  logger.debug(F("StartpageHandler"), F("Registering startpage routes"));

  auto result = router.addRoute(HTTP_GET, "/", [this]() { handleRoot(); });
  if (!result.isSuccess()) {
    return RouterResult::fail(
        RouterError::REGISTRATION_FAILED,
        "Failed to register root handler: " + result.getMessage());
  }

  return RouterResult::success();
}
