/**
 * @file admin_handler_core.cpp
 * @brief Core implementation of admin handler functionality
 * @details Provides route registration and main page handling
 */

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "configs/config.h"
#include "filesystem/config_fs.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/critical_section.h"
#include "utils/wifi.h" // For getActiveWiFiSlot()
#include "web/handler/admin_handler.h"

RouterResult AdminHandler::onRegisterRoutes(WebRouter& router) {
  logger.logMemoryStats(F("AdminRegisterRoutes"));

  // Register admin page route
  auto result = router.addRoute(HTTP_GET, "/admin", [this]() {
    if (!validateRequest()) {
      _server.requestAuthentication();
      return;
    }
    handleAdminPage();
  });
  if (!result.isSuccess()) {
    logger.error(F("AdminHandler"), F("Registrieren der /admin-Route fehlgeschlagen"));
    return result;
  }
  logger.debug(F("AdminHandler"), F("Registrierte /admin-Route"));

  // Note: Config updates are now handled by unified /admin/config/setConfigValue
  // Old /admin/updateSettings route has been removed.

  // Register config reset route
  result = router.addRoute(HTTP_POST, "/admin/reset", [this]() {
    if (!validateRequest()) {
      _server.requestAuthentication();
      return;
    }
    handleConfigReset();
  });
  if (!result.isSuccess()) {
    logger.error(F("AdminHandler"), F("Registrieren der /admin/reset-Route fehlgeschlagen"));
    return result;
  }
  logger.debug(F("AdminHandler"), F("Registrierte /admin/reset-Route"));

  // Register reboot route
  result = router.addRoute(HTTP_POST, "/admin/reboot", [this]() {
    if (!validateRequest()) {
      _server.requestAuthentication();
      return;
    }
    handleReboot();
  });
  if (!result.isSuccess()) {
    logger.error(F("AdminHandler"), F("Registrieren der /admin/reboot-Route fehlgeschlagen"));
    return result;
  }
  logger.debug(F("AdminHandler"), F("Registrierte /admin/reboot-Route"));

  // Register config set route
  // Note: /admin/config/set handled by legacy route in WebManager; admin
  // updates are consolidated to the single /admin/updateSettings endpoint.

  result = router.addRoute(HTTP_GET, "/admin/downloadLog", [this]() {
    if (!validateRequest()) {
      _server.requestAuthentication();
      return;
    }
    handleDownloadLog();
  });
  if (!result.isSuccess()) {
    logger.error(F("AdminHandler"), F("Registrieren der /admin/downloadLog-Route fehlgeschlagen"));
    return result;
  }
  logger.debug(F("AdminHandler"), F("Registrierte /admin/downloadLog-Route"));

  // NOTE: Download/upload routes removed - configuration now in Preferences (EEPROM)
  // OLD REMOVED: /admin/downloadConfig (GET)
  // OLD REMOVED: /admin/downloadSensors (GET)
  // OLD REMOVED: /admin/uploadConfig (POST)

  // Register WiFi settings update route
  result = router.addRoute(HTTP_POST, "/admin/updateWiFi", [this]() {
    if (!validateRequest()) {
      _server.requestAuthentication();
      return;
    }
    handleWiFiUpdate();
  });
  if (!result.isSuccess()) {
    logger.error(F("AdminHandler"), F("Registrieren der /admin/updateWiFi-Route fehlgeschlagen"));
    return result;
  }
  logger.debug(F("AdminHandler"), F("Registrierte /admin/updateWiFi-Route"));

  logger.info(F("AdminHandler"), F("Alle Admin-Routen erfolgreich registriert"));
  logger.logMemoryStats(F("AdminRegisterRoutes"));
  return result;
}

HandlerResult AdminHandler::handleGet(const String& uri, const std::map<String, String>& query) {
  // Let registerRoutes handle the actual routing
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Bitte registerRoutes verwenden");
}

HandlerResult AdminHandler::handlePost(const String& uri, const std::map<String, String>& params) {
  // Let registerRoutes handle the actual routing
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Bitte registerRoutes verwenden");
}

// AdminHandler::handleConfigSet removed - admin updates are handled via
// the single AJAX endpoint /admin/updateSettings.

void AdminHandler::handleAdminPage() {
  logger.debug(F("AdminHandler"), F("handleAdminPage called"));
  logger.logMemoryStats(F("AdminPageStart"));

  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin"};
  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin",
      [this]() {
        sendChunk(F("<div class='admin-grid'>"));
        generateAndSendSystemSettingsCard();
        generateAndSendSystemActionsCard();
        generateAndSendWiFiSettingsCard();
        generateAndSendSystemInfoCard();
#if USE_MAIL
        generateAndSendMailSettingsCard();
#endif
        generateAndSendDebugSettingsCard();
        sendChunk(F("</div>"));
      },
      css, js);
  logger.debug(F("AdminHandler"), F("Admin page sent successfully"));
}

void AdminHandler::handleDownloadLog() {
  if (!ConfigMgr.isFileLoggingEnabled()) {
    this->sendError(404, F("Datei-Logging ist auf diesem Gerät nicht aktiviert"));
    return;
  }

  const char* LOG_FILE = "/log.txt";
  if (!MainFS.exists(LOG_FILE)) {
    this->sendError(404, F("Keine Log-Datei gefunden"));
    return;
  }

  File logFile = MainFS.open(LOG_FILE, "r");
  if (!logFile) {
    this->sendError(500, F("Öffnen der Log-Datei fehlgeschlagen"));
    return;
  }

  size_t fileSize = logFile.size();
  if (fileSize == 0) {
    logFile.close();
    this->sendError(404, F("Log-Datei ist leer"));
    return;
  }

  _server.sendHeader(F("Content-Type"), F("text/plain"));
  _server.sendHeader(F("Content-Disposition"), F("attachment; filename=log.txt"));
  _server.sendHeader(F("Connection"), F("close"));
  _server.sendHeader(F("Content-Length"), String(fileSize));
  _server.setContentLength(fileSize);
  _server.send(200, F("text/plain"), ""); // Send headers

  const size_t CHUNK_SIZE = 1024;
  uint8_t buffer[CHUNK_SIZE];
  size_t remainingBytes = fileSize;

  while (remainingBytes > 0) {
    size_t bytesToRead = min(remainingBytes, CHUNK_SIZE);
    size_t bytesRead = logFile.read(buffer, bytesToRead);
    if (bytesRead == 0)
      break;
    _server.sendContent((char*)buffer, bytesRead);
    remainingBytes -= bytesRead;
    yield();
  }
  logFile.close();
}
