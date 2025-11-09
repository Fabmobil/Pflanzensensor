/**
 * @file web_manager_cache.cpp
 * @brief WebManager handler caching and memory management with lazy loading
 */

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"

void WebManager::initializeRemainingHandlers() {
  if (m_handlersInitialized)
    return;

  try {
    if (_router) {
      // Add middleware for lazy handler initialization and route registration
      // Handlers are created on-demand and cached using LRU policy
      _router->addMiddleware([this](HTTPMethod method, String url) {
        // Root route only
        if (url == "/") {
          BaseHandler* handler = getCachedHandler("startpage");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lazy-Loading: StartpageHandler"));
            auto newHandler = std::make_unique<StartpageHandler>(*_server, *_auth, *_cssService);

            // Set handler type context for route registration
            _router->setHandlerTypeContext("startpage");
            auto result = newHandler->registerRoutes(*_router);
            _router->clearHandlerTypeContext();

            if (!result.isSuccess()) {
              logger.error(F("WebManager"),
                           F("Lazy-Registrierung fehlgeschlagen (StartpageHandler): ") +
                               result.getMessage());
              return false; // Block request on registration failure
            }
            cacheHandler(std::move(newHandler), "startpage");
          } else {
            updateHandlerAccess("startpage");
          }
        }
        // Log routes
        else if (url.startsWith("/logs")) {
          BaseHandler* handler = getCachedHandler("log");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lazy-Loading: LogHandler"));
            auto newHandler = std::unique_ptr<LogHandler>(
                LogHandler::getInstance(*_server, *_auth, *_cssService));

            // Set handler type context for route registration
            _router->setHandlerTypeContext("log");
            auto result = newHandler->registerRoutes(*_router);
            _router->clearHandlerTypeContext();

            if (!result.isSuccess()) {
              logger.error(F("WebManager"), F("Lazy-Registrierung fehlgeschlagen (LogHandler): ") +
                                                result.getMessage());
              return false; // Block request on registration failure
            }
            cacheHandler(std::move(newHandler), "log");
          } else {
            updateHandlerAccess("log");
          }
        }
        // Admin sensor routes
        else if ((url.startsWith("/admin/sensors") || url == "/trigger_measurement" ||
                  url == "/admin/getSensorConfig") &&
                 _sensorManager) {
          BaseHandler* handler = getCachedHandler("admin_sensor");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lazy-Loading: AdminSensorHandler"));
            auto newHandler = std::make_unique<AdminSensorHandler>(*_server, *_auth, *_cssService,
                                                                   *_sensorManager);

            _router->setHandlerTypeContext("admin_sensor");
            auto result = newHandler->registerRoutes(*_router);
            _router->clearHandlerTypeContext();

            if (!result.isSuccess()) {
              logger.error(F("WebManager"),
                           F("Lazy-Registrierung fehlgeschlagen (AdminSensorHandler): ") +
                               result.getMessage());
              return false; // Block request on registration failure
            }
            cacheHandler(std::move(newHandler), "admin_sensor");
          } else {
            updateHandlerAccess("admin_sensor");
          }
        }
#if USE_DISPLAY
        // Display routes
        else if (url.startsWith("/admin/display")) {
          BaseHandler* handler = getCachedHandler("display");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lazy-Loading: AdminDisplayHandler"));
            auto newHandler = std::make_unique<AdminDisplayHandler>(*_server);

            _router->setHandlerTypeContext("display");
            auto result = newHandler->registerRoutes(*_router);
            _router->clearHandlerTypeContext();

            if (!result.isSuccess()) {
              logger.error(F("WebManager"),
                           F("Lazy-Registrierung fehlgeschlagen (AdminDisplayHandler): ") +
                               result.getMessage());
              return false; // Block request on registration failure
            }
            cacheHandler(std::move(newHandler), "display");
          } else {
            updateHandlerAccess("display");
          }
        }
#endif
        // General admin routes (excluding special cases handled above)
        else if (url.startsWith("/admin") && !url.startsWith("/admin/sensors") &&
                 !url.startsWith("/admin/display") && !(url == "/admin/update") &&
                 !url.startsWith("/admin/config/update") && !(url == "/admin/uploadConfig")) {
          BaseHandler* handler = getCachedHandler("admin");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lazy-Loading: AdminHandler für URL: ") + url);
            auto newHandler = std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);

            _router->setHandlerTypeContext("admin");
            auto result = newHandler->registerRoutes(*_router);
            _router->clearHandlerTypeContext();

            if (!result.isSuccess()) {
              logger.error(F("WebManager"),
                           F("Lazy-Registrierung fehlgeschlagen (AdminHandler): ") +
                               result.getMessage());
              return false; // Block request on registration failure
            }
            cacheHandler(std::move(newHandler), "admin");
          } else {
            updateHandlerAccess("admin");
          }
        }
        // Sensor data routes
        else if (url == "/getLatestValues" || (url.startsWith("/sensor") && _sensorManager)) {
          BaseHandler* handler = getCachedHandler("sensor");
          if (!handler) {
            logger.debug(F("WebManager"), F("Lazy-Loading: SensorHandler"));
            auto newHandler =
                std::make_unique<SensorHandler>(*_server, *_auth, *_cssService, *_sensorManager);

            _router->setHandlerTypeContext("sensor");
            auto result = newHandler->registerRoutes(*_router);
            _router->clearHandlerTypeContext();

            if (!result.isSuccess()) {
              logger.error(F("WebManager"),
                           F("Lazy-Registrierung fehlgeschlagen (SensorHandler): ") +
                               result.getMessage());
              return false; // Block request on registration failure
            }
            cacheHandler(std::move(newHandler), "sensor");
          } else {
            updateHandlerAccess("sensor");
          }
        }

        return true; // Continue with routing
      });

      m_handlersInitialized = true;
      logger.info(F("WebManager"), F("Lazy-Loading-Middleware aktiviert (LRU-Cache: ") +
                                       String(MAX_ACTIVE_HANDLERS) + F(" Handler)"));

      // Log initial route count (only essential routes registered)
      if (_router) {
        _router->logRouteStats();
      }
    }
  } catch (const std::exception& e) {
    logger.error(F("WebManager"),
                 String(F("Handler konnten nicht initialisiert werden: ")) + String(e.what()));
    cleanupNonEssentialHandlers();
  }
}

void WebManager::cleanupNonEssentialHandlers() {
  // Ensure all cached handlers are cleaned up before clearing the cache.
  // Some handlers (e.g. LogHandler) hold resources like WebSocket client
  // lists or callbacks that must be released via cleanup(). Simply
  // clearing the list would drop unique_ptrs without calling their
  // cleanup hooks which can leak memory/resources on constrained devices.
  logger.debug(F("WebManager"),
               F("Bereinige Handler-Cache (") + String(m_handlerCache.size()) + F(" Einträge)"));

  for (auto& entry : m_handlerCache) {
    if (entry.handler) {
      logger.debug(F("WebManager"), F("Cleanup: ") + entry.handlerType);
      entry.handler->cleanup();
    }
  }
  // Clear the entire handler cache
  m_handlerCache.clear();

  m_handlersInitialized = false;
}

void WebManager::cleanupHandlers() {
  logger.beginMemoryTracking(F("handlers_cleanup"));

  // Cleanup essential handlers (not in cache)
  if (_otaHandler)
    _otaHandler->cleanup();
  if (_minimalAdminHandler)
    _minimalAdminHandler->cleanup();

  // Cleanup all cached handlers
  for (auto& entry : m_handlerCache) {
    if (entry.handler) {
      logger.debug(F("WebManager"), F("Cleanup cached: ") + entry.handlerType);
      entry.handler->cleanup();
    }
  }
  m_handlerCache.clear();

  logger.endMemoryTracking(F("handlers_cleanup"));
}

void WebManager::cacheHandler(std::unique_ptr<BaseHandler> handler, const String& handlerType) {
  if (!handler) {
    logger.warning(F("WebManager"), F("Versuch, nullptr-Handler zu cachen: ") + handlerType);
    return;
  }

  // Check if handler already exists in cache
  for (auto& entry : m_handlerCache) {
    if (entry.handlerType == handlerType) {
      logger.debug(F("WebManager"), F("Handler bereits im Cache: ") + handlerType);
      entry.lastAccess = millis(); // Update access time
      return;
    }
  }

  // Remove oldest handler if at capacity
  if (m_handlerCache.size() >= MAX_ACTIVE_HANDLERS) {
    evictOldestHandler();
  }

  logger.info(F("WebManager"), F("Cache-Handler (") + String(m_handlerCache.size() + 1) + F("/") +
                                   String(MAX_ACTIVE_HANDLERS) + F("): ") + handlerType);

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

  // Log eviction with age information
  unsigned long age = (millis() - oldest->lastAccess) / 1000; // seconds
  logger.info(F("WebManager"), F("LRU-Eviction: ") + oldest->handlerType + F(" (inaktiv seit ") +
                                   String(age) + F("s)"));

  // Remove routes registered by this handler
  if (_router) {
    _router->removeHandlerRoutes(oldest->handlerType);
  }

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
