/**
 * @file admin_handler_core.h
 * @brief Core admin handler class definition and main functionality
 * @details Provides the main AdminHandler class with core functionality:
 *          - Route registration
 *          - Request validation
 *          - Main admin page handling
 */

#ifndef ADMIN_HANDLER_H
#define ADMIN_HANDLER_H

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/result_types.h"
#include "web/core/web_router.h"
#include "web/handler/base_handler.h"
#include "web/services/css_service.h"

/**
 * @class AdminHandler
 * @brief Handles all administrative web interface functionality
 * @details Provides comprehensive administrative interface functionality:
 *          - Configuration management
 *          - System monitoring
 *          - Performance statistics
 *          - Security controls
 *          - Maintenance operations
 */
class AdminHandler : public BaseHandler {
 public:
  /**
   * @brief Constructor
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @param cssService Reference to CSS management service
   * @details Initializes the admin handler with required services:
   *          - Sets up server connection
   *          - Configures authentication
   *          - Initializes CSS handling
   *          - Sets up logging
   */
  AdminHandler(ESP8266WebServer& server, [[maybe_unused]] WebAuth& auth,
               [[maybe_unused]] CSSService& cssService)
      : BaseHandler(server) {  // We only use the server parameter
    logger.debug(F("AdminHandler"), F("Initializing AdminHandler"));
    logger.logMemoryStats(F("Admihandler"));
  }

  /**
   * @brief Register admin routes with the router
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Registers all administrative endpoints:
   *          - Main admin page
   *          - Configuration endpoints
   *          - System control endpoints
   *          - Monitoring endpoints
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

  // Config API methods
  /**
   * @brief Handle setting configuration values via REST API
   * @details Processes configuration updates:
   *          - Validates JSON format
   *          - Updates settings
   *          - Tracks changes
   *          - Returns results
   * @note Expects JSON in format: {"key": "setting_name", "value": "new_value"}
   */
  void handleConfigSet();

  /**
   * @brief Streams the log file to the client for download if file logging is
   * enabled.
   */
  void handleDownloadLog();

  // Card generation methods - implemented in admin_handler_cards.cpp

#if USE_MAIL
  /**
   * @brief Generate and send the Mail Settings card for the admin page.
   */
  void generateAndSendMailSettingsCard();
#endif

  /**
   * @brief Generate and send the Debug Settings card for the admin page.
   */
  void generateAndSendDebugSettingsCard();

  /**
   * @brief Generate and send the System Settings card for the admin page.
   */
  void generateAndSendSystemSettingsCard();

  /**
   * @brief Generate and send the System Actions card for the admin page.
   */
  void generateAndSendSystemActionsCard();

  /**
   * @brief Generate and send the System Information card for the admin page.
   */
  void generateAndSendSystemInfoCard();

  /**
   * @brief Generate and send the LED Traffic Light Settings card for the admin
   * page.
   */
  void generateAndSendLedTrafficLightSettingsCard();

  /**
   * @brief Generate and send the WiFi Settings card for the admin page.
   */
  void generateAndSendWiFiSettingsCard();

  // WiFi handling methods - implemented in admin_handler_wifi.cpp
  /**
   * @brief Handle WiFi settings update POST request.
   */
  void handleWiFiUpdate();

 protected:
  /**
   * @brief Handle GET requests
   * @param uri Request URI
   * @param query Query parameters
   * @return Handler result indicating success or failure
   * @details Processes GET requests for:
   *          - Admin interface pages
   *          - System information
   *          - Configuration data
   *          - Status updates
   */
  HandlerResult handleGet(const String& uri,
                          const std::map<String, String>& query) override;

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters
   * @return Handler result indicating success or failure
   * @details Processes POST requests for:
   *          - Configuration updates
   *          - System controls
   *          - Maintenance operations
   *          - Security settings
   */
  HandlerResult handlePost(const String& uri,
                           const std::map<String, String>& params) override;

 private:
  friend class WebManager;
  String
      _tempChanges;  ///< Temporary storage for tracking configuration changes

  /**
   * @brief Handle main admin page request
   * @details Generates and serves admin interface:
   *          - System overview
   *          - Configuration options
   *          - Control panels
   *          - Status displays
   */
  void handleAdminPage();

  // System action methods - implemented in admin_handler_system.cpp
  /**
   * @brief Handle configuration update requests
   * @details Processes configuration changes:
   *          - Validates input
   *          - Applies changes
   *          - Updates storage
   *          - Logs modifications
   */
  void handleAdminUpdate();

  /**
   * @brief Handle configuration reset requests
   * @details Manages configuration reset:
   *          - Validates authorization
   *          - Resets settings
   *          - Logs changes
   *          - Confirms reset
   */
  void handleConfigReset();

  /**
   * @brief Handle system reboot requests
   * @details Manages system reboot process:
   *          - Validates authorization
   *          - Saves pending changes
   *          - Initiates reboot
   *          - Provides feedback
   */
  void handleReboot();

#if USE_MAIL
  /**
   * @brief Handle test mail sending requests
   * @details Sends test email using current SMTP configuration:
   *          - Validates mail settings
   *          - Sends test email
   *          - Provides feedback
   */
  void handleTestMail();
#endif

  // Utility methods - implemented in admin_handler_utils.cpp
  /**
   * @brief Processes configuration updates from form submission
   * @param changes String to store descriptions of changes made
   * @return bool True if any settings were updated
   * @details Handles updates for:
   *          - Debug flags
   *          - Log level
   *          - Measurement intervals
   *          - System settings
   */
  bool processConfigUpdates(String& changes);

  /**
   * @brief Format memory size in human readable format
   * @param bytes Size in bytes
   * @return Formatted string with appropriate unit
   * @details Converts byte values to readable format:
   *          - Selects appropriate unit
   *          - Formats numbers
   *          - Adds unit suffix
   */
  String formatMemorySize(size_t bytes) const;

  /**
   * @brief Format uptime in human readable format
   * @return Formatted string with days, hours, minutes, seconds
   * @details Formats system uptime:
   *          - Calculates time units
   *          - Formats string
   *          - Handles overflow
   */
  String formatUptime() const;

  /**
   * @brief Validate admin authentication
   * @return true if request is authenticated
   * @details Verifies admin access:
   *          - Checks credentials
   *          - Validates session
   *          - Logs attempts
   */
  bool validateRequest() const;

  /**
   * @brief Validate and apply a configuration value
   * @param key Name of the setting
   * @param value New value to set
   * @return true if setting was successfully applied
   * @details Processes configuration changes:
   *          - Validates key exists
   *          - Checks value format
   *          - Applies change
   *          - Updates storage
   */
  bool applyConfigValue(const String& key, const String& value);

  /**
   * @brief Custom cleanup logic for AdminHandler
   * @details Clears temporary storage and resets state.
   */
  void onCleanup() override { _tempChanges = String(); }
};

#endif  // ADMIN_HANDLER_H
