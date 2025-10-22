/**
 * @file web_manager_cache.cpp
 * @brief WebManager handler caching and memory management
 */

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::initializeRemainingHandlers() {
  if (m_handlersInitialized)
    return;

  try {
    if (_router) {
      // Add middleware for lazy handler initialization
      _router->addMiddleware([this](HTTPMethod method, String url) {
        // Root and sensor data routes
        if (url == "/" || url == "/getLatestValues") {
          BaseHandler* handler = getCachedHandler("startpage");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lade StartpageHandler bei Bedarf"));
            auto newHandler = std::make_unique<StartpageHandler>(*_server, *_auth, *_cssService);
            newHandler->registerRoutes(*_router);
            cacheHandler(std::move(newHandler), "startpage");
          } else {
            updateHandlerAccess("startpage");
          }
        }
        // Log routes
        else if (url.startsWith("/logs")) {
          BaseHandler* handler = getCachedHandler("log");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lade LogHandler bei Bedarf"));
            if (!_logHandler) {
              _logHandler = std::unique_ptr<LogHandler>(
                  LogHandler::getInstance(*_server, *_auth, *_cssService));
            }
            _logHandler->registerRoutes(*_router);
            cacheHandler(std::move(_logHandler), "log");
          } else {
            updateHandlerAccess("log");
          }
        }
        // Admin sensor routes are now handled by explicit registration in
        // setupRoutes No middleware intervention needed for /admin/sensors and
        // /trigger_measurement General admin routes
        else if (url.startsWith("/admin") && !url.startsWith("/admin/sensors") &&
                 !url.startsWith("/admin/display") && !(url == "/admin/update") &&
                 !url.startsWith("/admin/config/update")) {
          BaseHandler* handler = getCachedHandler("admin");
          if (!handler) {
            logger.debug(F("WebManager"),
                         String(F("Lade AdminHandler für URL bei Bedarf: ")) + url);
            auto newHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
            auto result = newHandler->registerRoutes(*_router);
            if (!result.isSuccess()) {
              logger.error(F("WebManager"),
                           String(F("Registrieren der Admin-Routen fehlgeschlagen: ")) +
                               result.getMessage());
              return true;
            }
            logger.debug(F("WebManager"), F("Admin-Routen erfolgreich registriert"));
            cacheHandler(std::move(newHandler), "admin");
          } else {
            updateHandlerAccess("admin");
          }
        }
        // **FIXED: Sensor routes - check sensor manager**
        else if (url.startsWith("/sensor") && _sensorManager) {
          BaseHandler* handler = getCachedHandler("sensor");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lade SensorHandler bei Bedarf"));
            auto newHandler =
                std::make_unique<SensorHandler>(*_server, *_auth, *_cssService, *_sensorManager);
            newHandler->registerRoutes(*_router);
            cacheHandler(std::move(newHandler), "sensor");
          } else {
            updateHandlerAccess("sensor");
          }
        }
#if USE_DISPLAY
        // Display routes
        else if (url.startsWith("/admin/display")) {
          BaseHandler* handler = getCachedHandler("display");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lade AdminDisplayHandler bei Bedarf"));
            auto newHandler = std::make_unique<AdminDisplayHandler>(*_server);
            auto result = newHandler->registerRoutes(*_router);
            if (!result.isSuccess()) {
              logger.error(F("WebManager"),
                           String(F("Registrieren der Display-Routen fehlgeschlagen: ")) +
                               result.getMessage());
              return true;
            }
            cacheHandler(std::move(newHandler), "display");
          } else {
            updateHandlerAccess("display");
          }
        }
#endif
        return true;
      });

      m_handlersInitialized = true;
      logger.info(F("WebManager"), F("Middleware zur Handler-Initialisierung registriert"));
    }
  } catch (const std::exception& e) {
    logger.error(F("WebManager"),
                 String(F("Handler konnten nicht initialisiert werden: ")) + String(e.what()));
    cleanupNonEssentialHandlers();
  }
}

void WebManager::cleanupNonEssentialHandlers() {
  // Cleanup LogHandler first to ensure WebSocket connections are closed
  if (_logHandler) {
    _logHandler->cleanup();
    _logHandler.reset();
  }

  // Ensure all cached handlers are cleaned up before clearing the cache.
  // Some handlers (e.g. LogHandler) hold resources like WebSocket client
  // lists or callbacks that must be released via cleanup(). Simply
  // clearing the list would drop unique_ptrs without calling their
  // cleanup hooks which can leak memory/resources on constrained devices.
  for (auto& entry : m_handlerCache) {
    if (entry.handler) {
      entry.handler->cleanup();
    }
  }
  // Clear the entire handler cache
  m_handlerCache.clear();

  m_handlersInitialized = false;
}

void WebManager::cleanupHandlers() {
  logger.beginMemoryTracking(F("handlers_cleanup"));

  // Cleanup all handlers
  if (_startHandler)
    _startHandler->cleanup();
  if (_adminHandler)
    _adminHandler->cleanup();
  if (_sensorHandler)
    _sensorHandler->cleanup();
  if (_adminSensorHandler)
    _adminSensorHandler->cleanup();
  if (_otaHandler)
    _otaHandler->cleanup();
  if (_logHandler)
    _logHandler->cleanup();
  if (_minimalAdminHandler)
    _minimalAdminHandler->cleanup();
    // WiFiSetupHandler deprecated/entfernt: keine explizite Bereinigung nötig
#if USE_DISPLAY
  if (_displayHandler)
    _displayHandler->cleanup();
#endif

  logger.endMemoryTracking(F("handlers_cleanup"));
}

void WebManager::cacheHandler(std::unique_ptr<BaseHandler> handler, const String& handlerType) {
  logger.debug(F("WebManager"), String(F("Cache-Handler: ")) + handlerType);

  // Remove oldest handler if at capacity
  if (m_handlerCache.size() >= MAX_ACTIVE_HANDLERS) {
    evictOldestHandler();
  }

  // Add new handler to cache
  HandlerCacheEntry entry{std::move(handler), millis(), handlerType};
  m_handlerCache.push_back(std::move(entry));
}

BaseHandler* WebManager::getCachedHandler(const String& handlerType) {
  for (auto& entry : m_handlerCache) {
    if (entry.handlerType == handlerType) {
      entry.lastAccess = millis(); // Update access time
      return entry.handler.get();
    }
  }
  return nullptr;
}

void WebManager::evictOldestHandler() {
  if (m_handlerCache.empty())
    return;

  // Find oldest entry
  auto oldest = m_handlerCache.begin();
  for (auto it = m_handlerCache.begin(); it != m_handlerCache.end(); ++it) {
    if (it->lastAccess < oldest->lastAccess) {
      oldest = it;
    }
  }

  // Log eviction
  logger.debug(F("WebManager"), String(F("Entferne Handler aus Cache: ")) + oldest->handlerType);

  // Cleanup handler before removing
  if (oldest->handler) {
    oldest->handler->cleanup();
  }

  // Remove from cache
  m_handlerCache.erase(oldest);
}

void WebManager::updateHandlerAccess(const String& handlerType) {
  for (auto& entry : m_handlerCache) {
    if (entry.handlerType == handlerType) {
      entry.lastAccess = millis();
      break;
    }
  }
}
