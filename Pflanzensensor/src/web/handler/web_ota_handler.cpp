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
  if (ConfigMgr.isFileSystemUpdatePending() &&
      ConfigMgr.isFirmwareUpdatePending()) {
    logger.error(F("WebOTAHandler"),
                 F("Invalid state: Both update flags are set"));
    ConfigMgr.setUpdateFlags(false, false);  // Reset flags
  }

  String response;
  serializeJson(doc, response);
  logger.debug(F("WebOTAHandler"), "Status response: " + response);
  sendJsonResponse(200, response);
}

RouterResult WebOTAHandler::onRegisterRoutes(WebRouter& router) {
  // Register status endpoint
  auto result =
      router.addRoute(HTTP_GET, "/status", [this]() { handleStatus(); });
  if (!result.isSuccess()) return result;

  // Register update page
  result = router.addRoute(HTTP_GET, "/admin/update",
                           [this]() { handleUpdatePage(); });
  if (!result.isSuccess()) return result;

  // Register update handler
  _server.on(
      "/update", HTTP_POST,
      [this]() {
        sendJsonResponse(200, Update.hasError() ? F("{\"success\":false}")
                                                : F("{\"success\":true}"));
      },
      [this]() { handleUpdateUpload(); });

  logger.info(F("WebOTAHandler"), F("OTA routes registered"));
  return RouterResult::success();
}

HandlerResult WebOTAHandler::handleGet(const String& uri,
                                       const std::map<String, String>& query) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             "Use registerRoutes instead");
}

HandlerResult WebOTAHandler::handlePost(
    const String& uri, const std::map<String, String>& params) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             "Use registerRoutes instead");
}

std::vector<String> css = {"admin"};
std::vector<String> js = {"ota"};
void WebOTAHandler::handleUpdatePage() {
  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin/update",
      [this]() {
        // System Information Card
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>System Information</h2>"));
        sendChunk(F("<table class='info-table'>"));

        // System info table content
        sendChunk(F("<tr><td>Version:</td><td>"));
        sendChunk(VERSION);
        sendChunk(F("</td></tr>"));

        sendChunk(F("<tr><td>Freier Heap:</td><td>"));
        sendChunk(String(ESP.getFreeHeap()));
        sendChunk(F(" bytes</td></tr>"));

        sendChunk(F("<tr><td>Freier Sketch Space:</td><td>"));
        sendChunk(String(ESP.getFreeSketchSpace()));
        sendChunk(F(" bytes</td></tr>"));

        sendChunk(F("</table></div>"));

        // Update section Card
        sendChunk(F("<div class='card update-section'>"));

        // Warning box
        sendChunk(F("<div class='warning-box'>"));
        sendChunk(F("<h3>⚠️ Wichtige Hinweise</h3><ul>"));
        sendChunk(
            F("<li>Stelle sicher, dass die neue Firmware für dieses Gerät "
              "geeignet ist</li>"));
        sendChunk(
            F("<li>Das Gerät wird nach erfolgreichem Update automatisch neu "
              "gestartet</li>"));
        sendChunk(F(
            "<li>Trenne während des Updates nicht die Stromversorgung!</li>"));
        sendChunk(F("</ul></div>"));

        // Upload form
        sendChunk(
            F("<form id='update-form' method='POST' class='config-form' "
              "action='/update' enctype='multipart/form-data'>"));

        // File input
        sendChunk(
            F("<div class='form-group'><label>Firmware Datei (.bin):</label>"));
        sendChunk(
            F("<input type='file' id='update-file' name='firmware' "
              "accept='.bin' required>"));
        sendChunk(F("</div>"));

        // MD5 input (nur wenn MD5 Verifikation aktiviert ist)
        if (ConfigMgr.isMD5Verification()) {
          sendChunk(F("<div class='form-group'><label>MD5 Prüfsumme:</label>"));
          sendChunk(
              F("<input type='text' id='md5-input' name='md5' required>"));
          sendChunk(F("</div>"));
        }

        // Progress and status containers
        sendChunk(
            F("<div id='progress-container' class='progress-container'>"));
        sendChunk(F("<div id='progress' class='progress'></div>"));
        sendChunk(F("</div>"));
        sendChunk(F("<div id='status' class='status'></div>"));

        // Submit button
        sendChunk(
            F("<button type='submit' id='update-button' class='button "
              "button-primary'>"));
        sendChunk(F("Update starten</button>"));

        sendChunk(F("</form></div>"));
      },
      css, js);
}

TypedResult<ResourceError, void> WebOTAHandler::beginUpdate(size_t size,
                                                            const String& md5,
                                                            bool isFilesystem) {
  // If we're in minimal mode but flags are not set, allow the update
  if (ConfigMgr.getDoFirmwareUpgrade()) {
    // Skip flag checks in minimal mode
    return TypedResult<ResourceError, void>::success();
  }

  // Normal mode checks
  if (!ConfigMgr.getDoFirmwareUpgrade()) {
    if (!isFilesystem && !ConfigMgr.isFirmwareUpdatePending()) {
      logger.error(F("WebOTAHandler"), F("Firmware update not pending"));
      return TypedResult<ResourceError, void>::fail(
          ResourceError::INVALID_STATE, F("Firmware update not pending"));
    }

    if (isFilesystem && !ConfigMgr.isFileSystemUpdatePending()) {
      logger.error(F("WebOTAHandler"), F("Filesystem update not pending"));
      return TypedResult<ResourceError, void>::fail(
          ResourceError::INVALID_STATE, F("Filesystem update not pending"));
    }
  }

  return TypedResult<ResourceError, void>::success();
}

TypedResult<ResourceError, void> WebOTAHandler::writeData(uint8_t* data,
                                                          size_t len) {
  if (!_status.inProgress) {
    return TypedResult<ResourceError, void>::fail(ResourceError::INVALID_STATE,
                                                  F("No update in progress"));
  }

  if (Update.write(data, len) != len) {
    String error = F("Write failed: ");
    error += Update.getError();
    return TypedResult<ResourceError, void>::fail(
        ResourceError::OPERATION_FAILED, error);
  }

  _status.currentProgress += len;
  return TypedResult<ResourceError, void>::success();
}

TypedResult<ResourceError, void> WebOTAHandler::endUpdate(bool reboot) {
  if (!_status.inProgress) {
    return TypedResult<ResourceError, void>::success();
  }

  if (!Update.end(true)) {
    String error = F("Update failed: ");
    error += Update.getError();
    return TypedResult<ResourceError, void>::fail(
        ResourceError::OPERATION_FAILED, error);
  }

  _status.inProgress = false;

  if (reboot) {
    logger.info(F("WebOTAHandler"), F("Update successful, rebooting..."));
    delay(1000);
    ESP.restart();
  }

  return TypedResult<ResourceError, void>::success();
}

void WebOTAHandler::abortUpdate() {
  if (_status.inProgress) {
    Update.end();
    _status = OTAStatus();
    logger.warning(F("WebOTAHandler"), F("Update aborted"));
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

      logger.info(F("WebOTAHandler"),
                  F("Starting upload: ") + filename + F(" (type: ") +
                      String(isFilesystem ? F("filesystem") : F("firmware")) +
                      F(")"));
      logger.debug(F("WebOTAHandler"),
                   F("Content length: ") + String(contentLength) + F(" bytes"));

      size_t freeSpace;
      if (isFilesystem) {
        {
          CriticalSection cs;
          FSInfo fs_info;
          if (LittleFS.info(fs_info)) {
            logger.debug(F("WebOTAHandler"), F("Filesystem total: ") +
                                                 String(fs_info.totalBytes) +
                                                 F(" bytes"));
            logger.debug(F("WebOTAHandler"), F("Filesystem used: ") +
                                                 String(fs_info.usedBytes) +
                                                 F(" bytes"));
            freeSpace = fs_info.totalBytes;

            if (contentLength > fs_info.totalBytes) {
              logger.debug(
                  F("WebOTAHandler"),
                  F("Adjusting content length to match filesystem size"));
              contentLength = fs_info.totalBytes;
            }
          } else {
            logger.error(F("WebOTAHandler"),
                         F("Failed to get filesystem info"));
            _status.lastError = F("Failed to get filesystem info");
            return;
          }
        }
      } else {
        freeSpace = ESP.getFreeSketchSpace();
        logger.debug(F("WebOTAHandler"), F("Free sketch space: ") +
                                             String(freeSpace) + F(" bytes"));
      }

      logger.debug(F("WebOTAHandler"),
                   F("Update mode: ") + String(ConfigMgr.getDoFirmwareUpgrade()
                                                   ? F("minimal")
                                                   : F("normal")));
      logger.debug(F("WebOTAHandler"), F("Final content length: ") +
                                           String(contentLength) + F(" bytes"));

      if (contentLength > freeSpace) {
        String error = F("Not enough space - required: ") +
                       String(contentLength) + F(", available: ") +
                       String(freeSpace);
        logger.error(F("WebOTAHandler"), error);
        _status.lastError = error;
        return;
      }

      uint8_t command = isFilesystem ? U_FS : U_FLASH;
      logger.debug(F("WebOTAHandler"), F("Update command: ") + String(command) +
                                   F(", contentLength: ") + String(contentLength) +
                                   F(", freeSpace: ") + String(freeSpace));
      if (!Update.begin(contentLength, command)) {
        String error = F("Update begin failed: ") + String(Update.getError());
        logger.error(F("WebOTAHandler"), error);
        logger.error(F("WebOTAHandler"),
                     F("Free space: ") + String(freeSpace) + F(" bytes"));
        logger.error(F("WebOTAHandler"),
                     F("Required: ") + String(contentLength) + F(" bytes"));
        _status.lastError = error;
        return;
      }

      if (_server.hasArg("md5")) {
        Update.setMD5(_server.arg("md5").c_str());
        logger.debug(F("WebOTAHandler"), F("MD5 set: ") + _server.arg("md5"));
      }

      _status.inProgress = true;
      _status.currentProgress = 0;
      _status.totalSize = contentLength;
      lastProgressTime = millis();
      lastProgressUpdate = 0;

      logger.info(F("WebOTAHandler"), F("Update started - size: ") +
                                          String(contentLength) + F(" bytes"));

#if USE_DISPLAY
      // Show update start on display
      if (displayManager) {
        String updateType = isFilesystem ? F("Filesystem") : F("Firmware");
        displayManager->showLogScreen(updateType + F(" update starting..."),
                                      false);
      }
#endif
      break;
    }

    case UPLOAD_FILE_WRITE: {
      if (!_status.inProgress) return;

      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        if (!errorReported) {
          String error = F("Update write failed: ") + String(Update.getError());
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
        logger.info(F("WebOTAHandler"),
                    F("Update progress: ") + String(progress) + F("%"));

#if USE_DISPLAY
        // Show progress on display
        if (displayManager) {
          displayManager->updateLogStatus(
              F("Progress: ") + String(progress) + F("%"), false);
        }
#endif

        lastProgressUpdate = progress;
        lastProgressTime = millis();
      }
      break;
    }

    case UPLOAD_FILE_END: {
      if (!_status.inProgress) return;

      if (Update.end(true)) {
        logger.info(
            F("WebOTAHandler"),
            F("Update Success: ") + String(upload.totalSize) + F(" bytes"));

#if USE_DISPLAY
        // Show success on display
        if (displayManager) {
          displayManager->updateLogStatus(F("Update completed successfully!"),
                                          false);
          delay(1000);  // Show success message briefly
          displayManager->endUpdateMode();
        }
#endif

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
        logger.info(F("WebOTAHandler"), F("Clearing update flags..."));
        auto result = ConfigMgr.setUpdateFlags(false, false);
        if (!result.isSuccess()) {
          logger.error(F("WebOTAHandler"), F("Failed to clear update flags"));
        }

        logger.info(F("WebOTAHandler"), F("Forcing immediate reset..."));
        ESP.wdtDisable();
        ESP.wdtEnable(1);
        while (1)
          ;  // Force watchdog reset
      } else {
        if (!errorReported) {
          // Provide additional diagnostic logging: the number of bytes the
          // upload reported, the expected total we set in begin(), and the
          // numeric Update error code returned by the Update API.
          logger.error(F("WebOTAHandler"), F("Update.end() returned failure"));
          logger.debug(F("WebOTAHandler"), F("Upload totalSize: ") + String(upload.totalSize) +
                                        F(", expected (status totalSize): ") + String(_status.totalSize));
          logger.debug(F("WebOTAHandler"), F("Update error code: ") + String(Update.getError()));
          String error = F("Update failed: ") + String(Update.getError());
          logger.error(F("WebOTAHandler"), error);

#if USE_DISPLAY
          // Show error on display
          if (displayManager) {
            displayManager->updateLogStatus(F("Update failed!"), false);
            delay(1000);  // Show error message briefly
            displayManager->endUpdateMode();
          }
#endif

          sendJsonResponse(
              500, F("{\"success\":false,\"error\":\"") + error + F("\"}"));
          errorReported = true;
        }
      }
      break;
    }

    case UPLOAD_FILE_ABORTED: {
      // Check if we already completed successfully
      if (Update.hasError() && !errorReported) {
        Update.end();
        _status.lastError = F("Update aborted");
        logger.error(F("WebOTAHandler"), F("Update aborted"));

#if USE_DISPLAY
        // Show aborted message on display
        if (displayManager) {
          displayManager->updateLogStatus(F("Update aborted!"), false);
          delay(1000);  // Show aborted message briefly
          displayManager->endUpdateMode();
        }
#endif

        StaticJsonDocument<200> response;
        response["success"] = false;
        response["error"] = "Update aborted";
        String jsonStr;
        serializeJson(response, jsonStr);
        sendJsonResponse(400, jsonStr);
        errorReported = true;
      }
      // No else block needed - let the device continue its normal flow
      break;
    }

    default:
      logger.warning(F("WebOTAHandler"), F("Unknown upload status"));
      break;
  }

  ESP.wdtFeed();
}

bool WebOTAHandler::checkMemory() const {
  return ESP.getFreeHeap() >= MIN_FREE_HEAP;
}

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
