// src/sensors/sensor_manager_limiter.h
/**
 * @file sensor_manager_limiter.h
 * @brief Manages access control for sensor measurements
 * @details Implements a singleton pattern to control concurrent access to
 * sensor measurement slots, preventing multiple sensors from measuring
 * simultaneously when they might interfere with each other.
 */
#ifndef SENSOR_MANAGER_LIMITER_H
#define SENSOR_MANAGER_LIMITER_H

#include <Arduino.h>

#include "logger/logger.h"
#include "managers/manager_config.h"

/**
 * @class SensorManagerLimiter
 * @brief Manages measurement slots for sensors to prevent concurrent access
 * @details Implements a singleton pattern to ensure only one sensor can perform
 *          measurements at a time when they might interfere with each other.
 *          Includes timeout mechanisms to prevent deadlocks.
 */
class SensorManagerLimiter {
 public:
  /// Maximum time a sensor can hold a measurement slot before forced release
  static constexpr unsigned long SLOT_TIMEOUT_MS = 20000;  // 20 second timeout

  /**
   * @brief Gets the singleton instance of the limiter
   * @return Reference to the singleton instance
   */
  static SensorManagerLimiter& getInstance() {
    static SensorManagerLimiter instance;
    return instance;
  }

  /**
   * @brief Attempts to acquire a measurement slot for a sensor
   * @param sensorId Unique identifier of the requesting sensor
   * @return true if slot was acquired, false if slot is currently held by
   * another sensor
   * @details Checks if slot is available or if current holder has timed out.
   *          If current holder has timed out, forces release of the slot.
   */
  bool acquireSlot(const String& sensorId) {
    unsigned long now = millis();

    // Check if current holder has timed out
    if (!m_currentSensor.isEmpty() &&
        (now - m_slotAcquiredTime >= SLOT_TIMEOUT_MS)) {
      logger.warning(F("SensorLimiter"), F("Forcing slot release from ") +
                                             m_currentSensor +
                                             F(" due to timeout"));
      m_currentSensor = "";
    }

    if (m_currentSensor.isEmpty()) {
      m_currentSensor = sensorId;
      m_slotAcquiredTime = now;
      if (ConfigMgr.isDebugMeasurementCycle()) {
        logger.debug(F("SensorLimiter"), F("Slot acquired by: ") + sensorId);
      }
      return true;
    }

    if (ConfigMgr.isDebugMeasurementCycle() &&
        m_lastBlockingSensor != m_currentSensor) {
      logger.debug(F("SensorLimiter"),
                   F("Slot acquisition failed for ") + sensorId +
                       F(" - currently held by: ") + m_currentSensor);
      m_lastBlockingSensor = m_currentSensor;
    }
    return false;
  }

  /**
   * @brief Releases a measurement slot held by a sensor
   * @param sensorId Unique identifier of the sensor releasing the slot
   * @details Only allows release by the current slot holder.
   *          Logs warning if a different sensor attempts to release the slot.
   */
  void releaseSlot(const String& sensorId) {
    if (m_currentSensor == sensorId) {
      if (ConfigMgr.isDebugMeasurementCycle()) {
        logger.debug(F("SensorLimiter"), F("Slot released by: ") + sensorId);
      }
      m_currentSensor = "";
      m_slotAcquiredTime = 0;
    } else if (!m_currentSensor.isEmpty()) {
      logger.warning(F("SensorLimiter"),
                     F("Attempt to release slot by ") + sensorId +
                         F(" but slot is held by: ") + m_currentSensor);
    }
  }

  /**
   * @brief Checks if a sensor currently holds the measurement slot
   * @param sensorId Unique identifier of the sensor to check
   * @return true if the specified sensor holds the slot
   */
  bool hasSlot(const String& sensorId) const {
    return m_currentSensor == sensorId;
  }

  /**
   * @brief Gets the ID of the sensor currently holding the slot
   * @return Reference to the current sensor's ID string
   */
  const String& getCurrentSensor() const { return m_currentSensor; }

  /**
   * @brief Gets the duration the current slot has been held
   * @return Time in milliseconds the slot has been held, 0 if slot is free
   */
  unsigned long getSlotHoldTime() const {
    if (m_slotAcquiredTime == 0) return 0;
    return millis() - m_slotAcquiredTime;
  }

 private:
  /**
   * @brief Private constructor for singleton pattern
   * @details Initializes random seed for potential future use
   */
  SensorManagerLimiter() { randomSeed(millis()); }

  /**
   * @brief Default destructor
   */
  ~SensorManagerLimiter() = default;

  // Prevent copying
  SensorManagerLimiter(const SensorManagerLimiter&) =
      delete;  ///< Copy constructor disabled
  SensorManagerLimiter& operator=(const SensorManagerLimiter&) =
      delete;  ///< Assignment operator disabled

  String m_currentSensor;       ///< ID of sensor currently holding the slot
  String m_lastBlockingSensor;  ///< ID of last sensor that was blocked from
                                ///< acquiring slot
  unsigned long m_slotAcquiredTime{
      0};  ///< Timestamp when current slot was acquired
};

#endif  // SENSOR_MANAGER_LIMITER_H
