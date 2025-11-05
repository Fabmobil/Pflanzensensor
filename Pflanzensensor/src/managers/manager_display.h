/**
 * @file display_manager.h
 * @brief DisplayManager class for managing display settings and operations
 */
#ifndef manager_display_H
#define manager_display_H

#include <Arduino.h>

#include <memory>
#include <vector>

#include "configs/config.h" // Must come before other includes

#if USE_DISPLAY
#include "display/display.h" // Include the full definition instead of forward declaration
#include "display/display_config.h"
#if USE_LED_TRAFFIC_LIGHT
#include "led-traffic-light/led.h"
#endif
#include "managers/manager_base.h"
#include "utils/result_types.h"

class DisplayManager : public Manager {
public:
  // Constructor with member initializer list
  DisplayManager()
      : Manager("DisplayManager"),
        m_display(nullptr),
#if USE_LED_TRAFFIC_LIGHT
        m_ledLights(nullptr),
#endif
        m_lastUpdate(0),
        m_currentScreen(0),
        m_config(),
        m_lastScreenChange(0),
        m_currentScreenIndex(0) {
  } // Fixed initialization order to match declaration order

  // Configuration methods
  DisplayResult setScreenDuration(unsigned long duration);
  DisplayResult setClockFormat(const String& format);
  DisplayResult setIpScreenEnabled(bool enabled);
  DisplayResult setClockEnabled(bool enabled);         // New method
  DisplayResult setFlowerImageEnabled(bool enabled);   // New method
  DisplayResult setFabmobilImageEnabled(bool enabled); // New method
  // Set display-only flag for a specific sensor measurement. This does not
  // disable the sensor itself, only whether the measurement should be shown
  // on the display rotation.
  DisplayResult setSensorMeasurementDisplay(const String& sensorId, size_t measurementIndex,
                                            bool enabled);

  // Query whether a given sensor measurement should be displayed. This
  // consults the display config's per-sensor entries; when no entry exists
  // the function falls back to the sensor's own measurement enabled flag.
  bool isSensorMeasurementShown(const String& sensorId, size_t measurementIndex) const;
  DisplayResult saveConfig();

  /**
   * @brief Log enabled sensors and their IDs. Call this after both managers are
   * initialized.
   */
  void logEnabledSensors();

  // Getters
  /**
   * @brief Get the screen duration
   * @return Screen duration in milliseconds
   */
  inline unsigned long getScreenDuration() const { return m_config.screenDuration; }

  /**
   * @brief Get the clock format
   * @return Clock format string
   */
  inline String getClockFormat() const { return m_config.clockFormat; }

  /**
   * @brief Check if IP screen is enabled
   * @return True if IP screen is enabled, false otherwise
   */
  inline bool isIpScreenEnabled() const { return m_config.showIpScreen; }

  /**
   * @brief Check if clock is enabled
   * @return True if clock is enabled, false otherwise
   */
  inline bool isClockEnabled() const { return m_config.showClock; }

  /**
   * @brief Check if flower image is enabled
   * @return True if flower image is enabled, false otherwise
   */
  inline bool isFlowerImageEnabled() const { return m_config.showFlowerImage; }

  /**
   * @brief Check if Fabmobil image is enabled
   * @return True if Fabmobil image is enabled, false otherwise
   */
  inline bool isFabmobilImageEnabled() const { return m_config.showFabmobilImage; }

  // Display operations
  void showInfoScreen(const String& ipAddress);
  void update();

  // Unified logging methods for both boot and update modes
  /**
   * @brief Show a log screen with initial status message
   * @param status Initial status message to display
   * @param isBootMode If true, shows boot screen; if false, shows update screen
   *
   * This method starts either boot mode or update mode and displays the initial
   * status. Use this to begin logging progress for either boot sequence or
   * update operations.
   */
  void showLogScreen(const String& status, bool isBootMode = true);

  /**
   * @brief Update the log status with a new message
   * @param status New status message to add
   * @param isBootMode If true, adds to boot log; if false, adds to update log
   *
   * This method adds a new status line to the current log. It automatically
   * handles scrolling when the maximum number of lines is reached.
   */
  void updateLogStatus(const String& status, bool isBootMode = true);

  /**
   * @brief Add a new log line to the current display
   * @param status Status message to add
   * @param isBootMode If true, adds to boot log; if false, adds to update log
   *
   * This is the core method that handles adding log lines. It's called by
   * updateLogStatus and handles the display logic.
   */
  void addLogLine(const String& status, bool isBootMode = true);

  /**
   * @brief End boot mode and allow normal screen rotation. Clears log lines.
   */
  inline void endBootMode() {
    m_bootMode = false;
    m_logLineCount = 0;
  }

  /**
   * @brief Start update mode for showing update progress
   */
  inline void startUpdateMode() {
    m_updateMode = true;
    m_logLineCount = 0;
  }

  /**
   * @brief End update mode and return to normal operation
   */
  inline void endUpdateMode() {
    m_updateMode = false;
    m_logLineCount = 0;
  }

  /**
   * @brief Public wrapper to reload display configuration from Preferences.
   *
   * This calls the internal (private) loadConfig() implementation. Providing
   * a public wrapper preserves encapsulation while allowing other managers to
   * request a reload when settings change.
   */
  DisplayResult reloadConfig();

protected:
  TypedResult<ResourceError, void> initialize() override;

private:
  std::unique_ptr<SSD1306Display> m_display;
#if USE_LED_TRAFFIC_LIGHT
  std::unique_ptr<LedLights> m_ledLights;
#endif
  unsigned long m_lastUpdate;
  int m_currentScreen;
  DisplayConfig m_config;
  unsigned long m_lastScreenChange;
  size_t m_currentScreenIndex;

  // Boot screen state
  bool m_bootMode = true;
  static constexpr size_t BOOT_LOG_LINES = 6;
  String m_logLines[BOOT_LOG_LINES]; ///< Log lines for boot/update screens
                                     ///< (fixed size)
  size_t m_logLineCount = 0;         ///< Number of log lines currently stored

  // Update mode state
  bool m_updateMode = false;

  // Helper methods
  DisplayResult loadConfig();
  DisplayResult validateConfig();
  void rotateScreen();
  void showSensorData(const String& sensorId, size_t measurementIndex = 0);
  void showClock();
  void showImage(const unsigned char* image);
#if USE_LED_TRAFFIC_LIGHT
  void setLedTrafficLight(const String& status);
#endif
};

extern std::unique_ptr<DisplayManager> displayManager;

#endif // USE_DISPLAY

#endif // manager_display_H
