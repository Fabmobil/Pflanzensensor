/**
 * @file web_manager_routes.cpp
 * @brief WebManager route setup and configuration
 */

#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::setupRoutes() {
  if (!_router) {
    LOG_ERROR(F("WebManager"), F("Kann Routen nicht registrieren - Router nicht initialisiert"));
    return;
  }

  LOG_DEBUG(F("WebManager"), F("Registriere essenzielle Routen (Lazy-Loading für Handler)"));

  // CRITICAL: Register file upload routes FIRST using _server.on()
  // These MUST be registered before any router routes to take priority
  // File uploads cannot go through the router system
  LOG_DEBUG(F("WebManager"), F("Registriere Upload-Routen (vor Router)"));

  // Config upload route - needs direct server registration for file upload support
  // ACHTUNG: _server->on() umgeht die Router-Middleware. Die Authentifizierung
  // muss deshalb in beiden Callbacks selbst geprüft werden, sonst könnte jeder
  // im Netz eine Konfiguration einspielen und damit einen Neustart auslösen.
  _server->on(
      "/admin/uploadConfig", HTTP_POST,
      [this]() {
        if (!m_configUploadAuthorized) {
          _server->requestAuthentication();
          return;
        }
        // POST handler - called after upload completes
        // Send response and trigger reboot here
        BaseHandler* handler = getCachedHandler("admin");
        if (handler) {
          // Check if upload was successful by checking if temp file exists
          if (LittleFS.exists("/prefs_upload_done.flag")) {
            LOG_INFO(F("WebManager"), F("Config-Upload erfolgreich, sende Antwort"));

            // Send success response
            _server->send(
                200, "application/json",
                "{\"success\":true,\"message\":\"Die Konfiguration wird wiederhergestellt. "
                "Dies kann bis zu 60 Sekunden dauern. Bitte warten "
                "Sie...\",\"rebootPending\":true}");

            // Give response time to reach client
            delay(500);
            _server->handleClient();
            delay(100);

            // Now restore and reboot (called from AdminHandler)
            static_cast<AdminHandler*>(handler)->handleUploadConfigRestore();
          } else if (LittleFS.exists("/prefs_upload_error.flag")) {
            LOG_ERROR(F("WebManager"), F("Config-Upload fehlgeschlagen"));
            _server->send(500, "application/json",
                          "{\"success\":false,\"error\":\"Upload fehlgeschlagen\"}");
            LittleFS.remove("/prefs_upload_error.flag");
          } else {
            LOG_ERROR(F("WebManager"), F("Kein Upload-Status gefunden"));
            _server->send(500, "application/json",
                          "{\"success\":false,\"error\":\"Unbekannter Fehler\"}");
          }
        } else {
          _server->send(500, F("text/plain"), F("Handler nicht gefunden"));
        }
      },
      [this]() {
        // Upload handler - called during file upload
        // Auth einmal zu Beginn prüfen, Ergebnis für alle Chunks merken
        if (_server->upload().status == UPLOAD_FILE_START) {
          m_configUploadAuthorized =
              _server->authenticate("admin", ConfigMgr.getAdminPassword().c_str());
          if (!m_configUploadAuthorized) {
            LOG_WARN(F("WebManager"), F("Config-Upload ohne gültige Anmeldung abgewiesen"));
          }
        }
        if (!m_configUploadAuthorized) {
          return;
        }

        // AdminHandler must be loaded for this
        BaseHandler* handler = getCachedHandler("admin");
        if (!handler) {
          LOG_DEBUG(F("WebManager"), F("Lazy-Loading AdminHandler für Upload"));
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
          LOG_ERROR(F("WebManager"), F("AdminHandler konnte nicht geladen werden"));
          _server->send(500, F("text/plain"), F("Handler-Ladefehler"));
        }
      });
  LOG_DEBUG(F("WebManager"), F("Upload-Route /admin/uploadConfig registriert"));

  // Essential routes that cannot be lazy-loaded due to special handling

  // Add update route - critical for OTA updates
  auto updateResult =
      _router->addRoute(HTTP_POST, "/admin/config/update", [this]() { handleSetUpdate(); });
  if (!updateResult.isSuccess()) {
    LOG_ERROR(F("WebManager"), String(F("Registrieren der Update-Route fehlgeschlagen: ")) +
                                   updateResult.getMessage());
  }

  // Add config value update route - used frequently
  _router->addRoute(HTTP_POST, "/admin/config/setConfigValue",
                    [this]() { handleSetConfigValue(); });

  // Register OTA routes - critical for firmware updates, cannot be lazy-loaded
  if (_otaHandler) {
    auto result = _otaHandler->registerRoutes(*_router);
    if (!result.isSuccess()) {
      LOG_ERROR(F("WebManager"),
                String(F("Registrieren der OTA-Routen fehlgeschlagen: ")) + result.getMessage());
    } else {
      LOG_INFO(F("WebManager"), F("OTA-Routen erfolgreich registriert"));
    }
  }

  // Catch-all handler - forward ALL requests to router (which runs middleware and finds routes)
  _server->onNotFound([this]() {
    String uri = _server->uri();
    HTTPMethod method = _server->method();

    // Statische Assets zuerst: /css/, /js/, /img/ und /favicon.ico kommen
    // direkt aus LittleFS, ohne dafür je eine eigene Route zu registrieren.
    if (method == HTTP_GET && tryServeStaticFile(uri)) {
      return;
    }

    // Let router handle the request (will run middleware and find routes)
    if (_router && _router->handleRequest(method, uri)) {
      // Request was handled by router
      return;
    }

    // No route found even after middleware
    LOG_WARN(F("WebManager"), String(F("404: Nicht gefunden: ")) + uri);
    _server->send(404, "text/plain", "404: Not Found");
  });

#if USE_PROMETHEUS_METRICS
  // Register /metrics endpoint for Prometheus
  _server->on(PROMETHEUS_METRICS_PATH, HTTP_GET, [this]() {
    if (_metricsHandler && _metricsHandler->isEnabled()) {
      _server->send(200, "text/plain; charset=utf-8", _metricsHandler->handleMetrics());
    } else {
      _server->send(503, "text/plain", "Prometheus metrics not available");
    }
  });
  LOG_DEBUG(F("WebManager"), F("Prometheus /metrics-Route registriert"));
#endif

  LOG_INFO(F("WebManager"),
           F("Essenzielle Routen registriert - Handler werden bei Bedarf geladen"));
}

void WebManager::setupMinimalRoutes() {
  if (!_router || !_server) {
    LOG_ERROR(
        F("WebManager"),
        F("Kann minimale Routen nicht registrieren - Router oder Server nicht initialisiert"));
    return;
  }

  LOG_DEBUG(F("WebManager"), F("Registriere minimale Routen (Lazy-Loading aktiv)"));

  // Create minimal admin handler
  _minimalAdminHandler = std::make_unique<AdminMinimalHandler>(*_server, *_auth);

  // Register existing OTA routes - critical for updates
  auto result = _otaHandler->registerRoutes(*_router);
  if (!result.isSuccess()) {
    LOG_ERROR(F("WebManager"),
              String(F("Registrieren der OTA-Routen fehlgeschlagen: ")) + result.getMessage());
    return;
  }
  LOG_INFO(F("WebManager"), F("OTA-Routen erfolgreich registriert"));

  // Register admin setUpdate route - critical
  auto rebootResult =
      _router->addRoute(HTTP_POST, "/admin/config/update", [this]() { handleSetUpdate(); });
  if (!rebootResult.isSuccess()) {
    LOG_ERROR(F("WebManager"),
              String(F("Registrieren der /admin/config/update-Route fehlgeschlagen: ")) +
                  result.getMessage());
    return;
  }

  // CRITICAL: Catch-all handler to forward requests to router in minimal mode
  _server->onNotFound([this]() {
    String uri = _server->uri();
    HTTPMethod method = _server->method();

    // Debug: Log every request that hits onNotFound
    LOG_DEBUG(F("WebManager"), String(F("onNotFound aufgerufen für: ")) +
                                   String(method == HTTP_GET    ? F("GET")
                                          : method == HTTP_POST ? F("POST")
                                                                : F("OTHER")) +
                                   String(F(" ")) + uri);

    // Statische Assets auch im Minimalmodus (Update-Seite braucht CSS und JS)
    if (method == HTTP_GET && tryServeStaticFile(uri)) {
      return;
    }

    // Let router handle the request
    if (_router && _router->handleRequest(method, uri)) {
      // Request was handled by router
      LOG_DEBUG(F("WebManager"), String(F("Router hat Request behandelt: ")) + uri);
      return;
    }

    // No route found
    LOG_WARN(F("WebManager"), String(F("404: Nicht gefunden: ")) + uri);
    _server->send(404, "text/plain", "404: Not Found");
  });

  LOG_INFO(F("WebManager"), F("Minimal-Routen registriert - Handler werden bei Bedarf geladen"));
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
    LOG_DEBUG(F("WebManager"), String(F("Entferne Route: ")) + methodToString(method) + " " + path);
  }
}
