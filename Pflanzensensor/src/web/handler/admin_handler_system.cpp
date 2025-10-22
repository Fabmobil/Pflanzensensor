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
#include "managers/manager_config_persistence.h"
#include "managers/manager_sensor_persistence.h"
#include "utils/persistence_utils.h"

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

// Download config.json
void AdminHandler::handleDownloadConfig() {
  const char* PATH = "/config.json";
  String clientIp = _server.client().remoteIP().toString();
  logger.info(F("AdminHandler"), F("Download-Anfrage für settings.json von ") + clientIp);
  if (!LittleFS.exists(PATH)) {
    logger.warning(F("AdminHandler"), F("Konfigurationsdatei nicht gefunden"));
    this->sendError(404, F("Konfigurationsdatei nicht gefunden"));
    return;
  }
  File f = LittleFS.open(PATH, "r");
  if (!f) {
    logger.error(F("AdminHandler"), F("Öffnen der Konfigurationsdatei fehlgeschlagen"));
    this->sendError(500, F("Öffnen der Konfigurationsdatei fehlgeschlagen"));
    return;
  }
  size_t size = f.size();
  logger.info(F("AdminHandler"),
              F("Sende settings.json, Größe: ") + String(size) + F(" Bytes an ") + clientIp);
  _server.sendHeader(F("Content-Type"), F("application/json"));
  _server.sendHeader(F("Content-Disposition"), F("attachment; filename=settings.json"));
  _server.sendHeader(F("Connection"), F("close"));
  _server.sendHeader(F("Content-Length"), String(size));
  _server.setContentLength(size);
  _server.send(200, F("application/json"), "");
  const size_t CHUNK = 1024;
  uint8_t buf[CHUNK];
  size_t rem = size;
  while (rem > 0) {
    size_t toRead = min(rem, CHUNK);
    size_t read = f.read(buf, toRead);
    if (read == 0) {
      logger.warning(F("AdminHandler"), F("Lesen lieferte 0 Bytes beim Senden von settings.json"));
      break;
    }
    _server.sendContent((char*)buf, read);
    rem -= read;
    yield();
  }
  f.close();
  logger.info(F("AdminHandler"), F("Download von settings.json abgeschlossen für ") + clientIp +
                                     F(" (") + String(size - rem) + F(" Bytes gesendet)"));
}

// Download sensors.json
void AdminHandler::handleDownloadSensors() {
  const char* PATH = "/sensors.json";
  String clientIp = _server.client().remoteIP().toString();
  logger.info(F("AdminHandler"), F("Download-Anfrage für sensors.json von ") + clientIp);
  if (!LittleFS.exists(PATH)) {
    logger.warning(F("AdminHandler"), F("Sensorkonfigurationsdatei nicht gefunden"));
    this->sendError(404, F("Sensorkonfigurationsdatei nicht gefunden"));
    return;
  }
  File f = LittleFS.open(PATH, "r");
  if (!f) {
    logger.error(F("AdminHandler"), F("Öffnen der Sensorkonfigurationsdatei fehlgeschlagen"));
    this->sendError(500, F("Öffnen der Sensorkonfigurationsdatei fehlgeschlagen"));
    return;
  }
  size_t size = f.size();
  logger.info(F("AdminHandler"),
              F("Sende sensors.json, Größe: ") + String(size) + F(" Bytes an ") + clientIp);
  _server.sendHeader(F("Content-Type"), F("application/json"));
  _server.sendHeader(F("Content-Disposition"), F("attachment; filename=sensors.json"));
  _server.sendHeader(F("Connection"), F("close"));
  _server.sendHeader(F("Content-Length"), String(size));
  _server.setContentLength(size);
  _server.send(200, F("application/json"), "");
  const size_t CHUNK = 1024;
  uint8_t buf[CHUNK];
  size_t rem = size;
  while (rem > 0) {
    size_t toRead = min(rem, CHUNK);
    size_t read = f.read(buf, toRead);
    if (read == 0) {
      logger.warning(F("AdminHandler"), F("Lesen lieferte 0 Bytes beim Senden von sensors.json"));
      break;
    }
    _server.sendContent((char*)buf, read);
    rem -= read;
    yield();
  }
  f.close();
  logger.info(F("AdminHandler"), F("Download von sensors.json abgeschlossen für ") + clientIp +
                                     F(" (") + String(size - rem) + F(" Bytes gesendet)"));
}

// note: upload handling uses _server.upload() inside the member handlers

// Handle config upload
void AdminHandler::handleUploadConfig() {
  // Expect a multipart upload; using _server.upload() to get data
  HTTPUpload& upload = _server.upload();
  static size_t upload_written = 0;
  String clientIp = _server.client().remoteIP().toString();
  if (upload.status == UPLOAD_FILE_START) {
    // remove any previous temp
    LittleFS.remove("/config.json.tmp");
    upload_written = 0;
    logger.info(F("AdminHandler"), F("Upload gestartet von ") + clientIp + F(" Dateiname: ") +
                                       upload.filename + F(" Gesamtgröße (falls bekannt): ") +
                                       String(upload.totalSize));
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    File tmp = LittleFS.open("/config.json.tmp", "a");
    if (tmp && upload.currentSize > 0) {
      tmp.write(upload.buf, upload.currentSize);
      upload_written += upload.currentSize;
      tmp.close();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    logger.info(F("AdminHandler"), F("Upload beendet von ") + clientIp +
                                       F(" Gesamtbytes geschrieben: ") + String(upload_written));
    // Validate JSON
    StaticJsonDocument<2048> doc;
    String err;
    if (!PersistenceUtils::readJsonFile("/config.json.tmp", doc, err)) {
      // remove tmp and return error
      logger.error(F("AdminHandler"),
                   String(F("Ungültiges JSON hochgeladen von ")) + clientIp + F(": ") + err);
      LittleFS.remove("/config.json.tmp");
      File rf = LittleFS.open("/upload_result.json", "w");
      if (rf) {
        rf.print(String("{\"success\":false,\"error\":\"") + err + "}");
        rf.close();
      }
      upload_written = 0;
      return;
    }

    // Heuristic: detect whether this is a config.json or sensors.json
    bool looksLikeConfig = false;
    bool looksLikeSensors = false;

    JsonObject root = doc.as<JsonObject>();
    // Check for common config keys
    if (root.containsKey("device_name") || root.containsKey("admin_password") ||
        root.containsKey("wifi_ssid_1")) {
      looksLikeConfig = true;
    }

    // Check for sensors structure: values are objects with 'measurements' or 'measurementInterval'
    if (!looksLikeConfig) {
      for (JsonPair kv : root) {
        if (kv.value().is<JsonObject>()) {
          JsonObject v = kv.value().as<JsonObject>();
          if (v.containsKey("measurements") || v.containsKey("measurementInterval")) {
            looksLikeSensors = true;
            break;
          }
        }
      }
    }

    String payload;
    if (looksLikeConfig) {
      logger.info(F("AdminHandler"),
                  F("Hochgeladenes JSON wurde als config.json erkannt von ") + clientIp);
      // Replace config.json
      if (LittleFS.exists("/config.json"))
        LittleFS.remove("/config.json");
      if (!LittleFS.rename("/config.json.tmp", "/config.json")) {
        logger.error(F("AdminHandler"),
                     F("Umbenennen der temporären Datei nach /config.json fehlgeschlagen"));
        payload = F(
            "{\"success\":false,\"error\":\"Umbenennen der Konfigurationsdatei fehlgeschlagen\"}");
      } else {
        auto loadResult = ConfigMgr.loadConfig();
        if (loadResult.isSuccess()) {
          logger.info(F("AdminHandler"), F("Konfiguration erfolgreich importiert von ") + clientIp +
                                             F(" (") + String(upload_written) + F(" Bytes)"));
          payload = F("{\"success\":true,\"message\":\"Konfiguration importiert\"}");
        } else {
          String escaped = loadResult.getMessage();
          escaped.replace("\"", "\\\"");
          logger.error(F("AdminHandler"),
                       String(F("Fehler beim Laden der Konfiguration nach Import von ")) +
                           clientIp + F(": ") + escaped);
          payload = String("{\"success\":false,\"error\":\"") + escaped + "}";
        }
      }
    } else if (looksLikeSensors) {
      logger.info(F("AdminHandler"),
                  F("Hochgeladenes JSON wurde als sensors.json erkannt von ") + clientIp);
      // Replace sensors.json
      if (LittleFS.exists("/sensors.json"))
        LittleFS.remove("/sensors.json");
      if (!LittleFS.rename("/config.json.tmp", "/sensors.json")) {
        logger.error(F("AdminHandler"),
                     F("Umbenennen der temporären Datei nach /sensors.json fehlgeschlagen"));
        payload = F("{\"success\":false,\"error\":\"Umbenennen der Sensorkonfigurationsdatei "
                    "fehlgeschlagen\"}");
      } else {
        auto reloadResult = SensorPersistence::loadFromFile();
        if (reloadResult.isSuccess()) {
          logger.info(F("AdminHandler"), F("Sensorkonfiguration erfolgreich importiert von ") +
                                             clientIp + F(" (") + String(upload_written) +
                                             F(" Bytes)"));
          payload = F("{\"success\":true,\"message\":\"Sensorkonfiguration importiert\"}");
        } else {
          String escaped = reloadResult.getMessage();
          escaped.replace("\"", "\\\"");
          logger.error(F("AdminHandler"),
                       String(F("Fehler beim Neuladen der Sensorkonfiguration nach Import von ")) +
                           clientIp + F(": ") + escaped);
          payload = String("{\"success\":false,\"error\":\"") + escaped + "}";
        }
      }
    } else {
      // Unknown file structure
      logger.warning(F("AdminHandler"),
                     F("Hochgeladenes JSON hat unbekannte Struktur von ") + clientIp);
      LittleFS.remove("/config.json.tmp");
      payload = F("{\"success\":false,\"error\":\"Unbekanntes JSON-Format\"}");
    }

    // Write small JSON result to /upload_result.json
    File rf = LittleFS.open("/upload_result.json", "w");
    if (rf) {
      rf.print(payload);
      rf.close();
    }
    upload_written = 0;
  }
}

#if USE_MAIL
void AdminHandler::handleTestMail() {
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};

  // Check if mail is enabled
  if (!ConfigMgr.isMailEnabled()) {
    renderAdminPage(
        ConfigMgr.getDeviceName(), "admin",
        [this]() {
          sendChunk(F("<div class='card'>"));
          sendChunk(F("<h2>⚠️ E-Mail-Funktionen deaktiviert</h2>"));
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
  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin",
      [this, success, errorMessage]() {
        sendChunk(F("<div class='card'>"));
        if (success) {
          sendChunk(F("<h2>✓ Test-Mail erfolgreich gesendet</h2>"));
          sendChunk(F("<p>Die Test-Mail wurde erfolgreich an <strong>"));
          sendChunk(ConfigMgr.getSmtpRecipient());
          sendChunk(F("</strong> gesendet.</p>"));
        } else {
          sendChunk(F("<h2>❌ Fehler beim Senden</h2>"));
          sendChunk(F("<p>Die Test-Mail konnte nicht gesendet werden.</p>"));
          if (!errorMessage.isEmpty()) {
            sendChunk(F("<p class='error-message'>Fehler: "));
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
