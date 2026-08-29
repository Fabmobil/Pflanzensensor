/**
 * @file main_led.cpp
 * @brief LED traffic light initialization module
 * @details Handles LED traffic light manager initialization
 */

#include <Arduino.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_display.h"
#include "managers/manager_led_traffic_light.h"
#include "managers/manager_resource.h"

/**
 * @brief Initialize LED traffic light manager
 * @return ResourceResult indicating success or failure
 * @details
 * 1. Create LedTrafficLightManager instance
 * 2. Initialize hardware
 * 3. Log status
 * @note This is optional - system continues even if it fails
 */
ResourceResult initializeLedTrafficLight() {
  LOG_INFO(F("main_led"), F("LED-Ampel-Manager Initialisierung gestartet"));

#if USE_LED_TRAFFIC_LIGHT
  if (!ledTrafficLightManager) {
    ledTrafficLightManager = std::make_unique<LedTrafficLightManager>();
  }

  ResourceResult result = ledTrafficLightManager->init();
  if (!result.isSuccess()) {
    LOG_WARN(F("main_led"),
             String(F("LED-Ampel-Manager Initialisierung fehlgeschlagen: ")) + result.getMessage());
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, result.getMessage());
  }

  LOG_INFO(F("main_led"), F("LED-Ampel-Manager Initialisierung erfolgreich"));
  return ResourceResult::success();
#else
  LOG_INFO(F("main_led"), F("LED-Ampel nicht aktiviert - Initialisierung übersprungen"));
  return ResourceResult::success();
#endif
}
