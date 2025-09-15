/**
 * @file admin_sensor_handler_security.cpp
 * @brief Implementation of security and authentication functionality
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_config.h"

bool AdminSensorHandler::validateRequest() const {
  logger.debug(F("AdminSensorHandler"), F("validateRequest() called"));

  if (!_server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    logger.debug(F("AdminSensorHandler"),
                 F("Authentication failed, requesting auth"));
    _server.requestAuthentication();
    return false;
  }

  logger.debug(F("AdminSensorHandler"), F("Authentication successful"));
  return true;
}
