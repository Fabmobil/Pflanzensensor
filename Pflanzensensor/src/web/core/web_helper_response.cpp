/**
 * @file web_helper_response.cpp
 * @brief Implementierung der Response-Builder-Klasse
 */

#include "web/core/web_helper_response.h"

// === Erfolgreiche Antworten ===

String ResponseBuilder::ok() { return String(F("{\"success\":true}")); }

String ResponseBuilder::ok(const String& message) {
  return String(F("{\"success\":true,\"message\":\"")) + message + F("\"}");
}

String ResponseBuilder::ok(const String& key, const String& value) {
  return String(F("{\"success\":true,\"")) + key + F("\":\"") + value + F("\"}");
}

// === Fehler-Antworten ===

String ResponseBuilder::error() { return String(F("{\"success\":false}")); }

String ResponseBuilder::error(const String& message) {
  return String(F("{\"success\":false,\"error\":\"")) + message + F("\"}");
}

String ResponseBuilder::error(int code, const String& message) {
  return String(F("{\"success\":false,\"error\":\"")) + message + F("\",\"code\":") + String(code) +
         String(F("}"));
}

// === Spezielle Antworten ===

String ResponseBuilder::notFound(const String& resource) {
  return error(404, resource + F(" not found"));
}

String ResponseBuilder::invalidInput(const String& field) {
  return error(400, String(F("Invalid ")) + field);
}

String ResponseBuilder::unauthorized() { return error(401, F("Unauthorized")); }

String ResponseBuilder::rateLimited() { return error(429, F("Rate limit exceeded")); }

// === Daten-Formatierung ===

String ResponseBuilder::dataPair(const String& key, const String& value) {
  return String(F("\"")) + key + F("\":\"") + value + F("\"");
}

String ResponseBuilder::dataPair(const String& key, int value) {
  return String(F("\"")) + key + F("\":") + String(value);
}

String ResponseBuilder::dataPair(const String& key, bool value) {
  return String(F("\"")) + key + F("\":") + (value ? F("true") : F("false"));
}