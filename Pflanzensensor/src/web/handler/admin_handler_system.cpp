/**
 * @file admin_handler_system.cpp
 * @brief System control functionality for admin handler
 * @details Handles system-level operations like config updates, resets, and
 * reboots
 */

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/critical_section.h"
#include "web/handler/admin_handler.h"
#if USE_MAIL
#include "mail/mail_helper.h"
#endif

void AdminHandler::handleAdminUpdate() {
  String changes;
  bool updated = processConfigUpdates(changes);

  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};

  if (!updated) {
    renderPage(
        F("Konfiguration"), "admin",
        [this]() {
          sendChunk(F("<div class='container'>"));
          sendChunk(F("<h2>Keine Änderungen vorgenommen</h2>"));
          sendChunk(F("<br><a href='/admin' class='button button-primary'>"));
          sendChunk(F("Zurück zur Administration</a>"));
          sendChunk(F("</div>"));
        },
        css, js);
    return;
  }

  // Save changes
  auto result = ConfigMgr.saveConfig();
  if (!result.isSuccess()) {
    renderPage(
        F("Fehler"), "admin",
        [this, &result]() {
          sendChunk(F("<div class='container'>"));
          sendChunk(F("<h2>Fehler beim Speichern der Konfiguration</h2>"));
          sendChunk(F("<p>"));
          sendChunk(result.getMessage());
          sendChunk(F("</p>"));
          sendChunk(F("<br><a href='/admin' class='button button-primary'>"));
          sendChunk(F("Zurück zur Administration</a>"));
          sendChunk(F("</div>"));
        },
        css, js);
    return;
  }

  // Show success page with changes
  renderPage(
      F("Konfiguration aktualisiert"), "admin",
      [this, changes]() {  // Pass changes by value since it's a String
        sendChunk(F("<div class='container'>"));
        sendChunk(F("<h2>Einstellungen gespeichert</h2>"));
        sendChunk(F("<p>Folgende Änderungen wurden vorgenommen:</p>"));
        sendChunk(F("<ul>"));
        sendChunk(changes);
        sendChunk(F("</ul>"));
        sendChunk(F("<br><a href='/admin' class='button button-primary'>"));
        sendChunk(F("Zurück zur Administration</a>"));
        sendChunk(F("</div>"));
      },
      css, js);
}

void AdminHandler::handleConfigReset() {
  auto result = ConfigMgr.resetToDefaults();
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};
  renderPage(
      F("Konfiguration zurücksetzen"), "admin",
      [this, &result]() {
        sendChunk(F("<div class='container'>"));

        if (result.isSuccess()) {
          sendChunk(F("<h2>Konfiguration zurückgesetzt</h2>"));
          sendChunk(
              F("<p>Die Konfiguration wurde auf Standardwerte "
                "zurückgesetzt.</p>"));
        } else {
          sendChunk(F("<h2>Fehler</h2><p>Fehler beim Zurücksetzen: "));
          sendChunk(result.getMessage());
          sendChunk(F("</p>"));
        }

        sendChunk(F("<br><a href='/admin' class='button button-primary'>"));
        sendChunk(F("Zurück zur Administration</a>"));
        sendChunk(F("</div>"));
      },
      css, js);
}

void AdminHandler::handleReboot() {
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};
  renderPage(
      F("System Neustart"), "admin",
      [this]() {
        sendChunk(F("<div class='container'>"));
        sendChunk(F("<h2>System wird neu gestartet...</h2>"));
        sendChunk(F("</div>"));
      },
      css, js);

  // Verzögerter Neustart
  delay(200);
  logger.warning(F("AdminHandler"), F("Starte ESP neu"));
  ESP.restart();
}

#if USE_MAIL
void AdminHandler::handleTestMail() {
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};

  // Check if mail is enabled
  if (!ConfigMgr.isMailEnabled()) {
    renderPage(
        F("Test-Mail"), "admin",
        [this]() {
          sendChunk(F("<div class='container'>"));
          sendChunk(F("<h2>E-Mail-Funktionen sind deaktiviert</h2>"));
          sendChunk(F("<p>Bitte aktivieren Sie die E-Mail-Funktionen in den Einstellungen.</p>"));
          sendChunk(F("<br><a href='/admin' class='button button-primary'>"));
          sendChunk(F("Zurück zur Administration</a>"));
          sendChunk(F("</div>"));
        },
        css, js);
    return;
  }

  // Try to send test mail
  bool success = false;
  String errorMessage = "";

  try {
    success = MailHelper::sendQuickTestMail().isSuccess();
  } catch (...) {
    errorMessage = F("Unbekannter Fehler beim Senden der Test-Mail");
  }

  // Show result
  renderPage(
      F("Test-Mail"), "admin",
      [this, success, errorMessage]() {
        sendChunk(F("<div class='container'>"));
        if (success) {
          sendChunk(F("<h2>Test-Mail erfolgreich gesendet</h2>"));
          sendChunk(F("<p>Die Test-Mail wurde erfolgreich an "));
          sendChunk(ConfigMgr.getSmtpRecipient());
          sendChunk(F(" gesendet.</p>"));
        } else {
          sendChunk(F("<h2>Fehler beim Senden der Test-Mail</h2>"));
          sendChunk(F("<p>Die Test-Mail konnte nicht gesendet werden.</p>"));
          if (!errorMessage.isEmpty()) {
            sendChunk(F("<p>Fehler: "));
            sendChunk(errorMessage);
            sendChunk(F("</p>"));
          }
          sendChunk(F("<p>Bitte überprüfen Sie Ihre SMTP-Einstellungen.</p>"));
        }
        sendChunk(F("<br><a href='/admin' class='button button-primary'>"));
        sendChunk(F("Zurück zur Administration</a>"));
        sendChunk(F("</div>"));
      },
      css, js);
}
#endif
