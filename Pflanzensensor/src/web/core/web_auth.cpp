/**
 * @file web_auth.cpp
 * @brief Implementation of authentication and authorization
 */

#include "web/core/web_auth.h"

#include <libb64/cdecode.h>

#include "logger/logger.h"

WebAuth::WebAuth(ESPWebServer& server) : _server(server) {
  LOG_DEBUG(F("WebAuth"), F("Initialisiere WebAuth"));
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
    LOG_ERROR(F("WebAuth"), F("Base64-Eingabe zu lang"));
    return String();
  }

  size_t len = base64_decode_block(input.c_str(), input.length(), decoded, &state);
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
