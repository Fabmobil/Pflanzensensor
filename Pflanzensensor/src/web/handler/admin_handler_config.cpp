/**
 * @file admin_handler_config.cpp
 * @brief Configuration download/upload handlers
 * @details Provides JSON export/import of Preferences-based configuration
 */

#include "admin_handler.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_config_persistence.h"
#include "managers/manager_resource.h"

void AdminHandler::handleDownloadConfig() {
  logger.info(F("AdminHandler"), F("Config-Download angefordert"));

  // Generate JSON from current Preferences (reuse existing backup function)
  if (!ConfigPersistence::backupPreferencesToFile()) {
    logger.error(F("AdminHandler"), F("Config-Generierung fehlgeschlagen"));
    _server.send(500, "text/plain", "Fehler beim Generieren der Konfiguration");
    return;
  }

  // Read the generated JSON file
  File configFile = LittleFS.open("/prefs_backup.json", "r");
  if (!configFile) {
    logger.error(F("AdminHandler"), F("Config-Datei konnte nicht geöffnet werden"));
    _server.send(500, "text/plain", "Fehler beim Öffnen der Konfigurationsdatei");
    return;
  }

  // Send as downloadable file
  _server.setContentLength(configFile.size());
  _server.sendHeader("Content-Type", "application/json");
  _server.sendHeader("Content-Disposition", "attachment; filename=config.json");
  _server.send(200);

  // Stream file content
  const size_t bufferSize = 1024;
  uint8_t buffer[bufferSize];
  while (configFile.available()) {
    size_t bytesRead = configFile.read(buffer, bufferSize);
    if (bytesRead > 0) {
      _server.sendContent_P((char*)buffer, bytesRead);
    }
  }

  configFile.close();

  // Clean up temporary file
  LittleFS.remove("/prefs_backup.json");

  logger.info(F("AdminHandler"), F("Config erfolgreich heruntergeladen"));
}

void AdminHandler::handleUploadConfig() {
  logger.info(F("AdminHandler"), F("Config-Upload angefordert"));

  // Check if file was uploaded
  HTTPUpload& upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    logger.debug(F("AdminHandler"), F("Upload gestartet: ") + filename);

    // Remove old upload file if exists
    if (LittleFS.exists("/prefs_upload.json")) {
      LittleFS.remove("/prefs_upload.json");
    }

    // Open file for writing
    File uploadFile = LittleFS.open("/prefs_upload.json", "w");
    if (!uploadFile) {
      logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht erstellen"));
      return;
    }
    uploadFile.close();
    logger.debug(F("AdminHandler"), F("Upload-Datei erstellt"));
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write uploaded data to file
    File uploadFile = LittleFS.open("/prefs_upload.json", "a");
    if (uploadFile) {
      size_t written = uploadFile.write(upload.buf, upload.currentSize);
      uploadFile.close();
      logger.debug(F("AdminHandler"), F("Schreibe ") + String(upload.currentSize) +
                                          F(" bytes (geschrieben: ") + String(written) + F(")"));
    } else {
      logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht im Append-Modus öffnen"));
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    logger.debug(F("AdminHandler"),
                 F("Upload abgeschlossen: ") + String(upload.totalSize) + F(" bytes"));

    // Check if file exists
    if (!LittleFS.exists("/prefs_upload.json")) {
      logger.error(F("AdminHandler"), F("Upload-Datei existiert nicht!"));
      _server.send(500, "text/plain", "Upload-Datei wurde nicht gefunden");
      return;
    }

    logger.info(F("AdminHandler"),
                F("Upload erfolgreich, sende JSON-Antwort und starte Verarbeitung..."));

    // Send JSON success response FIRST before doing any heavy processing
    _server.send(200, "application/json",
                 "{\"success\":true,\"message\":\"Konfiguration wird angewendet. Der ESP startet "
                 "neu...\",\"rebootPending\":true}");

    // Give response time to be sent
    delay(200);

    // Now do heavy processing with maximum RAM
    logger.info(F("AdminHandler"), F("Führe Emergency Cleanup für maximalen RAM durch..."));
    // memory stats suppressed to avoid verbose output during uploads

    // Stop webserver and free resources
    ResourceMgr.performEmergencyCleanup();

    logger.logMemoryStats(F("after_emergency_cleanup"));
    // memory stats suppressed to avoid verbose output during uploads

    // Validate and process JSON
    logger.info(F("AdminHandler"), F("Öffne und validiere JSON..."));
    File uploadFile = LittleFS.open("/prefs_upload.json", "r");
    if (!uploadFile) {
      logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht öffnen nach Cleanup"));
      LittleFS.remove("/prefs_upload.json");
      ESP.restart();
      return;
    }

    size_t fileSize = uploadFile.size();
    logger.info(F("AdminHandler"), F("Upload-Datei: ") + String(fileSize) + F(" bytes"));
    // memory stats suppressed before JSON parse

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, uploadFile);
    uploadFile.close();

    // memory stats suppressed after JSON parse

    if (error) {
      logger.error(F("AdminHandler"), F("Ungültige JSON-Datei: ") + String(error.c_str()));
      LittleFS.remove("/prefs_upload.json");
      ESP.restart();
      return;
    }

    logger.info(F("AdminHandler"), F("JSON validiert, stelle Preferences wieder her..."));
    // memory stats suppressed before restore

    // Restore from parsed JSON document
    bool success = ConfigPersistence::restorePreferencesFromJson(doc);

    // memory stats suppressed after restore

    // Clean up
    LittleFS.remove("/prefs_upload.json");

    if (success) {
      logger.info(F("AdminHandler"), F("Config erfolgreich wiederhergestellt, starte neu..."));
    } else {
      logger.error(F("AdminHandler"),
                   F("Config-Wiederherstellung fehlgeschlagen, starte trotzdem neu..."));
    }

    // Reboot to apply settings
    delay(500);
    ESP.restart();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    logger.warning(F("AdminHandler"), F("Upload abgebrochen"));
    LittleFS.remove("/prefs_upload.json");
  }
}
