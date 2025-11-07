/**
 * @file startpage_handler.h
 * @brief Handler for main landing page functionality
 * @details Provides comprehensive start page functionality including:
 *          - Sensor status display
 *          - System information
 *          - Update mode configuration
 *          - Real-time monitoring
 *          - User interface generation
 */

#ifndef STARTPAGE_HANDLER_H
#define STARTPAGE_HANDLER_H

#include "managers/manager_sensor.h"
#include "web/core/web_auth.h"
#include "web/handler/base_handler.h"
#include "web/services/css_service.h"

/**
 * @class StartpageHandler
 * @brief Manages main landing page functionality
 * @details Handles all aspects of the start page:
 *          - Sensor data display
 *          - Status monitoring
 *          - Interface generation
 *          - Update configuration
 *          - User interaction
 */
class StartpageHandler : public BaseHandler {
public:
  /**
   * @brief Constructor for StartpageHandler
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @param cssService Reference to CSS management service
   * @details Initializes start page handler:
   *          - Sets up server connection
   *          - Configures authentication
   *          - Prepares CSS handling
   *          - Initializes logging
   */
  StartpageHandler(ESP8266WebServer& server, WebAuth& auth, CSSService& cssService)
      : BaseHandler(server), _auth(auth), _cssService(cssService) {
    logger.debug(F("StartpageHandler"), F("Initialisiere StartpageHandler"));
    logger.logMemoryStats(F("StartpageHandler"));
  }

  /**
   * @brief Destructor
   * @details Ensures proper cleanup and vtable generation.
   */
  virtual ~StartpageHandler();

  /**
   * @brief Register routes with the router
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Sets up routes for:
   *          - Main landing page
   *          - Update configuration
   *          - Status endpoints
   *          - Data refresh
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

  /**
   * @brief Handle GET requests
   * @param uri Request URI
   * @param query Query parameters
   * @return Handler result indicating success or failure
   * @details Processes GET requests for:
   *          - Main page content
   *          - Status updates
   *          - Sensor data
   */
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters
   * @return Handler result indicating success or failure
   * @details Processes POST requests for:
   *          - Configuration updates
   *          - Mode changes
   *          - Control commands
   */
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

  /**
   * @brief Handle update mode configuration
   * @details Manages update mode settings:
   *          - Mode activation
   *          - Configuration changes
   *          - Status updates
   */
  void handleUpdateModeConfig();

  /**
   * @brief Generate and send sensor grid
   * @details Creates sensor display grid:
   *          - Formats sensor data
   *          - Arranges layout
   *          - Applies styling
   *          - Updates content
   */
  void generateAndSendSensorGrid();

  /**
   * @brief Generate sensor display box
   * @param sensor Pointer to sensor instance
   * @param value Current sensor value
   * @param name Sensor name
   * @param unit Measurement unit
   * @param status Sensor status
   * @param measurementIndex Index of the measurement
   * @param sensorIndex Global index for left/right positioning
   * @details Creates individual sensor display:
   *          - Formats data
   *          - Applies styling
   *          - Shows status
   *          - Handles errors
   */
  void generateSensorBox(const Sensor* sensor, float value, const String& name, const String& unit,
                         const char* status, size_t measurementIndex, size_t sensorIndex);

private:
  friend class WebManager; // Allow WebManager access to private members
  WebAuth& _auth;          ///< Reference to authentication service
  CSSService& _cssService; ///< Reference to CSS service

  /**
   * @brief Handle root page request
   * @details Generates main landing page:
   *          - Assembles components
   *          - Updates content
   *          - Manages state
   *          - Handles errors
   */
  void handleRoot();

  /**
   * @brief Translate status code to display text
   * @param status Status code to translate
   * @return Translated status text
   * @details Converts status codes:
   *          - Maps to readable text
   *          - Handles translations
   *          - Manages unknown states
   */
  const char* translateStatus(const char* status) const;
};

#endif // STARTPAGE_HANDLER_H
