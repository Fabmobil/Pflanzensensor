/**
 * @file web_manager_routes.cpp
 * @brief WebManager route setup and configuration
 */

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::setupRoutes() {
  if (!_router) {
    logger.error(F("WebManager"),
                 F("Cannot setup routes - router not initialized"));
    return;
  }

  // WiFi setup handler - only when in AP mode
  if (isCaptivePortalAPActive()) {
    if (!_wifiSetupHandler) {
      logger.info(F("WebManager"),
                  F("Device in AP mode - registering WiFi setup routes"));
      _wifiSetupHandler =
          std::make_unique<WiFiSetupHandler>(*_server, *_auth, *_cssService);
      auto result = _wifiSetupHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"), "Failed to register WiFi setup routes: " +
                                          result.getMessage());
      } else {
        logger.info(F("WebManager"), F("WiFi setup routes registered"));
      }
    }

  } else {
    logger.debug(F("WebManager"),
                 F("WiFi connected - skipping WiFi setup routes"));
  }

  // START PAGE - ALWAYS register (shows sensor data in both modes)
  _router->addRoute(HTTP_GET, "/", [this]() {
    if (!_startHandler) {
      _startHandler =
          std::make_unique<StartpageHandler>(*_server, *_auth, *_cssService);
    }
    _startHandler->handleRoot();
  });

  // SENSOR ROUTES - ALWAYS register these regardless of WiFi status
  if (_sensorManager) {
    if (!_sensorHandler) {
      logger.debug(F("WebManager"), F("Initializing sensor handler"));
      _sensorHandler = std::make_unique<SensorHandler>(
          *_server, *_auth, *_cssService, *_sensorManager);
      auto result = _sensorHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"), "Failed to register sensor routes: " +
                                          result.getMessage());
      } else {
        logger.info(F("WebManager"), F("Sensor routes registered"));
      }
    }

    // ADMIN SENSOR ROUTES - ALWAYS register these too
    if (!_adminSensorHandler) {
      logger.debug(F("WebManager"), F("Initializing AdminSensorHandler"));
      _adminSensorHandler = std::make_unique<AdminSensorHandler>(
          *_server, *_auth, *_cssService, *_sensorManager);
      auto result = _adminSensorHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(
            F("WebManager"),
            "Failed to register admin sensor routes: " + result.getMessage());
      } else {
        logger.info(F("WebManager"), F("AdminSensorHandler routes registered"));
      }
    }
  } else {
    logger.warning(F("WebManager"), F("Sensor manager not available"));
  }

  // Add update route first to ensure it's available
  auto updateResult = _router->addRoute(HTTP_POST, "/admin/config/update",
                                        [this]() { handleSetUpdate(); });
  if (!updateResult.isSuccess()) {
    logger.error(F("WebManager"), "Failed to register update route: " +
                                      updateResult.getMessage());
  }

  // Add config value update route
  _router->addRoute(HTTP_POST, "/admin/config/setConfigValue",
                    [this]() { handleSetConfigValue(); });

  // WiFi update route for admin interface (not captive portal)
  _router->addRoute(HTTP_POST, "/admin/updateWiFi", [this]() {
    // Delegate to admin handler for admin interface
    if (!_adminHandler) {
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleWiFiUpdate();
  });

  _router->addRoute(HTTP_POST, "/admin/reboot", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("Lazy loading AdminHandler for reboot"));
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleReboot();
  });

  // General admin routes
  _router->addRoute(HTTP_GET, "/admin", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("Lazy loading AdminHandler"));
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleAdminPage();
  });

  // Direct route for admin update settings
  _router->addRoute(HTTP_POST, "/admin/updateSettings", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("Lazy loading AdminHandler for update"));
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleAdminUpdate();
  });

  // Direct route for admin reset
  _router->addRoute(HTTP_POST, "/admin/reset", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("Lazy loading AdminHandler for reset"));
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleConfigReset();
  });

  // Direct route for admin config set
  _router->addRoute(HTTP_POST, "/admin/config/set", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"),
                   F("Lazy loading AdminHandler for config set"));
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleConfigSet();
  });

  // Direct route for admin download log
  _router->addRoute(HTTP_GET, "/admin/downloadLog", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"),
                   F("Lazy loading AdminHandler for downloadLog"));
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleDownloadLog();
  });

#if USE_DISPLAY
  if (!_displayHandler) {
    logger.debug(F("WebManager"), F("Initializing AdminDisplayHandler"));
    _displayHandler = std::make_unique<AdminDisplayHandler>(*_server);
    auto result = _displayHandler->registerRoutes(*_router);
    if (!result.isSuccess()) {
      logger.error(F("WebManager"),
                   "Failed to register display routes: " + result.getMessage());
    }
  }
#endif

  // Register OTA routes
  if (_otaHandler) {
    auto result = _otaHandler->registerRoutes(*_router);
    if (!result.isSuccess()) {
      logger.error(F("WebManager"),
                   "Failed to register OTA routes: " + result.getMessage());
    }
  }

  // Not found handler
  _server->onNotFound([this]() {
    logger.warning(F("WebManager"), "404: " + _server->uri());
    _server->send(404, "text/plain", "404: Not Found");
  });

  logger.debug(F("WebManager"), F("Routes setup complete"));
}

void WebManager::setupMinimalRoutes() {
  if (!_router || !_server) {
    logger.error(
        F("WebManager"),
        "Cannot setup minimal routes - router or server not initialized");
    return;
  }

  // WiFi setup only if in AP mode
  if (isCaptivePortalAPActive()) {
    if (!_wifiSetupHandler) {
      logger.info(F("WebManager"),
                  F("Minimal mode + AP mode - registering WiFi setup routes"));
      _wifiSetupHandler =
          std::make_unique<WiFiSetupHandler>(*_server, *_auth, *_cssService);
      auto result = _wifiSetupHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"), "Failed to register WiFi setup routes: " +
                                          result.getMessage());
      } else {
        logger.info(
            F("WebManager"),
            F("WiFi setup routes registered for minimal captive portal"));
      }
    }
  } else {
    logger.debug(
        F("WebManager"),
        F("Minimal mode but WiFi connected - no captive portal needed"));
  }

  // SENSOR FUNCTIONALITY - ALWAYS available in minimal mode too
  if (_sensorManager) {
    if (!_sensorHandler) {
      logger.debug(F("WebManager"),
                   F("Minimal mode - registering sensor routes"));
      _sensorHandler = std::make_unique<SensorHandler>(
          *_server, *_auth, *_cssService, *_sensorManager);
      auto result = _sensorHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register sensor routes in minimal mode: " +
                         result.getMessage());
      } else {
        logger.info(F("WebManager"),
                    F("Sensor routes registered in minimal mode"));
      }
    }
  }

  // START PAGE - ALWAYS available
  _router->addRoute(HTTP_GET, "/", [this]() {
    if (!_startHandler) {
      _startHandler =
          std::make_unique<StartpageHandler>(*_server, *_auth, *_cssService);
    }
    _startHandler->handleRoot();
  });

  // Create minimal admin handler
  _minimalAdminHandler =
      std::make_unique<AdminMinimalHandler>(*_server, *_auth);

  // Register existing OTA routes
  auto result = _otaHandler->registerRoutes(*_router);
  if (!result.isSuccess()) {
    logger.error(F("WebManager"),
                 "Failed to register OTA routes: " + result.getMessage());
    return;
  }
  logger.info(F("WebManager"), F("OTA routes registered"));

  // Register admin setUpdate route
  auto rebootResult = _router->addRoute(HTTP_POST, "/admin/config/update",
                                        [this]() { handleSetUpdate(); });
  if (!rebootResult.isSuccess()) {
    logger.error(F("WebManager"),
                 "Failed to register /admin/config/update route: " +
                     result.getMessage());
    return;
  }

  // Register admin reboot route
  result = _router->addRoute(HTTP_POST, "/admin/reboot", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("Lazy loading AdminHandler for reboot"));
      _adminHandler =
          std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      // Register all admin routes immediately
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     "Failed to register admin routes: " + result.getMessage());
      }
    }
    _adminHandler->handleReboot();
  });
  if (!result.isSuccess()) {
    logger.error(F("WebManager"),
                 "Failed to register reboot route: " + result.getMessage());
    return;
  }
  logger.debug(F("WebManager"), F("Admin reboot route registered"));

  logger.debug(F("WebManager"), F("Minimal routes setup complete"));
}

bool WebManager::hasRoute(const String& path, HTTPMethod method) const {
  if (!_router) {
    return false;
  }
  return _router->hasRoute(path, method);
}

void WebManager::removeRoute(const String& path, HTTPMethod method) {
  if (_router) {
    // Implementation depends on your WebRouter class
    // This is a placeholder
    logger.debug(F("WebManager"),
                 "Removing route: " + methodToString(method) + " " + path);
  }
}
