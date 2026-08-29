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

// Configuration storage: Preferences library (flash-based key-value store)
// All settings stored in Preferences namespaces, not JSON files

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
                      "zurückgesetzt. Der Neustart kann bis zu 1 Minute dauern.</p>"));
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
  // Kurze Pause damit Admin die Bestätigung sieht, dann Neustart.
  // 500ms sollte reichen für Rendern der Seite.
  delay(500);
  if (result.isSuccess()) {
    LOG_WARN(F("AdminHandler"), F("Neustart nach Zurücksetzen der Konfiguration"));
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
  LOG_WARN(F("AdminHandler"), F("Starte ESP neu"));
  ESP.restart();
}

// Configuration download/upload: See admin_handler_config.cpp
// - handleDownloadConfig() exports Preferences to JSON
// - handleUploadConfig() imports JSON to Preferences

// OLD REMOVED: handleUploadConfig()
