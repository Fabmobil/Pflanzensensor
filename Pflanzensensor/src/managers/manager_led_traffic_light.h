/**
 * @file manager_led_traffic_light.h
 * @brief LED traffic light manager implementation
 * @details Manages LED traffic light functionality independently of display
 * system
 */
#ifndef MANAGER_LED_TRAFFIC_LIGHT_H
#define MANAGER_LED_TRAFFIC_LIGHT_H

#include <memory>

#include "configs/config.h"
#include "managers/manager_base.h"
#include "utils/result_types.h"

#if USE_LED_TRAFFIC_LIGHT
#include "led-traffic-light/led.h"

/**
 * @class LedTrafficLightManager
 * @brief Manages LED traffic light functionality
 * @details Handles LED traffic light initialization and control based on sensor
 * status
 */
class LedTrafficLightManager : public Manager {
 public:
  /**
   * @brief Constructor for LedTrafficLightManager
   */
  LedTrafficLightManager()
      : Manager("LedTrafficLightManager"),
#if USE_LED_TRAFFIC_LIGHT
        m_ledLights(nullptr),
#endif
        m_lastStatus("") {
  }

  /**
   * @brief Set LED traffic light based on sensor status
   * @param status Status string ("green", "yellow", "red", or empty to turn
   * off)
   */
  void setStatus(const String& status);

  /**
   * @brief Set LED traffic light based on specific measurement status (for mode
   * 2)
   * @param measurementId Measurement identifier (format:
   * "sensorId_measurementIndex")
   * @param status Status string for the measurement
   */
  void setMeasurementStatus(const String& measurementId, const String& status);

  /**
   * @brief Get the last set status
   * @return Last status string
   */
  inline const String& getLastStatus() const { return m_lastStatus; }

  /**
   * @brief Get the current LED traffic light mode
   * @return 0 = off, 1 = all measurements, 2 = single measurement
   */
  uint8_t getMode() const;

  /**
   * @brief Check if LED traffic light is enabled (mode > 0)
   * @return true if enabled, false if off
   */
  inline bool isEnabled() const { return getMode() > 0; }

  /**
   * @brief Turn off all LEDs
   */
  void turnOffAllLeds();

  /**
   * @brief Update LED status for the selected measurement in mode 2
   * @details This method should be called periodically to ensure the LED shows
   *          the current status of the selected measurement
   */
  void updateSelectedMeasurementStatus();

  /**
   * @brief Handle display updates for mode 1
   * @details In mode 1, this should be called when the display shows
   * non-measurement screens to turn off the LED
   */
  void handleDisplayUpdate();

  /**
   * @brief Get the currently selected measurement for mode 2
   * @return Selected measurement ID string, empty if no measurement selected
   */
  String getSelectedMeasurement() const;

 protected:
  /**
   * @brief Initialize the LED traffic light manager
   * @return Result of initialization
   * @override
   */
  TypedResult<ResourceError, void> initialize() override;

 private:
#if USE_LED_TRAFFIC_LIGHT
  std::unique_ptr<LedLights> m_ledLights;
#endif
  String m_lastStatus;  ///< Last set status for tracking
};

/**
 * @brief Global LED traffic light manager instance
 */
extern std::unique_ptr<LedTrafficLightManager> ledTrafficLightManager;

#endif  // USE_LED_TRAFFIC_LIGHT

#endif  // MANAGER_LED_TRAFFIC_LIGHT_H
