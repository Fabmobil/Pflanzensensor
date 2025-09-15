/**
 * @file admin_minimal_handler.h
 * @brief Minimal administrative handler for update mode
 * @details Provides essential administrative functionality during update mode:
 *          - System reboot control
 *          - Basic authentication
 *          - Minimal routing
 *          - Security validation
 */

#ifndef ADMIN_MINIMAL_HANDLER_H
#define ADMIN_MINIMAL_HANDLER_H

#include "../core/web_auth.h"
#include "base_handler.h"

/**
 * @class AdminMinimalHandler
 * @brief Minimal handler for administrative functions in update mode
 * @details Implements essential administrative functionality:
 *          - System reboot control
 *          - Authentication validation
 *          - Basic request handling
 *          - Security checks
 */
class AdminMinimalHandler : public BaseHandler {
 public:
  /**
   * @brief Constructor for minimal admin handler
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @details Initializes handler with required services:
   *          - Sets up server connection
   *          - Configures authentication
   *          - Initializes logging
   */
  AdminMinimalHandler(ESP8266WebServer& server, WebAuth& auth)
      : BaseHandler(server), _auth(auth) {
    logger.debug(F("AdminMinimalHandler"),
                 F("Initializing AdminMinimalHandler"));
  }

  /**
   * @brief Register minimal admin routes
   * @param router Reference to router instance
   * @return Result of route registration
   * @details Sets up essential routes:
   *          - Reboot endpoint
   *          - Validates registration
   *          - Logs setup status
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override {
    auto result = router.addRoute(HTTP_POST, "/admin/reboot",
                                  [this]() { handleReboot(); });
    if (!result.isSuccess()) {
      return result;
    }
    logger.debug(F("AdminMinimalHandler"),
                 F("Minimal admin routes registered"));
    return RouterResult::success();
  }

  /**
   * @brief Handle GET requests
   * @param uri Request URI (unused)
   * @param query Query parameters (unused)
   * @return Handler result indicating not implemented
   * @details Minimal GET handling:
   *          - Returns not implemented
   *          - Maintains interface compliance
   *          - Logs attempt if needed
   */
  HandlerResult handleGet(
      [[maybe_unused]] const String& uri,
      [[maybe_unused]] const std::map<String, String>& query) override {
    return HandlerResult::fail(HandlerError::NOT_FOUND, "Not implemented");
  }

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters (unused)
   * @return Handler result indicating success or failure
   * @details Processes POST requests:
   *          - Handles reboot requests
   *          - Validates endpoints
   *          - Returns appropriate status
   */
  HandlerResult handlePost(
      const String& uri,
      [[maybe_unused]] const std::map<String, String>& params) override {
    if (uri == "/admin/reboot") {
      handleReboot();
      return HandlerResult::success();
    }
    return HandlerResult::fail(HandlerError::NOT_FOUND, "Unknown endpoint");
  }

  /**
   * @brief Handle reboot request
   * @details Manages system reboot process:
   *          - Validates authentication
   *          - Sends response page
   *          - Initiates reboot
   *          - Provides feedback
   */
  void handleReboot() {
    if (!validateRequest()) return;

    _server.send(200, "text/html",
                 F("<h2>Reboot in progress...</h2>"
                   "<p>Page will reload in 10 seconds.</p>"
                   "<script>setTimeout(function() { window.location.href = "
                   "'/'; }, 10000);</script>"));

    delay(500);  // Give time to send response
    logger.warning(F("AdminMinimalHandler"), F("Rebooting ESP"));
    ESP.restart();
  }

 private:
  WebAuth& _auth;  ///< Reference to authentication service

  /**
   * @brief Validate if the current request is authorized
   * @return true if request is authenticated, false otherwise
   * @details Verifies request authorization:
   *          - Checks authentication
   *          - Validates credentials
   *          - Ensures security
   */
  bool validateRequest() const {
    return _auth.authenticate();  // Changed from checkAuth to validateRequest
  }
};

#endif  // ADMIN_MINIMAL_HANDLER_H
