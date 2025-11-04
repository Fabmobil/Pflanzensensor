/**
 * @file admin_handler_system.cpp
 * @brief System control functionality for admin handler
 * @details Handles system-level operations like config updates, resets, and
 * reboots
 */

#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_config_persistence.h"
#include "managers/manager_sensor.h"
#include "managers/manager_sensor_persistence.h"
#include "utils/critical_section.h"
#include "web/handler/admin_handler.h"

// NOTE: ArduinoJson and PersistenceUtils includes removed - no longer needed
// Configuration now stored in Preferences (EEPROM), not JSON files

void AdminHandler::handleAdminUpdate() {
  String changes;
  String error;

  // Require AJAX partial updates only (centralized check)
  if (!requireAjaxRequest())
    return;

  bool updated = processConfigUpdates(changes, &error);

  if (!error.isEmpty()) {
    String payload = F("{\"success\":false,\"error\":\"");
    String escaped = error;
    escaped.replace("\"", "\\\"");
    escaped.replace('\n', ' ');
    payload += escaped;
    payload += F("\"}");
    sendJsonResponse(400, payload);
    return;
  }

  if (!updated) {
    sendJsonResponse(200, F("{\"success\":true,\"message\":\"Keine Änderungen\"}"));
    return;
  }

  // Save changes and return JSON
  auto result = ConfigMgr.saveConfig();
  if (!result.isSuccess()) {
    String payload = F("{\"success\":false,\"error\":\"");
    String escaped = result.getMessage();
    escaped.replace("\"", "\\\"");
    escaped.replace('\n', ' ');
    payload += escaped;
    payload += F("\"}");
    sendJsonResponse(500, payload);
    return;
  }

  // Success - include a short change summary
  logger.info(F("AdminHandler"), String(F("Konfiguration gespeichert, Änderungen: ")) + changes);
  String payload = F("{\"success\":true,\"changes\":\"");
  String escaped = changes;
  escaped.replace("\"", "\\\"");
  escaped.replace('\n', ' ');
  payload += escaped;
  payload += F("\"}");
  sendJsonResponse(200, payload);
}

void AdminHandler::handleConfigReset() {
  auto result = ConfigMgr.resetToDefaults();
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};
  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin",
      [this, &result]() {
        sendChunk(F("<div class='card'>"));

        if (result.isSuccess()) {
          sendChunk(F("<h2>✓ Konfiguration zurückgesetzt</h2>"));
          sendChunk(F("<p>Die Konfiguration wurde erfolgreich auf Standardwerte "
                      "zurückgesetzt.</p>"));
        } else {
          sendChunk(F("<h2>❌ Fehler</h2><p class='error-message'>Fehler beim Zurücksetzen: "));
          sendChunk(result.getMessage());
          sendChunk(F("</p>"));
        }

        sendChunk(F("<br><a href='/admin' class='button button-primary'>"));
        sendChunk(F("Zurück zur Administration</a>"));
        sendChunk(F("</div>"));
      },
      css, js);
  // Give the web response time to be sent to the client, then reboot.
  // This avoids the device immediately rebooting before the admin sees the
  // confirmation page. 2000ms is a reasonable short delay.
  delay(2000);
  if (result.isSuccess()) {
    logger.warning(F("AdminHandler"), F("Rebooting after config reset"));
    ESP.restart();
  }
}

void AdminHandler::handleReboot() {
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};
  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin",
      [this]() {
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>🔄 System wird neu gestartet...</h2>"));
        sendChunk(F("<p>Bitte warten Sie einen Moment, bis das Gerät wieder verfügbar ist.</p>"));
        sendChunk(F("</div>"));
      },
      css, js);

  // Verzögerter Neustart
  delay(200);
  logger.warning(F("AdminHandler"), F("Starte ESP neu"));
  ESP.restart();
}

// NOTE: JSON download/upload handlers removed - configuration now stored in Preferences (EEPROM)
// Users can edit configuration through the web interface at /admin and /admin/sensors
// All changes are saved directly to Preferences

// OLD REMOVED: handleDownloadConfig()
// OLD REMOVED: handleDownloadSensors()
// OLD REMOVED: handleUploadConfig()
