/**
 * @file sensor_handler.h
 * @brief Handler for sensor-related web functionality
 * @details Provides comprehensive sensor management including:
 *          - Sensor data retrieval
 *          - Configuration interface
 *          - Calibration controls
 *          - Real-time monitoring
 *          - Authentication handling
 */

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

#include <map>

#include "managers/manager_sensor.h"
#include "utils/result_types.h"
#include "web/core/components.h"
#include "web/core/web_auth.h"
#include "web/core/web_router.h"
#include "web/handler/base_handler.h"
#include "web/services/css_service.h"

/**
 * @class SensorHandler
 * @brief Handler for sensor-related web requests
 * @details Manages all sensor-related web functionality:
 *          - Data retrieval and display
 *          - Configuration management
 *          - Calibration interface
 *          - Authentication
 *          - Status monitoring
 */
class SensorHandler : public BaseHandler {
 public:
  static constexpr size_t MAX_VALUES =
      10;  // Maximum number of values per sensor

  /**
   * @brief Constructor for sensor handler
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @param cssService Reference to CSS management service
   * @param sensorManager Reference to sensor management service
   * @details Initializes handler with required services:
   *          - Server connection
   *          - Authentication
   *          - CSS management
   *          - Sensor management
   */
  SensorHandler(ESP8266WebServer& server, WebAuth& auth, CSSService& cssService,
                SensorManager& sensorManager)
      : BaseHandler(server),
        _auth(auth),
        _cssService(cssService),
        _sensorManager(sensorManager) {
    logger.debug(F("SensorHandler"), F("Initializing SensorHandler"));
  }

  /**
   * @brief Custom cleanup logic for SensorHandler
   * @details Clears cached content and ensures proper cleanup.
   */
  void onCleanup() override { _content.clear(); }

  /**
   * @brief Register sensor routes
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Sets up routes for:
   *          - Data retrieval
   *          - Configuration
   *          - Calibration
   *          - Status updates
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

 protected:
  /**
   * @brief Handle GET requests
   * @param uri Request URI
   * @param query Query parameters
   * @return Handler result indicating success or failure
   * @details Processes GET requests for:
   *          - Sensor data
   *          - Configuration
   *          - Status information
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
   *          - Calibration commands
   *          - Control operations
   */
  HandlerResult handlePost(const String& uri,
                           const std::map<String, String>& params) override;

 private:
  WebAuth& _auth;                 ///< Reference to authentication service
  CSSService& _cssService;        ///< Reference to CSS service
  SensorManager& _sensorManager;  ///< Reference to sensor manager
  String _content;                ///< Cached content storage

  /**
   * @brief Handle requests for latest sensor values
   * @details Retrieves and formats latest data:
   *          - Gets current values
   *          - Formats response
   *          - Handles errors
   *          - Updates cache
   */
  void handleGetLatestValues();

  /**
   * @brief Create login redirect URL
   * @return URL string for login redirect
   * @details Generates secure redirect:
   *          - Includes return path
   *          - Adds security tokens
   *          - Handles encoding
   */
  String createLoginRedirect() const;

  /**
   * @brief Generate sensor configuration HTML
   * @return HTML string for configuration section
   * @details Creates configuration interface:
   *          - Current settings
   *          - Input controls
   *          - Validation rules
   *          - Error handling
   */
  String createSensorConfigSection() const;

  /**
   * @brief Generate sensor list HTML
   * @details Creates sensor list display:
   *          - Sensor status
   *          - Current values
   *          - Control options
   *          - Error states
   */
  void createSensorListSection() const;

  /**
   * @brief Convert string to URL-safe format
   * @param str String to convert
   * @return URL-safe version of string
   * @details Makes string URL-safe:
   *          - Encodes special chars
   *          - Handles spaces
   *          - Preserves format
   */
  String toUrlSafe(const String& str) const;

  /**
   * @brief Validate request authentication
   * @return true if request is valid and authorized
   * @details Verifies request validity:
   *          - Checks authentication
   *          - Validates permissions
   *          - Verifies tokens
   *          - Logs attempts
   */
  bool validateRequest() const;
};

#endif
