/**
 * @file web_manager_utils.cpp
 * @brief WebManager utility functions
 */

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "logger/logger.h"
#include "web/core/web_manager.h"

bool WebManager::isCaptivePortalAPActive() const {
  // Captive-Portal logic removed. Always return false to disable AP-specific
  // routing and middleware.
  return false;
}
