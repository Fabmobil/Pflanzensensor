/**
 * @file web_manager_handlers.cpp
 * @brief WebManager request handling and processing
 */

#include <ArduinoJson.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::handleSetUpdate() {
  logger.debug(F("WebManager"), F("Enter WebManager::handleSetUpdate()"));

  // 1. Verify server instance
  if (!_server) {
    logger.error(F("WebManager"), F("Server instance is null"));
    return;
  }

  // 2. Basic auth check with detailed logging
  logger.debug(F("WebManager"), F("Checking authentication..."));
  if (!_server->authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    logger.warning(F("WebManager"),
                   F("Authentication failed for setUpdate request"));
    _server->requestAuthentication();
    return;
  }
  logger.debug(F("WebManager"), F("Authentication successful"));

  // 3. Verify request method
  if (_server->method() != HTTP_POST) {
    logger.warning(F("WebManager"), F("Invalid method for setUpdate"));
    sendErrorResponse(405, F("Method Not Allowed"));
    return;
  }

  // 4. Get and validate request body
  String json = _server->arg("plain");
  logger.debug(F("WebManager"),
               "Received update request body length: " + String(json.length()));
  logger.debug(F("WebManager"), "Raw request body: " + json);

  // 5. Validate request and extract flags
  bool fileSystemUpdate, firmwareUpdate, updateMode;
  auto validationResult =
      validateUpdateRequest(json, fileSystemUpdate, firmwareUpdate, updateMode);
  if (!validationResult.isSuccess()) {
    return;
  }

  // 6. Log the intended update type
  logger.debug(F("WebManager"), F("Setting flags - FS: ") +
                                    String(fileSystemUpdate) + F(", FW: ") +
                                    String(firmwareUpdate) + F(", Mode: ") +
                                    String(updateMode));

  // 7. Save configuration and prepare for update
  if (!prepareUpdateMode(fileSystemUpdate, firmwareUpdate, updateMode)) {
    return;
  }

  // 8. Send success response before potential reboot
  StaticJsonDocument<200> response;
  response["status"] = "OK";
  String jsonResponse;
  serializeJson(response, jsonResponse);

  logger.debug(F("WebManager"), F("Sending success response"));
  _server->send(200, F("application/json"), jsonResponse);
  _server->client().flush();
  logger.debug(F("WebManager"), F("Response sent"));

  // 9. Handle update mode and reboot if necessary
  if (updateMode) {
    logger.info(F("WebManager"),
                F("Update mode enabled, preparing for reboot..."));
    delay(500);  // Give more time for response and logging

    // Stop non-critical services
    if (_sensorManager) {
      logger.debug(F("WebManager"), F("Stopping sensor manager..."));
      _sensorManager->stopAll();
      _sensorManager = nullptr;
    }

    logger.debug(F("WebManager"), F("Running cleanup..."));
    cleanup();

    logger.info(F("WebManager"), F("Rebooting into update mode..."));
    delay(100);  // Small delay to ensure logs are written
    ESP.restart();
  }

  logger.debug(F("WebManager"), F("Exit WebManager::handleSetUpdate()"));
}

void WebManager::handleSetConfigValue() {
  if (!_server) {
    logger.error(F("WebManager"), F("Server instance is null"));
    return;
  }

  // Check authentication
  if (!_server->authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    logger.warning(F("WebManager"),
                   F("Authentication failed for setConfigValue request"));
    _server->requestAuthentication();
    return;
  }

  // Get request body
  String json = _server->arg("plain");
  logger.debug(F("WebManager"), "Received config update request: " + json);

  // Parse JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    String errorMsg = String(F("JSON parse error: ")) + error.c_str();
    logger.error(F("WebManager"), errorMsg);
    sendErrorResponse(400, errorMsg);
    return;
  }

  // Extract key and value
  const char* key = doc["key"] | "";
  const char* value = doc["value"] | "";

  if (!key[0]) {
    logger.error(F("WebManager"), F("Missing key in request"));
    sendErrorResponse(400, F("Missing key parameter"));
    return;
  }

  // Update config value
  auto result = ConfigMgr.setConfigValue(key, value);
  if (!result.isSuccess()) {
    logger.error(F("WebManager"),
                 "Failed to set config value: " + result.getMessage());
    sendErrorResponse(400, result.getMessage());
    return;
  }

  // Save config
  auto saveResult = ConfigMgr.saveConfig();
  if (!saveResult.isSuccess()) {
    logger.error(F("WebManager"),
                 "Failed to save config: " + saveResult.getMessage());
    sendErrorResponse(500, F("Failed to save configuration"));
    return;
  }

  // Send success response
  StaticJsonDocument<200> response;
  response["status"] = "OK";
  String jsonResponse;
  serializeJson(response, jsonResponse);
  _server->send(200, F("application/json"), jsonResponse);
}

ResourceResult WebManager::validateUpdateRequest(const String& json,
                                                 bool& fileSystemUpdate,
                                                 bool& firmwareUpdate,
                                                 bool& updateMode) {
  if (json.length() == 0) {
    logger.warning(F("WebManager"), F("Empty request body"));
    sendErrorResponse(400, F("Missing request body"));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR,
                                F("Missing request body"));
  }

  // Parse JSON with error handling
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    String errorMsg = String(F("JSON parse error: ")) + error.c_str();
    logger.error(F("WebManager"), errorMsg);
    sendErrorResponse(400, errorMsg);
    return ResourceResult::fail(
        ResourceError::VALIDATION_ERROR,
        String(F("JSON parse error: ")) + error.c_str());
  }

  // Extract and validate update flags
  fileSystemUpdate = doc["isFileSystemUpdatePending"] | false;
  firmwareUpdate = doc["isFirmwareUpdatePending"] | false;
  updateMode = doc["inUpdateMode"] | false;

  // Check that not both update types are requested
  if (fileSystemUpdate && firmwareUpdate) {
    logger.error(
        F("WebManager"),
        F("Cannot update both filesystem and firmware simultaneously"));
    sendErrorResponse(400, F("Only one update type allowed at a time"));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR,
                                F("Only one update type allowed at a time"));
  }

  return ResourceResult::success();
}

bool WebManager::prepareUpdateMode(bool fileSystemUpdate, bool firmwareUpdate,
                                   bool updateMode) {
  // Set flags in config with error handling
  auto result = ConfigMgr.setUpdateFlags(fileSystemUpdate, firmwareUpdate);
  if (!result.isSuccess()) {
    logger.error(F("WebManager"),
                 "Failed to set update flags: " + result.getMessage());
    sendErrorResponse(400, result.getMessage());
    return false;
  }

  logger.debug(F("WebManager"), F("Configuration saved successfully"));
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
