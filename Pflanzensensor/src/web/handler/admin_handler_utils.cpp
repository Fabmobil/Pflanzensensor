/**
 * @file admin_handler_utils.cpp
 * @brief Utility functions for admin handler
 * @details Provides helper functions for configuration processing, formatting,
 *          authentication, and configuration value handling
 */

#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "web/handler/admin_handler.h"

String AdminHandler::formatMemorySize(size_t bytes) const {
  if (bytes < 1024)
    return String(bytes) + F(" B");
  if (bytes < 1024 * 1024)
    return String(bytes / 1024.0, 1) + F(" KB");
  return String(bytes / 1024.0 / 1024.0, 1) + F(" MB");
}

String AdminHandler::formatUptime() const {
  unsigned long uptime = millis() / 1000;

  // Safety check for overflow
  if (uptime == 0 || uptime > 0xFFFFFFFF / 1000) {
    return F("Fehler");
  }

  unsigned int days = uptime / 86400;
  unsigned int hours = (uptime % 86400) / 3600;
  unsigned int minutes = (uptime % 3600) / 60;
  unsigned int seconds = uptime % 60;

  String formatted;
  if (days > 0)
    formatted += String(days) + F("d ");
  if (hours > 0)
    formatted += String(hours) + F("h ");
  formatted += String(minutes) + F("m ");
  formatted += String(seconds) + F("s");

  return formatted;
}

bool AdminHandler::validateRequest() const {
  if (!_server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    return false;
  }
  return true;
}
