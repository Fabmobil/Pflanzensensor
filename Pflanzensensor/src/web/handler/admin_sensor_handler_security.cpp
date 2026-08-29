/**
 * @file admin_sensor_handler_security.cpp
 * @brief Implementation of security and authentication functionality
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "web/core/web_auth.h"

bool AdminSensorHandler::validateRequest() const {
  LOG_DEBUG(F("AdminSensorHandler"), F("validateRequest() called"));

  if (!WebAuth::checkAdminCredentials(_server)) {
    LOG_DEBUG(F("AdminSensorHandler"), F("Authentication failed, requesting auth"));
    _server.requestAuthentication();
    return false;
  }

  LOG_DEBUG(F("AdminSensorHandler"), F("Authentication successful"));
  return true;
}
