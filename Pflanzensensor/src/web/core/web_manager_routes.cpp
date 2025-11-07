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

  logger.debug(F("WebManager"), F("Registriere essenzielle Routen (Lazy-Loading für Handler)"));

  // CRITICAL: Register file upload routes FIRST using _server.on()
  // These MUST be registered before any router routes to take priority
  // File uploads cannot go through the router system
  logger.debug(F("WebManager"), F("Registriere Upload-Routen (vor Router)"));

  // Config upload route - needs direct server registration for file upload support
  _server->on(
      "/admin/uploadConfig", HTTP_POST,
      [this]() {
        // Called after upload completes - response is sent in upload handler
      },
      [this]() {
        // Upload handler - called during file upload
        // AdminHandler must be loaded for this
        BaseHandler* handler = getCachedHandler("admin");
        if (!handler) {
          logger.debug(F("WebManager"), F("Lazy-Loading AdminHandler für Upload"));
          auto newHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
          auto result = newHandler->registerRoutes(*_router);
          if (result.isSuccess()) {
            cacheHandler(std::move(newHandler), "admin");
            handler = getCachedHandler("admin");
          }
        }
        if (handler) {
          static_cast<AdminHandler*>(handler)->handleUploadConfig();
        } else {
          logger.error(F("WebManager"), F("AdminHandler konnte nicht geladen werden"));
          _server->send(500, F("text/plain"), F("Handler-Ladefehler"));
        }
      });
  logger.debug(F("WebManager"), F("Upload-Route /admin/uploadConfig registriert"));

  // Essential routes that cannot be lazy-loaded due to special handling

  // Add update route - critical for OTA updates
  auto updateResult =
      _router->addRoute(HTTP_POST, "/admin/config/update", [this]() { handleSetUpdate(); });
  if (!updateResult.isSuccess()) {
    logger.error(F("WebManager"),
                 F("Registrieren der Update-Route fehlgeschlagen: ") + updateResult.getMessage());
  }

  // Add config value update route - used frequently
  _router->addRoute(HTTP_POST, "/admin/config/setConfigValue",
                    [this]() { handleSetConfigValue(); });

  // Register OTA routes - critical for firmware updates, cannot be lazy-loaded
  if (_otaHandler) {
    auto result = _otaHandler->registerRoutes(*_router);
    if (!result.isSuccess()) {
      logger.error(F("WebManager"),
                   F("Registrieren der OTA-Routen fehlgeschlagen: ") + result.getMessage());
    } else {
      logger.info(F("WebManager"), F("OTA-Routen erfolgreich registriert"));
    }
  }

  // Catch-all handler - forward ALL requests to router (which runs middleware and finds routes)
  _server->onNotFound([this]() {
    String uri = _server->uri();
    HTTPMethod method = _server->method();

    logger.debug(F("WebManager"), F("Router-Anfrage: ") + String(method) + F(" ") + uri);

    // Let router handle the request (will run middleware and find routes)
    if (_router && _router->handleRequest(method, uri)) {
      // Request was handled by router
      return;
    }

    // No route found even after middleware
    logger.warning(F("WebManager"), F("404: Nicht gefunden: ") + uri);
    _server->send(404, "text/plain", "404: Not Found");
  });

  logger.info(F("WebManager"),
              F("Essenzielle Routen registriert - Handler werden bei Bedarf geladen"));
}

void WebManager::setupMinimalRoutes() {
  if (!_router || !_server) {
    logger.error(
        F("WebManager"),
        F("Kann minimale Routen nicht registrieren - Router oder Server nicht initialisiert"));
    return;
  }

  logger.debug(F("WebManager"), F("Registriere minimale Routen (Lazy-Loading aktiv)"));

  // Create minimal admin handler
  _minimalAdminHandler = std::make_unique<AdminMinimalHandler>(*_server, *_auth);

  // Register existing OTA routes - critical for updates
  auto result = _otaHandler->registerRoutes(*_router);
  if (!result.isSuccess()) {
    logger.error(F("WebManager"),
                 F("Registrieren der OTA-Routen fehlgeschlagen: ") + result.getMessage());
    return;
  }
  logger.info(F("WebManager"), F("OTA-Routen erfolgreich registriert"));

  // Register admin setUpdate route - critical
  auto rebootResult =
      _router->addRoute(HTTP_POST, "/admin/config/update", [this]() { handleSetUpdate(); });
  if (!rebootResult.isSuccess()) {
    logger.error(F("WebManager"),
                 F("Registrieren der /admin/config/update-Route fehlgeschlagen: ") +
                     result.getMessage());
    return;
  }

  logger.info(F("WebManager"), F("Minimal-Routen registriert - Handler werden bei Bedarf geladen"));
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
