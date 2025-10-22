/**
 * @file web_manager.cpp
 * @brief Core WebManager implementation - singleton and lifecycle management
 */

#include "web/core/web_manager.h"

#include "configs/config.h"
#include "logger/logger.h"
#if USE_WEBSOCKET
#include "web/services/websocket.h"
#endif

// Memory thresholds
constexpr uint32_t LOW_MEMORY_THRESHOLD = 4096; // 4KB
#if USE_WEBSOCKET
constexpr uint32_t WEBSOCKET_MEMORY_THRESHOLD = 8192; // 8KB
#endif
constexpr uint32_t HANDLER_MEMORY_THRESHOLD = 6144; // 6KB

// Static members
char WebManager::s_responseBuffer[WebManager::BUFFER_SIZE];

// Global instance
WebManager& webManager = WebManager::getInstance();

WebManager& WebManager::getInstance() {
  static WebManager instance;
  return instance;
}

WebManager::WebManager()
    : _server(nullptr),
      _router(nullptr),
      _auth(nullptr),
      _cssService(nullptr),
      _sensorManager(nullptr),
      _initialized(false),
      _port(80) {}

WebManager::~WebManager() { stop(); }

void WebManager::handleClient() {
  if (!_initialized || !_server)
    return;

  // In minimal mode, only handle basic web requests
  if (ConfigMgr.getDoFirmwareUpgrade()) {
    _server->handleClient();
    return;
  }

  handleClientInternal();
}

void WebManager::handleClientInternal() {
  static unsigned long lastMemoryCheck = 0;
  static size_t lastHandlerCount = 0;
  const unsigned long currentTime = millis();

  // Initialize remaining handlers if not done yet
  if (!m_handlersInitialized && currentTime > 10000) { // Wait 10s after boot
    if (ESP.getFreeHeap() > 8192) {                    // Only if enough memory
      initializeRemainingHandlers();
    }
  }

  // Memory monitoring
  if (currentTime - lastMemoryCheck > 10000) {
    const uint32_t freeHeap = ESP.getFreeHeap();
    const size_t currentHandlerCount = m_handlerCache.size();

    // Only log if handler count has changed
    if (currentHandlerCount != lastHandlerCount) {
      logger.debug(F("WebManager"), F("Active handlers: ") + String(currentHandlerCount) + F("/") +
                                        String(MAX_ACTIVE_HANDLERS));
      lastHandlerCount = currentHandlerCount;
    }

    if (freeHeap < 4096) {
      logger.warning(F("WebManager"), "Low memory in web handler: " + String(freeHeap));
      cleanupNonEssentialHandlers();
      delay(100);
      return;
    }
    lastMemoryCheck = currentTime;
  }

  // Handle web server and WebSocket
  if (_initialized && _server) {
    _server->handleClient();

#if USE_WEBSOCKET
    auto& ws = WebSocketService::getInstance();
    if (ws.isInitialized()) {
      ws.loop();
    }
#endif
  }
}

void WebManager::stop() {
  logger.beginMemoryTracking(F("web_manager_stop"));

#if USE_WEBSOCKET
  // Stop WebSocket first
  WebSocketService::getInstance().stop();
#endif

  if (_server) {
    _server->close();
  }

  // Clean up in reverse order
  _otaHandler.reset();
  _router.reset();
  _auth.reset();
  _server.reset();

  _initialized = false;

  // Give system time to clean up
  delay(100);
  yield();

  logger.debug(F("WebManager"), F("WebManager stopped and cleaned up"));
  logger.endMemoryTracking(F("web_manager_stop"));
}

void WebManager::cleanup() {
  logger.beginMemoryTracking(F("web_manager_cleanup"));

#if USE_WEBSOCKET
  // Stop WebSocket service first
  WebSocketService::getInstance().stop();
#endif

  // Clean up LogHandler before other handlers
  if (_logHandler) {
    _logHandler->cleanup();
    _logHandler.reset();
  }

  // Clean up WiFi setup handler
  if (_wifiSetupHandler) {
    _wifiSetupHandler->cleanup();
    _wifiSetupHandler.reset();
  }

  // Clean up other handlers
  if (_startHandler) {
    _startHandler->cleanup();
    _startHandler.reset();
  }

  if (_adminHandler) {
    _adminHandler->cleanup();
    _adminHandler.reset();
  }

  if (_sensorHandler) {
    _sensorHandler->cleanup();
    _sensorHandler.reset();
  }

  if (_adminSensorHandler) {
    _adminSensorHandler->cleanup();
    _adminSensorHandler.reset();
  }

#if USE_DISPLAY
  if (_displayHandler) {
    _displayHandler->cleanup();
    _displayHandler.reset();
  }
#endif

  // Clean up services
  if (_server) {
    _server->close();
    _server.reset();
  }

  if (_router) {
    _router.reset();
  }

  if (_auth) {
    _auth.reset();
  }

  if (_cssService) {
    _cssService.reset();
  }

  if (_otaHandler) {
    _otaHandler->cleanup();
    _otaHandler.reset();
  }

  _initialized = false;
  m_handlersInitialized = false;

  logger.endMemoryTracking(F("web_manager_cleanup"));
}

ResourceResult WebManager::setFirmwareUpgradeFlag(bool enabled) {
  // Implementation would depend on your config system
  // This is a placeholder
  logger.info(F("WebManager"), "Setting firmware upgrade flag to: " + String(enabled));
  return ResourceResult::success();
}
