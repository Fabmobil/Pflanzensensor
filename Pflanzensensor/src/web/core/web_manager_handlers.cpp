/**
 * @file web_manager_handlers.cpp
 * @brief WebManager request handling and processing
 */

#include <ArduinoJson.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::handleSetUpdate() {
  LOG_DEBUG(F("WebManager"), F("Betrete WebManager::handleSetUpdate()"));

  // 1. Verify server instance
  if (!_server) {
    LOG_ERROR(F("WebManager"), F("Serverinstanz ist null"));
    return;
  }

  // 2. Basic auth check with detailed logging
  LOG_DEBUG(F("WebManager"), F("Prüfe Authentifizierung..."));
  if (!_server->authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    LOG_WARN(F("WebManager"), F("Authentifizierung für setUpdate-Anfrage fehlgeschlagen"));
    _server->requestAuthentication();
    return;
  }
  LOG_DEBUG(F("WebManager"), F("Authentifizierung erfolgreich"));

  // 3. Verify request method
  if (_server->method() != HTTP_POST) {
    LOG_WARN(F("WebManager"), F("Ungültige Methode für setUpdate"));
    sendErrorResponse(405, F("Methode nicht erlaubt"));
    return;
  }

  // 4. Get and validate request body
  String json = _server->arg("plain");
  LOG_DEBUG(F("WebManager"), "Empfangene Länge des Update-Request-Bodys: " + String(json.length()));
  LOG_DEBUG(F("WebManager"), "Roher Request-Body: " + json);

  // 5. Validate request and extract flags
  bool fileSystemUpdate, firmwareUpdate, updateMode;
  auto validationResult = validateUpdateRequest(json, fileSystemUpdate, firmwareUpdate, updateMode);
  if (!validationResult.isSuccess()) {
    return;
  }

  // 6. Log the intended update type
  LOG_DEBUG(F("WebManager"), String(F("Setze Flags - FS: ")) + String(fileSystemUpdate) +
                                 String(F(", FW: ")) + String(firmwareUpdate) +
                                 String(F(", Modus: ")) + String(updateMode));

  // 7. Save configuration and prepare for update
  if (!prepareUpdateMode(fileSystemUpdate, firmwareUpdate, updateMode)) {
    return;
  }

  // 8. Send success response before potential reboot
  StaticJsonDocument<200> response;
  response["status"] = "OK";
  String jsonResponse;
  serializeJson(response, jsonResponse);

  LOG_DEBUG(F("WebManager"), F("Sende Erfolgsantwort"));
  _server->send(200, F("application/json"), jsonResponse);
  // Try to flush TCP buffers and then close the client socket so the
  // browser receives the response reliably before we reboot.
  LOG_DEBUG(F("WebManager"), F("Flush und schließe Client-Socket (wenn möglich)"));
  _server->client().flush();
  // Small pause to let the stack push out bytes
  delay(200);
  // Explicitly stop the client to terminate the TCP connection cleanly
  _server->client().stop();
  LOG_DEBUG(F("WebManager"), F("Antwort gesendet und Client-Socket gestoppt"));

  // 9. Handle update mode and reboot if necessary
  if (updateMode) {
    LOG_INFO(F("WebManager"), F("Update-Modus aktiviert, bereite Neustart vor..."));

    // Die Antwort wurde oben bereits gesendet, geflusht und der Client-Socket
    // explizit geschlossen - es gibt also nichts mehr abzuwarten.
    //
    // Hier stand früher eine Schleife, die erneut _server->handleClient()
    // aufgerufen hat. Dieser re-entrante Aufruf aus einem Handler heraus, der
    // selbst aus handleClient() stammt, versetzt den Ablauf in den SYS-Kontext;
    // das anschließende delay(50) hat den Core dann mit
    // "Panic core_esp8266_main.cpp __yield" abgebrochen.
    //
    // Ebenso wurden hier vorher der Sensor-Manager gestoppt und cleanup()
    // aufgerufen. Das gab Objekte frei, auf die lwIP und das SDK noch
    // Callbacks bzw. pcbs hielten; der anschließende WiFi-Teardown lief dann
    // in eine "Fatal exception 9 (LoadStoreAlignmentCause)". Vor einem
    // ESP.restart() ist ein Teardown ohnehin sinnlos - der Chip wird komplett
    // zurückgesetzt. Alle persistenten Daten (Flash-Backup, Preferences,
    // Update-Flags) sind an dieser Stelle bereits geschrieben.

    LOG_INFO(F("WebManager"), F("Starte neu im Update-Modus..."));
    delay(100); // Kurze Pause, damit die Logzeilen noch rausgehen
    ESP.restart();
  }

  LOG_DEBUG(F("WebManager"), F("Verlasse WebManager::handleSetUpdate()"));
}

void WebManager::handleSetConfigValue() {
  if (!_server) {
    LOG_ERROR(F("WebManager"), F("Serverinstanz ist null"));
    return;
  }

  String namespaceName, key, value, typeStr;

  // NOTE: We used to require authentication for all setConfigValue requests.
  // For usability we allow unauthenticated updates for a small, safe subset
  // of configuration: the 'debug' namespace (debug flags) and the
  // 'log.level' setting so clients can toggle debug/log level without
  // providing admin credentials. All other configuration updates still
  // require authentication.

  // Check if request is form-encoded (new unified method) or JSON (legacy)
  String contentType = _server->header("Content-Type");
  bool isFormEncoded = contentType.indexOf("application/x-www-form-urlencoded") >= 0;

  if (isFormEncoded || _server->hasArg("namespace")) {
    // New unified method with namespace and type
    namespaceName = _server->arg("namespace");
    key = _server->arg("key");
    value = _server->arg("value");
    typeStr = _server->arg("type");

    if (namespaceName.isEmpty() || key.isEmpty()) {
      LOG_ERROR(F("WebManager"), F("Fehlender Namespace- oder Schlüssel-Parameter"));
      sendErrorResponse(400, F("Fehlender Namespace- oder Schlüssel-Parameter"));
      return;
    }

    // Parse type parameter
    ConfigValueType type = ConfigValueType::STRING; // default
    if (typeStr == "bool") {
      type = ConfigValueType::BOOL;
    } else if (typeStr == "int") {
      type = ConfigValueType::INT;
    } else if (typeStr == "uint") {
      type = ConfigValueType::UINT;
    } else if (typeStr == "float") {
      type = ConfigValueType::FLOAT;
    } else if (typeStr == "string") {
      type = ConfigValueType::STRING;
    }

    LOG_DEBUG(F("WebManager"), String(F("Setze Konfiguration: ")) + namespaceName + String(F(".")) +
                                   key + String(F(" = ")) + value + String(F(" (Typ: ")) + typeStr +
                                   F(")"));

    // Determine whether this particular update is allowed without auth.
    bool isPublicUpdate = false;
    if (namespaceName == "debug") {
      isPublicUpdate = true; // allow toggling debug flags without auth
    } else if (namespaceName == "log" && key == "level") {
      isPublicUpdate = true; // allow changing log level without auth
    }

    // If this is not a public update, require authentication
    if (!isPublicUpdate) {
      if (!_server->authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
        LOG_WARN(F("WebManager"), F("Authentifizierung für setConfigValue-Anfrage fehlgeschlagen"));
        _server->requestAuthentication();
        return;
      }
    }

    // Update config value using new method
    auto result = ConfigMgr.setConfigValue(namespaceName, key, value, type);
    if (!result.isSuccess()) {
      LOG_ERROR(F("WebManager"), String(F("Konfigurationswert konnte nicht gesetzt werden: ")) +
                                     result.getMessage());
      sendErrorResponse(400, result.getMessage());
      return;
    }

    // Send success response (no message - let frontend format it)
    StaticJsonDocument<200> response;
    response["success"] = true;
    String jsonResponse;
    serializeJson(response, jsonResponse);
    _server->send(200, F("application/json"), jsonResponse);

  } else {
    // Legacy JSON method - kept for backward compatibility during transition
    String json = _server->arg("plain");
    LOG_DEBUG(F("WebManager"), "Empfangene Legacy-Konfigurations-Update-Anfrage: " + json);

    // Parse JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
      String errorMsg = String(F("JSON-Parsefehler: ")) + error.c_str();
      LOG_ERROR(F("WebManager"), errorMsg);
      sendErrorResponse(400, errorMsg);
      return;
    }

    // Extract key and value
    const char* keyPtr = doc["key"] | "";
    const char* valuePtr = doc["value"] | "";

    if (!keyPtr[0]) {
      LOG_ERROR(F("WebManager"), F("Schlüssel in Anfrage fehlt"));
      sendErrorResponse(400, F("Fehlender Schlüssel-Parameter"));
      return;
    }

    // Update config value using legacy method
    auto result = ConfigMgr.setConfigValue(keyPtr, valuePtr);
    if (!result.isSuccess()) {
      LOG_ERROR(F("WebManager"), String(F("Konfigurationswert konnte nicht gesetzt werden: ")) +
                                     result.getMessage());
      sendErrorResponse(400, result.getMessage());
      return;
    }

    // Save config
    auto saveResult = ConfigMgr.saveConfig();
    if (!saveResult.isSuccess()) {
      LOG_ERROR(F("WebManager"), String(F("Konfiguration konnte nicht gespeichert werden: ")) +
                                     saveResult.getMessage());
      sendErrorResponse(500, F("Konfiguration konnte nicht gespeichert werden"));
      return;
    }

    // Send success response
    StaticJsonDocument<200> response;
    response["status"] = "OK";
    String jsonResponse;
    serializeJson(response, jsonResponse);
    _server->send(200, F("application/json"), jsonResponse);
  }
}

ResourceResult WebManager::validateUpdateRequest(const String& json, bool& fileSystemUpdate,
                                                 bool& firmwareUpdate, bool& updateMode) {
  if (json.length() == 0) {
    LOG_WARN(F("WebManager"), F("Leerer Request-Body"));
    sendErrorResponse(400, F("Fehlender Request-Body"));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR, F("Missing request body"));
  }

  // Parse JSON with error handling
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    String errorMsg = String(F("JSON-Parsefehler: ")) + error.c_str();
    LOG_ERROR(F("WebManager"), errorMsg);
    sendErrorResponse(400, errorMsg);
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR,
                                String(F("JSON-Parsefehler: ")) + error.c_str());
  }

  // Extract and validate update flags
  fileSystemUpdate = doc["isFileSystemUpdatePending"] | false;
  firmwareUpdate = doc["isFirmwareUpdatePending"] | false;
  updateMode = doc["inUpdateMode"] | false;

  // Check that not both update types are requested
  if (fileSystemUpdate && firmwareUpdate) {
    LOG_ERROR(F("WebManager"), F("Kann nicht gleichzeitig Dateisystem und Firmware aktualisieren"));
    sendErrorResponse(400, F("Es ist nur ein Aktualisierungstyp gleichzeitig erlaubt"));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR,
                                F("Es ist nur ein Aktualisierungstyp gleichzeitig erlaubt"));
  }

  return ResourceResult::success();
}

bool WebManager::prepareUpdateMode(bool fileSystemUpdate, bool firmwareUpdate, bool updateMode) {
  // Set flags in config with error handling
  auto result = ConfigMgr.setUpdateFlags(fileSystemUpdate, firmwareUpdate);
  if (!result.isSuccess()) {
    LOG_ERROR(F("WebManager"), "Failed to set update flags: " + result.getMessage());
    sendErrorResponse(400, result.getMessage());
    return false;
  }

  LOG_DEBUG(F("WebManager"), F("Konfiguration erfolgreich gespeichert"));
  return true;
}

void WebManager::sendErrorResponse(int code, const String& message) {
  StaticJsonDocument<200> doc;
  doc["error"] = message;
  String response;
  serializeJson(doc, response);
  _server->send(code, "application/json", response);
}

String WebManager::methodToString(HTTPMethod method) {
  switch (method) {
  case HTTP_GET:
    return "GET";
  case HTTP_POST:
    return "POST";
  case HTTP_PUT:
    return "PUT";
  case HTTP_PATCH:
    return "PATCH";
  case HTTP_DELETE:
    return "DELETE";
  case HTTP_OPTIONS:
    return "OPTIONS";
  default:
    return "UNKNOWN";
  }
}
