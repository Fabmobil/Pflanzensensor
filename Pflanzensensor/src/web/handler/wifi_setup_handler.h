/**
 * @file wifi_setup_handler.h
 * @brief Handler for WiFi configuration updates
 * @details Provides WiFi configuration functionality for updating credentials.
 *          The setup form is now integrated directly into the startpage when
 *          in AP mode, eliminating the need for a separate setup page.
 */

#ifndef WIFI_SETUP_HANDLER_H
#define WIFI_SETUP_HANDLER_H

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "utils/result_types.h"
#include "web/core/web_auth.h"
#include "web/core/web_router.h"
#include "web/handler/base_handler.h"
#include "web/services/css_service.h"

/**
 * @class WiFiSetupHandler
 * @brief Handles WiFi configuration updates
 * @details Manages WiFi credential updates:
 *          - Credential validation
 *          - Configuration storage
 *          - Connection testing
 *          - Setup completion workflow
 */
class WiFiSetupHandler : public BaseHandler {
public:
  /**
   * @brief Constructor for WiFi setup handler
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @param cssService Reference to CSS management service
   * @details Initializes handler with required services:
   *          - Server connection
   *          - Authentication (optional for captive portal)
   *          - CSS management
   *          - Network scanning
   */
  WiFiSetupHandler(ESP8266WebServer& server, WebAuth& auth, CSSService& cssService)
      : BaseHandler(server), _auth(auth), _cssService(cssService) {
    logger.debug(F("WiFiSetupHandler"), F("Initializing WiFiSetupHandler"));
  }

  /**
   * @brief Register WiFi configuration routes
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Sets up routes for:
   *          - WiFi credential updates
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

protected:
  /**
   * @brief Handle GET requests (not supported in this handler)
   * @param uri Request URI
   * @param query Query parameters
   * @return Handler result indicating failure
   * @details This handler only supports POST requests for WiFi updates.
   *          GET requests are not supported since the form is integrated
   *          into the startpage.
   */
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override {
    return HandlerResult::fail(HandlerError::NOT_FOUND, "GET not supported");
  }

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters
   * @return Handler result indicating success or failure
   * @details Processes POST requests for:
   *          - WiFi credential updates
   *          - Network selection
   *          - Connection attempts
   */
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

private:
  friend class WebManager; // Allow WebManager to access private members

  WebAuth& _auth;          ///< Reference to authentication service
  CSSService& _cssService; ///< Reference to CSS service

  /**
   * @brief Handle WiFi credential update
   * @details Processes credential updates:
   *          - Validates input
   *          - Updates configuration
   *          - Tests connection
   *          - Provides feedback
   */
  void handleWiFiUpdate();

  /**
   * @brief Generate network selection dropdown
   * @return HTML string for network selection
   * @details Creates network selection interface:
   *          - Scans available networks
   *          - Formats signal strength
   *          - Handles security indicators
   *          - Manages scan errors
   */
  String generateNetworkSelection();

  /**
   * @brief Generate credential slots display
   * @return HTML string for credential slots
   * @details Creates credential management interface:
   *          - Shows saved networks
   *          - Indicates active connections
   *          - Provides slot selection
   *          - Handles empty slots
   */
  String generateCredentialSlots();

  /**
   * @brief Get active WiFi slot
   * @return Slot number (1-3) or 0 if none active
   * @details Determines which credential slot is currently active:
   *          - Compares current SSID
   *          - Handles connection states
   *          - Returns slot identifier
   */
  int getActiveWiFiSlot();

  /**
   * @brief Validate WiFi credentials
   * @param ssid Network SSID to validate
   * @param password Network password to validate
   * @return true if credentials are valid format
   * @details Validates credential format:
   *          - Checks SSID length
   *          - Validates password requirements
   *          - Handles special characters
   */
  bool validateCredentials(const String& ssid, const String& password);

  /**
   * @brief Test WiFi connection
   * @param ssid Network SSID to test
   * @param password Network password to test
   * @return true if connection test successful
   * @details Tests network connectivity:
   *          - Attempts connection
   *          - Validates signal strength
   *          - Checks internet access
   *          - Restores previous state
   */
  bool testConnection(const String& ssid, const String& password);

  /**
   * @brief Format signal strength indicator
   * @param rssi Signal strength in dBm
   * @return HTML string for signal indicator
   * @details Creates visual signal strength:
   *          - Converts dBm to bars
   *          - Applies appropriate styling
   *          - Handles edge cases
   */
  String formatSignalStrength(int32_t rssi);

  /**
   * @brief Check if device is in captive portal mode
   * @return true if in captive portal mode
   * @details Determines captive portal state:
   *          - Checks WiFi connection status
   *          - Validates network accessibility
   *          - Considers AP mode state
   * @note This is a private helper method, not exposed publicly
   */
  bool isCaptivePortalMode();
};

#endif // WIFI_SETUP_HANDLER_H
