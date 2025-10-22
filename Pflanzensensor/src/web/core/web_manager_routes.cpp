/**
 * @file web_manager_routes.cpp
 * @brief WebManager route setup and configuration
 */

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::setupRoutes() {
  if (!_router) {
    logger.error(F("WebManager"), F("Kann Routen nicht registrieren - Router nicht initialisiert"));
    return;
  }

  // Hinweis: Captive-Portal / AP-spezifische WiFi-Setup-Routen wurden entfernt.
  // WiFi-Updates werden über den Admin-Bereich (/admin) verwaltet.

  // START PAGE - ALWAYS register (shows sensor data in both modes)
  _router->addRoute(HTTP_GET, "/", [this]() {
    if (!_startHandler) {
      _startHandler = std::make_unique<StartpageHandler>(*_server, *_auth, *_cssService);
    }
    _startHandler->handleRoot();
  });

  // SENSOR ROUTES - ALWAYS register these regardless of WiFi status
  if (_sensorManager) {
    if (!_sensorHandler) {
      logger.debug(F("WebManager"), F("Initialisiere Sensor-Handler"));
      _sensorHandler =
          std::make_unique<SensorHandler>(*_server, *_auth, *_cssService, *_sensorManager);
      auto result = _sensorHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Sensor-Routen fehlgeschlagen: ") + result.getMessage());
      } else {
        logger.info(F("WebManager"), F("Sensor-Routen registriert"));
      }
    }

    // ADMIN SENSOR ROUTES - ALWAYS register these too
    if (!_adminSensorHandler) {
      logger.debug(F("WebManager"), F("Initialisiere AdminSensorHandler"));
      _adminSensorHandler =
          std::make_unique<AdminSensorHandler>(*_server, *_auth, *_cssService, *_sensorManager);
      auto result = _adminSensorHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"), F("Registrieren der Admin-Sensor-Routen fehlgeschlagen: ") +
                                          result.getMessage());
      } else {
        logger.info(F("WebManager"), F("AdminSensorHandler-Routen registriert"));
      }
    }
  } else {
    logger.warning(F("WebManager"), F("Sensor-Manager nicht verfügbar"));
  }

  // Add update route first to ensure it's available
  auto updateResult =
      _router->addRoute(HTTP_POST, "/admin/config/update", [this]() { handleSetUpdate(); });
  if (!updateResult.isSuccess()) {
    logger.error(F("WebManager"),
                 F("Registrieren der Update-Route fehlgeschlagen: ") + updateResult.getMessage());
  }

  // Add config value update route
  _router->addRoute(HTTP_POST, "/admin/config/setConfigValue",
                    [this]() { handleSetConfigValue(); });

  // /admin/updateWiFi wird vom AdminHandler registriert; keine explizite Delegierung hier nötig.

  _router->addRoute(HTTP_POST, "/admin/reboot", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("AdminHandler für Neustart nachladen"));
      _adminHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Admin-Routen fehlgeschlagen: ") + result.getMessage());
      }
    }
    _adminHandler->handleReboot();
  });

  // General admin routes
  _router->addRoute(HTTP_GET, "/admin", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("AdminHandler nachladen"));
      _adminHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Admin-Routen fehlgeschlagen: ") + result.getMessage());
      }
    }
    _adminHandler->handleAdminPage();
  });

  // Direct route for admin update settings
  _router->addRoute(HTTP_POST, "/admin/updateSettings", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("AdminHandler für Update nachladen"));
      _adminHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Admin-Routen fehlgeschlagen: ") + result.getMessage());
      }
    }
    _adminHandler->handleAdminUpdate();
  });

  // Direct route for admin reset
  _router->addRoute(HTTP_POST, "/admin/reset", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("AdminHandler für Reset nachladen"));
      _adminHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Admin-Routen fehlgeschlagen: ") + result.getMessage());
      }
    }
    _adminHandler->handleConfigReset();
  });

  // /admin/config/set is registered by AdminHandler::onRegisterRoutes to
  // centralize admin-related routes. Avoid registering it here to prevent
  // duplicate route entries.

  // Direct route for admin download log
  _router->addRoute(HTTP_GET, "/admin/downloadLog", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("AdminHandler für Log-Download nachladen"));
      _adminHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Admin-Routen fehlgeschlagen: ") + result.getMessage());
      }
    }
    _adminHandler->handleDownloadLog();
  });

#if USE_DISPLAY
  if (!_displayHandler) {
    logger.debug(F("WebManager"), F("Initialisiere AdminDisplayHandler"));
    _displayHandler = std::make_unique<AdminDisplayHandler>(*_server);
    auto result = _displayHandler->registerRoutes(*_router);
    if (!result.isSuccess()) {
      logger.error(F("WebManager"),
                   F("Registrieren der Display-Routen fehlgeschlagen: ") + result.getMessage());
    }
  }
#endif

  // Register OTA routes
  if (_otaHandler) {
    auto result = _otaHandler->registerRoutes(*_router);
    if (!result.isSuccess()) {
      logger.error(F("WebManager"),
                   F("Registrieren der OTA-Routen fehlgeschlagen: ") + result.getMessage());
    }
  }

  // Not found handler
  _server->onNotFound([this]() {
    logger.warning(F("WebManager"), F("404: Nicht gefunden: ") + _server->uri());
    _server->send(404, "text/plain", "404: Not Found");
  });

  logger.debug(F("WebManager"), F("Routen-Setup abgeschlossen"));
}

void WebManager::setupMinimalRoutes() {
  if (!_router || !_server) {
    logger.error(
        F("WebManager"),
        F("Kann minimale Routen nicht registrieren - Router oder Server nicht initialisiert"));
    return;
  }

  // Hinweis: Captive-Portal-spezifische WiFi-Setup-Routen im Minimalmodus entfernt.
  // Admin-API bleibt für konfigurierte Admin-Routen verfügbar.

  // SENSOR FUNCTIONALITY - ALWAYS available in minimal mode too
  if (_sensorManager) {
    if (!_sensorHandler) {
      logger.debug(F("WebManager"), F("Minimalmodus - registriere Sensor-Routen"));
      _sensorHandler =
          std::make_unique<SensorHandler>(*_server, *_auth, *_cssService, *_sensorManager);
      auto result = _sensorHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Sensor-Routen im Minimalmodus fehlgeschlagen: ") +
                         result.getMessage());
      } else {
        logger.info(F("WebManager"), F("Sensor-Routen im Minimalmodus registriert"));
      }
    }
  }

  // START PAGE - ALWAYS available
  _router->addRoute(HTTP_GET, "/", [this]() {
    if (!_startHandler) {
      _startHandler = std::make_unique<StartpageHandler>(*_server, *_auth, *_cssService);
    }
    _startHandler->handleRoot();
  });

  // Create minimal admin handler
  _minimalAdminHandler = std::make_unique<AdminMinimalHandler>(*_server, *_auth);

  // Register existing OTA routes
  auto result = _otaHandler->registerRoutes(*_router);
  if (!result.isSuccess()) {
    logger.error(F("WebManager"),
                 F("Registrieren der OTA-Routen fehlgeschlagen: ") + result.getMessage());
    return;
  }
  logger.info(F("WebManager"), F("OTA-Routen registriert"));

  // Register admin setUpdate route
  auto rebootResult =
      _router->addRoute(HTTP_POST, "/admin/config/update", [this]() { handleSetUpdate(); });
  if (!rebootResult.isSuccess()) {
    logger.error(F("WebManager"),
                 F("Registrieren der /admin/config/update-Route fehlgeschlagen: ") +
                     result.getMessage());
    return;
  }

  // Register admin reboot route
  result = _router->addRoute(HTTP_POST, "/admin/reboot", [this]() {
    if (!_adminHandler) {
      logger.debug(F("WebManager"), F("AdminHandler für Neustart nachladen"));
      _adminHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      // Register all admin routes immediately
      auto result = _adminHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"),
                     F("Registrieren der Admin-Routen fehlgeschlagen: ") + result.getMessage());
      }
    }
    _adminHandler->handleReboot();
  });
  if (!result.isSuccess()) {
    logger.error(F("WebManager"),
                 F("Registrieren der Reboot-Route fehlgeschlagen: ") + result.getMessage());
    return;
  }
  logger.debug(F("WebManager"), F("Admin-Neustart-Route registriert"));

  logger.debug(F("WebManager"), F("Minimal-Routen-Konfiguration abgeschlossen"));
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
    logger.debug(F("WebManager"), F("Entferne Route: ") + methodToString(method) + " " + path);
  }
}
