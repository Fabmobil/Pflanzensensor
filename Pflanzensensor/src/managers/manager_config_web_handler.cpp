/**
 * @file manager_config_web_handler.cpp
 * @brief Implementation of ConfigWebHandler class
 */

#include "manager_config_web_handler.h"

#include "../logger/logger.h"
#include "manager_config.h"

ConfigWebHandler::ConfigWebHandler(ConfigManager& configManager) : m_configManager(configManager) {}

ConfigWebHandler::WebResult ConfigWebHandler::updateFromWebRequest(ESP8266WebServer& server) {
  bool configChanged = false;

  // Process boolean settings
  auto boolResult = processBooleanSettings(server, configChanged);
  if (!boolResult.isSuccess()) {
    return boolResult;
  }

  // Save if any changes were made
  if (configChanged) {
    return m_configManager.saveConfig();
  }

  return WebResult::success();
}

ConfigWebHandler::WebResult ConfigWebHandler::processBooleanSettings(ESP8266WebServer& server,
                                                                     bool& configChanged) {
  // Get current configuration data
  bool currentMD5 = m_configManager.isMD5Verification();
  bool currentCollectd = m_configManager.isCollectdEnabled();
  bool currentFileLogging = m_configManager.isFileLoggingEnabled();

  // Process MD5 verification
  bool newMD5Verification = server.hasArg("md5_verification");
  if (newMD5Verification != currentMD5) {
    auto result = m_configManager.setMD5Verification(newMD5Verification);
    if (!result.isSuccess()) {
      return WebResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                             result.getMessage());
    }
    configChanged = true;
  }

  // Process Collectd setting
  bool newCollectdEnabled = server.hasArg("collectd_enabled");
  if (newCollectdEnabled != currentCollectd) {
    auto result = m_configManager.setCollectdEnabled(newCollectdEnabled);
    if (!result.isSuccess()) {
      return WebResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                             result.getMessage());
    }
    configChanged = true;
  }

  // Process file logging setting
  bool newFileLoggingEnabled = server.hasArg("file_logging_enabled");
  if (newFileLoggingEnabled != currentFileLogging) {
    auto result = m_configManager.setFileLoggingEnabled(newFileLoggingEnabled);
    if (!result.isSuccess()) {
      return WebResult::fail(result.error().value_or(ConfigError::UNKNOWN_ERROR),
                             result.getMessage());
    }
    configChanged = true;
  }

  return WebResult::success();
}
