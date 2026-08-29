/**
 * @file manager_sensor.h
 * @brief Header file containing the SensorManager class for managing sensor
 * operations
 * @details This file defines the SensorManager class which handles sensor
 * initialization, measurement cycles, and cleanup of all sensor-related
 * operations in the system.
 */

#ifndef MANAGER_SENSOR_H
#define MANAGER_SENSOR_H

#include <map>
#include <memory>

#include "configs/config_validation_rules.h"
#include "managers/manager_base.h"
#include "managers/manager_config.h"
#include "sensors/sensor_factory.h"
#include "sensors/sensor_measurement_cycle.h"
#include "sensors/sensors.h"

/**
 * @class SensorManager
 * @brief Manages all sensor-related operations in the system
 * @details The SensorManager class is responsible for:
 *          - Managing the lifecycle of all sensors
 *          - Coordinating sensor measurements
 *          - Tracking sensor states and measurement cycles
 *          - Handling sensor cleanup and resource management
 * @inherits Manager
 */
class SensorManager : public Manager {
public:
  /**
   * @brief Constructs a new SensorManager instance
   * @details Initializes the sensor management system and sets up logging
   */
  SensorManager() : Manager("SensorManager") {}

  /**
   * @brief Destroys the SensorManager instance
   * @details Performs cleanup of all sensor resources and managed objects
   */
  ~SensorManager() { cleanup(); }

  /**
   * @brief Updates measurements for all enabled sensors
   * @details Processes each sensor's measurement cycle and handles state
   * transitions. This method:
   *          - Checks each sensor's enabled status
   *          - Manages measurement state transitions
   *          - Processes measurement cycles when appropriate
   *          - Handles debug logging of state changes
   * @note Only processes sensors if the manager is in INITIALIZED state
   */
  void updateMeasurements();

  /**
   * @brief Retrieves a sensor by its ID
   * @param id The unique identifier of the sensor
   * @return Pointer to the sensor if found, nullptr otherwise
   */
  Sensor* getSensor(const String& id) {
    // Remove excessive runtime logging - this is called constantly during
    // normal operation
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(),
                           [&id](const auto& sensor) { return sensor && sensor->getId() == id; });
    return it != m_sensors.end() ? it->get() : nullptr;
  }

  /**
   * @brief Gets all sensors managed by this class
   * @return Const reference to the vector of sensor pointers
   */
  const std::vector<std::unique_ptr<Sensor>>& getSensors() const { return m_sensors; }

  /**
   * @brief Stops all sensors and deinitializes them if required
   * @return SensorResult indicating success or failure
   */
  SensorResult stopAll() {
    logger.debug(F("SensorManager"), F("stopAll aufgerufen"));
    for (auto& sensor : m_sensors) {
      if (sensor) {
        sensor->stop();
        if (sensor->shouldDeinitializeAfterMeasurement()) {
          sensor->deinitialize();
        }
      }
    }
    return SensorResult::success();
  }

  /**
   * @brief Cleans up all sensor resources
   * @details Stops all sensors and clears internal containers
   */
  void cleanup() {
    stopAll();
    m_cycleManagers.clear();
    m_sensors.clear();
  }

  /**
   * @brief Forces the next measurement for a sensor ASAP
   * @param id The unique identifier of the sensor
   * @return true if successful, false otherwise
   */
  bool forceImmediateMeasurement(const String& id) {
    auto it = m_cycleManagers.find(id);
    if (it == m_cycleManagers.end() || !it->second)
      return false;
    auto* cycleManager = it->second.get();
    cycleManager->forceImmediateMeasurement();
    return true;
  }

  /**
   * @brief Applies sensor settings from the configuration file
   * @details Loads sensor configuration from /sensors.json and applies
   *          settings to all initialized sensors
   */
  void applySensorSettingsFromConfig();

protected:
  /**
   * @brief Initialisiert das Sensormanagement-System
   * @return TypedResult mit Erfolg oder Fehlerdetails
   * @details Erstellt Sensoren über die Factory und richtet Zyklusmanager ein
   */
  TypedResult<ResourceError, void> initialize() override;

private:
  static constexpr unsigned long MEMORY_LOG_INTERVAL = 60000; // 1 minute

  std::vector<std::unique_ptr<Sensor>> m_sensors;
  std::map<String, std::unique_ptr<SensorMeasurementCycleManager>> m_cycleManagers;
  unsigned long m_lastMemoryLog{0};

  /**
   * @struct SensorStateLog
   * @brief Tracks the state and update history of a sensor
   */
  struct SensorStateLog {
    MeasurementState lastState{MeasurementState::WAITING_FOR_DUE};
    bool lastUpdateResult{false};
    unsigned long lastStateLogTime{0};
    static constexpr unsigned long LOG_THROTTLE_INTERVAL =
        5000; // Only log same state every 5 seconds
  };
  std::map<String, SensorStateLog> m_sensorStates;
};

#endif // MANAGER_SENSOR_H
