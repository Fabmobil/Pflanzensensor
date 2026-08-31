/**
 * @file components.cpp
 * @brief Implementation of web components and HTML utilities
 */

#include "web/core/components.h"

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <algorithm>

#include "logger/logger.h"
#include "utils/helper.h"

namespace Component {

// Return the appropriate IP to display in the web UI.
// If the device is running an AP (or AP+STA) return the softAP IP,
// otherwise return the station local IP. If no IP is assigned, return
// a localized placeholder.
String getDisplayIP() {
  IPAddress ip;
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    ip = WiFi.softAPIP();
  } else {
    ip = WiFi.localIP();
  }
  String ipStr = ip.toString();
  if (ipStr == "0.0.0.0")
    return String(F("(IP nicht gesetzt)"));
  return ipStr;
}

String getDisplaySSID() {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    String ssid = WiFi.softAPSSID();
    if (ssid.length() == 0)
      return String(F("(AP SSID unbekannt)"));
    return ssid;
  }
  String ssid = WiFi.SSID();
  if (ssid.length() == 0)
    return String(F("(SSID unbekannt)"));
  return ssid;
}

ResourceResult beginResponse(ESPWebServer& server, const String& title,
                             const std::vector<String>& additionalCss) {
  static const char TEXT_HTML[] PROGMEM = "text/html";
  static const char CACHE_CONTROL[] PROGMEM = "Cache-Control";
  static const char NO_CACHE[] PROGMEM = "no-cache";

  // Check memory before starting
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < SAFE_HEAP_SIZE) {
    server.send(503, F("text/plain"), F("Unzureichender Speicher, bitte später erneut versuchen"));
    return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY,
                                F("Unzureichender Speicher für HTML-Antwort"));
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  // Weder Content-Type noch Connection hier per sendHeader() setzen:
  // Content-Type setzt send() unten, Connection verwaltet der Webserver selbst.
  // Vorher lieferte jede Seite beide Header doppelt und dabei sogar
  // widersprüchlich aus:
  //   Content-Type: text/html
  //   Content-Type: text/html
  //   Connection: close
  //   Connection: keep-alive
  server.sendHeader(FPSTR(CACHE_CONTROL), FPSTR(NO_CACHE));
  server.send(200, FPSTR(TEXT_HTML), F(""));

  // Send initial HTML
  sendChunk(server, F("<!DOCTYPE html><html lang='de'><head>"
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

// sendChunk() ist die mit Abstand meistgenutzte Funktion im Web-Layer
// (über 900 Aufrufstellen). Die frühere Implementierung nahm ausschließlich
// einen const String& entgegen und kostete pro Aufruf drei Heap-Allokationen:
//
//   1. sendChunk(server, F("...")) konvertierte das Flash-Literal implizit in
//      einen Heap-String - die F()-Ersparnis war damit zur Laufzeit wieder weg
//   2. chunk.substring() legte pro 127-Byte-Block einen weiteren String an
//      (nur um in einen statischen 128-Byte-Puffer zu kopieren)
//   3. server.sendContent(const char*) baute daraus erneut einen String
//
// Beim Aufbau einer Admin-Seite ergab das mehrere hundert kurzlebige
// Allokationen - der Hauptgrund für die Heap-Fragmentierung.
//
// Die Überladungen greifen automatisch für alle bestehenden Aufrufstellen:
// F("...") trifft jetzt die Flash-Variante (ganz ohne Heap), Zeichenketten-
// Literale die const char*-Variante.

void sendChunk(ESPWebServer& server, const __FlashStringHelper* chunk) {
  if (!chunk) {
    return;
  }
  PGM_P p = reinterpret_cast<PGM_P>(chunk);
  size_t len = strlen_P(p);
  if (len > 0) {
    server.sendContent_P(p, len); // direkt aus dem Flash, keine Allokation
  }
  optimistic_yield(1000);
}

void sendChunk(ESPWebServer& server, const char* chunk) {
  if (!chunk || chunk[0] == '\0') {
    return;
  }
  server.sendContent(chunk, strlen(chunk));
  optimistic_yield(1000);
}

void sendChunk(ESPWebServer& server, const String& chunk) {
  if (chunk.length() > 0) {
    server.sendContent(chunk.c_str(), chunk.length());
  }
  optimistic_yield(1000);
}

void sendPixelatedFooter(ESPWebServer& server, const String& version, const String& buildDate,
                         const String& activeSection) {
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
  sendChunk(server, F("<li><a href='/' id='nav-start' class='nav-item"));
  if (activeSection == "start" || activeSection == "/" || activeSection == "")
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>START</a></li>"));

  sendChunk(server, F("<li><a href='/chronik' id='nav-chronik' class='nav-item"));
  if (activeSection == "chronik")
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>CHRONIK</a></li>"));

  sendChunk(server, F("<li><a href='/logs' id='nav-logs' class='nav-item"));
  if (activeSection == "logs")
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>LOGS</a></li>"));

  sendChunk(server, F("<li><a href='/admin' id='nav-admin' class='nav-item"));
  if (activeSection.startsWith("admin"))
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>ADMIN</a></li>"));

  sendChunk(server, F("</ul></nav>"));

  // Stats Table (labels and values in one row for perfect vertical alignment)
  sendChunk(server, F("<div class='stats-table' id='footer-stats-table'"));
  if (activeSection.startsWith("admin")) {
    sendChunk(server, F(" style='display:none'"));
  }
  sendChunk(server, F(">"));
  // Row 1: Zeit
  sendChunk(server, F("<div class='stats-row'><span class='stats-label'>📅 Zeit</span><span "
                      "class='stats-value'>"));
  sendChunk(server, Helper::getFormattedDate());
  sendChunk(server, F(" "));
  sendChunk(server, Helper::getFormattedTime());
  sendChunk(server, F("</span></div>"));
  // Row 2: SSID
  sendChunk(server, F("<div class='stats-row'><span class='stats-label'>🌐 SSID</span><span "
                      "class='stats-value'>"));
  sendChunk(server, getDisplaySSID());
  sendChunk(server, F("</span></div>"));
  // Row 3: IP
  sendChunk(
      server,
      F("<div class='stats-row'><span class='stats-label'>💻 IP</span><span class='stats-value'>"));
  sendChunk(server, getDisplayIP());
  sendChunk(server, F("</span></div>"));
  // Row 4: WIFI
  sendChunk(server, F("<div class='stats-row'><span class='stats-label'>📶 WIFI</span><span "
                      "class='stats-value'>"));
  sendChunk(server, String(WiFi.RSSI()));
  sendChunk(server, F(" dBm</span></div>"));
  // Row 5: UPTIME
  sendChunk(server, F("<div class='stats-row'><span class='stats-label'>⏲️ UPTIME</span><span "
                      "class='stats-value'>"));
  sendChunk(server, Helper::getFormattedUptime());
  sendChunk(server, F("</span></div>"));
  // Row 6: RESTARTS
  sendChunk(server, F("<div class='stats-row'><span class='stats-label'>🔄 RESTARTS</span><span "
                      "class='stats-value'>"));
  sendChunk(server, String(Helper::getRebootCount()));
  sendChunk(server, F("</span></div>"));
  sendChunk(server, F("</div>"));

  // Admin submenu (now as stats-table, hidden on non-admin pages)
  sendChunk(server, F("<div class='stats-table' id='footer-admin-menu'"));
  // Only hide by default if not on start page and not on admin page
  if (!(activeSection == "admin" || activeSection.startsWith("admin/") || activeSection == "/" ||
        activeSection == "" || activeSection == "/index.html")) {
    sendChunk(server, F(" style='display:none'"));
  }
  sendChunk(server, F(">"));
  // Einstellungen
  sendChunk(server, F("<div class='stats-row'>"));
  sendChunk(server, F("<span class='stats-label'>"));
  sendChunk(server, F("<a href='/admin' class='nav-item"));
  if (activeSection == "admin")
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>Einstellungen</a>"));
  sendChunk(server, F("</span><span class='stats-value'>&nbsp;</span></div>"));
  // Sensoren
  sendChunk(server, F("<div class='stats-row'>"));
  sendChunk(server, F("<span class='stats-label'>"));
  sendChunk(server, F("<a href='/admin/sensors' class='nav-item"));
  if (activeSection == "admin/sensors")
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>Sensoren</a>"));
  sendChunk(server, F("</span><span class='stats-value'>&nbsp;</span></div>"));
#if USE_DISPLAY
  sendChunk(server, F("<div class='stats-row'>"));
  sendChunk(server, F("<span class='stats-label'>"));
  sendChunk(server, F("<a href='/admin/display' class='nav-item"));
  if (activeSection == "admin/display")
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>Display</a>"));
  sendChunk(server, F("</span><span class='stats-value'>&nbsp;</span></div>"));
#endif
  sendChunk(server, F("<div class='stats-row'>"));
  sendChunk(server, F("<span class='stats-label'>"));
  sendChunk(server, F("<a href='/admin/update' class='nav-item"));
  if (activeSection == "admin/update")
    sendChunk(server, F(" active"));
  sendChunk(server, F("'>Update</a>"));
  sendChunk(server, F("</span><span class='stats-value'>&nbsp;</span></div>"));
  sendChunk(server, F("</div>"));

  // Admin values placeholder (empty for now) - visible only when admin menu is active
  sendChunk(server, F("<ul class='stats-values' id='footer-admin-values'"));
  if (!activeSection.startsWith("admin"))
    sendChunk(server, F(" style='display:none'"));
  sendChunk(server, F("></ul>"));

  // Logo (Row 2, Column 1)
  sendChunk(server, F("<div class='footer-logo'><a href='https://www.fabmobil.org' "
                      "target='_blank'><img src='/img/fabmobil.png' alt='FABMOBIL' /></a></div>"));

  // Version (Row 2, Column 2)
  sendChunk(server, F("<div class='footer-version'>V "));
  sendChunk(server, version);
  sendChunk(server, F("</div>"));

  // Build (Row 2, Column 3)
  sendChunk(server, F("<div class='footer-build'>BUILD: "));
  sendChunk(server, buildDate);
  sendChunk(server, F("</div>"));

  sendChunk(server, F("</div>"));    // Close footer-grid
  sendChunk(server, F("</footer>")); // Close base-overlay
  sendChunk(server, F("</div>"));    // Close base
  sendChunk(server, F("</div>"));    // Close footer
}

void endResponse(ESPWebServer& server, const std::vector<String>& additionalScripts) {
  // Add each additional script
  for (const auto& script : additionalScripts) {
    if (!script.isEmpty()) {
      sendChunk(server, F("<script src='/js/"));
      sendChunk(server, script);
      sendChunk(server, F(".js'></script>"));
    }
  }

  sendChunk(server, F("</body></html>"));
  server.sendContent(F("")); // Final empty chunk to signify end of response
}

void formGroup(ESPWebServer& server, const String& label, const String& content) {
  sendChunk(server, F("<div class='form-group'>"));
  sendChunk(server, F("<label>"));
  sendChunk(server, label);
  sendChunk(server, F("</label>"));
  sendChunk(server, content);
  sendChunk(server, F("</div>"));
}

void button(ESPWebServer& server, const String& text, const String& type, const String& className,
            bool disabled, const String& id) {
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

void beginPixelatedPage(ESPWebServer& server, const String& statusClass) {
  sendChunk(server, F("<div class='box "));
  sendChunk(server, statusClass);
  sendChunk(server, F("'><div class='group'>"));
}

void sendCloudTitle(ESPWebServer& server, const String& title) {
  sendChunk(server, F("<div class='cloud' aria-label='"));
  sendChunk(server, title);
  sendChunk(server, F("'>"));
  sendChunk(server, F("<img class='cloud-img' src='/img/cloud_big.png' alt='' />"));
  sendChunk(server, F("<div class='cloud-label'>"));
  sendChunk(server, title);
  sendChunk(server, F("</div></div>"));
}

void beginContentBox(ESPWebServer& server, const String& section) {
  sendChunk(server, F("<div class='admin-content-box'"));
  if (!section.isEmpty()) {
    sendChunk(server, F(" data-section='"));
    sendChunk(server, section);
    sendChunk(server, F("'"));
  }
  sendChunk(server, F(">"));
}

void endContentBox(ESPWebServer& server) { sendChunk(server, F("</div>")); }

void endPixelatedPage(ESPWebServer& server) {
  sendChunk(server, F("</div></div>")); // Close group and box
}

} // namespace Component
