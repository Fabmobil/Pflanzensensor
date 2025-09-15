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
  void updateMeasurements() {
    if (getState() != ManagerState::INITIALIZED) {  // Using getState() from
                                                    // Manager base class
      return;
    }

    for (const auto& sensor : m_sensors) {
      if (!sensor || !sensor->isEnabled()) {
        continue;
      }

      auto& stateLog = m_sensorStates[sensor->getId()];
      auto cycleManager = m_cycleManagers[sensor->getId()].get();

      if (!cycleManager) {
        logger.error(F("SensorManager"),
                     F("No cycle manager for sensor: ") + sensor->getId());
        continue;
      }

      MeasurementState currentState = cycleManager->getCurrentState();
      unsigned long now = millis();

      // Check for state changes and update tracking
      bool stateChanged = (currentState != stateLog.lastState);
      stateLog.lastState = currentState;  // Update state tracking immediately

      // Log only actual state changes
      if (stateChanged && ConfigMgr.isDebugMeasurementCycle()) {
        logger.debug(F("SensorManager"),
                     F("Sensor: ") + sensor->getId() + F(" State: ") +
                         String(static_cast<int>(currentState)) +
                         F(" (Changed)"));
        stateLog.lastStateLogTime = now;
      }

      // Process measurement cycle if:
      // 1. Sensor is in WAITING_FOR_DUE state and is due, or
      // 2. Sensor is in any other active state
      bool shouldProcess = (currentState == MeasurementState::WAITING_FOR_DUE &&
                            cycleManager->isDue()) ||
                           (currentState != MeasurementState::WAITING_FOR_DUE);

      if (shouldProcess) {
        bool cycleResult = cycleManager->updateMeasurementCycle();
        bool resultChanged = (cycleResult != stateLog.lastUpdateResult);
        stateLog.lastUpdateResult =
            cycleResult;  // Update result tracking immediately

        // Log only on result changes
        if (resultChanged && ConfigMgr.isDebugMeasurementCycle()) {
          logger.debug(F("SensorManager"),
                       F("Sensor: ") + sensor->getId() + F(" Cycle: ") +
                           (cycleResult ? F("Complete") : F("In Progress")) +
                           F(" (Changed)"));
        }
      }

      // Allow other processes to run
      yield();
    }
  }

  /**
   * @brief Retrieves a sensor by its ID
   * @param id The unique identifier of the sensor
   * @return Pointer to the sensor if found, nullptr otherwise
   */
  Sensor* getSensor(const String& id) {
    // Remove excessive runtime logging - this is called constantly during
    // normal operation
    auto it = std::find_if(
        m_sensors.begin(), m_sensors.end(),
        [&id](const auto& sensor) { return sensor && sensor->getId() == id; });
    return it != m_sensors.end() ? it->get() : nullptr;
  }

  /**
   * @brief Gets all sensors managed by this class
   * @return Const reference to the vector of sensor pointers
   */
  const std::vector<std::unique_ptr<Sensor>>& getSensors() const {
    return m_sensors;
  }

  /**
   * @brief Stops all sensors and deinitializes them if required
   * @return SensorResult indicating success or failure
   */
  SensorResult stopAll() {
    logger.debug(F("SensorManager"), F("stopAll called"));
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
    if (it == m_cycleManagers.end() || !it->second) return false;
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
   * @brief Initializes the sensor management system
   * @return TypedResult indicating success or failure with error details
   * @details Creates sensors using factory and sets up cycle managers
   */
  TypedResult<ResourceError, void> initialize() override {
    // Create sensors using factory
    auto result = SensorFactory::createAllSensors(m_sensors, this);
    if (!result.isSuccess() && !result.isPartialSuccess()) {
      return TypedResult<ResourceError, void>::fail(
          ResourceError::OPERATION_FAILED,
          F("Failed to create sensors: ") + result.getMessage());
    }

    if (result.isPartialSuccess()) {
      logger.warning(F("SensorM"), F("Some sensors failed to initialize: ") +
                                       result.getMessage());
    }

    // Sensor construction is now complete

    // Check for previously failed sensors
    bool hasFailedSensors = false;
    for (const auto& sensor : m_sensors) {
      if (sensor && sensor->config().hasPersistentError) {
        // Skip re-initialization for sensors that were deinitialized during
        // factory testing This prevents accessing deallocated measurement data
        if (!sensor->isInitialized()) {
          logger.debug(F("SensorM"), F("Previously failed sensor ") +
                                         sensor->getName() +
                                         F(" was deinitialized during factory "
                                           "testing, clearing error flag"));
          // Clear the error flag since the sensor is working (it was just
          // deinitialized for memory savings)
          sensor->mutableConfig().hasPersistentError = false;
          continue;
        }

        if (sensor->init().isSuccess()) {
          // Sensor recovered after reboot
          logger.info(F("SensorM"), F("Previously failed sensor ") +
                                        sensor->getName() +
                                        F(" recovered after reboot"));
          sensor->mutableConfig().hasPersistentError = false;
        } else {
          // Sensor still failing after reboot
          logger.error(F("SensorM"), F("Previously failed sensor ") +
                                         sensor->getName() +
                                         F(" still failing after reboot"));
          sensor->stop();
          hasFailedSensors = true;
        }
      }
    }

    // Log enabled sensor details
    logger.debug(F("SensorM"), F("Checking enabled sensors:"));
    for (const auto& sensor : m_sensors) {
      if (sensor) {
        String msg = F("Sensor ID: ");
        msg += sensor->getId();
        msg += F(", Name: ");
        msg += sensor->getName();
        msg += F(", Enabled: ");
        msg += sensor->isEnabled() ? F("yes") : F("no");
        logger.debug(F("SensorM"), msg);
      }
    }

    // Create cycle managers for each sensor
    size_t enabledCount = 0;
    for (auto& sensor : m_sensors) {
      if (sensor && sensor->isEnabled()) {
        auto cycleManager =
            std::make_unique<SensorMeasurementCycleManager>(sensor.get());
        String sensorId = sensor->getId();
        m_cycleManagers[sensorId] = std::move(cycleManager);
        enabledCount++;
        logger.debug(F("SensorM"),
                     F("Created cycle manager for sensor: ") + sensorId);
      }
    }

    String msg = F("Created ");
    msg += String(enabledCount);
    msg += F(" cycle managers out of ");
    msg += String(m_sensors.size());
    msg += F(" total sensors");
    logger.debug(F("SensorM"), msg);

    logger.info(F("SensorM"),
                F("Sensor manager initialization completed with ") +
                    String(m_sensors.size()) + F(" sensors (") +
                    String(enabledCount) + F(" enabled)"));

    // Set state to INITIALIZED before applying settings
    setState(ManagerState::INITIALIZED);

    // Apply sensor settings from configuration file AFTER setting state to
    // INITIALIZED This ensures that the sensor manager is ready when loading
    // configuration
    applySensorSettingsFromConfig();

    if (hasFailedSensors) {
      return TypedResult<ResourceError, void>::partialSuccess(
          F("Some sensors are still failing after reboot"));
    }

    return TypedResult<ResourceError, void>::success();
  }

 private:
  static constexpr unsigned long MEMORY_LOG_INTERVAL = 60000;  // 1 minute

  std::vector<std::unique_ptr<Sensor>> m_sensors;
  std::map<String, std::unique_ptr<SensorMeasurementCycleManager>>
      m_cycleManagers;
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
        5000;  // Only log same state every 5 seconds
  };
  std::map<String, SensorStateLog> m_sensorStates;
};

#endif  // MANAGER_SENSOR_H
