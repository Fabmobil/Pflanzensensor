/**
 * @file web_metrics_handler.h
 * @brief Prometheus metrics HTTP handler
 * @details Handles /metrics endpoint and exports metrics in Prometheus format
 */

#ifndef WEB_METRICS_HANDLER_H
#define WEB_METRICS_HANDLER_H

#include "utils/platform_compat.h"

#include "managers/manager_base.h"
#include "managers/manager_sensor.h"
#include "utils/memory_manager.h"
#include "utils/result_types.h"

/**
 * @class WebMetricsHandler
 * @brief HTTP handler for Prometheus metrics endpoint
 * @details Exposes sensor and system metrics via /metrics endpoint:
 *          - System metrics (uptime, memory, WiFi)
 *          - Sensor readings
 *          - Request counters
 */
class WebMetricsHandler : public Manager {
public:
  /**
   * @brief Constructs a new WebMetricsHandler instance
   */
  WebMetricsHandler();

  /**
   * @brief Set sensor manager reference
   * @param sensorManager Reference to sensor manager instance
   */
  void setSensorManager(SensorManager& sensorManager);

  /**
   * @brief Set active WebSocket connections count
   * @param count Current number of active connections
   */
  void setActiveConnections(uint8_t count);

  /**
   * @brief Increment request counter for a handler
   * @param handlerName Handler name
   * @param statusCode HTTP status code
   */
  void incrementRequestCounter(const char* handlerName, int statusCode);

  /**
   * @brief Handle metrics request
   * @return String containing Prometheus-formatted metrics
   */
  String handleMetrics();

  /**
   * @brief Check if metrics handler is enabled
   * @return true if enabled, false otherwise
   */
  bool isEnabled() const { return getState() == ManagerState::INITIALIZED; }

private:
  SensorManager* _sensorManager{nullptr}; ///< Reference to sensor manager

  uint8_t _activeConnections{0};             ///< Active WebSocket connections
  std::map<String, uint32_t> _requestCounts; ///< Request counters per handler

  /**
   * @brief Collect and export all metrics
   * @return String containing formatted Prometheus metrics
   */
  String collectMetrics();

  /**
   * @brief Collect system metrics
   * @param deviceName Device name for metric labels
   */
  void collectSystemMetrics(String deviceName);

  /**
   * @brief Collect sensor metrics
   * @param deviceName Device name for metric labels
   */
  void collectSensorMetrics(String deviceName);

protected:
  /**
   * @brief Initialize the metrics handler
   * @return Always success for metrics handler
   */
  TypedResult<ResourceError, void> initialize() override {
    setState(ManagerState::INITIALIZED);
    return TypedResult<ResourceError, void>::success();
  }
};

#endif // WEB_METRICS_HANDLER_H
