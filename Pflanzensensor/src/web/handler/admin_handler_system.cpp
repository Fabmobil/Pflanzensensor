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

namespace {

/**
 * @brief Minimale, in sich geschlossene Antwortseite für Abläufe mit Neustart
 * @details Bewusst OHNE renderAdminPage(): dessen Seite verweist auf
 *          /css/*.css und /js/admin.js, das Gerät startet aber unmittelbar
 *          danach neu. Beide Nachladevorgänge scheitern zuverlässig - die
 *          Seite kam bisher unformatiert und ohne Skript beim Nutzer an.
 *          Diese Fassung braucht keine Unterressourcen.
 *
 *          Gebraucht wird sie nur noch ohne JavaScript; mit JS fängt
 *          admin.js das Absenden ab, bleibt auf /admin und bekommt JSON.
 */
String minimalRebootPage(const String& heading, const String& text) {
  String html;
  html.reserve(320 + heading.length() + text.length());
  html += F("<!DOCTYPE html><html lang='de'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>Pflanzensensor</title></head><body style='font-family:sans-serif;"
            "max-width:40em;margin:3em auto;padding:0 1em'><h2>");
  html += heading;
  html += F("</h2><p>");
  html += text;
  html += F("</p><p><a href='/admin'>Zurück zur Administration</a></p></body></html>");
  return html;
}

} // namespace

void AdminHandler::handleConfigReset() {
  auto result = ConfigMgr.resetToDefaults();

  if (isAjaxRequest()) {
    String json;
    json.reserve(128);
    json += F("{\"success\":");
    json += result.isSuccess() ? F("true") : F("false");
    json += F(",\"message\":\"");
    json += escapeJson(result.isSuccess() ? String(F("Einstellungen zurückgesetzt"))
                                          : result.getMessage());
    json += F("\"}");
    sendJsonResponse(result.isSuccess() ? 200 : 500, json);
  } else if (result.isSuccess()) {
    sendHtmlResponse(200, minimalRebootPage(F("Konfiguration zurückgesetzt"),
                                            F("Das Gerät startet neu. WLAN-Zugangsdaten und "
                                              "Admin-Passwort stehen wieder auf den "
                                              "Werkseinstellungen.")));
  } else {
    sendHtmlResponse(500, minimalRebootPage(F("Fehler beim Zurücksetzen"), result.getMessage()));
  }

  if (!result.isSuccess()) {
    return;
  }

  LOG_WARN(F("AdminHandler"), F("Neustart nach Zurücksetzen der Konfiguration"));
  finishAndRestart();
}

void AdminHandler::handleReboot() {
  if (isAjaxRequest()) {
    sendJsonResponse(200, F("{\"success\":true,\"message\":\"Neustart wird ausgeführt\"}"));
  } else {
    sendHtmlResponse(200, minimalRebootPage(F("System wird neu gestartet"),
                                            F("Das Gerät ist in einigen Sekunden wieder "
                                              "erreichbar.")));
  }

  LOG_WARN(F("AdminHandler"), F("Starte ESP neu"));
  finishAndRestart();
}

void AdminHandler::finishAndRestart() {
  // Dieselbe Reihenfolge wie in WebManager::handleSetUpdate(): Antwort
  // rausdrücken, Verbindung sauber schließen, dann erst neu starten. Vor allem
  // wird handleClient() hier NICHT erneut betreten - ein solcher reentranter
  // Aufruf aus einem Handler heraus versetzt den Ablauf in den SYS-Kontext und
  // hat dort schon einmal "Panic core_esp8266_main.cpp __yield" ausgelöst.
  _server.client().flush();
  _server.client().stop();
  delay(100);
  ESP.restart();
}

// Configuration download/upload: See admin_handler_config.cpp
// - handleDownloadConfig() exports Preferences to JSON
// - handleUploadConfig() imports JSON to Preferences

// OLD REMOVED: handleUploadConfig()
