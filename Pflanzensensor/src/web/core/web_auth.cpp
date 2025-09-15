/**
 * @file web_auth.cpp
 * @brief Implementation of authentication and authorization
 */

#include "web/core/web_auth.h"

#include <libb64/cdecode.h>

#include <algorithm>

#include "logger/logger.h"

WebAuth::WebAuth(ESP8266WebServer& server) : _server(server) {
  logger.debug(F("WebAuth"), F("Initialisiere WebAuth"));
}

String WebAuth::base64_decode(const String& input) {
  // Use stack allocation with reasonable max size
  const size_t MAX_DECODE_LENGTH = 128;
  char decoded[MAX_DECODE_LENGTH];

  base64_decodestate state;
  base64_init_decodestate(&state);

  // Ensure we don't overflow our buffer
  size_t expectedLength = base64_decode_expected_len(input.length());
  if (expectedLength >= MAX_DECODE_LENGTH) {
    logger.error(F("WebAuth"), F("Base64-Eingabe zu lang"));
    return String();
  }

  size_t len =
      base64_decode_block(input.c_str(), input.length(), decoded, &state);
  decoded[len] = '\0';

  return String(decoded);
}

bool WebAuth::authenticate(UserRole requiredRole) {
  if (_server.hasHeader("Authorization")) {
    String authHeader = _server.header("Authorization");

    // Basic Auth Format: "Basic base64(username:password)"
    if (authHeader.startsWith("Basic ")) {
      String encodedAuth = authHeader.substring(6);
      String decodedAuth = base64_decode(encodedAuth);

      int colonIndex = decodedAuth.indexOf(':');
      if (colonIndex > 0) {
        String username = decodedAuth.substring(0, colonIndex);
        String password = decodedAuth.substring(colonIndex + 1);

        // Check credentials
        if (username == "admin" && password == ConfigMgr.getAdminPassword()) {
          return true;
        }
      }
    }
  }

  // Auth failed, request credentials
  _server.sendHeader("WWW-Authenticate", "Basic realm=\"Login Required\"");
  _server.send(401, "text/plain", "Authentifizierung erforderlich");
  return false;
}

void WebAuth::setCredentials(const String& username, const String& password,
                             UserRole role) {
  _credentials[username] = password;
  _roles[username] = role;
  logger.debug(F("WebAuth"), String(F("Zugangsdaten gesetzt für Benutzer: ")) + username);
}

String WebAuth::createSession(const String& username, UserRole role) {
  cleanupSessions();

  // Check session limit
  if (_sessions.size() >= MAX_SESSIONS) {
    // Remove oldest session
    auto oldestSession = std::min_element(
        _sessions.begin(), _sessions.end(), [](const auto& a, const auto& b) {
          return a.second.lastAccess < b.second.lastAccess;
        });
    _sessions.erase(oldestSession);
  }

  String token = generateToken();
  SessionInfo session;
  session.username = username;
  session.role = role;
  session.lastAccess = millis();
  session.token = token;

  _sessions[token] = session;
  return token;
}

bool WebAuth::validateSession(const String& token) {
  auto it = _sessions.find(token);
  if (it == _sessions.end()) {
    return false;
  }

  unsigned long now = millis();
  if (now - it->second.lastAccess > SESSION_TIMEOUT) {
    _sessions.erase(it);
    return false;
  }

  it->second.lastAccess = now;
  return true;
}

void WebAuth::cleanupSessions() {
  unsigned long now = millis();
  auto it = _sessions.begin();

  while (it != _sessions.end()) {
    if (now - it->second.lastAccess > SESSION_TIMEOUT) {
      logger.debug(F("WebAuth"),
                   String(F("Entferne abgelaufene Sitzung für Benutzer: ")) + it->second.username);
      it = _sessions.erase(it);
    } else {
      ++it;
    }
  }
}

bool WebAuth::checkBasicAuth(const String& username, const String& password) {
  // Check if username exists in credentials
  auto it = _credentials.find(username);
  if (it == _credentials.end()) {
    logger.warning(F("WebAuth"),
                   String(F("Authentifizierung fehlgeschlagen: unbekannter Benutzer '")) + username + "'");
    return false;
  }

  // Compare passwords
  if (it->second != password) {
  logger.warning(
    F("WebAuth"),
    String(F("Authentifizierung fehlgeschlagen: ungültiges Passwort für Benutzer '")) + username + "'");
    return false;
  }

  return true;
}

AuthType WebAuth::getAuthType() {
  if (_server.hasHeader("X-Auth-Token")) {
    return AuthType::TOKEN;
  }
  return AuthType::BASIC;
}

void WebAuth::requestAuth() {
  _server.sendHeader("WWW-Authenticate", "Basic realm=\"Login Required\"");
  _server.send(401, "text/plain", "Authentifizierung erforderlich");
}

bool WebAuth::checkTokenAuth(const String& token) {
  return validateSession(token);
}

void WebAuth::logAuthAttempt(const String& username, bool success) {
  logger.info(F("WebAuth"), String(F("Auth-Versuch für Benutzer '")) + username +
                                String(F("': ")) + (success ? String(F("Erfolg")) : String(F("Fehler"))));
}
