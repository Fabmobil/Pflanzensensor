/**
 * @file sensor_analog_multiplexer.h
 * @brief Header file for controlling an analog sensor multiplexer.
 */

#ifndef SENSOR_ANALOG_MULTIPLEXER_H
#define SENSOR_ANALOG_MULTIPLEXER_H

#include <Arduino.h>

#include "logger/logger.h"
#include "utils/result_types.h"

/**
 * @class Multiplexer
 * @brief Class to control a multiplexer for analog sensors.
 *
 * Maps 8 sensors to multiplexer addresses in reverse order:
 * Sensor 1 (Light level) -> 111 (7)
 * Sensor 2 (Soil moisture) -> 110 (6)
 * Sensor 3 -> 101 (5)
 * Sensor 4 -> 100 (4)
 * Sensor 5 -> 011 (3)
 * Sensor 6 -> 010 (2)
 * Sensor 7 -> 001 (1)
 * Sensor 8 -> 000 (0)
 */
class Multiplexer {
 public:
  /**
   * @brief Constructor
   */
  Multiplexer();

  /**
   * @brief Destructor
   */
  ~Multiplexer();

  /**
   * @brief Initializes the multiplexer pins.
   * @return SensorResult indicating success or failure with error details
   */
  SensorResult init();

  /**
   * @brief Switches the multiplexer to the specified sensor.
   * @param sensorIndex Index of the sensor (0-7)
   * @return true if switch is complete, false if still waiting for settling
   * time
   */
  bool switchToSensor(int sensorIndex);

 private:
  static constexpr unsigned long SWITCH_DELAY = 50;     // 50ms settling time
  static constexpr unsigned long SWITCH_TIMEOUT = 100;  // 100ms timeout
  static constexpr int MAX_CHANNELS = 8;  // Maximum number of channels

  bool m_initialized{false};           // Track initialization state
  unsigned long m_switchStartTime{0};  // Track when switch started
  bool m_switchInProgress{false};      // Track if switch is in progress
  int m_currentChannel{-1};            // Currently selected channel
  int m_targetChannel{-1};             // Target channel during switch

  /**
   * @brief Verifies that multiplexer pins match expected states
   * @param muxAddress Address to verify (0-7)
   * @return true if states match, false otherwise
   */
  bool verifyPinStates(int muxAddress);
};

#endif  // SENSOR_ANALOG_MULTIPLEXER_H
