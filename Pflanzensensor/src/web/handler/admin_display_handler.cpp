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
  logger.debug(F("AdminDisplayHandler"), F("Initialisiere AdminDisplayHandler"));
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

        // Show QR code screen
        sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
        sendChunk(F("<input type='checkbox' class='show-qr-checkbox'"));
        if (displayManager && displayManager->isQrCodeEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> QR-Code-Seite anzeigen</label></div>"));

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
                if (displayManager->isSensorMeasurementShown(id, i)) {
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
              // The checkbox controls the DISPLAY of the measurement on the
              // display, not whether the sensor itself is enabled. If the
              // sensor exposes measurement metadata we prefer the
              // measurement enabled flag; otherwise fall back to sensor
              // enabled state for compatibility.
              sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
              sendChunk(
                  F("<input type='checkbox' class='sensor-display-checkbox' data-sensor-id='"));
              sendChunk(id);
              sendChunk(F("'"));
              bool checked = false;
              // Prefer display-only flag when available
              if (displayManager) {
                if (displayManager->isSensorMeasurementShown(id, 0)) {
                  checked = true;
                }
              } else {
                // Prefer per-measurement enabled flag when available
                if (measurementData.isValid() && measurementData.activeValues >= 1) {
                  if (sensor->config().measurements.size() > 0 &&
                      sensor->config().measurements[0].enabled) {
                    checked = true;
                  }
                } else {
                  // Fallback (older sensors): use sensor enabled state
                  if (sensor->isEnabled()) {
                    checked = true;
                  }
                }
              }
              if (checked) {
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

void AdminDisplayHandler::handleMeasurementDisplayToggle() {
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  // Require new AJAX params 'measurement' and 'enabled'
  if (!_server.hasArg("measurement") || !_server.hasArg("enabled")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Fehlende Parameter: measurement und "
                            "enabled erwartet\"}"));
    return;
  }

  String sensorId = _server.arg("measurement");
  bool enabled = _server.arg("enabled") == "true";

  if (sensorManager) {
    auto sensor = sensorManager->getSensor(sensorId);
    if (!sensor) {
      sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
      return;
    }

    // Toggle only the display state for measurements (do NOT change sensor
    // sampling enabled flags). Persist in display configuration so the
    // rotation logic can read it.
    if (_server.hasArg("measurement_index")) {
      int index = _server.arg("measurement_index").toInt();
      if (index < 0) {
        sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messungsindex\"}"));
        return;
      }

      // Ask display manager to persist the display-only flag
      if (displayManager) {
        auto res = displayManager->setSensorMeasurementDisplay(sensorId, static_cast<size_t>(index),
                                                               enabled);
        if (!res.isSuccess()) {
          sendJsonResponse(500,
                           String("{\"success\":false,\"error\":\"") + res.getMessage() + "\"}");
          return;
        }
      } else {
        sendJsonResponse(500,
                         F("{\"success\":false,\"error\":\"Display Manager nicht verfügbar\"}"));
        return;
      }
    } else {
      // No index provided: apply to all measurements of this sensor. We
      // don't modify sensor enabled flags; instead set display flags for
      // each measurement index up to activeMeasurements (or 1 if unknown).
      size_t cnt = sensor->config().activeMeasurements ? sensor->config().activeMeasurements : 1;
      if (!displayManager) {
        sendJsonResponse(500,
                         F("{\"success\":false,\"error\":\"Display Manager nicht verfügbar\"}"));
        return;
      }
      for (size_t i = 0; i < cnt; ++i) {
        auto res = displayManager->setSensorMeasurementDisplay(sensorId, i, enabled);
        if (!res.isSuccess()) {
          sendJsonResponse(500,
                           String("{\"success\":false,\"error\":\"") + res.getMessage() + "\"}");
          return;
        }
      }
    }

    // Success
    sendJsonResponse(200, F("{\"success\":true}"));
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
  logger.debug(F("AdminDisplayHandler"), F("Registriere Display-Routen"));

  auto result = router.addRoute(HTTP_GET, "/admin/display", [this]() { handleDisplayConfig(); });
  if (!result.isSuccess())
    return result;

  // Note: Screen duration, clock format, and display toggles are now handled
  // by the unified /admin/config/setConfigValue route with namespace="display"

  // Keep measurement toggle for now as it's managed by DisplayManager
  result = router.addRoute(HTTP_POST, "/admin/display/measurement_toggle",
                           [this]() { handleMeasurementDisplayToggle(); });
  if (!result.isSuccess())
    return result;

  return RouterResult::success();
}

#endif // USE_DISPLAY
