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
  logger.debug(F("StartpageHandler"), F("Startseite angefordert"));
  _cleaned = false;
  std::vector<String> css = {"start"};
  std::vector<String> js = {"sensors"};

  // Custom rendering without navigation and footer wrapper
  if (!Component::beginResponse(_server, ConfigMgr.getDeviceName(), css)) {
    return;
  }

  // Main container with dynamic status class - this is the entire page
  sendChunk(F("<div class='box status-unknown'>"));
  sendChunk(F("<div class='group'><div class='div'>"));

  // Cloud/Title with device name
  sendChunk(F("<div class='cloud' aria-label='"));
  sendChunk(ConfigMgr.getDeviceName());
  sendChunk(F("'>"));
  sendChunk(F("<img class='cloud-img' src='/img/cloud_big.png' alt='' />"));
  sendChunk(F("<div class='cloud-label'>"));
  sendChunk(ConfigMgr.getDeviceName());
  sendChunk(F("</div></div>"));

  // Flower with animated face
  sendChunk(F("<div class='flower-wrap'>"));
  sendChunk(F("<img class='flower' src='/img/flower_big.gif' alt='Flower' />"));
  sendChunk(F("<img class='face' src='/img/face-neutral.gif' alt='Face' />"));
  sendChunk(F("</div>"));

  // Sensors container
  generateAndSendSensorGrid();

  // Footer with earth and info
  generateAndSendFooter();

  sendChunk(F("</div></div></div>"));

  // End response with scripts
  Component::endResponse(_server, js);

  logger.debug(F("StartpageHandler"), F("Startseite erfolgreich gesendet"));
}

void StartpageHandler::generateAndSendSensorGrid() {
  sendChunk(F("<div class='sensors-container'>"));

  if (sensorManager) {
    size_t sensorIndex = 0;
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
                          statusStr.c_str(), i, sensorIndex);
        sensorIndex++;
        yield();
      }
    }
  }
  sendChunk(F("</div>"));
}

void StartpageHandler::generateSensorBox(const Sensor* sensor, float value,
                                         const String& name, const String& unit,
                                         const char* status,
                                         size_t measurementIndex, size_t sensorIndex) {
  // Safety check: ensure sensor exists
  if (!sensor) {
    logger.warning(F("StartpageHandler"),
                   F("Attempted to generate sensor box for null sensor"));
    return;
  }

  // Get current sensor status for this measurement
  String currentStatus = status ? String(status) : "unknown";
  const char* statusStr = currentStatus.c_str();

  // Determine if this sensor should be on left or right
  const char* position = (sensorIndex % 2 == 0) ? "left" : "right";

  // Start sensor container
  sendChunk(F("<div class='sensor "));
  sendChunk(position);
  sendChunk(F("' data-sensor='"));
  sendChunk(sensor->getId());
  sendChunk(F("_"));
  sendChunk(String(measurementIndex));
  sendChunk(F("'>"));

  // Leaf image
  sendChunk(F("<img class='leaf' src='/img/sensor-leaf2.png' alt='' />"));

  // Card with sensor data
  sendChunk(F("<div class='card'>"));

  // Label (measurement name)
  sendChunk(F("<div class='label'><span>"));
  String upperName = name;
  upperName.toUpperCase();
  sendChunk(upperName);
  sendChunk(F("</span></div>"));

  // Value
  sendChunk(F("<div class='value'><span>"));
  if (sensor->isInitialized() && !isnan(value) && isfinite(value)) {
    char valueStr[10];
    dtostrf(value, 1, 1, valueStr);
    sendChunk(valueStr);
    if (unit.length() > 0) {
      sendChunk(unit);
    }
  } else {
    sendChunk(F("--"));
  }
  sendChunk(F("</span></div>"));

  // Status
  sendChunk(F("<div class='status "));
  sendChunk(statusStr);
  sendChunk(F("'><span>STATUS: "));
  sendChunk(translateStatus(statusStr));
  sendChunk(F("</span></div>"));

  // Interval/timing
  sendChunk(F("<div class='interval'><span>"));
  if (sensor->isInitialized()) {
    unsigned long lastMeasurement = sensor->getMeasurementStartTime();
    uint32_t interval = sensor->getMeasurementInterval();
    unsigned long currentTime = millis();

    if (lastMeasurement > 0) {
      unsigned long elapsed = (currentTime - lastMeasurement) / 1000;
      uint32_t intervalSec = interval / 1000;
      sendChunk(F("("));
      sendChunk(String(elapsed));
      sendChunk(F("s/"));
      sendChunk(String(intervalSec));
      sendChunk(F("s)"));
    } else {
      sendChunk(F("(--/--)"));
    }
  } else {
    sendChunk(F("(--/--)"));
  }
  sendChunk(F("</span></div>"));

  sendChunk(F("</div>"));  // Close card
  sendChunk(F("</div>"));  // Close sensor
}

void StartpageHandler::generateAndSendFooter() {
  sendChunk(F("<div class='footer'>"));
  sendChunk(F("<div class='base'>"));

  // Earth image
  sendChunk(F("<img class='earth' src='/img/earth.png' alt='Earth' />"));

  // Base overlay with navigation and stats
  sendChunk(F("<footer class='base-overlay' aria-label='Statusleiste'>"));
  sendChunk(F("<div class='footer-grid'>"));

  // Navigation (Row 1, Column 1)
  sendChunk(F("<nav class='nav-box' aria-label='Navigation'><ul class='nav-list'>"));
  sendChunk(F("<li><a href='/' class='nav-item'>START</a></li>"));
  sendChunk(F("<li><a href='/logs' class='nav-item'>LOGS</a></li>"));
  sendChunk(F("<li><a href='/admin' class='nav-item'>ADMIN</a></li>"));
  sendChunk(F("</ul></nav>"));

  // Stats Labels (Row 1, Column 2)
  sendChunk(F("<ul class='stats-labels'>"));
  sendChunk(F("<li>📅 Zeit</li>"));
  sendChunk(F("<li>🌐 SSID</li>"));
  sendChunk(F("<li>💻 IP</li>"));
  sendChunk(F("<li>📶 WIFI</li>"));
  sendChunk(F("<li>⏲️ UPTIME</li>"));
  sendChunk(F("<li>🔄 RESTARTS</li>"));
  sendChunk(F("</ul>"));

  // Stats Values (Row 1, Column 3)
  sendChunk(F("<ul class='stats-values'>"));
  sendChunk(F("<li>"));
  sendChunk(Helper::getFormattedDate());
  sendChunk(F(" "));
  sendChunk(Helper::getFormattedTime());
  sendChunk(F("</li><li>"));
  sendChunk(WiFi.SSID());
  sendChunk(F("</li><li>"));
  sendChunk(WiFi.localIP().toString());
  sendChunk(F("</li><li>"));
  sendChunk(String(WiFi.RSSI()));
  sendChunk(F(" dBm"));
  sendChunk(F("</li><li>"));
  sendChunk(Helper::getFormattedUptime());
  sendChunk(F("</li><li>"));
  sendChunk(String(Helper::getRebootCount()));
  sendChunk(F("</li></ul>"));

  // Logo (Row 2, Column 1)
  sendChunk(F("<div class='footer-logo'><img src='/img/fabmobil.png' alt='FABMOBIL' /></div>"));

  // Version (Row 2, Column 2)
  sendChunk(F("<div class='footer-version'>V "));
  sendChunk(VERSION);
  sendChunk(F("</div>"));

  // Build (Row 2, Column 3)
  sendChunk(F("<div class='footer-build'>BUILD: "));
  sendChunk(__DATE__);
  sendChunk(F("</div>"));

  sendChunk(F("</div>"));  // Close footer-grid
  sendChunk(F("</footer>"));  // Close base-overlay
  sendChunk(F("</div>"));  // Close base
  sendChunk(F("</div>"));  // Close footer
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
