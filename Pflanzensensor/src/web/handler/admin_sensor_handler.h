/**
 * @file admin_sensor_handler.h
 * @brief Core header for sensor configuration handler
 */

#pragma once

#include "managers/manager_sensor.h"
#include "web/core/web_auth.h"
#include "web/handler/base_handler.h"

class CSSService; ///< Forward declaration for CSS service
class WebManager; ///< Forward declaration for web manager

/**
 * @class AdminSensorHandler
 * @brief Handles administrative sensor management
 * @details Provides functionality for:
 *          - Sensor configuration
 *          - Threshold management
 *          - Measurement triggering
 *          - Security validation
 *          - Interface generation
 */
class AdminSensorHandler : public BaseHandler {
  friend class WebManager; // Allow WebManager to access private members
public:
  /**
   * @brief Constructor for sensor handler
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @param cssService Reference to CSS management service
   * @param sensorManager Reference to sensor management service
   */
  AdminSensorHandler(ESP8266WebServer& server, WebAuth& auth, CSSService& cssService,
                     SensorManager& sensorManager)
      : BaseHandler(server), _auth(auth), _cssService(cssService), _sensorManager(sensorManager) {}

  // Core routing and request handling
  /**
   * @brief Register admin sensor routes with the router
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Registers all sensor admin endpoints:
   *          - Sensor config
   *          - Thresholds
   *          - Measurement control
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

  // Main page handlers
  void handleSensorConfig();
  void handleSensorUpdate();
  void handleTriggerMeasurement();
  void handleFlowerStatusUpdate(const std::map<String, String>& params);

  // AJAX handlers
  void handleSingleSensorUpdate();
  void handleGetSensorConfigJson();
  void handleMeasurementInterval();
  void handleAnalogMinMax();
  void handleAnalogInverted();
  void handleThresholds();
  void handleMeasurementName();
  void handleResetAbsoluteMinMax();
  void handleResetAbsoluteRawMinMax();
  void handleAnalogAutocal();
  // NOTE: handleResetAutoCalibration removed — use reset absolute raw/min endpoints instead

  // Add this declaration for the new UI row rendering function
  void renderSensorMeasurementRow(Sensor* sensor, size_t i, size_t nRows);

  // Flower status sensor configuration
  void renderFlowerStatusSensorCard();

protected:
  WebAuth& _auth;                ///< Reference to authentication service
  CSSService& _cssService;       ///< Reference to CSS service
  SensorManager& _sensorManager; ///< Reference to sensor manager

  // Threshold management
  void generateThresholdConfig(Sensor* sensor, size_t measurementIdx);
  bool processThresholds(Sensor* sensor, size_t measurementIdx);
  bool updateThreshold(const String& sensorId, const String& thresholdName,
                       const float& currentValue, float& newValue);

  // Security
  bool validateRequest() const;
};
