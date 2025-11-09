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
  // This is the UPLOAD handler - receives file chunks
  // Does NOT send HTTP response - that's done in POST handler
  HTTPUpload& upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    logger.info(F("AdminHandler"), F("Config-Upload gestartet: ") + filename);

    // Remove old upload file and flags if they exist
    LittleFS.remove("/prefs_upload.json");
    LittleFS.remove("/prefs_upload_done.flag");
    LittleFS.remove("/prefs_upload_error.flag");

    // Open file for writing
    File uploadFile = LittleFS.open("/prefs_upload.json", "w");
    if (!uploadFile) {
      logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht erstellen"));
      // Set error flag for POST handler
      File errorFlag = LittleFS.open("/prefs_upload_error.flag", "w");
      if (errorFlag) {
        errorFlag.println("Could not create upload file");
        errorFlag.close();
      }
      return;
    }
    uploadFile.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write uploaded data to file (no logging per chunk to avoid spam)
    File uploadFile = LittleFS.open("/prefs_upload.json", "a");
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
      uploadFile.close();
    } else {
      logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht im Append-Modus öffnen"));
      // Set error flag
      File errorFlag = LittleFS.open("/prefs_upload_error.flag", "w");
      if (errorFlag) {
        errorFlag.println("Could not append to upload file");
        errorFlag.close();
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    logger.info(F("AdminHandler"),
                F("Upload abgeschlossen: ") + String(upload.totalSize) + F(" bytes"));

    // Check if file exists
    if (!LittleFS.exists("/prefs_upload.json")) {
      logger.error(F("AdminHandler"), F("Upload-Datei existiert nicht!"));
      // Set error flag
      File errorFlag = LittleFS.open("/prefs_upload_error.flag", "w");
      if (errorFlag) {
        errorFlag.println("Upload file not found");
        errorFlag.close();
      }
      return;
    }

    // Set success flag for POST handler
    File doneFlag = LittleFS.open("/prefs_upload_done.flag", "w");
    if (doneFlag) {
      doneFlag.println("Upload complete");
      doneFlag.close();
      logger.info(F("AdminHandler"), F("Upload-Flag gesetzt"));
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    logger.warning(F("AdminHandler"), F("Upload abgebrochen"));
    LittleFS.remove("/prefs_upload.json");
    LittleFS.remove("/prefs_upload_done.flag");
    LittleFS.remove("/prefs_upload_error.flag");
  }
}

void AdminHandler::handleUploadConfigRestore() {
  // This is called from POST handler AFTER upload completes
  // HTTP response has already been sent
  logger.info(F("AdminHandler"), F("Verarbeite hochgeladene Konfiguration..."));

  // Remove the done flag
  LittleFS.remove("/prefs_upload_done.flag");

  File uploadFile = LittleFS.open("/prefs_upload.json", "r");
  if (!uploadFile) {
    logger.error(F("AdminHandler"), F("Konnte Upload-Datei nicht öffnen"));
    LittleFS.remove("/prefs_upload.json");
    delay(1000);
    ESP.restart();
    return;
  }

  size_t fileSize = uploadFile.size();
  logger.info(F("AdminHandler"), F("Verarbeite Datei: ") + String(fileSize) + F(" bytes"));

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, uploadFile);
  uploadFile.close();

  if (error) {
    logger.error(F("AdminHandler"), F("Ungültige JSON-Datei: ") + String(error.c_str()));
    LittleFS.remove("/prefs_upload.json");
    delay(1000);
    ESP.restart();
    return;
  }

  logger.info(F("AdminHandler"), F("Stelle Preferences wieder her..."));

  // Restore from parsed JSON document (this takes several seconds!)
  bool success = ConfigPersistence::restorePreferencesFromJson(doc);

  // Clean up
  LittleFS.remove("/prefs_upload.json");

  if (success) {
    logger.info(F("AdminHandler"), F("Wiederherstellung erfolgreich, starte neu..."));
  } else {
    logger.error(F("AdminHandler"), F("Wiederherstellung fehlgeschlagen, starte trotzdem neu..."));
  }

  // Reboot to apply settings
  delay(500);
  ESP.restart();
}
