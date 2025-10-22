/**
 * @file admin_display_handler.cpp
 * @brief Implementation of display configuration handler
 */

#include "web/handler/admin_display_handler.h"

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "managers/manager_sensor_persistence.h"
#include "web/core/web_auth.h"

// Declare external global sensor manager
extern std::unique_ptr<SensorManager> sensorManager;

#if USE_DISPLAY
extern std::unique_ptr<DisplayManager> displayManager;

AdminDisplayHandler::~AdminDisplayHandler() = default;

AdminDisplayHandler::AdminDisplayHandler(ESP8266WebServer& server) : BaseHandler(server) {
  logger.debug(F("AdminDisplayHandler"), F("Initializing AdminDisplayHandler"));
}

void AdminDisplayHandler::handleDisplayConfig() {
  if (!validateRequest())
    return;
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin", "admin_display"};

  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin/display",
      [this]() {
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h3>Display-Einstellungen</h3>"));

        // Screen duration
        sendChunk(F("<div class='form-group'>"));
        sendChunk(F("<label>Anzeigedauer pro Bildschirm (Sekunden):</label>"));
        sendChunk(F("<input type='number' class='screen-duration-input' value='"));
        sendChunk(String(displayManager ? displayManager->getScreenDuration() / 1000 : 5));
        sendChunk(F("' min='1' max='60'>"));
        sendChunk(F("</div>"));

        // Clock format
        sendChunk(F("<div class='form-group'>"));
        sendChunk(F("<label>Uhrzeitformat:</label>"));
        sendChunk(F("<select class='clock-format-select'>"));
        String currentFormat = displayManager ? displayManager->getClockFormat() : "24h";
        sendChunk(F("<option value='24h'"));
        if (currentFormat == "24h")
          sendChunk(F(" selected"));
        sendChunk(F(">24-Stunden</option>"));
        sendChunk(F("<option value='12h'"));
        if (currentFormat == "12h")
          sendChunk(F(" selected"));
        sendChunk(F(">12-Stunden (AM/PM)</option>"));
        sendChunk(F("</select></div>"));

        // Show IP screen
        sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
        sendChunk(F("<input type='checkbox' class='show-ip-checkbox'"));
        if (displayManager && displayManager->isIpScreenEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> IP-Adresse anzeigen</label></div>"));

        // Show clock
        sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
        sendChunk(F("<input type='checkbox' class='show-clock-checkbox'"));
        if (displayManager && displayManager->isClockEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> Datum und Uhrzeit anzeigen</label></div>"));

        // Show flower image
        sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
        sendChunk(F("<input type='checkbox' class='show-flower-checkbox'"));
        if (displayManager && displayManager->isFlowerImageEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> Blumen-Bild anzeigen</label></div>"));

        // Show fabmobil image
        sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
        sendChunk(F("<input type='checkbox' class='show-fabmobil-checkbox'"));
        if (displayManager && displayManager->isFabmobilImageEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> Fabmobil-Logo anzeigen</label></div>"));

        sendChunk(F("</div>")); // Close first card

        // Sensor and measurement selection in separate card
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h3>Messungen anzeigen</h3>"));
        if (sensorManager) {
          for (const auto& sensor : sensorManager->getSensors()) {
            if (!sensor)
              continue;
            String id = sensor->getId();
            auto measurementData = sensor->getMeasurementData();

            if (measurementData.isValid() && measurementData.activeValues > 1) {
              // Sensor has multiple measurements - show individual measurements
              sendChunk(F("<div class='card-section'>"));
              sendChunk(F("<h4>"));
              sendChunk(sensor->getName());
              sendChunk(F("</h4>"));

              for (size_t i = 0; i < measurementData.activeValues; i++) {
                // Use user-friendly measurement name
                String measurementName = sensor->getMeasurementName(i);
                if (measurementName.isEmpty()) {
                  measurementName = String(measurementData.fieldNames[i]);
                }
                if (measurementName.isEmpty()) {
                  measurementName = "Messung " + String(i + 1);
                }

                sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
                sendChunk(F("<input type='checkbox' class='measurement-display-checkbox' "
                            "data-sensor-id='"));
                sendChunk(id);
                sendChunk(F("' data-measurement-index='"));
                sendChunk(String(i));
                sendChunk(F("'"));
                if (sensor->config().measurements[i].enabled) {
                  sendChunk(F(" checked"));
                }
                sendChunk(F("> "));
                sendChunk(measurementName);
                sendChunk(F(" ("));
                sendChunk(String(measurementData.units[i]));
                sendChunk(F(")</label></div>"));
              }
              sendChunk(F("</div>"));
            } else {
              // Sensor has only one measurement - show as simple checkbox
              sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
              sendChunk(
                  F("<input type='checkbox' class='sensor-display-checkbox' data-sensor-id='"));
              sendChunk(id);
              sendChunk(F("'"));
              if (sensor->isEnabled()) {
                sendChunk(F(" checked"));
              }
              sendChunk(F("> "));
              sendChunk(sensor->getName());
              sendChunk(F("</label></div>"));
            }
            yield();
          }
        }
        sendChunk(F("</div>")); // Close card
      },
      css, js);
}

void AdminDisplayHandler::handleScreenDurationUpdate() {
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("duration")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Fehlende Parameter\"}"));
    return;
  }

  int duration = _server.arg("duration").toInt();
  if (duration < 1 || duration > 60) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültige Dauer (1-60 Sekunden)\"}"));
    return;
  }

  if (displayManager) {
    displayManager->setScreenDuration(duration * 1000);
    auto result = displayManager->saveConfig();
    if (result.isSuccess()) {
      sendJsonResponse(200, F("{\"success\":true}"));
    } else {
      sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Speichern\"}"));
    }
  } else {
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Display Manager nicht verfügbar\"}"));
  }
}

void AdminDisplayHandler::handleClockFormatUpdate() {
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("format")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Fehlende Parameter\"}"));
    return;
  }

  String format = _server.arg("format");
  if (format != "24h" && format != "12h") {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiges Format\"}"));
    return;
  }

  if (displayManager) {
    displayManager->setClockFormat(format);
    auto result = displayManager->saveConfig();
    if (result.isSuccess()) {
      sendJsonResponse(200, F("{\"success\":true}"));
    } else {
      sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Speichern\"}"));
    }
  } else {
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Display Manager nicht verfügbar\"}"));
  }
}

void AdminDisplayHandler::handleDisplayToggle() {
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("setting") || !_server.hasArg("enabled")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Fehlende Parameter\"}"));
    return;
  }

  String setting = _server.arg("setting");
  bool enabled = _server.arg("enabled") == "true";

  if (displayManager) {
    if (setting == "show_ip") {
      displayManager->setIpScreenEnabled(enabled);
    } else if (setting == "show_clock") {
      displayManager->setClockEnabled(enabled);
    } else if (setting == "show_flower") {
      displayManager->setFlowerImageEnabled(enabled);
    } else if (setting == "show_fabmobil") {
      displayManager->setFabmobilImageEnabled(enabled);
    } else {
      sendJsonResponse(400, F("{\"success\":false,\"error\":\"Unbekannte Einstellung\"}"));
      return;
    }

    auto result = displayManager->saveConfig();
    if (result.isSuccess()) {
      sendJsonResponse(200, F("{\"success\":true}"));
    } else {
      sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Speichern\"}"));
    }
  } else {
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Display Manager nicht verfügbar\"}"));
  }
}

void AdminDisplayHandler::handleMeasurementDisplayToggle() {
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("enabled")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Fehlende Parameter\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  bool enabled = _server.arg("enabled") == "true";

  if (sensorManager) {
    auto sensor = sensorManager->getSensor(sensorId);
    if (!sensor) {
      sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
      return;
    }

    if (_server.hasArg("measurement_index")) {
      // Toggle individual measurement
      int index = _server.arg("measurement_index").toInt();
      if (index >= 0 && static_cast<size_t>(index) < sensor->config().measurements.size()) {
        sensor->mutableConfig().measurements[static_cast<size_t>(index)].enabled = enabled;
      } else {
        sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messungsindex\"}"));
        return;
      }
    } else {
      // Toggle entire sensor
      sensor->setEnabled(enabled);
    }

    // Save sensor config
    auto result = SensorPersistence::saveToFileMinimal();
    if (result.isSuccess()) {
      sendJsonResponse(200, F("{\"success\":true}"));
    } else {
      sendJsonResponse(500, F("{\"success\":false,\"error\":\"Fehler beim Speichern\"}"));
    }
  } else {
    sendJsonResponse(500, F("{\"success\":false,\"error\":\"Sensor Manager nicht verfügbar\"}"));
  }
}

bool AdminDisplayHandler::validateRequest() const {
  if (!_server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    _server.requestAuthentication();
    return false;
  }
  return true;
}

RouterResult AdminDisplayHandler::onRegisterRoutes(WebRouter& router) {
  auto result = router.addRoute(HTTP_GET, "/admin/display", [this]() { handleDisplayConfig(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_POST, "/admin/display/screen_duration",
                           [this]() { handleScreenDurationUpdate(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_POST, "/admin/display/clock_format",
                           [this]() { handleClockFormatUpdate(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_POST, "/admin/display/toggle", [this]() { handleDisplayToggle(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_POST, "/admin/display/measurement_toggle",
                           [this]() { handleMeasurementDisplayToggle(); });
  if (!result.isSuccess())
    return result;

  logger.info(F("AdminDisplayHandler"), F("Display config routes registered"));
  return RouterResult::success();
}

#endif // USE_DISPLAY
