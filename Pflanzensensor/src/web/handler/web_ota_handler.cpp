/**
 * @file web_ota_handler.cpp
 * @brief Implementation of web-based OTA update handler
 */

#include "web_ota_handler.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#ifdef ESP32
#include <Update.h>
#include <mbedtls/md5.h>
#else
#include <MD5Builder.h>
#endif

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_config_persistence.h"
#include "managers/manager_config_preferences.h"
#if USE_DISPLAY
#include "managers/manager_display.h"
#endif
#include "utils/critical_section.h"
// Flash persistence used to check for existing backup and restore after FS update
#include "../../utils/flash_persistence.h"

extern std::unique_ptr<SensorManager> sensorManager;
#if USE_DISPLAY
extern std::unique_ptr<DisplayManager> displayManager;
#endif

WebOTAHandler::WebOTAHandler(ESPWebServer& server, WebAuth& auth)
    : BaseHandler(server), _auth(auth) {}

void WebOTAHandler::handleStatus() {
  StaticJsonDocument<256> doc;

  doc["uptime"] = millis() / 1000;
  doc["isFileSystemUpdatePending"] = ConfigMgr.isFileSystemUpdatePending();
  doc["isFirmwareUpdatePending"] = ConfigMgr.isFirmwareUpdatePending();
  doc["inUpdateMode"] = ConfigMgr.getDoFirmwareUpgrade();
  doc["version"] = VERSION;

  // Zusätzliche Validierung
  if (ConfigMgr.isFileSystemUpdatePending() && ConfigMgr.isFirmwareUpdatePending()) {
    LOG_ERROR(F("WebOTAHandler"), F("Ungültiger Zustand: Beide Update-Flags sind gesetzt"));
    ConfigMgr.setUpdateFlags(false, false); // Reset flags
  }

  String response;
  serializeJson(doc, response);
  LOG_DEBUG(F("WebOTAHandler"), String(F("Status-Antwort: ")) + response);
  sendJsonResponse(200, response);
}

RouterResult WebOTAHandler::onRegisterRoutes(WebRouter& router) {
  LOG_DEBUG(F("WebOTAHandler"), F("Registriere OTA-Routen"));

  // Register status endpoint
  auto result = router.addRoute(HTTP_GET, "/status", [this]() { handleStatus(); });
  if (!result.isSuccess())
    return result;

  // Register update page
  result = router.addRoute(HTTP_GET, "/admin/update", [this]() { handleUpdatePage(); });
  if (!result.isSuccess())
    return result;

  // Register update handler
  // ACHTUNG: _server.on() umgeht die Router-Middleware, daher muss die
  // Authentifizierung hier in beiden Callbacks selbst geprüft werden.
  _server.on(
      "/update", HTTP_POST,
      [this]() {
        if (!_uploadAuthorized) {
          _server.requestAuthentication();
          return;
        }
        // Send response immediately after upload completes
        // Don't wait here as the response was already sent in UPLOAD_FILE_END
        // Just ensure clean exit from this handler
        if (!Update.hasError()) {
          // Success response was already sent during UPLOAD_FILE_END
          // This callback is called after upload completes
          yield();
        } else {
          sendJsonResponse(500, F("{\"success\":false,\"error\":\"Update failed\"}"));
        }
      },
      [this]() { handleUpdateUpload(); });

  return RouterResult::success();
}

HandlerResult WebOTAHandler::handleGet(const String& uri, const std::map<String, String>& query) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Use registerRoutes instead");
}

HandlerResult WebOTAHandler::handlePost(const String& uri, const std::map<String, String>& params) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Use registerRoutes instead");
}

void WebOTAHandler::handleUpdatePage() {
  // Diese beiden Vektoren standen vorher auf Dateiebene - ohne static, ohne
  // anonymen Namespace. Das waren zwei dauerhafte Heap-Allokationen bei der
  // statischen Initialisierung, und die Symbole "css" und "js" hatten externe
  // Bindung: jede weitere Definition gleichen Namens in einer anderen
  // Übersetzungseinheit wäre ein ODR-Verstoß gewesen.
  const std::vector<String> css = {"admin"};
  const std::vector<String> js = {"ota"};

  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin/update",
      [this]() {
        // System Information Card
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>Systeminformationen</h2>"));
        sendChunk(F("<table class='info-table'>"));

        // System info table content
        sendChunk(F("<tr><td>Version:</td><td>"));
        sendChunk(VERSION);
        sendChunk(F("</td></tr>"));

        sendChunk(F("<tr><td>Build Datum:</td><td>"));
        sendChunk(__DATE__);
        sendChunk(F("</td></tr></table>"));
        sendChunk(F("</div>"));

        // Update section Card
        sendChunk(F("<div class='card update-section'>"));

        // Warning box
        sendChunk(F("<div class='warning-box'>"));
        sendChunk(F("<h3>⚠️ Wichtige Hinweise ⚠️</h3><ul>"));
        sendChunk(F("<li>Die aktuelle Firmware für den Gerät kannst du auf der "));
        sendChunk(
            F("<a href='https://github.com/Fabmobil/Pflanzensensor/releases' target='_blank'>"));
        sendChunk(F("Pflanzensensor Github Seite</a> herunterladen.</li>"));
        sendChunk(F("<li>Beim aktualisieren bleiben deine "));
        sendChunk(F("Einstellungen normalerweise erhalten.</li> "));
        sendChunk(
            F("<ul><li>Eine Sicherheitskopie deiner Einstellungen kann aber nicht schaden:<br>"));
        sendChunk(F("<form action='/admin/downloadConfig' method='GET' class='inline'>"));
        sendChunk(F("<button type='submit' class='button button-primary'>Konfiguration "
                    "herunterladen</button>"));
        sendChunk(F("</form>"));
        sendChunk(F("</li></ul>"));
        sendChunk(
            F("<li>Die Reihenfolge ist wichtig: bei einem Update muss du immer <b>zu erst</b> "));
        sendChunk(F("die Firmware (<b>firmware.bin</b>) und <b>danach</b> das Dateisystem "
                    "(<b>littlefs.bin</b>) "
                    "aktualisieren.</li>"));
        sendChunk(F("<li>Das Gerät wird nach erfolgreichem Update automatisch neu gestartet</li>"));
        sendChunk(F("<li>Trenne während des Updates nicht die Stromversorgung!</li>"));
        sendChunk(F("</ul></div>"));

        // Upload form
        sendChunk(F("<form id='update-form' method='POST' class='config-form' "
                    "action='/update' enctype='multipart/form-data'>"));

        // File input
        sendChunk(F("<div class='form-group'><label>Firmware Datei (firmware.bin) oder Dateisystem "
                    "Datei (littlefs.bin):</label>"));
        sendChunk(F("<input type='file' id='update-file' name='firmware' "
                    "accept='.bin' required>"));
        sendChunk(F("</div>"));

        // MD5 input (nur wenn MD5 Verifikation aktiviert ist)
        if (ConfigMgr.isMD5Verification()) {
          sendChunk(F("<div class='form-group'><label>MD5 Prüfsumme:</label>"));
          sendChunk(F("<input type='text' id='md5-input' name='md5' required>"));
          sendChunk(F("</div>"));
        }

        // Progress and status containers
        sendChunk(F("<div id='progress-container' class='progress-container'>"));
        sendChunk(F("<div id='progress' class='progress'></div>"));
        sendChunk(F("</div>"));
        sendChunk(F("<div id='status' class='status'></div>"));

        // Submit button
        sendChunk(F("<button type='submit' id='update-button' class='button "
                    "button-primary'>"));
        sendChunk(F("Update starten</button>"));

        sendChunk(F("</form></div>"));
      },
      css, js);
}

TypedResult<ResourceError, void> WebOTAHandler::beginUpdate(size_t size, const String& md5,
                                                            bool isFilesystem) {
  // If we're in minimal mode but flags are not set, allow the update
  if (ConfigMgr.getDoFirmwareUpgrade()) {
    // Skip flag checks in minimal mode
    return TypedResult<ResourceError, void>::success();
  }

  // Normal mode checks
  if (!ConfigMgr.getDoFirmwareUpgrade()) {
    if (!isFilesystem && !ConfigMgr.isFirmwareUpdatePending()) {
      LOG_ERROR(F("WebOTAHandler"), F("Kein Firmware-Update ausstehend"));
      return TypedResult<ResourceError, void>::fail(ResourceError::INVALID_STATE,
                                                    F("Kein Firmware-Update ausstehend"));
    }

    if (isFilesystem && !ConfigMgr.isFileSystemUpdatePending()) {
      LOG_ERROR(F("WebOTAHandler"), F("Kein Dateisystem-Update ausstehend"));
      return TypedResult<ResourceError, void>::fail(ResourceError::INVALID_STATE,
                                                    F("Kein Dateisystem-Update ausstehend"));
    }
  }

  return TypedResult<ResourceError, void>::success();
}

TypedResult<ResourceError, void> WebOTAHandler::writeData(uint8_t* data, size_t len) {
  if (!_status.inProgress) {
    return TypedResult<ResourceError, void>::fail(ResourceError::INVALID_STATE,
                                                  F("No update in progress"));
  }

  if (Update.write(data, len) != len) {
    String error = F("Write failed: ");
    error += Update.getError();
    return TypedResult<ResourceError, void>::fail(ResourceError::OPERATION_FAILED, error);
  }

  _status.currentProgress += len;
  return TypedResult<ResourceError, void>::success();
}

TypedResult<ResourceError, void> WebOTAHandler::endUpdate(bool reboot) {
  if (!_status.inProgress) {
    return TypedResult<ResourceError, void>::success();
  }

  if (!Update.end(true)) {
    String error = F("Update fehlgeschlagen: ");
    error += Update.getError();
    return TypedResult<ResourceError, void>::fail(ResourceError::OPERATION_FAILED, error);
  }

  _status.inProgress = false;

  if (reboot) {
    LOG_INFO(F("WebOTAHandler"), F("Update erfolgreich, Neustart..."));
    delay(500);
    ESP.restart();
  }

  return TypedResult<ResourceError, void>::success();
}

void WebOTAHandler::abortUpdate() {
  if (_status.inProgress) {
    Update.end();
    _status = OTAStatus();
    LOG_WARN(F("WebOTAHandler"), F("Update abgebrochen"));
  }
}

OTAStatus WebOTAHandler::getStatus() const { return _status; }

bool WebOTAHandler::requireUploadAuth() {
  if (_server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    return true;
  }
  LOG_WARN(F("WebOTAHandler"), F("Firmware-Upload ohne gültige Anmeldung abgewiesen"));
  return false;
}

void WebOTAHandler::handleUpdateUpload() {
  HTTPUpload& upload = _server.upload();
  static bool isFilesystem = false;
  static bool errorReported = false;
  static unsigned long lastProgressTime = 0;
  static uint8_t lastProgressUpdate = 0;

  // Display integration for update progress visualization

  // Auth einmal zu Beginn des Uploads prüfen und Ergebnis für alle weiteren
  // Chunks merken. Ohne gültige Anmeldung wird kein Byte in den Flash geschrieben.
  if (upload.status == UPLOAD_FILE_START) {
    _uploadAuthorized = requireUploadAuth();
  }
  if (!_uploadAuthorized) {
    return;
  }

  switch (upload.status) {
  case UPLOAD_FILE_START: {
    String uploadName = upload.filename;
    isFilesystem = _server.hasArg("mode") && _server.arg("mode") == "fs";
#ifdef ESP32
    size_t contentLength = upload.totalSize;
#else
    size_t contentLength = upload.contentLength;
#endif
    errorReported = false;

    LOG_INFO(F("WebOTAHandler"),
             String(F("Upload gestartet: ")) + uploadName + String(F(" (Typ: ")) +
                 String(isFilesystem ? F("Dateisystem") : F("Firmware")) + F(")"));
    LOG_DEBUG(F("WebOTAHandler"), String(F("Inhaltlänge: ")) + String(contentLength) + F(" Bytes"));

    // FLASH-BASED PERSISTENCE: Config backup was already created BEFORE reboot
    // (in ConfigManager::setUpdateFlags when the update flag was set)
    // The backup will be restored automatically on next boot if FS update is detected
    // DO NOT backup here - it would interfere with the ongoing HTTP upload and cause crashes!
    if (isFilesystem) {
      if (FlashPersistence::hasValidConfig()) {
        LOG_INFO(F("WebOTAHandler"),
                 F("Flash-Backup vorhanden - wird nach Neustart wiederhergestellt"));
      } else {
        LOG_WARN(F("WebOTAHandler"),
                 F("Kein Flash-Backup gefunden - Konfiguration könnte verloren gehen"));
      }
    }

    size_t freeSpace;
    if (isFilesystem) {
      {
#ifdef ESP32
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        LOG_DEBUG(F("WebOTAHandler"),
                  String(F("Dateisystem gesamt: ")) + String(totalBytes) + String(F(" Bytes")));
        LOG_DEBUG(F("WebOTAHandler"),
                  String(F("Dateisystem belegt: ")) + String(usedBytes) + String(F(" Bytes")));
        freeSpace = totalBytes;

        if (contentLength > totalBytes) {
          LOG_DEBUG(F("WebOTAHandler"), F("Inhaltslänge an Dateisystemgröße angepasst"));
          contentLength = totalBytes;
        }
#else
        FSInfo fs_info;
        if (LittleFS.info(fs_info)) {
          LOG_DEBUG(F("WebOTAHandler"), String(F("Dateisystem gesamt: ")) +
                                            String(fs_info.totalBytes) + String(F(" Bytes")));
          LOG_DEBUG(F("WebOTAHandler"), String(F("Dateisystem belegt: ")) +
                                            String(fs_info.usedBytes) + String(F(" Bytes")));
          freeSpace = fs_info.totalBytes;

          if (contentLength > fs_info.totalBytes) {
            LOG_DEBUG(F("WebOTAHandler"), F("Inhaltslänge an Dateisystemgröße angepasst"));
            contentLength = fs_info.totalBytes;
          }
        } else {
          LOG_ERROR(F("WebOTAHandler"), F("Fehler beim Lesen der Dateisysteminformationen"));
          _status.lastError = F("Fehler beim Lesen der Dateisysteminformationen");
          return;
        }
#endif
      }
    } else {
      freeSpace = ESP.getFreeSketchSpace();
      LOG_DEBUG(F("WebOTAHandler"),
                String(F("Freier Sketch-Speicher: ")) + String(freeSpace) + String(F(" Bytes")));
    }

    LOG_DEBUG(F("WebOTAHandler"),
              String(F("Update-Modus: ")) +
                  String(ConfigMgr.getDoFirmwareUpgrade() ? F("minimal") : F("normal")));
    LOG_DEBUG(F("WebOTAHandler"),
              String(F("Endgültige Inhaltslänge: ")) + String(contentLength) + F(" Bytes"));

    if (contentLength > freeSpace) {
      String error = String(F("Nicht genug Speicherplatz - benötigt: ")) + String(contentLength) +
                     String(F(", verfügbar: ")) + String(freeSpace);
      LOG_ERROR(F("WebOTAHandler"), error);
      _status.lastError = error;
      return;
    }

#ifdef ESP32
    uint8_t command = isFilesystem ? U_SPIFFS : U_FLASH;
#else
    uint8_t command = isFilesystem ? U_FS : U_FLASH;
#endif
    LOG_DEBUG(F("WebOTAHandler"), String(F("Update-Befehl: ")) + String(command) +
                                      String(F(", Inhaltslänge: ")) + String(contentLength) +
                                      String(F(", verfügbarer Speicher: ")) + String(freeSpace));

    // Note: Preferences backup/restore happens BEFORE Update.begin()
    // The backup file was created before first reboot and already restored above
    // After filesystem update, Preferences will be intact from the restore

    if (!Update.begin(contentLength, command)) {
      String error = String(F("Start des Updates fehlgeschlagen: ")) + String(Update.getError());
      LOG_ERROR(F("WebOTAHandler"), error);
      LOG_ERROR(F("WebOTAHandler"),
                String(F("Verfügbarer Speicher: ")) + String(freeSpace) + F(" Bytes"));
      LOG_ERROR(F("WebOTAHandler"), String(F("Benötigt: ")) + String(contentLength) + F(" Bytes"));
      _status.lastError = error;
      return;
    }

    if (_server.hasArg("md5")) {
      Update.setMD5(_server.arg("md5").c_str());
      LOG_DEBUG(F("WebOTAHandler"), String(F("MD5 gesetzt: ")) + _server.arg("md5"));
    }

    _status.inProgress = true;
    _status.currentProgress = 0;
    _status.totalSize = contentLength;
    lastProgressTime = millis();
    lastProgressUpdate = 0;

    LOG_INFO(F("WebOTAHandler"),
             String(F("Update gestartet - Größe: ")) + String(contentLength) + F(" Bytes"));

#if USE_DISPLAY
    // Show update start on display
    if (displayManager) {
      String updateType = isFilesystem ? F("Filesystem") : F("Firmware");
      if (displayManager) {
        displayManager->showLogScreen(updateType + String(F(" update starting...")), false);
      }
    }
#endif
    break;
  }

  case UPLOAD_FILE_WRITE: {
    if (!_status.inProgress)
      return;

    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      if (!errorReported) {
        String error =
            String(F("Update-Schreibvorgang fehlgeschlagen: ")) + String(Update.getError());
        LOG_ERROR(F("WebOTAHandler"), error);
        _status.lastError = error;
        errorReported = true;
      }
      return;
    }

    // Yield to prevent watchdog timeout and keep HTTP connection alive
    yield();

    _status.currentProgress = Update.progress();
    uint8_t progress = (_status.currentProgress * 100) / _status.totalSize;

    if (progress != lastProgressUpdate &&
        (progress % 25 == 0 || millis() - lastProgressTime >= 5000)) {
      LOG_INFO(F("WebOTAHandler"), String(F("Update-Fortschritt: ")) + String(progress) + F("%"));

#if USE_DISPLAY
      // Show progress on display
      if (displayManager) {
        if (displayManager) {
          displayManager->updateLogStatus(
              String(F("Progress: ")) + String(progress) + String(F("%")), false);
        }
      }
#endif

      lastProgressUpdate = progress;
      lastProgressTime = millis();
    }
    break;
  }

  case UPLOAD_FILE_END: {
    if (!_status.inProgress)
      return;

    if (Update.end(true)) {
      LOG_INFO(F("WebOTAHandler"),
               String(F("Update erfolgreich: ")) + String(upload.totalSize) + F(" Bytes"));

#if USE_DISPLAY
      // Show success on display
      if (displayManager) {
        if (displayManager) {
          displayManager->updateLogStatus(F("Update erfolgreich durchgeführt!"), false);
        }
        delay(500); // Show success message briefly
        if (displayManager) {
          displayManager->endUpdateMode();
        }
      }
#endif

      LOG_INFO(F("WebOTAHandler"), F("Filesystem-Update erfolgreich"));

      // Send success response to deploy script IMMEDIATELY after update succeeds
      // For FS updates with restore pending, indicate this in the response
      StaticJsonDocument<200> response;
      response["success"] = true;
      response["needsReboot"] = true;

      if (isFilesystem && FlashPersistence::hasValidConfig()) {
        response["restorePending"] = true;
        response["message"] =
            "Filesystem aktualisiert. Einstellungen werden nach Neustart wiederhergestellt.";
      }

      String jsonStr;
      serializeJson(response, jsonStr);
      sendJsonResponse(200, jsonStr);

      // Give the response time to be sent
      delay(200);

      // **CRITICAL: Set flag for flash restore after reboot for FS updates**
      // Don't restore here - HTTP context is still active and heap too fragmented
      // Restore will happen cleanly on next boot with full heap available
      if (isFilesystem && FlashPersistence::hasValidConfig()) {
        LOG_INFO(F("WebOTAHandler"),
                 F("FS-Update abgeschlossen - setze Restore-Flag für nächsten Boot..."));
        File flagFile = LittleFS.open("/.restore_from_flash", "w");
        if (flagFile) {
          flagFile.println("1");
          flagFile.close();
          LOG_INFO(F("WebOTAHandler"), F("Restore-Flag erfolgreich gesetzt"));
        } else {
          LOG_WARN(F("WebOTAHandler"), F("Konnte Restore-Flag nicht setzen"));
        }
      }

      // Now try to clear update flags (this might crash, but response is
      // already sent)
      LOG_INFO(F("WebOTAHandler"), F("Update-Flags werden zurückgesetzt..."));
      auto result = ConfigMgr.setUpdateFlags(false, false);
      if (!result.isSuccess()) {
        LOG_ERROR(F("WebOTAHandler"), F("Fehler beim Zurücksetzen der Update-Flags"));
      }

      LOG_INFO(F("WebOTAHandler"), F("Sofortiger Reset wird erzwungen..."));
#ifdef ESP32
      ESP.restart();
#else
      ESP.wdtDisable();
      ESP.wdtEnable(1);
      while (1)
        ; // Force watchdog reset
#endif
    } else {
      if (!errorReported) {
        // Provide additional diagnostic logging: the number of bytes the
        // upload reported, the expected total we set in begin(), and the
        // numeric Update error code returned by the Update API.
        LOG_ERROR(F("WebOTAHandler"), F("Update.end() gab einen Fehler zurück"));
        LOG_DEBUG(F("WebOTAHandler"),
                  String(F("Hochgeladene Gesamtgröße: ")) + String(upload.totalSize) +
                      String(F(", erwartet (status totalSize): ")) + String(_status.totalSize));
        LOG_DEBUG(F("WebOTAHandler"), String(F("Update Fehlercode: ")) + String(Update.getError()));
        String error = String(F("Update fehlgeschlagen: ")) + String(Update.getError());
        LOG_ERROR(F("WebOTAHandler"), error);

#if USE_DISPLAY
        // Show error on display
        if (displayManager) {
          if (displayManager) {
            displayManager->updateLogStatus(F("Update failed!"), false);
          }
          delay(500); // Show error message briefly
          if (displayManager) {
            displayManager->endUpdateMode();
          }
        }
#endif

        sendJsonResponse(500,
                         String(F("{\"success\":false,\"error\":\"")) + error + String(F("\"}")));
        errorReported = true;
      }
    }
    break;
  }

  case UPLOAD_FILE_ABORTED: {
    // Check if we already completed successfully
    if (Update.hasError() && !errorReported) {
      Update.end();
      _status.lastError = F("Update abgebrochen");
      LOG_ERROR(F("WebOTAHandler"), F("Update abgebrochen"));

#if USE_DISPLAY
      // Show aborted message on display
      if (displayManager) {
        if (displayManager) {
          displayManager->updateLogStatus(F("Update aborted!"), false);
        }
        delay(500); // Show aborted message briefly
        if (displayManager) {
          displayManager->endUpdateMode();
        }
      }
#endif

      StaticJsonDocument<200> response;
      response["success"] = false;
      response["error"] = "Update abgebrochen";
      String jsonStr;
      serializeJson(response, jsonStr);
      sendJsonResponse(400, jsonStr);
      errorReported = true;
    }
    // No else block needed - let the device continue its normal flow
    break;
  }

  default:
    LOG_WARN(F("WebOTAHandler"), F("Unbekannter Upload-Status"));
    break;
  }

#ifndef ESP32
  ESP.wdtFeed();
#endif
}

bool WebOTAHandler::checkMemory() const { return ESP.getFreeHeap() >= MIN_FREE_HEAP; }

String WebOTAHandler::calculateMD5(uint8_t* data, size_t len) {
#ifdef ESP32
  // ESP32 uses mbedtls for MD5
  char md5str[33];
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, data, len);
  unsigned char digest[16];
  mbedtls_md5_finish(&ctx, digest);
  mbedtls_md5_free(&ctx);
  for (int i = 0; i < 16; i++) {
    sprintf(md5str + (i * 2), "%02x", digest[i]);
  }
  return String(md5str);
#else
  MD5Builder md5;
  md5.begin();
  md5.add(data, len);
  md5.calculate();
  return md5.toString();
#endif
}

size_t WebOTAHandler::calculateRequiredSpace(bool isFilesystem) const {
  if (isFilesystem) {
#ifdef ESP32
    return LittleFS.totalBytes();
#else
    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
      return fs_info.totalBytes;
    }
    return 0;
#endif
  } else {
    return (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  }
}
