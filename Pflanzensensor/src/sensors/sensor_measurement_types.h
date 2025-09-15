/**
 * @file sensor_measurement_types.h
 * @brief Defines types and classes for managing sensor measurements and queuing
 */
#ifndef SENSOR_MEASUREMENT_TYPES_H
#define SENSOR_MEASUREMENT_TYPES_H

#include <queue>

#include "sensor_manager_limiter.h"
#include "sensors/sensor_types.h"
#include "sensors/sensors.h"

/**
 * @enum SensorQueueState
 * @brief Represents the current state of a sensor in the measurement queue
 */
enum class SensorQueueState {
  FREE,              ///< No sensor measuring
  WAITING_FOR_SLOT,  ///< Sensor wants to measure but slot occupied
  INITIALIZING,      ///< Sensor getting initialized
  MEASURING,         ///< Sensor actively measuring
  CLEANUP            ///< Sensor finishing/deinitializing
};

/**
 * @struct SensorTiming
 * @brief Tracks timing information for sensor measurements
 */
struct SensorTiming {
  unsigned long lastMeasurement{0};  ///< Timestamp of last measurement
  unsigned long nextDueTime{0};      ///< Timestamp when next measurement is due
  uint8_t errorCount{0};             ///< Count of consecutive errors

  /**
   * @brief Checks if a new measurement is due
   * @return true if measurement is due, false otherwise
   */
  bool isDue() const {
    if (lastMeasurement == 0) return true;  // First measurement
    unsigned long now = millis();
    return now >= nextDueTime;
  }

  /**
   * @brief Updates timing information after a measurement
   * @param interval Time interval until next measurement in milliseconds
   */
  void updateTiming(unsigned long interval) {
    lastMeasurement = millis();
    nextDueTime = lastMeasurement + interval;
  }

  /**
   * @brief Resets all timing information to initial state
   */
  void reset() {
    lastMeasurement = 0;
    nextDueTime = 0;
    errorCount = 0;
  }
};

/**
 * @class SensorQueue
 * @brief Manages a queue of sensors waiting to take measurements
 */
class SensorQueue {
 public:
  /**
   * @brief Adds a sensor to the measurement queue
   * @param sensor Pointer to the sensor to enqueue
   * @return true if sensor was successfully queued, false otherwise
   */
  bool enqueue(Sensor* sensor) {
    if (!sensor || !sensor->isEnabled()) {
      return false;
    }

    auto timing = m_timings.find(sensor);
    if (timing == m_timings.end()) {
      m_timings[sensor] = SensorTiming();
      timing = m_timings.find(sensor);
    }

    if (timing->second.isDue()) {
      // Check if sensor is already in queue
      for (const auto* queued : m_queue) {
        if (queued == sensor) {
          return false;  // Already queued
        }
      }

      m_queue.push_back(sensor);
      logger.debug(F("SensorQueue"),
                   sensor->getName() + F(": Added to measurement queue"));
      return true;
    }
    return false;
  }

  /**
   * @brief Processes the next sensor in the queue
   * @details Handles initialization, measurement start, and error handling
   */
  void processNext() {
    if (m_state == SensorQueueState::FREE && !m_queue.empty()) {
      m_activeSensor = m_queue.front();
      m_queue.erase(m_queue.begin());  // Remove from front of queue

      if (!SensorManagerLimiter::getInstance().acquireSlot(
              m_activeSensor->getId())) {
        m_state = SensorQueueState::WAITING_FOR_SLOT;
        m_queue.push_back(m_activeSensor);  // Put back in queue
        m_activeSensor = nullptr;
        return;
      }

      if (MEASUREMENT_DEINITIALIZE_SENSORS &&
          !m_activeSensor->isInitialized()) {
        m_state = SensorQueueState::INITIALIZING;
        if (!m_activeSensor->initialize()) {
          handleError("Failed to initialize");
          return;
        }
      }

      m_state = SensorQueueState::MEASURING;
      if (!m_activeSensor->startMeasurement()) {
        handleError("Failed to start measurement");
        return;
      }

      logger.debug(F("SensorQueue"),
                   m_activeSensor->getName() + F(": Starting measurement"));
    }
  }

 private:
  static constexpr uint8_t MAX_RETRIES =
      2;                            ///< Maximum number of retry attempts
  std::vector<Sensor*> m_queue;     ///< Queue of sensors waiting to measure
  Sensor* m_activeSensor{nullptr};  ///< Currently active sensor
  SensorQueueState m_state{SensorQueueState::FREE};  ///< Current queue state
  std::map<Sensor*, SensorTiming> m_timings;  ///< Timing info for each sensor

  /**
   * @brief Handles errors during sensor measurement
   * @param message Error message to log
   * @details Manages error counting, sensor cleanup, and retry logic
   */
  void handleError(const String& message) {
    if (!m_activeSensor) return;

    logger.error(F("SensorQueue"),
                 m_activeSensor->getName() + F(": ") + message);

    auto& timing = m_timings[m_activeSensor];
    timing.errorCount++;

    if (MEASUREMENT_DEINITIALIZE_SENSORS) {
      m_activeSensor->deinitialize();
    }

    SensorManagerLimiter::getInstance().releaseSlot(m_activeSensor->getId());

    // Re-queue if retries left
    if (timing.errorCount < MAX_RETRIES) {
      m_queue.push_back(m_activeSensor);
    }

    m_activeSensor = nullptr;
    m_state = SensorQueueState::FREE;
  }
};

#endif  // SENSOR_MEASUREMENT_TYPES_H
