/**
 * @file web_auth.cpp
 * @brief Implementation of authentication and authorization
 */

#include "web/core/web_auth.h"

#include "logger/logger.h"

WebAuth::WebAuth(ESPWebServer& server) : _server(server) {
  LOG_DEBUG(F("WebAuth"), F("Initialisiere WebAuth"));
}

bool WebAuth::checkAdminCredentials(ESPWebServer& server) {
  // Regulärer Weg: das eingestellte Adminpasswort
  if (server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    return true;
  }

  // Rückfallebene: das fest eincompilierte Notfallpasswort. Gedacht für den
  // Fall, dass das gesetzte Adminpasswort vergessen wurde.
  if (server.authenticate("admin", EMERGENCY_ADMIN_PASSWORD)) {
    // Gedrosselt protokollieren. Die Prüfung läuft pro Anfrage teils zweifach
    // (Middleware und zusätzlich validateRequest im Handler), und beim Bedienen
    // der Oberfläche entstehen viele Anfragen - ungedrosselt würde die Warnung
    // das Log fluten und dabei ihre Signalwirkung verlieren.
    static unsigned long lastWarn = 0;
    unsigned long now = millis();
    if (lastWarn == 0 || now - lastWarn >= EMERGENCY_WARN_INTERVAL_MS) {
      lastWarn = now;
      LOG_WARN(F("WebAuth"), F("Zugriff über das Notfallpasswort - bitte in den "
                               "Einstellungen ein neues Adminpasswort vergeben"));
    }
    return true;
  }

  return false;
}

bool WebAuth::authenticate(UserRole requiredRole) {
  if (checkAdminCredentials(_server)) {
    return true;
  }

  // Auth failed, request credentials
  _server.sendHeader("WWW-Authenticate", "Basic realm=\"Login Required\"");
  _server.send(401, "text/plain", "Authentifizierung erforderlich");
  return false;
}
