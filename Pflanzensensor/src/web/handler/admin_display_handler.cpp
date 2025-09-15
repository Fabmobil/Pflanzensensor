/**
 * @file admin_display_handler.cpp
 * @brief Implementation of display configuration handler
 */

#include "web/handler/admin_display_handler.h"

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "web/core/web_auth.h"

// Declare external global sensor manager
extern std::unique_ptr<SensorManager> sensorManager;

#if USE_DISPLAY
extern std::unique_ptr<DisplayManager> displayManager;

AdminDisplayHandler::~AdminDisplayHandler() = default;

AdminDisplayHandler::AdminDisplayHandler(ESP8266WebServer& server)
    : BaseHandler(server) {
  logger.debug(F("AdminDisplayHandler"), F("Initializing AdminDisplayHandler"));
}

void AdminDisplayHandler::handleDisplayConfig() {
  if (!validateRequest()) return;
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};

  renderPage(
      String(ConfigMgr.getDeviceName()) + F(" Display Konfiguration"), "admin",
      [this]() {
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>"));
        sendChunk(ConfigMgr.getDeviceName());
        sendChunk(F(" Displayeinstellungen</h2>"));
        sendChunk(F("</div>"));

        sendChunk(F("<form action='/admin/display' method='post'>"));

        // Screen duration
        sendChunk(F("<div class='form-group'>"));
        sendChunk(F("<label>Anzeigedauer pro Bildschirm (Sekunden):</label>"));
        sendChunk(F("<input type='number' name='screen_duration' value='"));
        sendChunk(String(
            displayManager ? displayManager->getScreenDuration() / 1000 : 5));
        sendChunk(F("' min='1' max='60'>"));
        sendChunk(F("</div>"));

        // Clock format
        sendChunk(F("<div class='form-group'>"));
        sendChunk(F("<label>Uhrzeitformat:</label>"));
        sendChunk(F("<select name='clock_format'>"));
        String currentFormat =
            displayManager ? displayManager->getClockFormat() : "24h";
        sendChunk(F("<option value='24h'"));
        if (currentFormat == "24h") sendChunk(F(" selected"));
        sendChunk(F(">24-Stunden</option>"));
        sendChunk(F("<option value='12h'"));
        if (currentFormat == "12h") sendChunk(F(" selected"));
        sendChunk(F(">12-Stunden (AM/PM)</option>"));
        sendChunk(F("</select></div>"));

        // Show IP screen
        sendChunk(F("<div class='form-group'><label>"));
        sendChunk(F("<input type='checkbox' name='show_ip'"));
        if (displayManager && displayManager->isIpScreenEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> IP-Adresse anzeigen</label></div>"));

        // Show clock
        sendChunk(F("<div class='form-group'><label>"));
        sendChunk(F("<input type='checkbox' name='show_clock'"));
        if (displayManager && displayManager->isClockEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> Datum und Uhrzeit anzeigen</label></div>"));

        // Show flower image
        sendChunk(F("<div class='form-group'><label>"));
        sendChunk(F("<input type='checkbox' name='show_flower'"));
        if (displayManager && displayManager->isFlowerImageEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> Blumen-Bild anzeigen</label></div>"));

        // Show fabmobil image
        sendChunk(F("<div class='form-group'><label>"));
        sendChunk(F("<input type='checkbox' name='show_fabmobil'"));
        if (displayManager && displayManager->isFabmobilImageEnabled()) {
          sendChunk(F(" checked"));
        }
        sendChunk(F("> Fabmobil-Logo anzeigen</label></div>"));

        // Sensor and measurement selection
        sendChunk(F("<h3>Messungen anzeigen</h3>"));
        if (sensorManager) {
          for (const auto& sensor : sensorManager->getSensors()) {
            if (!sensor) continue;
            String id = sensor->getId();
            auto measurementData = sensor->getMeasurementData();

            if (measurementData.isValid() && measurementData.activeValues > 1) {
              // Sensor has multiple measurements - show individual measurements
              sendChunk(F("<div class='form-group'>"));
              sendChunk(F("<strong>"));
              sendChunk(sensor->getName());
              sendChunk(F(":</strong><br>"));

              for (size_t i = 0; i < measurementData.activeValues; i++) {
                // Use user-friendly measurement name
                String measurementName = sensor->getMeasurementName(i);
                if (measurementName.isEmpty()) {
                  measurementName = String(measurementData.fieldNames[i]);
                }
                if (measurementName.isEmpty()) {
                  measurementName = "Messung " + String(i + 1);
                }

                sendChunk(F("<label style='margin-left: 20px;'>"));
                sendChunk(F("<input type='checkbox' name='measurement_"));
                sendChunk(id);
                sendChunk(F("_"));
                sendChunk(String(i));
                sendChunk(F("'"));
                if (sensor->config().measurements[i].enabled) {
                  sendChunk(F(" checked"));
                }
                sendChunk(F("> "));
                sendChunk(measurementName);
                sendChunk(F(" ("));
                sendChunk(String(measurementData.units[i]));
                sendChunk(F(")</label><br>"));
              }
              sendChunk(F("</div>"));
            } else {
              // Sensor has only one measurement - show as simple checkbox
              sendChunk(F("<div class='form-group'><label>"));
              sendChunk(F("<input type='checkbox' name='sensor_"));
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

        sendChunk(F("<button type='submit' class='button button-primary'>"));
        sendChunk(F("Speichern</button>"));
        sendChunk(F("</form>"));
      },
      css, js);
}

void AdminDisplayHandler::handleDisplayUpdate() {
  if (!validateRequest()) return;
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};
  renderPage(
      F("Display Konfiguration"), "admin",
      [this]() {
        if (displayManager) {
          // Update screen duration
          if (_server.hasArg("screen_duration")) {
            displayManager->setScreenDuration(
                _server.arg("screen_duration").toInt() * 1000);
          }

          // Update clock format
          if (_server.hasArg("clock_format")) {
            displayManager->setClockFormat(_server.arg("clock_format"));
          }

          // Update IP screen setting
          displayManager->setIpScreenEnabled(_server.hasArg("show_ip"));

          // Update clock display setting
          displayManager->setClockEnabled(_server.hasArg("show_clock"));

          // Update flower image setting
          displayManager->setFlowerImageEnabled(_server.hasArg("show_flower"));

          // Update fabmobil image setting
          displayManager->setFabmobilImageEnabled(
              _server.hasArg("show_fabmobil"));

          // Process sensor and measurement selections
          if (sensorManager) {
            for (const auto& sensor : sensorManager->getSensors()) {
              if (!sensor) continue;
              String id = sensor->getId();
              auto measurementData = sensor->getMeasurementData();

              if (measurementData.isValid() &&
                  measurementData.activeValues > 1) {
                // Process individual measurements
                for (size_t i = 0; i < measurementData.activeValues; i++) {
                  String measurementArg = "measurement_" + id + "_" + String(i);
                  sensor->mutableConfig().measurements[i].enabled =
                      _server.hasArg(measurementArg);
                }
              } else {
                // Process single measurement sensor
                sensor->setEnabled(_server.hasArg("sensor_" + id));
              }
            }
          }

          // Save settings
          auto result = displayManager->saveConfig();
          if (!result.isSuccess()) {
            sendChunk(
                F("<h2>Fehler</h2><p>Fehler beim Speichern der "
                  "Display-Konfiguration: "));
            sendChunk(result.getMessage());
            sendChunk(F("</p>"));
          } else {
            sendChunk(F("<h2>Konfiguration gespeichert</h2>"));
            sendChunk(
                F("<p>Die Display-Einstellungen wurden erfolgreich "
                  "aktualisiert.</p>"));
          }
        } else {
          sendChunk(F("<h2>Fehler</h2>"));
          sendChunk(F("<p>Display Manager nicht initialisiert</p>"));
        }

        // Link zurück
        sendChunk(
            F("<br><a href='/admin/display' class='button button-primary'>"));
        sendChunk(F("Zurück zu Display-Einstellungen</a>"));
      },
      css, js);
}

bool AdminDisplayHandler::validateRequest() const {
  if (!_server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    _server.requestAuthentication();
    return false;
  }
  return true;
}

RouterResult AdminDisplayHandler::onRegisterRoutes(WebRouter& router) {
  auto result = router.addRoute(HTTP_GET, "/admin/display",
                                [this]() { handleDisplayConfig(); });
  if (!result.isSuccess()) return result;

  result = router.addRoute(HTTP_POST, "/admin/display",
                           [this]() { handleDisplayUpdate(); });
  if (!result.isSuccess()) return result;

  logger.info(F("AdminDisplayHandler"), F("Display config routes registered"));
  return RouterResult::success();
}

#endif  // USE_DISPLAY
