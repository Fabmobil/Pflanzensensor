/**
 * @file web_manager_static.cpp
 * @brief WebManager static file serving functionality
 */

#include <LittleFS.h>

#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::serveStaticFile(const String& path, const String& contentType,
                                 const String& cacheControl) {
  if (!LittleFS.exists(path)) {
    logger.warning(F("WebManager"), "Static file not found: " + path);
    _server->send(404, "text/plain", "File not found");
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    logger.error(F("WebManager"), "Failed to open static file: " + path);
    _server->send(500, "text/plain", "Internal server error");
    return;
  }

  // Set headers
  _server->setContentLength(file.size());
  _server->sendHeader("Content-Type", contentType);
  _server->sendHeader("Cache-Control", cacheControl);
  _server->sendHeader("Access-Control-Allow-Origin", "*");

  // Send headers
  _server->send(200);

  // Send file content in chunks to avoid memory issues
  const size_t bufferSize = 1024;
  uint8_t buffer[bufferSize];

  while (file.available()) {
    size_t bytesRead = file.read(buffer, bufferSize);
    if (bytesRead > 0) {
      _server->sendContent_P((char*)buffer, bytesRead);
    }
  }

  file.close();
}
