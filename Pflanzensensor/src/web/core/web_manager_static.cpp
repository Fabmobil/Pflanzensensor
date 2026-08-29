/**
 * @file web_manager_static.cpp
 * @brief WebManager static file serving functionality
 */

#include <LittleFS.h>

#include "logger/logger.h"
#include "web/core/web_manager.h"

/**
 * @brief MIME-Typ anhand der Dateiendung bestimmen
 * @param path Dateipfad
 * @return Passender Content-Type, Standard text/plain
 */
static const char* contentTypeFor(const String& path) {
  if (path.endsWith(F(".css")))
    return "text/css";
  if (path.endsWith(F(".js")))
    return "application/javascript";
  if (path.endsWith(F(".png")))
    return "image/png";
  if (path.endsWith(F(".gif")))
    return "image/gif";
  if (path.endsWith(F(".ico")))
    return "image/x-icon";
  if (path.endsWith(F(".json")))
    return "application/json";
  if (path.endsWith(F(".html")))
    return "text/html";
  if (path.endsWith(F(".svg")))
    return "image/svg+xml";
  return "text/plain";
}

bool WebManager::tryServeStaticFile(const String& uri) {
  // Nur die bekannten Asset-Verzeichnisse ausliefern
  if (!(uri.startsWith(F("/css/")) || uri.startsWith(F("/js/")) || uri.startsWith(F("/img/")) ||
        uri == F("/favicon.ico"))) {
    return false;
  }

  // Pfad-Traversal ausschließen: kein ".." und keine Backslashes
  if (uri.indexOf(F("..")) >= 0 || uri.indexOf('\\') >= 0) {
    LOG_WARN(F("WebManager"), String(F("Abgewiesener Asset-Pfad: ")) + uri);
    return false;
  }

  if (!LittleFS.exists(uri)) {
    return false;
  }

  serveStaticFile(uri, contentTypeFor(uri), F("max-age=86400"));
  return true;
}

void WebManager::serveStaticFile(const String& path, const String& contentType,
                                 const String& cacheControl) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    LOG_ERROR(F("WebManager"), String(F("Statische Datei nicht lesbar: ")) + path);
    _server->send(500, F("text/plain"), F("Internal server error"));
    return;
  }

  _server->setContentLength(file.size());
  _server->sendHeader(F("Cache-Control"), cacheControl);
  _server->send(200, contentType, F(""));

  // 256 statt 1024 Byte: der Stack ist auf dem ESP8266 nur 4 KB groß, und
  // diese Funktion läuft mitten im Web-Handler-Aufrufpfad.
  uint8_t buffer[256];
  while (file.available()) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    if (bytesRead == 0) {
      break;
    }
    _server->sendContent(reinterpret_cast<const char*>(buffer), bytesRead);
    optimistic_yield(1000);
  }

  file.close();
}
