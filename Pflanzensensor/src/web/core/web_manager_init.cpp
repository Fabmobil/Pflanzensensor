/**
 * @file web_manager_init.cpp
 * @brief WebManager initialization and service setup
 */

#include <stdexcept>

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"
#include "web/handler/log_handler.h"
#include "web/handler/wifi_setup_handler.h"
#if USE_WEBSOCKET
#include "web/services/websocket.h"
#endif
#include "utils/wifi.h"

ResourceResult WebManager::begin(uint16_t port) {
  if (_initialized) {
    logger.warning(F("WebManager"), F("WebManager already initialized"));
    return ResourceResult::success();
  }

  _port = port;
  logger.info(F("WebManager"),
              "Initializing WebManager on port " + String(_port));

  try {
    // Initialize essential services first
    _server = std::make_unique<ESP8266WebServer>(_port);
    _auth = std::make_unique<WebAuth>(*_server);
    _router = std::make_unique<WebRouter>(*_server);
    _cssService = std::make_unique<CSSService>(*_server);
    _otaHandler = std::make_unique<WebOTAHandler>(*_server, *_auth);

#if USE_WEBSOCKET
    // Initialize WebSocket server first
    if (!WebSocketService::getInstance().init(81, nullptr)) {
      logger.error(F("WebManager"), F("Failed to initialize WebSocket server"));
      return ResourceResult::fail(ResourceError::WEBSOCKET_ERROR,
                                  F("Failed to initialize WebSocket server"));
    }
#endif

    // Create and initialize LogHandler
    _logHandler = std::unique_ptr<LogHandler>(
        LogHandler::getInstance(*_server, *_auth, *_cssService));
    if (!_logHandler || !_logHandler->isInitialized()) {
      logger.error(F("WebManager"),
                   F("Failed to create or initialize LogHandler"));
      return ResourceResult::fail(
          ResourceError::WEBSERVER_ERROR,
          F("Failed to create or initialize LogHandler"));
    }

    // Set up real-time log broadcasting
#if USE_WEBSOCKET
    logger.setCallback([](LogLevel level, const String& message) {
      auto* logHandler = LogHandler::s_instance;
      if (logHandler && logHandler->isInitialized()) {
        logHandler->broadcastLog(level, message);
      }
    });
#endif

    // Set up log forwarding
#if USE_WEBSOCKET
    // Register LogHandler with WebSocket after creation
    auto& ws = WebSocketService::getInstance();
    if (!ws.isInitialized()) {
      logger.error(F("WebManager"), F("WebSocket not initialized"));
      return ResourceResult::fail(ResourceError::WEBSOCKET_ERROR,
                                  F("WebSocket not initialized"));
    }

    // Update the event handler for the existing WebSocket instance
    ws.setEventHandler(
        [this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
          if (_logHandler && _logHandler->isInitialized()) {
            _logHandler->handleWebSocketEvent(num, type, payload, length);
          }
        });
#endif

    // Set up basic routes and middleware
    setupMiddleware();

    // Initialize web services (including static files)
    setupServices();

    // Register LogHandler routes first
    if (_logHandler && _logHandler->isInitialized()) {
      auto result = _logHandler->registerRoutes(*_router);
      if (!result.isSuccess()) {
        logger.error(F("WebManager"), "Failed to register LogHandler routes: " +
                                          result.getMessage());
        return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                                    F("Failed to register LogHandler routes"));
      }
    }

    // Setup other routes
    setupRoutes();

    _server->begin();
    _initialized = true;

    // Schedule deferred handler initialization
    m_handlersInitialized = false;

    return ResourceResult::success();
  } catch (const std::exception& e) {
    logger.error(F("WebManager"),
                 "Failed to initialize WebManager: " + String(e.what()));
    cleanup();
    return ResourceResult::fail(ResourceError::WEBSERVER_INIT_FAILED,
                                String("Exception: ") + e.what());
  }
}

ResourceResult WebManager::beginUpdateMode() {
  logger.info(F("WebManager"), F("Entering minimal update mode"));

  try {
    // Set the update mode start time for timeout recovery
    m_updateModeStartTime = millis();
    logger.debug(F("WebManager"), F("Update mode start time set: ") +
                                      String(m_updateModeStartTime));

    // Stop all services first
    if (_sensorManager) {
      logger.info(F("WebManager"), F("Stopping sensor manager"));
      _sensorManager->stopAll();
      _sensorManager = nullptr;
    }

    // Free memory before handling update
    stop();
    cleanup();

    delay(500);
    ESP.wdtFeed();

    // Create minimal services with explicit memory allocations
    logger.logMemoryStats(F("before_minimal_services"));
    auto setupResult = setupMinimalServices();
    if (!setupResult.isSuccess()) {
      logger.error(F("WebManager"), F("Failed to setup minimal services: ") +
                                        setupResult.getMessage());
      return ResourceResult::fail(
          ResourceError::WEBSERVER_ERROR,
          F("Failed to setup minimal services: ") + setupResult.getMessage());
    }

    // Explicitly mark as minimal mode
    m_handlersInitialized = true;  // Prevent full handler initialization

    // Setup minimal mode routes only
    setupMinimalRoutes();

    _server->begin();
    logger.info(F("WebManager"), F("Update server started in minimal mode"));
    logger.logMemoryStats(F("update_mode_complete"));

    _initialized = true;
    return ResourceResult::success();

  } catch (const std::exception& e) {
    logger.error(F("WebManager"),
                 "Failed to enter update mode: " + String(e.what()));
    cleanup();
    return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                                String("Exception: ") + e.what());
  }
}

ResourceResult WebManager::setupMinimalServices() {
  try {
    // Allocate services in specific order
    _server = std::make_unique<ESP8266WebServer>(_port);
    if (!_server) {
      return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                  F("Failed to allocate web server"));
    }

    _auth = std::make_unique<WebAuth>(*_server);
    if (!_auth) {
      return ResourceResult::fail(
          ResourceError::RESOURCE_ERROR,
          F("Failed to allocate authentication service"));
    }

    _router = std::make_unique<WebRouter>(*_server);
    if (!_router) {
      return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                  F("Failed to allocate web router"));
    }

    // Create OTA handler without template engine dependency
    _otaHandler = std::make_unique<WebOTAHandler>(*_server, *_auth);
    if (!_otaHandler) {
      return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                  F("Failed to allocate OTA handler"));
    }

    _server->begin();

    return ResourceResult::success();
  } catch (const std::exception& e) {
    return ResourceResult::fail(
        ResourceError::WEBSERVER_ERROR,
        String("Exception in minimal services: ") + e.what());
  }
}

ResourceResult WebManager::setupServices() {
  logger.debug(F("WebManager"), F("Setting up static file serving..."));

  try {
    // Set up static file serving with custom handlers to avoid MD5 issues
    logger.debug(F("WebManager"), F("Setting up static file serving"));

    // CSS files
    _server->on("/css/style.css", HTTP_GET, [this]() {
      serveStaticFile("/css/style.css", "text/css", "max-age=86400");
    });

    _server->on("/css/start.css", HTTP_GET, [this]() {
      serveStaticFile("/css/start.css", "text/css", "max-age=86400");
    });

    _server->on("/css/admin.css", HTTP_GET, [this]() {
      serveStaticFile("/css/admin.css", "text/css", "max-age=86400");
    });

    _server->on("/css/logs.css", HTTP_GET, [this]() {
      serveStaticFile("/css/logs.css", "text/css", "max-age=86400");
    });



    // JavaScript files
    _server->on("/js/sensors.js", HTTP_GET, [this]() {
      serveStaticFile("/js/sensors.js", "application/javascript",
                      "max-age=86400");
    });

    _server->on("/js/admin.js", HTTP_GET, [this]() {
      serveStaticFile("/js/admin.js", "application/javascript",
                      "max-age=86400");
    });

    _server->on("/js/logs.js", HTTP_GET, [this]() {
      serveStaticFile("/js/logs.js", "application/javascript", "max-age=86400");
    });

    _server->on("/js/ota.js", HTTP_GET, [this]() {
      serveStaticFile("/js/ota.js", "application/javascript", "max-age=86400");
    });

    _server->on("/js/admin_sensors.js", HTTP_GET, [this]() {
      serveStaticFile("/js/admin_sensors.js", "application/javascript",
                      "max-age=86400");
    });

    _server->on("/js/admin_display.js", HTTP_GET, [this]() {
      serveStaticFile("/js/admin_display.js", "application/javascript",
                      "max-age=86400");
    });

    // Images
    _server->on("/img/cloud_big.png", HTTP_GET, [this]() {
      serveStaticFile("/img/cloud_big.png", "image/png", "max-age=86400");
    });

    _server->on("/img/flower_big.gif", HTTP_GET, [this]() {
      serveStaticFile("/img/flower_big.gif", "image/gif", "max-age=86400");
    });

    _server->on("/img/face-happy.gif", HTTP_GET, [this]() {
      serveStaticFile("/img/face-happy.gif", "image/gif", "max-age=86400");
    });

    _server->on("/img/face-neutral.gif", HTTP_GET, [this]() {
      serveStaticFile("/img/face-neutral.gif", "image/gif", "max-age=86400");
    });

    _server->on("/img/face-sad.gif", HTTP_GET, [this]() {
      serveStaticFile("/img/face-sad.gif", "image/gif", "max-age=86400");
    });

    _server->on("/img/face-error.gif", HTTP_GET, [this]() {
      serveStaticFile("/img/face-error.gif", "image/gif", "max-age=86400");
    });

    _server->on("/img/sensor-leaf.png", HTTP_GET, [this]() {
      serveStaticFile("/img/sensor-leaf.png", "image/png", "max-age=86400");
    });

    _server->on("/img/sensor-stem.png", HTTP_GET, [this]() {
      serveStaticFile("/img/sensor-stem.png", "image/png", "max-age=86400");
    });

    _server->on("/img/earth.png", HTTP_GET, [this]() {
      serveStaticFile("/img/earth.png", "image/png", "max-age=86400");
    });

    _server->on("/img/fabmobil.png", HTTP_GET, [this]() {
      serveStaticFile("/img/fabmobil.png", "image/png", "max-age=86400");
    });

    // Favicon
    _server->on("/favicon.ico", HTTP_GET, [this]() {
      serveStaticFile("/favicon.ico", "image/x-icon", "max-age=86400");
    });

    logger.debug(F("WebManager"), F("Static files routes configured"));

    logger.info(F("WebManager"),
                F("Static file serving initialized successfully"));

    return ResourceResult::success();

  } catch (const std::exception& e) {
    logger.error(F("WebManager"), "Failed to initialize static file serving: " +
                                      String(e.what()));
    throw;
  }
}

void WebManager::setupMiddleware() {
  logger.debug(F("WebManager"), F("Setting up middleware..."));

  // AP mode middleware - allow WiFi setup and sensor data access
  _router->addMiddleware([this](HTTPMethod method, String url) {
    if (isCaptivePortalAPActive()) {
      // Allow WiFi setup, sensor data, start page, and static assets
      if (url == "/" || url == "/getLatestValues" || url.startsWith("/css/") ||
          url.startsWith("/js/") || url.startsWith("/img/") || url.startsWith("/favicon")) {
        // Allow these URLs in AP mode
        return true;
      }

      // Allow WiFi update endpoint in AP mode
      if (url == "/admin/updateWiFi") {
        return true;
      }

      // For other URLs, allow them (no forced redirects)
      return true;
    }

    // Normal mode - add authentication for admin routes
    if (url.startsWith("/admin")) {
      if (!_server->authenticate("admin",
                                 ConfigMgr.getAdminPassword().c_str())) {
        _server->requestAuthentication();
        return false;
      }
    }
    return true;
  });

  // Add logging middleware
  _router->addMiddleware([this](HTTPMethod method, String url) {
    logger.debug(F("WebManager"),
                 F("Request: ") + methodToString(method) + F(" ") + url);
    return true;
  });

  logger.debug(F("WebManager"), F("Middleware setup complete"));
}
