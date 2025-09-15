/**
 * @file manager_config_sensor_tracker.h
 * @brief Sensor error tracking and threshold management
 */

#ifndef MANAGER_CONFIG_SENSOR_TRACKER_H
#define MANAGER_CONFIG_SENSOR_TRACKER_H

#include <Arduino.h>

#include <map>

#include "../utils/result_types.h"
#include "manager_config_types.h"

class ConfigNotifier;  // Forward declaration

class SensorErrorTracker {
 public:
  using ErrorResult = TypedResult<ConfigError, void>;

  /**
   * @brief Constructor
   * @param notifier Reference to the configuration notifier
   */
  explicit SensorErrorTracker(ConfigNotifier& notifier);

  /**
   * @brief Set or get persistent error flags directly on each sensor's config.
   * Error tracking is now per-sensor, not via a map.
   */
 private:
  ConfigNotifier& m_notifier;
  // REMOVED: std::map<String, bool> m_sensorErrorFlags;
  // REMOVED: std::map<String, Thresholds> m_sensorThresholds;
};

#endif
