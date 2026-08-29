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
#include "managers/manager_sensor_preemption.h"
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
    LOG_DEBUG(F("SensorManager"), F("stopAll aufgerufen"));
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
    m_sensors.clear(); // Zyklus-Manager gehören den Sensoren und gehen mit
  }

  /**
   * @brief Forces the next measurement for a sensor ASAP
   * @param id The unique identifier of the sensor
   * @return true if successful, false otherwise
   * @details Die eigentliche Entscheidungslogik (wer wird ggf. verdrängt)
   *          steckt in SensorPreemption::forceImmediateMeasurement() - eigene,
   *          hardwareunabhängige Datei, siehe dort. Diese Methode ist nur
   *          noch ein dünner Wrapper, der zusätzlich das Sammelkennzeichen
   *          für main.cpp pflegt.
   */
  bool forceImmediateMeasurement(const String& id) {
    if (!SensorPreemption::forceImmediateMeasurement(m_sensors, id)) {
      return false;
    }
    m_forcedMeasurementActive = true;
    return true;
  }

  /**
   * @brief Läuft gerade eine manuell ausgelöste Messung?
   * @details main.cpp schaltet die Zustandsmaschine normalerweise nur einmal
   *          pro Sekunde weiter. Bei fünf Zustandswechseln bis zur ersten Probe
   *          sind das mehrere Sekunden Wartezeit. Solange dieses Kennzeichen
   *          gesetzt ist, läuft die Zustandsmaschine in jedem
   *          Schleifendurchlauf — die Messung startet damit ohne Verzögerung.
   */
  bool hasForcedMeasurement() const { return m_forcedMeasurementActive; }

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

  // Beide std::map sind entfallen: der Zyklus-Manager gehört jetzt dem Sensor
  // (Sensor::cycleManager()), und der Debug-Zustand liegt im Manager selbst.
  // Das spart pro Sensor einen Rot-Schwarz-Baum-Knoten samt String-Schlüssel
  // und zwei Baumsuchen pro Sekunde.
  std::vector<std::unique_ptr<Sensor>> m_sensors;
  unsigned long m_lastMemoryLog{0};
  /// true, solange mindestens ein Sensor eine manuell ausgelöste Messung fährt.
  /// Wird in updateMeasurements() aus den Zyklusmanagern nachgeführt, damit
  /// main.cpp den Zustand ohne Schleife über alle Sensoren abfragen kann.
  bool m_forcedMeasurementActive{false};
};

#endif // MANAGER_SENSOR_H
