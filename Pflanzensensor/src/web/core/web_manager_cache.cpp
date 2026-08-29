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

  if (!_router) {
    return;
  }

  // Lazy-Loading-Middleware.
  //
  // Welche URLs zu welchem Handler gehören, sagt jetzt der Handler selbst über
  // sein statisches ownsUrl(). Vorher stand diese Zuordnung hier als zweite,
  // handgepflegte Liste - und die war auseinandergelaufen: der
  // AdminSensorHandler registriert 13 Routen, geladen wurde er aber nur für
  // "/admin/sensors", "/trigger_measurement" und "/admin/getSensorConfig".
  // Die übrigen 9 (analog_minmax, analog_autocal, thresholds,
  // measurement_name, measurement_interval, sensor_update, die beiden
  // reset_absolute_* und analog_autocal_duration) lieferten nach einem
  // frischen Boot 404, solange niemand vorher die Sensorseite geöffnet hatte.
  // Sie fielen stattdessen in den allgemeinen /admin-Zweig und luden den
  // falschen Handler.
  _router->addMiddleware([this](HTTPMethod method, String url) {
    if (StartpageHandler::ownsUrl(url)) {
      return ensureHandler("startpage", [this]() -> std::unique_ptr<BaseHandler> {
        return std::make_unique<StartpageHandler>(*_server, *_auth, *_cssService);
      });
    }

    if (LogHandler::ownsUrl(url)) {
      return ensureHandler("log", [this]() -> std::unique_ptr<BaseHandler> {
        return std::unique_ptr<LogHandler>(LogHandler::getInstance(*_server, *_auth, *_cssService));
      });
    }

    if (AdminSensorHandler::ownsUrl(url)) {
      if (!_sensorManager) {
        return true; // ohne Sensor-Manager gibt es nichts zu registrieren
      }
      return ensureHandler("admin_sensor", [this]() -> std::unique_ptr<BaseHandler> {
        return std::make_unique<AdminSensorHandler>(*_server, *_auth, *_cssService,
                                                    *_sensorManager);
      });
    }

#if USE_DISPLAY
    if (AdminDisplayHandler::ownsUrl(url)) {
      return ensureHandler("display", [this]() -> std::unique_ptr<BaseHandler> {
        return std::make_unique<AdminDisplayHandler>(*_server);
      });
    }
#endif

    if (AdminHandler::ownsUrl(url)) {
      return ensureHandler("admin", [this]() -> std::unique_ptr<BaseHandler> {
        return std::make_unique<AdminHandler>(*_server, *_auth, *_cssService);
      });
    }

    if (SensorHandler::ownsUrl(url)) {
      if (!_sensorManager) {
        return true;
      }
      return ensureHandler("sensor", [this]() -> std::unique_ptr<BaseHandler> {
        return std::make_unique<SensorHandler>(*_server, *_auth, *_cssService, *_sensorManager);
      });
    }

    return true; // Keine Zuständigkeit - Router entscheidet weiter
  });

  m_handlersInitialized = true;
  LOG_INFO(F("WebManager"), String(F("Lazy-Loading-Middleware aktiviert (LRU-Cache: ")) +
                                String(MAX_ACTIVE_HANDLERS) + F(" Handler)"));
  _router->logRouteStats();
}

bool WebManager::ensureHandler(const char* handlerType,
                               std::function<std::unique_ptr<BaseHandler>()> factory) {
  if (getCachedHandler(handlerType)) {
    updateHandlerAccess(handlerType);
    return true;
  }

  LOG_DEBUG(F("WebManager"), String(F("Lazy-Loading: ")) + handlerType);
  auto newHandler = factory();
  if (!newHandler) {
    LOG_ERROR(F("WebManager"), String(F("Handler konnte nicht erstellt werden: ")) + handlerType);
    return false;
  }

  // Routen dem Handler zuordnen, damit sie bei seiner Verdrängung
  // wieder entfernt werden können
  _router->setHandlerTypeContext(handlerType);
  auto result = newHandler->registerRoutes(*_router);
  _router->clearHandlerTypeContext();

  if (!result.isSuccess()) {
    LOG_ERROR(F("WebManager"), String(F("Lazy-Registrierung fehlgeschlagen (")) + handlerType +
                                   String(F("): ")) + result.getMessage());
    return false; // Anfrage blockieren statt mit halb registriertem Handler weiterzumachen
  }

  cacheHandler(std::move(newHandler), handlerType);
  return true;
}

void WebManager::cleanupNonEssentialHandlers() {
  // Ensure all cached handlers are cleaned up before clearing the cache.
  // Some handlers (e.g. LogHandler) hold resources like WebSocket client
  // lists or callbacks that must be released via cleanup(). Simply
  // clearing the list would drop unique_ptrs without calling their
  // cleanup hooks which can leak memory/resources on constrained devices.
  LOG_DEBUG(F("WebManager"), String(F("Bereinige Handler-Cache (")) +
                                 String(m_handlerCache.size()) + F(" Einträge)"));

  for (auto& entry : m_handlerCache) {
    if (entry.handler) {
      LOG_DEBUG(F("WebManager"), String(F("Cleanup: ")) + entry.handlerType);
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
      LOG_DEBUG(F("WebManager"), String(F("Cleanup cached: ")) + entry.handlerType);
      entry.handler->cleanup();
    }
  }
  m_handlerCache.clear();

  logger.endMemoryTracking(F("handlers_cleanup"));
}

void WebManager::cacheHandler(std::unique_ptr<BaseHandler> handler, const String& handlerType) {
  if (!handler) {
    LOG_WARN(F("WebManager"), String(F("Versuch, nullptr-Handler zu cachen: ")) + handlerType);
    return;
  }

  // Check if handler already exists in cache
  for (auto& entry : m_handlerCache) {
    if (entry.handlerType == handlerType) {
      LOG_DEBUG(F("WebManager"), String(F("Handler bereits im Cache: ")) + handlerType);
      entry.lastAccess = millis(); // Update access time
      return;
    }
  }

  // Remove oldest handler if at capacity
  if (m_handlerCache.size() >= MAX_ACTIVE_HANDLERS) {
    evictOldestHandler();
  }

  LOG_INFO(F("WebManager"), String(F("Cache-Handler (")) + String(m_handlerCache.size() + 1) +
                                String(F("/")) + String(MAX_ACTIVE_HANDLERS) + String(F("): ")) +
                                handlerType);

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
  LOG_INFO(F("WebManager"), String(F("LRU-Eviction: ")) + oldest->handlerType +
                                String(F(" (inaktiv seit ")) + String(age) + F("s)"));

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
