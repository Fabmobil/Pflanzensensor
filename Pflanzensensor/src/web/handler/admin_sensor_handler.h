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
  AdminSensorHandler(ESPWebServer& server, WebAuth& auth, CSSService& cssService,
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

  /**
   * @brief Gehört diese URL zu diesem Handler?
   * @param url Angefragter Pfad
   * @return true wenn der Handler dafür geladen werden muss
   * @details Wird von der Lazy-Loading-Middleware des WebManagers benutzt. Die
   *          Liste steht direkt neben onRegisterRoutes(), damit beide nicht
   *          auseinanderlaufen können.
   */
  static bool ownsUrl(const String& url);

  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

  // Main page handlers
  void handleSensorConfig();
  void handleSensorUpdate();
  void handleTriggerMeasurement();
  // Note: handleFlowerStatusUpdate removed - use unified /admin/config/setConfigValue

  // AJAX handlers
  void handleSingleSensorUpdate();
  void handleGetSensorConfigJson();
  void handleMeasurementInterval();
  void handleAnalogMinMax();
  // NOTE: handleAnalogInverted removed — use unified setConfigValue with namespace=s_<sensorId>, key=m<idx>_inv
  void handleThresholds();
  void handleMeasurementName();
  void handleResetAbsoluteMinMax();
  void handleResetAbsoluteRawMinMax();
  void handleAnalogAutocal();
  void handleAnalogAutocalDuration();
  // NOTE: handleResetAutoCalibration removed — use reset absolute raw/min endpoints instead

  // Add this declaration for the new UI row rendering function
  void renderSensorMeasurementRow(Sensor* sensor, size_t i, size_t nRows);

  // Flower status sensor configuration
  void renderFlowerStatusSensorCard();

  // LED Traffic Light Settings
  void generateAndSendLedTrafficLightSettingsCard();

protected:
  WebAuth& _auth;                ///< Reference to authentication service
  CSSService& _cssService;       ///< Reference to CSS service
  SensorManager& _sensorManager; ///< Reference to sensor manager

  /**
   * @brief Validiert measurementIndex gegen die aktiven Messungen eines Sensors
   * @param sensor Zeiger auf den Sensor
   * @param measurementIndex Der zu prüfende Index
   * @return true wenn gültig, false wenn ungültig (sendet automatisch 400-Fehler)
   */
  bool validateMeasurementIndex(Sensor* sensor, size_t measurementIndex) {
    if (!sensor || measurementIndex >= sensor->config().activeMeasurements) {
      sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messungsindex\"}"));
      return false;
    }
    return true;
  }

  // Threshold management
  void generateThresholdConfig(Sensor* sensor, size_t measurementIdx);
  bool processThresholds(Sensor* sensor, size_t measurementIdx);
  bool updateThreshold(const String& sensorId, const String& thresholdName,
                       const float& currentValue, float& newValue);

  // Security
  bool validateRequest() const;
};
