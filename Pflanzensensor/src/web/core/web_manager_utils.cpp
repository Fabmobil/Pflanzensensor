/**
 * @file web_manager_utils.cpp
 * @brief WebManager utility functions
 */

#include <ESP8266WiFi.h>

#include "logger/logger.h"
#include "web/core/web_manager.h"

bool WebManager::isCaptivePortalAPActive() const {
  // Check if we're in AP mode or have no WiFi connection
  return (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA ||
          WiFi.status() != WL_CONNECTED);
}
