/**
 * @file display_config.h
 * @brief Configuration structure for display settings
 */

#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include <Arduino.h>

#include <vector>

#include "configs/config.h"

// Check if USE_DISPLAY is defined
#ifndef USE_DISPLAY
#define USE_DISPLAY false
#warning "USE_DISPLAY not defined in config file, defaulting to false"
#endif

// Check if DISPLAY_DEFAULT_TIME is defined
#ifndef DISPLAY_DEFAULT_TIME
#define DISPLAY_DEFAULT_TIME 5
#warning "DISPLAY_DEFAULT_TIME not defined in config file, defaulting to 5 seconds"
#endif

/**
 * @brief Default configuration structure for display settings
 */
struct DisplayConfig {
  bool showIpScreen;
  bool showClock;         // New field for clock display
  bool showFlowerImage;   // New field for flower image
  bool showFabmobilImage; // New field for fabmobil image
  bool showQrCode;        // New field for QR code display (now as separate screen)
  unsigned long screenDuration;
  String clockFormat;
  // Per-sensor display settings: for each sensor id a vector of booleans
  // indicating whether the individual measurements should be shown on the
  // display. If a sensor is not present here, the default behaviour is to
  // show measurements according to the sensor's own measurement enabled
  // flags.
  struct SensorDisplayEntry {
    String sensorId;
    std::vector<bool> showMeasurements;
  };
  std::vector<SensorDisplayEntry> sensorDisplays;
  /**
   * @brief Default constructor
   */
  DisplayConfig()
      : showIpScreen(true),
        showClock(true),
        showFlowerImage(true),
        showFabmobilImage(true),
        showQrCode(false),
        screenDuration(DISPLAY_DEFAULT_TIME * 1000),
        clockFormat("24h") {}
  // Constructor with parameters (not used in practice, but kept for
  // compatibility)
  DisplayConfig(bool showIp, bool showClk, bool showFlower, bool showFabmobil, bool showQr,
                unsigned long duration, const String& format,
                const std::vector<SensorDisplayEntry>& displays)
      : showIpScreen(showIp),
        showClock(showClk),
        showFlowerImage(showFlower),
        showFabmobilImage(showFabmobil),
        showQrCode(showQr),
        screenDuration(duration),
        clockFormat(format),
        sensorDisplays(displays) {}
};

// Display defaults
#define DISPLAY_DEFAULTS DisplayConfig()

#endif // DISPLAY_CONFIG_H
