/**
 * @file admin_display_handler.h
 * @brief Handler for display configuration web interface
 * @details Provides functionality for managing display settings through
 *          the web interface, including:
 *          - Display configuration
 *          - Layout management
 *          - Brightness control
 *          - Content organization
 *          - Update handling
 */

#ifndef ADMIN_DISPLAY_HANDLER_H
#define ADMIN_DISPLAY_HANDLER_H

#include <ESP8266WebServer.h>

#include "managers/manager_display.h"
#include "managers/manager_sensor.h"
#include "utils/result_types.h"
#include "web/core/components.h"
#include "web/core/web_auth.h"
#include "web/core/web_router.h"
#include "web/handler/base_handler.h"

#if USE_DISPLAY

/**
 * @class AdminDisplayHandler
 * @brief Manages display configuration through web interface
 * @details Provides comprehensive display management functionality:
 *          - Configuration interface
 *          - Settings validation
 *          - Update handling
 *          - Layout control
 *          - Display customization
 */
class AdminDisplayHandler : public BaseHandler {
 public:
  /**
   * @brief Constructor
   * @param server Reference to web server instance
   * @details Initializes the display handler:
   *          - Sets up server connection
   *          - Prepares display management
   *          - Configures routing
   */
  explicit AdminDisplayHandler(ESP8266WebServer& server);

  /**
   * @brief Destructor
   * @details Ensures proper cleanup and vtable generation.
   */
  virtual ~AdminDisplayHandler();

  /**
   * @brief Register display configuration routes
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Sets up routes for:
   *          - Configuration page
   *          - Settings updates
   *          - Display control
   *          - Status monitoring
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

 protected:
  /**
   * @brief Handle GET requests
   * @param uri Request URI
   * @param query Query parameters
   * @return Handler result indicating success or failure
   * @details Processes GET requests:
   *          - Not directly handled
   *          - Redirects to route registration
   *          - Returns appropriate error
   */
  HandlerResult handleGet(const String& uri,
                          const std::map<String, String>& query) override {
    return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                               "Use registerRoutes instead");
  }

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters
   * @return Handler result indicating success or failure
   * @details Processes POST requests:
   *          - Not directly handled
   *          - Redirects to route registration
   *          - Returns appropriate error
   */
  HandlerResult handlePost(const String& uri,
                           const std::map<String, String>& params) override {
    return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                               "Use registerRoutes instead");
  }

 private:
  friend class WebManager;

  /**
   * @brief Handle display configuration page
   * @details Serves configuration interface with AJAX handlers
   */
  void handleDisplayConfig();

  /**
   * @brief Handle screen duration update via AJAX
   */
  void handleScreenDurationUpdate();

  /**
   * @brief Handle clock format update via AJAX
   */
  void handleClockFormatUpdate();

  /**
   * @brief Handle display toggle settings (IP, clock, images) via AJAX
   */
  void handleDisplayToggle();

  /**
   * @brief Handle measurement display toggle via AJAX
   */
  void handleMeasurementDisplayToggle();

  /**
   * @brief Validate display configuration request
   * @return true if request is valid, false otherwise
   */
  bool validateRequest() const;
};

#endif  // USE_DISPLAY

#endif  // ADMIN_DISPLAY_HANDLER_H
