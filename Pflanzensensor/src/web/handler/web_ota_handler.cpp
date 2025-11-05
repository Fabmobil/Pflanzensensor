/**
 * @file web_ota_handler.cpp
 * @brief Implementation of web-based OTA update handler
 */

#include "web_ota_handler.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <MD5Builder.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_config_preferences.h"
#if USE_DISPLAY
#include "managers/manager_display.h"
#endif
#include "utils/critical_section.h"

extern std::unique_ptr<SensorManager> sensorManager;
#if USE_DISPLAY
extern std::unique_ptr<DisplayManager> displayManager;
#endif

WebOTAHandler::WebOTAHandler(ESP8266WebServer& server, WebAuth& auth)
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
    logger.error(F("WebOTAHandler"), F("Ungültiger Zustand: Beide Update-Flags sind gesetzt"));
    ConfigMgr.setUpdateFlags(false, false); // Reset flags
  }

  String response;
  serializeJson(doc, response);
  logger.debug(F("WebOTAHandler"), F("Status-Antwort: ") + response);
  sendJsonResponse(200, response);
}

RouterResult WebOTAHandler::onRegisterRoutes(WebRouter& router) {
  // Register status endpoint
  auto result = router.addRoute(HTTP_GET, "/status", [this]() { handleStatus(); });
  if (!result.isSuccess())
    return result;

  // Register update page
  result = router.addRoute(HTTP_GET, "/admin/update", [this]() { handleUpdatePage(); });
  if (!result.isSuccess())
    return result;

  // Register update handler
  _server.on(
      "/update", HTTP_POST,
      [this]() {
        sendJsonResponse(200,
                         Update.hasError() ? F("{\"success\":false}") : F("{\"success\":true}"));
      },
      [this]() { handleUpdateUpload(); });

  logger.info(F("WebOTAHandler"), F("OTA-Routen registriert"));
  return RouterResult::success();
}

HandlerResult WebOTAHandler::handleGet(const String& uri, const std::map<String, String>& query) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Use registerRoutes instead");
}

HandlerResult WebOTAHandler::handlePost(const String& uri, const std::map<String, String>& params) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Use registerRoutes instead");
}

std::vector<String> css = {"admin"};
std::vector<String> js = {"ota"};
void WebOTAHandler::handleUpdatePage() {
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
        sendChunk(F("Einstellungen erhalten.</li> "));
        sendChunk(F("</ul>"));
        sendChunk(F("<li>Das Gerät wird nach erfolgreichem Update automatisch neu "
                    "gestartet</li>"));
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
      logger.error(F("WebOTAHandler"), F("Kein Firmware-Update ausstehend"));
      return TypedResult<ResourceError, void>::fail(ResourceError::INVALID_STATE,
                                                    F("Kein Firmware-Update ausstehend"));
    }

    if (isFilesystem && !ConfigMgr.isFileSystemUpdatePending()) {
      logger.error(F("WebOTAHandler"), F("Kein Dateisystem-Update ausstehend"));
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
    logger.info(F("WebOTAHandler"), F("Update erfolgreich, Neustart..."));
    delay(1000);
    ESP.restart();
  }

  return TypedResult<ResourceError, void>::success();
}

void WebOTAHandler::abortUpdate() {
  if (_status.inProgress) {
    Update.end();
    _status = OTAStatus();
    logger.warning(F("WebOTAHandler"), F("Update abgebrochen"));
  }
}

OTAStatus WebOTAHandler::getStatus() const { return _status; }

void WebOTAHandler::handleUpdateUpload() {
  HTTPUpload& upload = _server.upload();
  static bool isFilesystem = false;
  static bool errorReported = false;
  static unsigned long lastProgressTime = 0;
  static uint8_t lastProgressUpdate = 0;

  // Display integration for update progress visualization

  switch (upload.status) {
  case UPLOAD_FILE_START: {
    String filename = upload.filename;
    isFilesystem = _server.hasArg("mode") && _server.arg("mode") == "fs";
    size_t contentLength = upload.contentLength;
    errorReported = false;

    logger.info(F("WebOTAHandler"), F("Upload gestartet: ") + filename + F(" (Typ: ") +
                                        String(isFilesystem ? F("Dateisystem") : F("Firmware")) +
                                        F(")"));
    logger.debug(F("WebOTAHandler"), F("Inhaltlänge: ") + String(contentLength) + F(" Bytes"));

    size_t freeSpace;
    if (isFilesystem) {
      {
        CriticalSection cs;
        FSInfo fs_info;
        if (LittleFS.info(fs_info)) {
          logger.debug(F("WebOTAHandler"),
                       F("Dateisystem gesamt: ") + String(fs_info.totalBytes) + F(" Bytes"));
          logger.debug(F("WebOTAHandler"),
                       F("Dateisystem belegt: ") + String(fs_info.usedBytes) + F(" Bytes"));
          freeSpace = fs_info.totalBytes;

          if (contentLength > fs_info.totalBytes) {
            logger.debug(F("WebOTAHandler"), F("Inhaltslänge an Dateisystemgröße angepasst"));
            contentLength = fs_info.totalBytes;
          }
        } else {
          logger.error(F("WebOTAHandler"), F("Fehler beim Lesen der Dateisysteminformationen"));
          _status.lastError = F("Fehler beim Lesen der Dateisysteminformationen");
          return;
        }
      }
    } else {
      freeSpace = ESP.getFreeSketchSpace();
      logger.debug(F("WebOTAHandler"),
                   F("Freier Sketch-Speicher: ") + String(freeSpace) + F(" Bytes"));
    }

    logger.debug(F("WebOTAHandler"),
                 F("Update-Modus: ") +
                     String(ConfigMgr.getDoFirmwareUpgrade() ? F("minimal") : F("normal")));
    logger.debug(F("WebOTAHandler"),
                 F("Endgültige Inhaltslänge: ") + String(contentLength) + F(" Bytes"));

    if (contentLength > freeSpace) {
      String error = F("Nicht genug Speicherplatz - benötigt: ") + String(contentLength) +
                     F(", verfügbar: ") + String(freeSpace);
      logger.error(F("WebOTAHandler"), error);
      _status.lastError = error;
      return;
    }

    uint8_t command = isFilesystem ? U_FS : U_FLASH;
    logger.debug(F("WebOTAHandler"), F("Update-Befehl: ") + String(command) + F(", Inhaltlänge: ") +
                                         String(contentLength) + F(", verfügbarer Speicher: ") +
                                         String(freeSpace));

    // Backup Preferences before filesystem update (they're stored in LittleFS)
    if (isFilesystem) {
      if (!backupAllPreferences()) {
        logger.warning(F("WebOTAHandler"),
                       F("Preferences-Sicherung fehlgeschlagen - Fortfahren trotzdem"));
      }
    }

    if (!Update.begin(contentLength, command)) {
      String error = F("Start des Updates fehlgeschlagen: ") + String(Update.getError());
      logger.error(F("WebOTAHandler"), error);
      logger.error(F("WebOTAHandler"),
                   F("Verfügbarer Speicher: ") + String(freeSpace) + F(" Bytes"));
      logger.error(F("WebOTAHandler"), F("Benötigt: ") + String(contentLength) + F(" Bytes"));
      _status.lastError = error;
      return;
    }

    if (_server.hasArg("md5")) {
      Update.setMD5(_server.arg("md5").c_str());
      logger.debug(F("WebOTAHandler"), F("MD5 gesetzt: ") + _server.arg("md5"));
    }

    _status.inProgress = true;
    _status.currentProgress = 0;
    _status.totalSize = contentLength;
    lastProgressTime = millis();
    lastProgressUpdate = 0;

    logger.info(F("WebOTAHandler"),
                F("Update gestartet - Größe: ") + String(contentLength) + F(" Bytes"));

#if USE_DISPLAY
    // Show update start on display
    if (displayManager) {
      String updateType = isFilesystem ? F("Filesystem") : F("Firmware");
      displayManager->showLogScreen(updateType + F(" update starting..."), false);
    }
#endif
    break;
  }

  case UPLOAD_FILE_WRITE: {
    if (!_status.inProgress)
      return;

    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      if (!errorReported) {
        String error = F("Update-Schreibvorgang fehlgeschlagen: ") + String(Update.getError());
        logger.error(F("WebOTAHandler"), error);
        _status.lastError = error;
        errorReported = true;
      }
      return;
    }

    _status.currentProgress = Update.progress();
    uint8_t progress = (_status.currentProgress * 100) / _status.totalSize;

    if (progress != lastProgressUpdate &&
        (progress % 25 == 0 || millis() - lastProgressTime >= 5000)) {
      logger.info(F("WebOTAHandler"), F("Update-Fortschritt: ") + String(progress) + F("%"));

#if USE_DISPLAY
      // Show progress on display
      if (displayManager) {
        displayManager->updateLogStatus(F("Progress: ") + String(progress) + F("%"), false);
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
      logger.info(F("WebOTAHandler"),
                  F("Update erfolgreich: ") + String(upload.totalSize) + F(" Bytes"));

#if USE_DISPLAY
      // Show success on display
      if (displayManager) {
        displayManager->updateLogStatus(F("Update completed successfully!"), false);
        delay(1000); // Show success message briefly
        displayManager->endUpdateMode();
      }
#endif

      // Restore Preferences after successful filesystem update
      if (Update.getCommand() == U_FS && _prefsBackup.hasBackup) {
        logger.info(F("WebOTAHandler"),
                    F("Filesystem-Update erfolgreich, stelle Preferences wieder her..."));
        delay(100); // Brief delay to let filesystem stabilize
        restoreAllPreferences();
      }

      // Send success response to deploy script IMMEDIATELY after update
      // succeeds Do this BEFORE any JSON operations that might cause crashes
      StaticJsonDocument<200> response;
      response["success"] = true;
      response["needsReboot"] = true;
      String jsonStr;
      serializeJson(response, jsonStr);
      sendJsonResponse(200, jsonStr);

      // Give the response time to be sent
      delay(200);

      // Now try to clear update flags (this might crash, but response is
      // already sent)
      logger.info(F("WebOTAHandler"), F("Update-Flags werden zurückgesetzt..."));
      auto result = ConfigMgr.setUpdateFlags(false, false);
      if (!result.isSuccess()) {
        logger.error(F("WebOTAHandler"), F("Fehler beim Zurücksetzen der Update-Flags"));
      }

      logger.info(F("WebOTAHandler"), F("Sofortiger Reset wird erzwungen..."));
      ESP.wdtDisable();
      ESP.wdtEnable(1);
      while (1)
        ; // Force watchdog reset
    } else {
      if (!errorReported) {
        // Provide additional diagnostic logging: the number of bytes the
        // upload reported, the expected total we set in begin(), and the
        // numeric Update error code returned by the Update API.
        logger.error(F("WebOTAHandler"), F("Update.end() gab einen Fehler zurück"));
        logger.debug(F("WebOTAHandler"),
                     F("Hochgeladene Gesamtgröße: ") + String(upload.totalSize) +
                         F(", erwartet (status totalSize): ") + String(_status.totalSize));
        logger.debug(F("WebOTAHandler"), F("Update Fehlercode: ") + String(Update.getError()));
        String error = F("Update fehlgeschlagen: ") + String(Update.getError());
        logger.error(F("WebOTAHandler"), error);

#if USE_DISPLAY
        // Show error on display
        if (displayManager) {
          displayManager->updateLogStatus(F("Update failed!"), false);
          delay(1000); // Show error message briefly
          displayManager->endUpdateMode();
        }
#endif

        sendJsonResponse(500, F("{\"success\":false,\"error\":\"") + error + F("\"}"));
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
      logger.error(F("WebOTAHandler"), F("Update abgebrochen"));

#if USE_DISPLAY
      // Show aborted message on display
      if (displayManager) {
        displayManager->updateLogStatus(F("Update aborted!"), false);
        delay(1000); // Show aborted message briefly
        displayManager->endUpdateMode();
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
    logger.warning(F("WebOTAHandler"), F("Unbekannter Upload-Status"));
    break;
  }

  ESP.wdtFeed();
}

bool WebOTAHandler::checkMemory() const { return ESP.getFreeHeap() >= MIN_FREE_HEAP; }

String WebOTAHandler::calculateMD5(uint8_t* data, size_t len) {
  MD5Builder md5;
  md5.begin();
  md5.add(data, len);
  md5.calculate();
  return md5.toString();
}

size_t WebOTAHandler::calculateRequiredSpace(bool isFilesystem) const {
  if (isFilesystem) {
    CriticalSection cs;
    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
      return fs_info.totalBytes;
    }
    return 0;
  } else {
    return (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  }
}

bool WebOTAHandler::backupAllPreferences() {
  logger.info(F("WebOTAHandler"), F("Sichere Preferences vor Dateisystem-Update..."));

  Preferences prefs;
  _prefsBackup.hasBackup = false;

  // Backup general namespace
  if (!prefs.begin(PreferencesNamespaces::GENERAL, true)) {
    logger.error(F("WebOTAHandler"), F("Fehler beim Öffnen von General-Preferences"));
    return false;
  }
  _prefsBackup.device_name = PreferencesManager::getString(prefs, "device_name", "Pflanzensensor");
  _prefsBackup.admin_pwd = PreferencesManager::getString(prefs, "admin_pwd", "admin");
  _prefsBackup.md5_verify = PreferencesManager::getBool(prefs, "md5_verify", true);
  _prefsBackup.collectd_en = PreferencesManager::getBool(prefs, "collectd_en", false);
  _prefsBackup.file_log = PreferencesManager::getBool(prefs, "file_log", false);
  _prefsBackup.flower_sens = PreferencesManager::getString(prefs, "flower_sens", "");
  prefs.end();

  // Backup WiFi namespace
  if (!prefs.begin(PreferencesNamespaces::WIFI, true)) {
    logger.error(F("WebOTAHandler"), F("Fehler beim Öffnen von WiFi-Preferences"));
    return false;
  }
  _prefsBackup.wifi_ssid1 = PreferencesManager::getString(prefs, "ssid1", "");
  _prefsBackup.wifi_ssid2 = PreferencesManager::getString(prefs, "ssid2", "");
  _prefsBackup.wifi_ssid3 = PreferencesManager::getString(prefs, "ssid3", "");
  _prefsBackup.wifi_pwd1 = PreferencesManager::getString(prefs, "pwd1", "");
  _prefsBackup.wifi_pwd2 = PreferencesManager::getString(prefs, "pwd2", "");
  _prefsBackup.wifi_pwd3 = PreferencesManager::getString(prefs, "pwd3", "");
  prefs.end();

  // Backup display namespace
  if (!prefs.begin(PreferencesNamespaces::DISP, true)) {
    logger.error(F("WebOTAHandler"), F("Fehler beim Öffnen von Display-Preferences"));
    return false;
  }
  _prefsBackup.show_ip = PreferencesManager::getBool(prefs, "show_ip", true);
  _prefsBackup.show_clock = PreferencesManager::getBool(prefs, "show_clock", true);
  _prefsBackup.show_flower = PreferencesManager::getBool(prefs, "show_flower", true);
  _prefsBackup.show_fabmobil = PreferencesManager::getBool(prefs, "show_fabmobil", true);
  _prefsBackup.screen_dur = PreferencesManager::getUInt(prefs, "screen_dur", 5);
  _prefsBackup.clock_fmt = PreferencesManager::getString(prefs, "clock_fmt", "24h");
  prefs.end();

  // Backup debug namespace
  if (!prefs.begin(PreferencesNamespaces::DEBUG, true)) {
    logger.error(F("WebOTAHandler"), F("Fehler beim Öffnen von Debug-Preferences"));
    return false;
  }
  _prefsBackup.debug_ram = PreferencesManager::getBool(prefs, "ram", false);
  _prefsBackup.debug_meas_cycle = PreferencesManager::getBool(prefs, "meas_cycle", false);
  _prefsBackup.debug_sensor = PreferencesManager::getBool(prefs, "sensor", false);
  _prefsBackup.debug_display = PreferencesManager::getBool(prefs, "display", false);
  _prefsBackup.debug_websocket = PreferencesManager::getBool(prefs, "websocket", false);
  prefs.end();

  // Backup log namespace
  if (!prefs.begin(PreferencesNamespaces::LOG, true)) {
    logger.error(F("WebOTAHandler"), F("Fehler beim Öffnen von Log-Preferences"));
    return false;
  }
  _prefsBackup.log_level = PreferencesManager::getUChar(prefs, "level", 3);
  _prefsBackup.log_file_en = PreferencesManager::getBool(prefs, "file_enabled", false);
  prefs.end();

  // Backup LED traffic namespace
  if (!prefs.begin(PreferencesNamespaces::LED_TRAFFIC, true)) {
    logger.error(F("WebOTAHandler"), F("Fehler beim Öffnen von LED-Traffic-Preferences"));
    return false;
  }
  _prefsBackup.led_mode = PreferencesManager::getUChar(prefs, "mode", 0);
  _prefsBackup.led_sel_meas = PreferencesManager::getString(prefs, "sel_meas", "");
  prefs.end();

  _prefsBackup.hasBackup = true;
  logger.info(F("WebOTAHandler"), F("Preferences erfolgreich gesichert"));
  return true;
}

bool WebOTAHandler::restoreAllPreferences() {
  if (!_prefsBackup.hasBackup) {
    logger.warning(F("WebOTAHandler"),
                   F("Keine Preferences-Sicherung zum Wiederherstellen vorhanden"));
    return false;
  }

  logger.info(F("WebOTAHandler"), F("Stelle Preferences nach Dateisystem-Update wieder her..."));

  // Restore general namespace
  auto r1 = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "device_name",
                                                  _prefsBackup.device_name);
  auto r2 = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "admin_pwd",
                                                  _prefsBackup.admin_pwd);
  auto r3 = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "md5_verify",
                                                _prefsBackup.md5_verify);
  auto r4 = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "collectd_en",
                                                _prefsBackup.collectd_en);
  auto r5 = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "file_log",
                                                _prefsBackup.file_log);
  auto r6 = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "flower_sens",
                                                  _prefsBackup.flower_sens);

  // Restore WiFi credentials
  auto r7 =
      PreferencesManager::updateWiFiCredentials(1, _prefsBackup.wifi_ssid1, _prefsBackup.wifi_pwd1);
  auto r8 =
      PreferencesManager::updateWiFiCredentials(2, _prefsBackup.wifi_ssid2, _prefsBackup.wifi_pwd2);
  auto r9 =
      PreferencesManager::updateWiFiCredentials(3, _prefsBackup.wifi_ssid3, _prefsBackup.wifi_pwd3);

  // Restore display settings
  auto r10 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_ip",
                                                 _prefsBackup.show_ip);
  auto r11 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_clock",
                                                 _prefsBackup.show_clock);
  auto r12 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_flower",
                                                 _prefsBackup.show_flower);
  auto r13 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_fabmobil",
                                                 _prefsBackup.show_fabmobil);
  auto r14 = PreferencesManager::updateUIntValue(PreferencesNamespaces::DISP, "screen_dur",
                                                 _prefsBackup.screen_dur);
  auto r15 = PreferencesManager::updateStringValue(PreferencesNamespaces::DISP, "clock_fmt",
                                                   _prefsBackup.clock_fmt);

  // Restore debug settings
  auto r16 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "ram",
                                                 _prefsBackup.debug_ram);
  auto r17 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "meas_cycle",
                                                 _prefsBackup.debug_meas_cycle);
  auto r18 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "sensor",
                                                 _prefsBackup.debug_sensor);
  auto r19 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "display",
                                                 _prefsBackup.debug_display);
  auto r20 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "websocket",
                                                 _prefsBackup.debug_websocket);

  // Restore log settings
  auto r21 = PreferencesManager::updateUInt8Value(PreferencesNamespaces::LOG, "level",
                                                  _prefsBackup.log_level);
  auto r22 = PreferencesManager::updateBoolValue(PreferencesNamespaces::LOG, "file_enabled",
                                                 _prefsBackup.log_file_en);

  // Restore LED traffic settings
  auto r23 = PreferencesManager::updateUInt8Value(PreferencesNamespaces::LED_TRAFFIC, "mode",
                                                  _prefsBackup.led_mode);
  auto r24 = PreferencesManager::updateStringValue(PreferencesNamespaces::LED_TRAFFIC, "sel_meas",
                                                   _prefsBackup.led_sel_meas);

  // Check if any restore failed
  bool allSuccess = r1.isSuccess() && r2.isSuccess() && r3.isSuccess() && r4.isSuccess() &&
                    r5.isSuccess() && r6.isSuccess() && r7.isSuccess() && r8.isSuccess() &&
                    r9.isSuccess() && r10.isSuccess() && r11.isSuccess() && r12.isSuccess() &&
                    r13.isSuccess() && r14.isSuccess() && r15.isSuccess() && r16.isSuccess() &&
                    r17.isSuccess() && r18.isSuccess() && r19.isSuccess() && r20.isSuccess() &&
                    r21.isSuccess() && r22.isSuccess() && r23.isSuccess() && r24.isSuccess();

  if (allSuccess) {
    logger.info(F("WebOTAHandler"), F("Preferences erfolgreich wiederhergestellt"));
  } else {
    logger.warning(F("WebOTAHandler"),
                   F("Einige Preferences konnten nicht wiederhergestellt werden"));
  }

  _prefsBackup.hasBackup = false;
  return allSuccess;
}
