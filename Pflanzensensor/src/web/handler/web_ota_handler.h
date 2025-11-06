/**
 * @file web_ota_handler.h
 * @brief Handler for OTA (Over-The-Air) updates via web interface
 * @details Provides functionality for managing firmware and filesystem updates
 *          through the web interface, including:
 *          - Update status tracking
 *          - Progress monitoring
 *          - Memory management
 *          - Error handling
 *          - Secure update validation
 */

#pragma once

#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#include <memory>

#include "base_handler.h"
#include "utils/result_types.h"

// Forward declarations
class WebAuth;
class WebRouter;

/**
 * @struct OTAStatus
 * @brief Status information for OTA updates
 * @details Tracks the current state and progress of an OTA update:
 *          - Update progress tracking
 *          - Error state monitoring
 *          - Reboot requirement tracking
 *          - Size information
 */
struct OTAStatus {
  bool inProgress = false;    ///< Whether an update is currently in progress
  size_t currentProgress = 0; ///< Current bytes processed in update
  size_t totalSize = 0;       ///< Total size of the update in bytes
  bool needsReboot = false;   ///< Whether system needs reboot after update
  String lastError;           ///< Last error message if update failed
};

/**
 * @class WebOTAHandler
 * @brief Handles Over-The-Air updates via web interface
 * @details Manages the entire OTA update process:
 *          - Update file upload
 *          - Progress tracking
 *          - Memory management
 *          - Update validation
 *          - Error handling
 * @inherits BaseHandler
 */
class WebOTAHandler final : public BaseHandler {
public:
  /**
   * @brief Constructor
   * @param server Reference to web server instance
   * @param auth Reference to authentication manager
   * @details Initializes the OTA handler with required dependencies
   * @throws None
   */
  WebOTAHandler(ESP8266WebServer& server, WebAuth& auth);

  // Prevent copying
  WebOTAHandler(const WebOTAHandler&) = delete;
  WebOTAHandler& operator=(const WebOTAHandler&) = delete;

  /**
   * @brief Handle status request
   * @details Sends current OTA update status as JSON response:
   *          - Update progress
   *          - Error state
   *          - Reboot requirement
   * @throws None
   */
  void handleStatus();

  /**
   * @brief Register OTA routes with router
   * @param router Reference to web router
   * @return Result of route registration
   * @details Sets up routes for:
   *          - Update page
   *          - Status endpoint
   *          - Upload endpoint
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

  /**
   * @brief Handle GET requests
   * @param uri Request URI
   * @param query Query parameters
   * @return Result of request handling
   * @details Handles GET requests for:
   *          - Update page
   *          - Status information
   * @override
   */
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters
   * @return Result of request handling
   * @details Handles POST requests for:
   *          - Update file upload
   *          - Update commands
   * @override
   */
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

  /**
   * @brief Calculate required space for update
   * @param isFilesystem Whether update is for filesystem
   * @return Required space in bytes
   * @details Calculates needed space considering:
   *          - Update type
   *          - Safety margins
   *          - System requirements
   * @throws None
   */
  size_t calculateRequiredSpace(bool isFilesystem) const;

protected:
  /**
   * @brief Handle update page request
   * @details Serves the update interface page:
   *          - Upload form
   *          - Status display
   *          - Progress tracking
   * @throws None
   */
  void handleUpdatePage();

  /**
   * @brief Handle update file upload
   * @details Processes uploaded update file:
   *          - Validates file
   *          - Manages memory
   *          - Tracks progress
   *          - Reports errors
   * @throws None
   */
  void handleUpdateUpload();

  /**
   * @brief Write update data
   * @param data Pointer to data buffer
   * @param len Length of data
   * @return Result of write operation
   * @details Writes update data to flash:
   *          - Validates data
   *          - Updates progress
   *          - Handles errors
   * @throws ResourceError
   */
  TypedResult<ResourceError, void> writeData(uint8_t* data, size_t len);

  /**
   * @brief Finish update process
   * @param reboot Whether to reboot after update
   * @return Result of update completion
   * @details Finalizes the update:
   *          - Validates update
   *          - Updates status
   *          - Handles reboot
   * @throws ResourceError
   */
  TypedResult<ResourceError, void> endUpdate(bool reboot = true);

  /**
   * @brief Abort current update
   * @details Safely aborts update process:
   *          - Cleans up resources
   *          - Updates status
   *          - Logs error
   * @throws None
   */
  void abortUpdate();

  /**
   * @brief Get current update status
   * @return Current OTA status
   * @details Provides update status information:
   *          - Progress
   *          - Error state
   *          - Reboot requirement
   * @throws None
   */
  OTAStatus getStatus() const;

  /**
   * @brief Start update process
   * @param size Expected update size
   * @param md5 Expected MD5 hash
   * @param isFilesystem Whether update is for filesystem
   * @return Result of update initialization
   * @details Prepares for update:
   *          - Validates parameters
   *          - Checks memory
   *          - Initializes update
   * @throws ResourceError
   */
  TypedResult<ResourceError, void> beginUpdate(size_t size, const String& md5, bool isFilesystem);

  /**
   * @brief Check memory availability
   * @return true if enough memory available
   * @details Verifies system has enough memory:
   *          - Checks free heap
   *          - Validates thresholds
   *          - Ensures safe operation
   * @throws None
   */
  bool checkMemory() const;

  /**
   * @brief Calculate MD5 hash
   * @param data Data buffer
   * @param len Buffer length
   * @return MD5 hash string
   * @details Calculates MD5 hash for validation:
   *          - Processes data chunks
   *          - Formats hash string
   *          - Handles errors
   * @throws None
   */
  String calculateMD5(uint8_t* data, size_t len);

  /**
   * @brief Send error response
   * @param code HTTP status code
   * @param message Error message
   * @details Sends JSON formatted error:
   *          - Sets status code
   *          - Formats message
   *          - Sends response
   * @throws None
   */
  void sendErrorResponse(int code, const String& message) {
    StaticJsonDocument<200> response;
    response["success"] = false;
    response["error"] = message;
    String jsonStr;
    serializeJson(response, jsonStr);
    _server.send(code, "application/json", jsonStr);
  }

private:
  /// Size of upload buffer for processing update data
  static const size_t UPLOAD_BUFFER_SIZE = 4096;
  /// Minimum required free heap space for safe operation
  static const size_t MIN_FREE_HEAP = 5500;

  /**
   * @brief Back up all Preferences before filesystem update
   * @return true if backup successful
   * @details Saves all config data to RAM before LittleFS update
   */
  
  // Removed obsolete RAM-based backup/restore methods and PreferencesBackup struct.
  // Now using file-based backup exclusively (see ConfigPersistence::backupPreferencesToFile).

  WebAuth& _auth;    ///< Reference to authentication manager
  OTAStatus _status; ///< Current update status
};

