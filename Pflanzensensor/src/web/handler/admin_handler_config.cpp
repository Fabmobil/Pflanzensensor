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

    // Open file for writing
    File uploadFile = LittleFS.open("/prefs_upload.json", "w");
    if (!uploadFile) {
      logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht erstellen"));
      return;
    }
    uploadFile.close();
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write uploaded data to file
    File uploadFile = LittleFS.open("/prefs_upload.json", "a");
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
      uploadFile.close();
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    logger.debug(F("AdminHandler"), F("Upload abgeschlossen: ") + String(upload.totalSize) + F(" bytes"));

    // Validate JSON
    File uploadFile = LittleFS.open("/prefs_upload.json", "r");
    if (!uploadFile) {
      logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht öffnen"));
      _server.send(500, "text/plain", "Fehler beim Öffnen der hochgeladenen Datei");
      return;
    }

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, uploadFile);
    uploadFile.close();

    if (error) {
      logger.error(F("AdminHandler"), F("Ungültige JSON-Datei: ") + String(error.c_str()));
      LittleFS.remove("/prefs_upload.json");
      _server.send(400, "text/plain", "Ungültige JSON-Datei");
      return;
    }

    // Rename to backup file name for restoration
    LittleFS.remove("/prefs_backup.json");
    LittleFS.rename("/prefs_upload.json", "/prefs_backup.json");

    // Restore from file (reuse existing restore function)
    if (!ConfigPersistence::restorePreferencesFromFile()) {
      logger.error(F("AdminHandler"), F("Config-Wiederherstellung fehlgeschlagen"));
      LittleFS.remove("/prefs_backup.json");
      _server.send(500, "text/plain", "Fehler beim Wiederherstellen der Konfiguration");
      return;
    }

    // Clean up
    LittleFS.remove("/prefs_backup.json");

    // Reload configuration from Preferences
    ConfigMgr.loadConfig();

    logger.info(F("AdminHandler"), F("Config erfolgreich hochgeladen und angewendet"));

    // Send success response
    _server.send(200, "text/html", 
      "<html><body><h2>Konfiguration erfolgreich hochgeladen</h2>"
      "<p>Die Einstellungen wurden übernommen.</p>"
      "<p><a href='/admin'>Zurück zur Admin-Seite</a></p>"
      "<script>setTimeout(function(){ window.location.href='/admin'; }, 3000);</script>"
      "</body></html>");
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    logger.warning(F("AdminHandler"), F("Upload abgebrochen"));
    LittleFS.remove("/prefs_upload.json");
  }
}
