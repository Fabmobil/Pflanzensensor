/**
 * @file manager_config_web_handler.h
 * @brief Web interface handler for configuration updates
 */

#ifndef MANAGER_CONFIG_WEB_HANDLER_H
#define MANAGER_CONFIG_WEB_HANDLER_H

#include <ESP8266WebServer.h>

#include "../utils/result_types.h"
#include "manager_config_types.h"

class ConfigManager; // Forward declaration

class ConfigWebHandler {
public:
  using WebResult = TypedResult<ConfigError, void>;

  /**
   * @brief Constructor
   * @param configManager Reference to the main ConfigManager
   */
  explicit ConfigWebHandler(ConfigManager& configManager);

  /**
   * @brief Update configuration from web server request
   * @param server Reference to the ESP8266 web server
   * @return WebResult indicating success or failure
   */
  WebResult updateFromWebRequest(ESP8266WebServer& server);

private:
  /**
   * @brief Process boolean configuration settings from web request
   * @param server Reference to the ESP8266 web server
   * @param configChanged Reference to boolean indicating if config changed
   * @return WebResult indicating success or failure
   */
  WebResult processBooleanSettings(ESP8266WebServer& server, bool& configChanged);

private:
  ConfigManager& m_configManager;
};

#endif
