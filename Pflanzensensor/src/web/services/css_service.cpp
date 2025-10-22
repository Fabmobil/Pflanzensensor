// css_service.cpp
#include "web/services/css_service.h"

#include <LittleFS.h>

#include "logger/logger.h"
#include "managers/manager_resource.h"
#include "utils/critical_section.h"
#include "web/core/components.h"

CSSService::CSSService(ESP8266WebServer& server) : BaseHandler(server) {
  logger.debug(F("CSSService"), F("Initialisiere CSS-Service"));
  initModules();
}

void CSSService::initModules() {
  _modules = {CSSModule("base", "Base Styles", "/css/style.css"),
              CSSModule("start", "Start Page", "/css/start.css"),
              CSSModule("admin", "Admin Pages", "/css/admin.css"),
              CSSModule("logs", "Log Pages", "/css/logs.css")};
}

RouterResult CSSService::onRegisterRoutes(WebRouter& router) {
  logger.info(F("CSSService"), F("CSS-Routen registriert"));
  return RouterResult::success();
}

HandlerResult CSSService::handleGet(const String& uri, const std::map<String, String>& query) {
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unknown endpoint");
}

HandlerResult CSSService::handlePost(const String& uri, const std::map<String, String>& params) {
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unknown endpoint");
}

bool CSSService::createBackup(const String& path) const {
  CriticalSection cs;

  if (!LittleFS.exists(path)) {
    return true; // Nothing to backup
  }

  String backupPath = path + ".bak";
  if (LittleFS.exists(backupPath)) {
    LittleFS.remove(backupPath);
  }

  File currentFile = LittleFS.open(path, "r");
  File backupFile = LittleFS.open(backupPath, "w");

  if (!currentFile || !backupFile) {
    return false;
  }

  size_t fileSize = currentFile.size();
  bool success = true;

  // Copy in chunks to avoid memory issues
  const size_t CHUNK_SIZE = 1024;
  uint8_t buffer[CHUNK_SIZE];

  while (fileSize > 0) {
    size_t chunk = std::min(fileSize, CHUNK_SIZE);
    size_t bytesRead = currentFile.read(buffer, chunk);
    if (bytesRead != chunk || backupFile.write(buffer, chunk) != chunk) {
      success = false;
      break;
    }
    fileSize -= chunk;
  }

  currentFile.close();
  backupFile.close();
  return success;
}

String CSSService::loadCSS(const String& path) const {
  CriticalSection cs;

  if (!LittleFS.exists(path)) {
    logger.warning(F("CSSService"), String(F("CSS-Datei nicht gefunden: ")) + path);
    return "";
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    logger.error(F("CSSService"), String(F("Öffnen der CSS-Datei fehlgeschlagen: ")) + path);
    return "";
  }

  String content = file.readString();
  file.close();
  return content;
}

bool CSSService::saveCSS(const String& path, const String& content) const {
  CriticalSection cs;

  File file = LittleFS.open(path, "w");
  if (!file) {
    logger.error(F("CSSService"),
                 String(F("Öffnen der CSS-Datei zum Schreiben fehlgeschlagen: ")) + path);
    return false;
  }

  size_t written = file.print(content);
  file.close();

  if (written != content.length()) {
    logger.error(F("CSSService"), F("Vollständiges Schreiben der CSS-Inhalte fehlgeschlagen"));
    return false;
  }

  return true;
}

const CSSService::CSSModule* CSSService::getModule(const String& id) const {
  auto it = std::find_if(_modules.begin(), _modules.end(),
                         [&id](const CSSModule& m) { return m.id == id; });
  return it != _modules.end() ? &(*it) : nullptr;
}
