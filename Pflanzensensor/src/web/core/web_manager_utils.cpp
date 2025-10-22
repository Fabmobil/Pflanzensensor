/**
 * @file web_manager_utils.cpp
 * @brief WebManager utility functions
 */

#include <ESP8266WiFi.h>

#include "logger/logger.h"
#include "web/core/web_manager.h"

bool WebManager::isCaptivePortalAPActive() const {
  // Captive-Portal logic removed. Always return false to disable AP-specific
  // routing and middleware.
  return false;
}
