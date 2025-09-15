/**
 * @file components.cpp
 * @brief Implementation of web components and HTML utilities
 */

#include "web/core/components.h"

#include <algorithm>

#include "logger/logger.h"

namespace Component {

ResourceResult beginResponse(ESP8266WebServer& server, const String& title,
                             const std::vector<String>& additionalCss) {
  static const char CONTENT_TYPE[] PROGMEM = "Content-Type";
  static const char TEXT_HTML[] PROGMEM = "text/html";
  static const char CONNECTION[] PROGMEM = "Connection";
  static const char CLOSE[] PROGMEM = "close";
  static const char CACHE_CONTROL[] PROGMEM = "Cache-Control";
  static const char NO_CACHE[] PROGMEM = "no-cache";

  // Check memory before starting
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < SAFE_HEAP_SIZE) {
    server.send(503, F("text/plain"),
                F("Unzureichender Speicher, bitte später erneut versuchen"));
    return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY,
                                F("Unzureichender Speicher für HTML-Antwort"));
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader(FPSTR(CONTENT_TYPE), FPSTR(TEXT_HTML));
  server.sendHeader(FPSTR(CONNECTION), FPSTR(CLOSE));
  server.sendHeader(FPSTR(CACHE_CONTROL), FPSTR(NO_CACHE));
  server.send(200, FPSTR(TEXT_HTML), F(""));

  // Send initial HTML
  sendChunk(
      server,
      F("<!DOCTYPE html><html lang='de'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>"));
  sendChunk(server, title);
  sendChunk(server, F("</title>"
                      "<link rel='stylesheet' href='/css/style.css'>"));

  // Add each additional CSS file
  for (const auto& css : additionalCss) {
    if (!css.isEmpty()) {
      sendChunk(server, F("<link rel='stylesheet' href='/css/"));
      sendChunk(server, css);
      sendChunk(server, F(".css'>"));
    }
  }

  sendChunk(server, F("</head><body>"));
  return ResourceResult::success();
}

void sendChunk(ESP8266WebServer& server, const String& chunk) {
  static char buffer[128];  // Reuse buffer
  size_t remaining = chunk.length();
  size_t offset = 0;
  static unsigned long lastYield = 0;
  const unsigned long YIELD_INTERVAL = 100;  // Yield every 100ms

  while (remaining > 0) {
    size_t toSend = std::min<size_t>(
        remaining, sizeof(buffer) - 1);  // Leave space for null terminator
    chunk.substring(offset, offset + toSend)
        .toCharArray(buffer, sizeof(buffer));
    server.sendContent(buffer);
    remaining -= toSend;
    offset += toSend;

    // Yield periodically to prevent watchdog timeouts
    if (millis() - lastYield > YIELD_INTERVAL) {
      yield();
      lastYield = millis();
    }
  }
}

void sendNavigation(ESP8266WebServer& server, const String& activeItem) {
  // First row: Start, Logs, Admin
  sendChunk(server, F("<nav class='navbar'>\n"
                      "  <ul class='nav-row nav-row-main'>\n"
                      "    <li class='nav-item'><a href='/' class='nav-link"));
  if (activeItem == "start") sendChunk(server, F(" active"));
  sendChunk(server,
            F("'>Start</a></li>\n"
              "    <li class='nav-item'><a href='/logs' class='nav-link"));
  if (activeItem == "logs") sendChunk(server, F(" active"));
  sendChunk(
      server,
      F("'>Logs</a></li>\n"
        "    <li class='nav-item nav-admin'><a href='/admin' class='nav-link"));
  if (activeItem.startsWith("admin")) sendChunk(server, F(" active"));
  sendChunk(server, F("'>Admin</a></li>\n"
                      "  </ul>\n"));

  // Second row: only if on admin page
  if (activeItem.startsWith("admin")) {
    sendChunk(server,
              F("  <ul class='nav-row nav-row-secondary'>\n"
                "    <li class='nav-item'><a href='/admin' class='nav-link"));
    if (activeItem == "admin") sendChunk(server, F(" active"));
    sendChunk(
        server,
        F("'>Einstellungen</a></li>\n"
          "    <li class='nav-item'><a href='/admin/sensors' class='nav-link"));
    if (activeItem == "admin/sensors") sendChunk(server, F(" active"));
    sendChunk(server, F("'>Sensoren</a></li>\n"));
#if USE_DISPLAY
    sendChunk(
        server,
        F("    <li class='nav-item'><a href='/admin/display' class='nav-link"));
    if (activeItem == "admin/display") sendChunk(server, F(" active"));
    sendChunk(server, F("'>Display</a></li>\n"));
#endif
    sendChunk(
        server,
        F("    <li class='nav-item'><a href='/admin/update' class='nav-link"));
    if (activeItem == "admin/update") sendChunk(server, F(" active"));
    sendChunk(server, F("'>OTA Update</a></li>\n"
                        "  </ul>\n"));
  }
  sendChunk(server, F("</nav>\n"));
}

void sendFooter(ESP8266WebServer& server, const String& version,
                const String& buildDate) {
  sendChunk(server, F("    <footer class='footer'>\n"
                      "        <div class='footer-content'>\n"
                      "            <p>Version: "));
  sendChunk(server, version);
  sendChunk(server, F(" ("));
  sendChunk(server,
            buildDate);  // Use buildDate directly since it's already formatted
  sendChunk(server, F(")</p>\n"
                      "        </div>\n"
                      "    </footer>\n"));
}

void endResponse(ESP8266WebServer& server,
                 const std::vector<String>& additionalScripts) {
  // Add each additional script
  for (const auto& script : additionalScripts) {
    if (!script.isEmpty()) {
      sendChunk(server, F("<script src='/js/"));
      sendChunk(server, script);
      sendChunk(server, F(".js'></script>"));
    }
  }

  sendChunk(server, F("</body></html>"));
  server.sendContent(F(""));  // Final empty chunk to signify end of response
}

void formGroup(ESP8266WebServer& server, const String& label,
               const String& content) {
  sendChunk(server, F("<div class='form-group'>"));
  sendChunk(server, F("<label>"));
  sendChunk(server, label);
  sendChunk(server, F("</label>"));
  sendChunk(server, content);
  sendChunk(server, F("</div>"));
}

void button(ESP8266WebServer& server, const String& text, const String& type,
            const String& className, bool disabled, const String& id) {
  sendChunk(server, F("<button type='"));
  sendChunk(server, type);
  sendChunk(server, F("' class='button "));
  sendChunk(server, className);
  sendChunk(server, F("'"));

  if (id.length() > 0) {
    sendChunk(server, F(" id='"));
    sendChunk(server, id);
    sendChunk(server, F("'"));
  }

  if (disabled) {
    sendChunk(server, F(" disabled"));
  }

  sendChunk(server, F(">"));
  sendChunk(server, text);
  sendChunk(server, F("</button>"));
}

}  // namespace Component
