/**
 * @file web_manager_init.cpp
 * @brief WebManager-Initialisierung und Dienst-Einrichtung
 */

#include <stdexcept>

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"
#include "web/handler/log_handler.h"
#if USE_WEBSOCKET
#include "web/services/websocket.h"
#endif
#include "utils/wifi.h"

ResourceResult WebManager::begin(uint16_t port) {
  if (_initialized) {
    logger.warning(F("WebManager"), F("WebManager bereits initialisiert"));
    return ResourceResult::success();
  }

  _port = port;
  logger.info(F("WebManager"), "Initialisiere WebManager auf Port " + String(_port));

  try {
    // Wichtige Dienste zuerst initialisieren
    _server = std::make_unique<ESP8266WebServer>(_port);
    _auth = std::make_unique<WebAuth>(*_server);
    _router = std::make_unique<WebRouter>(*_server);
    _cssService = std::make_unique<CSSService>(*_server);
    _otaHandler = std::make_unique<WebOTAHandler>(*_server, *_auth);

#if USE_WEBSOCKET
    // WebSocket-Server zuerst initialisieren
    if (!WebSocketService::getInstance().init(81, nullptr)) {
      logger.error(F("WebManager"), F("WebSocket-Server konnte nicht initialisiert werden"));
      return ResourceResult::fail(ResourceError::WEBSOCKET_ERROR,
                                  F("WebSocket-Server konnte nicht initialisiert werden"));
    }

    // Event-Handler für WebSocket setzen - LogHandler wird dynamisch aus Cache geholt
    auto& ws = WebSocketService::getInstance();
    ws.setEventHandler([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
      // LogHandler aus Cache holen (wird lazy-geladen wenn nötig)
      auto* logHandler = getCachedHandler("log");
      if (logHandler) {
        static_cast<LogHandler*>(logHandler)->handleWebSocketEvent(num, type, payload, length);
      }
    });
#endif

    // Middleware und Basisrouten einrichten
    setupMiddleware();

    // Webdienste (inkl. statischer Dateien) initialisieren
    setupServices();

    // Routen einrichten (Handler werden lazy-geladen)
    setupRoutes();

    // Lazy-Loading-Middleware sofort registrieren
    initializeRemainingHandlers();

    _server->begin();
    _initialized = true;

    return ResourceResult::success();
  } catch (const std::exception& e) {
    logger.error(F("WebManager"),
                 "WebManager konnte nicht initialisiert werden: " + String(e.what()));
    cleanup();
    return ResourceResult::fail(ResourceError::WEBSERVER_INIT_FAILED,
                                String("Ausnahme: ") + e.what());
  }
}

ResourceResult WebManager::beginUpdateMode() {
  logger.info(F("WebManager"), F("Wechsel in minimalen Update-Modus"));

  try {
    // Startzeit für Update-Modus setzen (Timeout-Absicherung)
    m_updateModeStartTime = millis();
    logger.debug(F("WebManager"),
                 F("Update-Modus Startzeit gesetzt: ") + String(m_updateModeStartTime));

    // Alle Dienste zuerst stoppen
    if (_sensorManager) {
      logger.info(F("WebManager"), F("Sensor-Manager wird gestoppt"));
      _sensorManager->stopAll();
      _sensorManager = nullptr;
    }

    // Speicher freigeben vor Update
    stop();
    cleanup();

    delay(500);
    ESP.wdtFeed();

    // Minimale Dienste mit expliziten Speicherzuweisungen erstellen
    logger.logMemoryStats(F("vor_minimalen_diensten"));
    auto setupResult = setupMinimalServices();
    if (!setupResult.isSuccess()) {
      logger.error(F("WebManager"), F("Minimale Dienste konnten nicht eingerichtet werden: ") +
                                        setupResult.getMessage());
      return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                                  F("Minimale Dienste konnten nicht eingerichtet werden: ") +
                                      setupResult.getMessage());
    }

    // Explizit als Minimalmodus markieren
    m_handlersInitialized = true; // Volle Handler-Initialisierung verhindern

    // Nur Minimalmodus-Routen einrichten
    setupMinimalRoutes();

    _server->begin();
    logger.info(F("WebManager"), F("Update-Server im Minimalmodus gestartet"));
    logger.logMemoryStats(F("update_modus_abgeschlossen"));

    _initialized = true;
    return ResourceResult::success();

  } catch (const std::exception& e) {
    logger.error(F("WebManager"),
                 "Update-Modus konnte nicht gestartet werden: " + String(e.what()));
    cleanup();
    return ResourceResult::fail(ResourceError::WEBSERVER_ERROR, String("Ausnahme: ") + e.what());
  }
}

ResourceResult WebManager::setupMinimalServices() {
  try {
    // Dienste in bestimmter Reihenfolge anlegen
    _server = std::make_unique<ESP8266WebServer>(_port);
    if (!_server) {
      return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                  F("Webserver konnte nicht angelegt werden"));
    }

    _auth = std::make_unique<WebAuth>(*_server);
    if (!_auth) {
      return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                  F("Authentifizierungsdienst konnte nicht angelegt werden"));
    }

    _router = std::make_unique<WebRouter>(*_server);
    if (!_router) {
      return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                  F("Webrouter konnte nicht angelegt werden"));
    }

    // OTA-Handler ohne Template-Engine-Abhängigkeit erstellen
    _otaHandler = std::make_unique<WebOTAHandler>(*_server, *_auth);
    if (!_otaHandler) {
      return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                  F("OTA-Handler konnte nicht angelegt werden"));
    }

    // Register essential static files for OTA update page
    // CSS files needed for update page
    _server->on("/css/style.css", HTTP_GET,
                [this]() { serveStaticFile("/css/style.css", "text/css", "max-age=86400"); });

    _server->on("/css/admin.css", HTTP_GET,
                [this]() { serveStaticFile("/css/admin.css", "text/css", "max-age=86400"); });

    // JavaScript file for OTA functionality
    _server->on("/js/ota.js", HTTP_GET, [this]() {
      serveStaticFile("/js/ota.js", "application/javascript", "max-age=86400");
    });

    // Favicon (often requested by browsers)
    _server->on("/favicon.ico", HTTP_GET,
                [this]() { serveStaticFile("/favicon.ico", "image/x-icon", "max-age=86400"); });

    logger.debug(F("WebManager"), F("Statische Dateien für Update-Modus registriert"));

    _server->begin();

    return ResourceResult::success();
  } catch (const std::exception& e) {
    return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                                String("Ausnahme in minimalen Diensten: ") + e.what());
  }
}

ResourceResult WebManager::setupServices() {
  try {
    // CSS files
    _server->on("/css/style.css", HTTP_GET,
                [this]() { serveStaticFile("/css/style.css", "text/css", "max-age=86400"); });

    _server->on("/css/start.css", HTTP_GET,
                [this]() { serveStaticFile("/css/start.css", "text/css", "max-age=86400"); });

    _server->on("/css/admin.css", HTTP_GET,
                [this]() { serveStaticFile("/css/admin.css", "text/css", "max-age=86400"); });

    _server->on("/css/logs.css", HTTP_GET,
                [this]() { serveStaticFile("/css/logs.css", "text/css", "max-age=86400"); });

    // JavaScript files
    _server->on("/js/sensors.js", HTTP_GET, [this]() {
      serveStaticFile("/js/sensors.js", "application/javascript", "max-age=86400");
    });

    _server->on("/js/admin.js", HTTP_GET, [this]() {
      serveStaticFile("/js/admin.js", "application/javascript", "max-age=86400");
    });

    _server->on("/js/logs.js", HTTP_GET, [this]() {
      serveStaticFile("/js/logs.js", "application/javascript", "max-age=86400");
    });

    _server->on("/js/ota.js", HTTP_GET, [this]() {
      serveStaticFile("/js/ota.js", "application/javascript", "max-age=86400");
    });

    _server->on("/js/admin_sensors.js", HTTP_GET, [this]() {
      serveStaticFile("/js/admin_sensors.js", "application/javascript", "max-age=86400");
    });

    _server->on("/js/admin_display.js", HTTP_GET, [this]() {
      serveStaticFile("/js/admin_display.js", "application/javascript", "max-age=86400");
    });

    // Images
    _server->on("/img/cloud_big.png", HTTP_GET,
                [this]() { serveStaticFile("/img/cloud_big.png", "image/png", "max-age=86400"); });

    _server->on("/img/flower_big.gif", HTTP_GET,
                [this]() { serveStaticFile("/img/flower_big.gif", "image/gif", "max-age=86400"); });

    _server->on("/img/face-happy.gif", HTTP_GET,
                [this]() { serveStaticFile("/img/face-happy.gif", "image/gif", "max-age=86400"); });

    _server->on("/img/face-neutral.gif", HTTP_GET, [this]() {
      serveStaticFile("/img/face-neutral.gif", "image/gif", "max-age=86400");
    });

    _server->on("/img/face-sad.gif", HTTP_GET,
                [this]() { serveStaticFile("/img/face-sad.gif", "image/gif", "max-age=86400"); });

    _server->on("/img/face-error.gif", HTTP_GET,
                [this]() { serveStaticFile("/img/face-error.gif", "image/gif", "max-age=86400"); });

    _server->on("/img/sensor-leaf.png", HTTP_GET, [this]() {
      serveStaticFile("/img/sensor-leaf.png", "image/png", "max-age=86400");
    });

    _server->on("/img/sensor-stem.png", HTTP_GET, [this]() {
      serveStaticFile("/img/sensor-stem.png", "image/png", "max-age=86400");
    });

    _server->on("/img/earth.png", HTTP_GET,
                [this]() { serveStaticFile("/img/earth.png", "image/png", "max-age=86400"); });

    _server->on("/img/fabmobil.png", HTTP_GET,
                [this]() { serveStaticFile("/img/fabmobil.png", "image/png", "max-age=86400"); });

    // Favicon
    _server->on("/favicon.ico", HTTP_GET,
                [this]() { serveStaticFile("/favicon.ico", "image/x-icon", "max-age=86400"); });

    logger.debug(F("WebManager"), F("Routen für statische Dateien konfiguriert"));

    logger.info(F("WebManager"), F("Statische Dateiauslieferung erfolgreich initialisiert"));

    return ResourceResult::success();

  } catch (const std::exception& e) {
    logger.error(F("WebManager"),
                 "Statische Dateiauslieferung konnte nicht initialisiert werden: " +
                     String(e.what()));
    throw;
  }
}

void WebManager::setupMiddleware() {
  logger.debug(F("WebManager"), F("Middleware wird eingerichtet..."));

  // Middleware: Öffentliche Assets und Startseite sind zugänglich; Admin-Routen benötigen Authentifizierung.
  _router->addMiddleware([this](HTTPMethod method, String url) {
    // Öffentliche Routen
    if (url == "/" || url == "/getLatestValues" || url.startsWith("/css/") ||
        url.startsWith("/js/") || url.startsWith("/img/") || url.startsWith("/favicon")) {
      return true;
    }

    // Admin-Routen benötigen Authentifizierung
    if (url.startsWith("/admin")) {
      if (!_server->authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
        _server->requestAuthentication();
        return false;
      }
    }
    return true;
  });

  // Logging-Middleware hinzufügen
  _router->addMiddleware([this](HTTPMethod method, String url) {
    logger.debug(F("WebManager"), F("Anfrage: ") + methodToString(method) + F(" ") + url);
    return true;
  });

  logger.debug(F("WebManager"), F("Middleware-Konfiguration abgeschlossen"));
}
