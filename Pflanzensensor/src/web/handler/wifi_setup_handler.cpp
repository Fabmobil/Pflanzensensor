/**
 * @file wifi_setup_handler.cpp
 * @brief Implementation of WiFi setup and captive portal handler
 */

#include "web/handler/wifi_setup_handler.h"

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/wifi.h"
#include "web/core/components.h"

RouterResult WiFiSetupHandler::onRegisterRoutes(WebRouter& router) {
  logger.debug(F("WiFiSetupHandler"), F("Registriere WiFi-Einrichtungsrouten"));

  // Register WiFi update endpoint (GET route removed - form is now integrated
  // in startpage)
  auto result = router.addRoute(HTTP_POST, "/admin/updateWiFi", [this]() {
  logger.debug(F("WiFiSetupHandler"), F("POST /admin/updateWiFi aufgerufen"));
    handleWiFiUpdate();
  });
  if (!result.isSuccess()) {
  logger.error(
    F("WiFiSetupHandler"),
    "Registrierung POST /admin/updateWiFi fehlgeschlagen: " + result.getMessage());
    return result;
  }

  logger.info(F("WiFiSetupHandler"), F("WiFi-Einrichtungsrouten registriert"));
  return RouterResult::success();
}

HandlerResult WiFiSetupHandler::handlePost(
    const String& uri, const std::map<String, String>& params) {
  if (uri == "/admin/updateWiFi") {
    handleWiFiUpdate();
    return HandlerResult::success();
  }
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unbekannter Endpunkt");
}

void WiFiSetupHandler::handleWiFiUpdate() {
  logger.debug(F("WiFiSetupHandler"), F("Verarbeite WiFi-Aktualisierungsanfrage"));

  // Validate required parameters
  if (!_server.hasArg("wifi_slot") || !_server.hasArg("wifi_ssid") ||
      !_server.hasArg("wifi_password")) {
    logger.error(F("WiFiSetupHandler"), F("Fehlende erforderliche Parameter"));
    _server.send(400, F("text/plain"), F("Fehlende Parameter"));
    return;
  }

  int slot = _server.arg("wifi_slot").toInt();
  String ssid = _server.arg("wifi_ssid");
  String password = _server.arg("wifi_password");

  logger.info(F("WiFiSetupHandler"), F("Aktualisiere WiFi-Zugangsdaten - Slot: ") +
                                         String(slot) + F(", SSID: ") + ssid);

  // Validate slot number
  if (slot < 1 || slot > 3) {
    logger.error(F("WiFiSetupHandler"),
                 F("Ungültige Slot-Nummer: ") + String(slot));
    _server.send(400, F("text/plain"), F("Ungültiger Slot"));
    return;
  }

  // Validate credentials
  if (!validateCredentials(ssid, password)) {
    logger.error(F("WiFiSetupHandler"), F("Ungültige Zugangsdaten"));
    _server.send(400, F("text/plain"),
                 F("Ungültige SSID oder Passwort (zu kurz/lang)"));
    return;
  }

  // Update configuration based on slot
  bool updated = false;
  switch (slot) {
    case 1:
      ConfigMgr.setWiFiSSID1(ssid);
      ConfigMgr.setWiFiPassword1(password);
      updated = true;
      break;
    case 2:
      ConfigMgr.setWiFiSSID2(ssid);
      ConfigMgr.setWiFiPassword2(password);
      updated = true;
      break;
    case 3:
      ConfigMgr.setWiFiSSID3(ssid);
      ConfigMgr.setWiFiPassword3(password);
      updated = true;
      break;
  }

  if (!updated) {
    logger.error(F("WiFiSetupHandler"), F("Aktualisierung der Zugangsdaten fehlgeschlagen"));
    _server.send(500, F("text/plain"),
                 F("Konfiguration konnte nicht gespeichert werden"));
    return;
  }

  // Save configuration
  auto saveResult = ConfigMgr.saveConfig();
  if (!saveResult.isSuccess()) {
    logger.error(F("WiFiSetupHandler"),
                 F("Konfiguration konnte nicht gespeichert werden: ") + saveResult.getMessage());
    _server.send(500, F("text/plain"),
                 F("Konfiguration konnte nicht gespeichert werden"));
    return;
  }

  // Send success response with redirect to startpage
  _server.sendHeader("Location", "/", true);
  _server.send(302, F("text/plain"), F("WiFi gespeichert. Neustart..."));

  // Give response time to be sent
  delay(500);

  logger.info(F("WiFiSetupHandler"),
              F("WiFi-Zugangsdaten aktualisiert, starte neu..."));

  // Try to connect with all available credentials
  tryAllWiFiCredentials();

  // Restart device
  ESP.restart();
}

String WiFiSetupHandler::generateNetworkSelection() {
  String html = F("<select name='ssid' id='ssid' required>");

  // Scan for networks
  logger.debug(F("WiFiSetupHandler"), F("Scanne nach WiFi-Netzwerken..."));
  int networkCount = WiFi.scanNetworks();

  if (networkCount == 0) {
  html += F("<option value=''>Keine Netzwerke gefunden</option>");
  logger.warning(F("WiFiSetupHandler"), F("Keine WiFi-Netzwerke gefunden"));
  } else if (networkCount > 0) {
  logger.info(F("WiFiSetupHandler"),
        F("Gefunden: ") + String(networkCount) + F(" WiFi-Netzwerke"));

    // Add networks to dropdown
    for (int i = 0; i < networkCount && i < 20; ++i) {  // Limit to 20 networks
      String networkSSID = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      uint8_t encType = WiFi.encryptionType(i);

      // Skip empty SSIDs
      if (networkSSID.length() == 0) continue;

      html += F("<option value='");
      html += networkSSID;
      html += F("'>");
      html += networkSSID;
      html += F(" (");
      html += formatSignalStrength(rssi);
      html += F(", ");
      html += (encType == ENC_TYPE_NONE) ? F("offen") : F("verschlüsselt");
      html += F(")</option>");
    }
  } else {
  html += F("<option value=''>Scan-Fehler</option>");
  logger.error(F("WiFiSetupHandler"), F("WiFi-Scan fehlgeschlagen"));
  }

  html += F("</select>");
  return html;
}

int WiFiSetupHandler::getActiveWiFiSlot() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;  // Keine aktive Verbindung
  }

  String currentSSID = WiFi.SSID();

  if (currentSSID == ConfigMgr.getWiFiSSID1()) return 1;
  if (currentSSID == ConfigMgr.getWiFiSSID2()) return 2;
  if (currentSSID == ConfigMgr.getWiFiSSID3()) return 3;

  return 0;  // Connected but not to a configured network
}

bool WiFiSetupHandler::validateCredentials(const String& ssid,
                                           const String& password) {
  // SSID validation
  if (ssid.length() == 0 || ssid.length() > 32) {
    logger.warning(F("WiFiSetupHandler"),
                   F("Invalid SSID length: ") + String(ssid.length()));
    return false;
  }

  // Password validation (WPA requires at least 8 characters, max 64)
  if (password.length() > 0 &&
      (password.length() < 8 || password.length() > 64)) {
    logger.warning(F("WiFiSetupHandler"),
                   F("Invalid password length: ") + String(password.length()));
    return false;
  }

  return true;
}

bool WiFiSetupHandler::testConnection(const String& ssid,
                                      const String& password) {
  logger.debug(F("WiFiSetupHandler"), F("Testing connection to: ") + ssid);

  // Save current WiFi state
  String originalSSID = WiFi.SSID();
  bool wasConnected = (WiFi.status() == WL_CONNECTED);

  // Attempt connection
  WiFi.begin(ssid.c_str(), password.c_str());

  // Wait for connection with timeout
  unsigned long startTime = millis();
  const unsigned long timeout = 10000;  // 10 seconds

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
    delay(100);
    ESP.wdtFeed();
  }

  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected) {
    logger.info(F("WiFiSetupHandler"), F("Test connection successful"));
  } else {
    logger.warning(F("WiFiSetupHandler"), F("Test connection failed"));

    // Restore original connection if possible
    if (wasConnected && originalSSID.length() > 0) {
      // Try to reconnect to original network
      // This is a simplified restoration - in practice, you'd need
      // to find and use the original password
      logger.debug(F("WiFiSetupHandler"),
                   F("Attempting to restore connection"));
    }
  }

  return connected;
}

String WiFiSetupHandler::formatSignalStrength(int32_t rssi) {
  if (rssi > -50) return F("Ausgezeichnet");
  if (rssi > -60) return F("Gut");
  if (rssi > -70) return F("Mäßig");
  return F("Schwach");
}

bool WiFiSetupHandler::isCaptivePortalMode() {
  // Check if we're in AP mode or have no WiFi connection
  return (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA ||
          WiFi.status() != WL_CONNECTED);
}
