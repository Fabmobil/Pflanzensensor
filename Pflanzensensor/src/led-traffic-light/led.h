/**
 * @file led.h
 * @brief Traffic light LED control implementation
 * @details Provides classes and structures for managing a three-color
 *          traffic light system with red, yellow, and green LEDs.
 */
#ifndef LED_H
#define LED_H

#include "configs/config.h"
#include "utils/result_types.h"

#if USE_LED_TRAFFIC_LIGHT

/**
 * @struct LedStatus
 * @brief Status structure for LED traffic light
 * @details Holds the current state of each LED in the traffic light system.
 *          Each boolean field represents whether the corresponding LED is on or
 * off.
 */
struct LedStatus {
  bool red;    ///< State of the red LED (true = on, false = off)
  bool yellow; ///< State of the yellow LED (true = on, false = off)
  bool green;  ///< State of the green LED (true = on, false = off)
};

/**
 * @class LedLights
 * @brief Controls a traffic light with red, yellow and green LEDs
 * @details Manages the hardware interface for a three-color traffic light
 * system. Provides methods for initializing and controlling individual LEDs.
 *          Uses pin configurations from the global config.
 */
class LedLights {
public:
  /// @name LED Color Constants
  /// @{
  static const int RED = 1;    ///< Identifier for red LED
  static const int YELLOW = 2; ///< Identifier for yellow LED
  static const int GREEN = 3;  ///< Identifier for green LED
  /// @}

  /**
   * @brief Initialize LED pins
   * @return ResourceResult indicating success or failure with error details
   * @details Configures GPIO pins for LED control based on settings
   *          from the global configuration.
   */
  ResourceResult init();

  /**
   * @brief Switch on specified LED
   * @param color LED color (use RED, YELLOW or GREEN constants)
   * @return ResourceResult indicating success or failure with error details
   * @details Sets the specified LED to its ON state. The color parameter
   *          must be one of the defined color constants.
   */
  ResourceResult switchLedOn(int color);

  /**
   * @brief Switch off specified LED
   * @param color LED color (use RED, YELLOW or GREEN constants)
   * @return ResourceResult indicating success or failure with error details
   * @details Sets the specified LED to its OFF state. The color parameter
   *          must be one of the defined color constants.
   */
  ResourceResult switchLedOff(int color);

  /**
   * @brief Get the current state of all LEDs
   * @return LedStatus struct with on/off state for each LED
   */
  LedStatus getStatus() const;

private:
  /**
   * @brief Validate LED color value
   * @param color Color value to check
   * @return true if color is valid, false otherwise
   * @details Checks if the provided color value matches one of the
   *          defined color constants (RED, YELLOW, or GREEN).
   */
  bool isValidColor(int color) const { return color >= RED && color <= GREEN; }
};

#endif // USE_LED_TRAFFIC_LIGHT

#endif // LED_H
