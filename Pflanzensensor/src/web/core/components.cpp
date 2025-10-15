/**
 * @file components.cpp
 * @brief Implementation of web components and HTML utilities
 */

#include "web/core/components.h"

#include <algorithm>
#include <ESP8266WiFi.h>

#include "logger/logger.h"
#include "utils/helper.h"

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
  // Navigation wird im Footer angezeigt (siehe sendPixelatedFooter)
  // Diese Funktion wird nicht mehr genutzt, bleibt aber für Kompatibilität
}

void sendFooter(ESP8266WebServer& server, const String& version,
                const String& buildDate) {
  // Footer wird mit sendPixelatedFooter erstellt
  // Diese Funktion wird nicht mehr genutzt, bleibt aber für Kompatibilität
}

void sendPixelatedFooter(ESP8266WebServer& server, const String& version,
                         const String& buildDate, const String& activeSection) {
  sendChunk(server, F("<div class='footer'>"));
  sendChunk(server, F("<div class='base'>"));

  // Earth image
  sendChunk(server, F("<img class='earth' src='/img/earth.png' alt='Earth' />"));

  // Base overlay with navigation and stats
  sendChunk(server, F("<footer class='base-overlay' aria-label='Statusleiste'>"));
  sendChunk(server, F("<div class='footer-grid'>"));

  // Navigation (Row 1, Column 1)
  sendChunk(server, F("<nav class='nav-box' aria-label='Navigation'><ul class='nav-list'>"));

  // Main navigation
  sendChunk(server, F("<li><a href='/' class='nav-item"));
  if (activeSection == "start" || activeSection == "/" || activeSection == "") sendChunk(server, F(" active"));
  sendChunk(server, F("'>START</a></li>"));

  sendChunk(server, F("<li><a href='/logs' class='nav-item"));
  if (activeSection == "logs") sendChunk(server, F(" active"));
  sendChunk(server, F("'>LOGS</a></li>"));

  sendChunk(server, F("<li><a href='/admin' class='nav-item"));
  if (activeSection.startsWith("admin")) sendChunk(server, F(" active"));
  sendChunk(server, F("'>ADMIN</a></li>"));

  sendChunk(server, F("</ul></nav>"));

  // Stats Labels (Row 1, Column 2)
  sendChunk(server, F("<ul class='stats-labels'>"));

  if (activeSection.startsWith("admin")) {
    // Admin submenu - only highlight exact match
    sendChunk(server, F("<li><a href='/admin' class='nav-item"));
    if (activeSection == "admin") sendChunk(server, F(" active"));  // Only /admin, not /admin/xyz
    sendChunk(server, F("'>Einstellungen</a></li>"));

    sendChunk(server, F("<li><a href='/admin/sensors' class='nav-item"));
    if (activeSection == "admin/sensors") sendChunk(server, F(" active"));
    sendChunk(server, F("'>Sensoren</a></li>"));

#if USE_DISPLAY
    sendChunk(server, F("<li><a href='/admin/display' class='nav-item"));
    if (activeSection == "admin/display") sendChunk(server, F(" active"));
    sendChunk(server, F("'>Display</a></li>"));
#endif

    sendChunk(server, F("<li><a href='/admin/update' class='nav-item"));
    if (activeSection == "admin/update") sendChunk(server, F(" active"));
    sendChunk(server, F("'>OTA Update</a></li>"));
  } else {
    // System info for non-admin pages
    sendChunk(server, F("<li>📅 Zeit</li>"));
    sendChunk(server, F("<li>🌐 SSID</li>"));
    sendChunk(server, F("<li>💻 IP</li>"));
    sendChunk(server, F("<li>📶 WIFI</li>"));
    sendChunk(server, F("<li>⏲️ UPTIME</li>"));
    sendChunk(server, F("<li>🔄 RESTARTS</li>"));
  }
  sendChunk(server, F("</ul>"));

  // Stats Values (Row 1, Column 3) - only for non-admin pages
  if (!activeSection.startsWith("admin")) {
    sendChunk(server, F("<ul class='stats-values'>"));
    sendChunk(server, F("<li>"));

    sendChunk(server, Helper::getFormattedDate());
    sendChunk(server, F(" "));
    sendChunk(server, Helper::getFormattedTime());

    sendChunk(server, F("</li><li>"));
    sendChunk(server, WiFi.SSID());
    sendChunk(server, F("</li><li>"));
    sendChunk(server, WiFi.localIP().toString());
    sendChunk(server, F("</li><li>"));
    sendChunk(server, String(WiFi.RSSI()));
    sendChunk(server, F(" dBm"));
    sendChunk(server, F("</li><li>"));
    sendChunk(server, Helper::getFormattedUptime());
    sendChunk(server, F("</li><li>"));
    sendChunk(server, String(Helper::getRebootCount()));
    sendChunk(server, F("</li></ul>"));
  } else {
    sendChunk(server, F("<ul class='stats-values'></ul>"));
  }

  // Logo (Row 2, Column 1)
  sendChunk(server, F("<div class='footer-logo'><img src='/img/fabmobil.png' alt='FABMOBIL' /></div>"));

  // Version (Row 2, Column 2)
  sendChunk(server, F("<div class='footer-version'>V "));
  sendChunk(server, version);
  sendChunk(server, F("</div>"));

  // Build (Row 2, Column 3)
  sendChunk(server, F("<div class='footer-build'>BUILD: "));
  sendChunk(server, buildDate);
  sendChunk(server, F("</div>"));

  sendChunk(server, F("</div>"));  // Close footer-grid
  sendChunk(server, F("</footer>"));  // Close base-overlay
  sendChunk(server, F("</div>"));  // Close base
  sendChunk(server, F("</div>"));  // Close footer
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

void beginPixelatedPage(ESP8266WebServer& server, const String& statusClass) {
  sendChunk(server, F("<div class='box "));
  sendChunk(server, statusClass);
  sendChunk(server, F("'><div class='group'>"));
}

void sendCloudTitle(ESP8266WebServer& server, const String& title) {
  sendChunk(server, F("<div class='cloud' aria-label='"));
  sendChunk(server, title);
  sendChunk(server, F("'>"));
  sendChunk(server, F("<img class='cloud-img' src='/img/cloud_big.png' alt='' />"));
  sendChunk(server, F("<div class='cloud-label'>"));
  sendChunk(server, title);
  sendChunk(server, F("</div></div>"));
}

void beginContentBox(ESP8266WebServer& server, const String& section) {
  sendChunk(server, F("<div class='admin-content-box'"));
  if (!section.isEmpty()) {
    sendChunk(server, F(" data-section='"));
    sendChunk(server, section);
    sendChunk(server, F("'"));
  }
  sendChunk(server, F(">"));
}

void endContentBox(ESP8266WebServer& server) {
  sendChunk(server, F("</div>"));
}

void endPixelatedPage(ESP8266WebServer& server) {
  sendChunk(server, F("</div></div>"));  // Close group and box
}

}  // namespace Component
