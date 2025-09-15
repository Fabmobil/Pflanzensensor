/**
 * @file helper.h
 * @brief Utility functions for system operations and time management
 * @details Provides helper functions for common operations such as time
 * formatting, system statistics tracking, and firmware upgrade preparation.
 */
#ifndef HELPER_H
#define HELPER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

#include "utils/result_types.h"
#include "utils/wifi.h"
#include "web/core/web_manager.h"

// Forward declaration
class DisplayManager;

constexpr const char* REBOOT_COUNT_FILE = "/reboot_count.txt";

/**
 * @class Helper
 * @brief Static utility class providing helper functions
 * @details Contains utility functions for time formatting, system statistics,
 *          and firmware management. All methods are static and can be called
 *          without instantiation.
 */
class Helper {
 public:
  /**
   * @brief Template function for initializing components with consistent error
   * handling
   * @tparam F Initialization function type that returns a ResourceResult
   * @param componentName Name of the component being initialized
   * @param initFunc Function to perform initialization
   * @return True if initialization succeeded, false otherwise
   */
  template <typename F>
  static bool initializeComponent(const __FlashStringHelper* componentName,
                                  F initFunc) {
    logger.info(F("main"), String(F("Initializing ")) + componentName);
    try {
      auto result = initFunc();
      if (!result.isSuccess()) {
        logger.error(F("main"), String(F("Failed to initialize ")) +
                                    componentName + F(": ") +
                                    result.getMessage());
        return false;
      }
      logger.debug(F("main"),
                   String(componentName) + F(" initialized successfully"));
      return true;
    } catch (const std::exception& e) {
      logger.error(F("main"), String(F("Exception during ")) + componentName +
                                  F(" initialization: ") + String(e.what()));
      return false;
    } catch (...) {
      logger.error(F("main"), String(F("Unknown exception during ")) +
                                  componentName + F(" initialization"));
      return false;
    }
  }

  /**
   * @brief Get current formatted date string
   * @return String containing date in format DD.MM.YYYY
   * @details Returns the current date formatted according to the local time
   * zone. Requires that system time has been synchronized.
   */
  static String getFormattedDate();

  /**
   * @brief Get current formatted time string
   * @param use24Hour Whether to use 24-hour format (default: true)
   * @return String containing time in format HH:MM (24h) or HH:MM AM/PM (12h)
   * @details Returns the current time formatted according to the specified
   * format. Requires that system time has been synchronized.
   */
  static String getFormattedTime(bool use24Hour = true);

  /**
   * @brief Get current epoch time
   * @return Current epoch time or 0 if time not synchronized
   * @details Returns the number of seconds since Unix epoch (January 1, 1970).
   *          Returns 0 if the system time has not been synchronized.
   */
  static time_t getCurrentTime();

  /**
   * @brief Get system reboot count
   * @return Number of system reboots
   * @details Retrieves the number of times the system has been rebooted.
   *          Value is persisted in flash memory across reboots.
   */
  static uint32_t getRebootCount();

  /**
   * @brief Format uptime into human readable string
   * @return Formatted uptime string
   * @details Converts the system uptime into a human-readable format
   *          showing days, hours, minutes, and seconds.
   */
  static String getFormattedUptime();

  /**
   * @brief Increment and save reboot counter to flash
   * @return ResourceResult indicating success or failure
   * @details Increments the reboot counter and persists it to flash memory.
   *          Should be called during system initialization.
   */
  static ResourceResult incrementRebootCount();

  /**
   * @brief Initialize minimal system for firmware upgrade
   * @return ResourceResult indicating success or failure
   * @details Prepares the system for a firmware upgrade by initializing
   *          only essential components needed for the upgrade process.
   */
  static ResourceResult initializeUpgradeMode();

  /**
   * @brief Display WiFi connection attempts information on the display
   * @param displayManager Pointer to the display manager
   * @param attemptsInfo String containing WiFi connection attempts info
   * @param isBootMode Whether this is boot mode (true) or update mode (false)
   * @details Parses and displays WiFi connection attempts information in a
   *          user-friendly format, splitting long strings into readable lines.
   */
#if USE_DISPLAY
  static void displayWiFiConnectionAttempts(DisplayManager* displayManager,
                                            const String& attemptsInfo,
                                            bool isBootMode);
#endif
};

/**
 * @brief Check if a sensor is an analog sensor (by ID prefix)
 * @param sensor Pointer to the sensor
 * @return true if the sensor ID starts with "ANALOG", false otherwise
 */
inline bool isAnalogSensor(const Sensor* sensor) {
  if (!sensor) return false;
  String id = sensor->getId();
  return id.startsWith("ANALOG");
}

#endif  // HELPER_H
