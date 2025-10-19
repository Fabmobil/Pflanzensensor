/**
 * @file log_handler.cpp
 * @brief Implementation of log viewing and WebSocket streaming
 */

#include "log_handler.h"

#include <ArduinoJson.h>

#include "../../managers/manager_resource.h"
#include "managers/manager_config.h"

LogHandler* LogHandler::s_instance = nullptr;
bool LogHandler::s_initialized = false;

RouterResult LogHandler::onRegisterRoutes(WebRouter& router) {
  if (!isInitialized()) {
    logger.error(
        F("LogHandler"),
        F("Kann Routen nicht registrieren - LogHandler nicht initialisiert"));
    return RouterResult::fail(RouterError::INITIALIZATION_ERROR,
                              F("LogHandler nicht initialisiert"));
  }

  logger.debug(F("LogHandler"), F("Registriere /logs Route"));
  auto result = router.addRoute(HTTP_GET, "/logs", [this]() {
    logger.debug(F("LogHandler"), F("Log route handler called"));
    handleLogs();
  });
  if (!result.isSuccess()) return result;

#if USE_WEBSOCKET
  if (!initWebSocket()) {
    return RouterResult::fail(RouterError::OPERATION_FAILED,
                              F("WebSocket-Server konnte nicht initialisiert werden"));
  }
  logger.info(F("LogHandler"), F("Log-Routen und WebSocket registriert"));
#else
  logger.info(F("LogHandler"), F("Log-Routen registriert"));
#endif
  return RouterResult::success();
}

HandlerResult LogHandler::handleGet(const String& uri,
                                    const std::map<String, String>& query) {
  if (!isInitialized()) {
    logger.error(
        F("LogHandler"),
        F("Kann GET-Anfrage nicht verarbeiten - LogHandler nicht initialisiert"));
    return HandlerResult::fail(HandlerError::INITIALIZATION_ERROR,
                               F("LogHandler nicht initialisiert"));
  }
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             F("Bitte registerRoutes verwenden"));
}

HandlerResult LogHandler::handlePost(const String& uri,
                                     const std::map<String, String>& params) {
  if (!isInitialized()) {
    logger.error(
        F("LogHandler"),
        F("Kann POST-Anfrage nicht verarbeiten - LogHandler nicht initialisiert"));
    return HandlerResult::fail(HandlerError::INITIALIZATION_ERROR,
                               F("LogHandler nicht initialisiert"));
  }
  return HandlerResult::fail(HandlerError::INVALID_REQUEST,
                             F("Bitte registerRoutes verwenden"));
}

void LogHandler::handleLogs() {
  if (!isInitialized()) {
    logger.error(F("LogHandler"),
                 F("Cannot handle logs - LogHandler not properly initialized"));
    _server.send(500, F("text/plain"), F("LogHandler not initialized"));
    return;
  }

  logger.debug(F("LogHandler"), F("Verarbeite Logseiten-Anfrage"));
  _cleaned = false;

  // Check memory before proceeding
  if (ESP.getFreeHeap() < 6000) {  // Higher threshold for log handling
    logger.warning(F("LogHandler"), F("Wenig Speicher, liefere minimale Log-Seite"));
    _server.send(200, F("text/html"),
                 F("<!DOCTYPE html><html><body><h1>Wenig Speicher</h1>"
                   "<p>Bitte versuchen Sie es in wenigen Momenten erneut.</p></body></html>"));
    return;
  }

  std::vector<String> css = {"admin", "logs"};
  std::vector<String> js = {"logs"};

  this->renderAdminPage(
      ConfigMgr.getDeviceName(), "logs",
      [this]() {
#if USE_WEBSOCKET
        // Add WebSocket authentication script first
        sendChunk(F("<script>window.wsAuth = '"));
        sendChunk(ConfigMgr.getAdminPassword());
        sendChunk(F("';</script>"));
#endif

        // Controls row with two cards, each with two rows
        sendChunk(F("<div class='log-controls-row'>"));
        // Card 1: Log-Level
        sendChunk(F("<div class='card log-controls-card'>"));
        sendChunk(F("<div class='log-controls-label'>Log-Level:</div>"));
        sendChunk(F("<div class='button-group'>"));
        sendChunk(
            F("<button onclick='setLogLevel(\"DEBUG\")' class='button "
              "button-debug log-level-btn level-debug'>DEBUG</button>"));
        sendChunk(
            F("<button onclick='setLogLevel(\"INFO\")' class='button "
              "button-info log-level-btn level-info'>INFO</button>"));
        sendChunk(
            F("<button onclick='setLogLevel(\"WARNING\")' class='button "
              "button-warning log-level-btn level-warning'>WARNING</button>"));
        sendChunk(
            F("<button onclick='setLogLevel(\"ERROR\")' class='button "
              "button-error log-level-btn level-error'>ERROR</button>"));
        sendChunk(F("</div>"));
        sendChunk(F("</div>"));
        // Card 2: WebSocket Status and Auto-scroll
        sendChunk(F("<div class='card log-controls-card'>"));
        sendChunk(F(
            "<div class='log-controls-label'>WebSocket Status: <span "
            "id='wsStatusCard' class='ws-status'>Connecting...</span></div>"));
        sendChunk(F("<div class='button-group'>"));
        sendChunk(
            F("<button id='autoScrollBtn' class='button "
              "button-primary'>Auto-scroll: ON</button>"));
        sendChunk(F("</div>"));
        sendChunk(F("</div>"));
        sendChunk(F("</div>"));

#if USE_WEBSOCKET
        // Main log container
        sendChunk(F("<div id='logContainer' class='log-container'>"));
        sendChunk(F("<div class='log-entry system'>"));
        sendChunk(F("Initializing log viewer..."));
        sendChunk(F("</div>"));
        sendChunk(F("</div>"));

        // Add WebSocket port information for the client
        sendChunk(F("<script>window.wsPort = 81;</script>"));
#else
        // Static log container for non-WebSocket mode
        sendChunk(F("<div class='log-container'>"));
        sendChunk(F("<div class='log-entry system'>"));
        sendChunk(
            F("WebSocket functionality is disabled. Logs will not update in "
              "real-time."));
        sendChunk(F("</div>"));
        sendChunk(F("</div>"));
#endif
      },
      css, js);

  logger.debug(F("LogHandler"), F("Log-Seite erfolgreich gesendet"));
}

void LogHandler::cleanupLogs() {
#if USE_WEBSOCKET
  auto& ws = WebSocketService::getInstance();

  // Remove disconnected clients
  for (auto it = _clients.begin(); it != _clients.end();) {
    if (!ws.isInitialized() || !ws.clientIsConnected(*it)) {
      it = _clients.erase(it);
    } else {
      ++it;
    }
  }

  // Send cleanup notification to remaining clients
  StaticJsonDocument<128> doc;
  doc["type"] = "cleanup";
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);

  for (auto client : _clients) {
    if (ws.isInitialized()) {
      ws.sendTXT(client, json);
    }
  }
#endif
  yield();
}

String LogHandler::getLogLevelColor(LogLevel level) const {
  switch (level) {
    case LogLevel::DEBUG:
      return F("#569cd6");  // Blue
    case LogLevel::INFO:
      return F("#6a9955");  // Green
    case LogLevel::WARNING:
      return F("#dcdcaa");  // Yellow
    case LogLevel::ERROR:
      return F("#f44747");  // Red
    default:
      return F("#808080");  // Gray
  }
}

#if USE_WEBSOCKET
bool LogHandler::initWebSocket() {
  if (!isInitialized()) {
    logger.error(
        F("LogHandler"),
        F("Cannot initialize WebSocket - LogHandler not properly initialized"));
    return false;
  }

  auto& ws = WebSocketService::getInstance();
  if (!ws.isInitialized()) {
    logger.error(F("LogHandler"), F("WebSocket server not initialized"));
    return false;
  }

  logger.debug(F("LogHandler"), F("WebSocket server already initialized"));

  // Register logger callback for broadcasting logs
  logger.setCallback([](LogLevel level, const String& message) {
    auto* logHandler = LogHandler::s_instance;
    if (logHandler && logHandler->isInitialized()) {
      logHandler->broadcastLog(level, message);
    }
  });
  return true;
}

void LogHandler::loop() {
#if USE_WEBSOCKET
  if (!isInitialized()) return;

  auto& ws = WebSocketService::getInstance();
  if (ws.isInitialized()) {
    ws.loop();
  }

  // Clean up old logs periodically
  unsigned long now = millis();
  if (now - _lastCleanup >= LOG_CLEANUP_INTERVAL) {
    cleanupLogs();
    _lastCleanup = now;
  }
#endif
}

void LogHandler::broadcastLog(LogLevel level, const String& message) {
#if USE_WEBSOCKET
  static bool inBroadcast = false;
  if (inBroadcast) return;  // Prevent recursion from log callback
  inBroadcast = true;

  // DO NOT log inside this function! Logging here would cause infinite
  // recursion.

  if (!isInitialized()) {
    inBroadcast = false;
    return;
  }

  // Check if we're in a critical operation (like config save)
  if (ESP.getFreeHeap() < 4000 || ResourceMgr.isInCriticalOperation()) {
    inBroadcast = false;
    return;
  }

  // Additional safety check - if the logger callback is disabled, don't
  // broadcast
  if (!logger.isCallbackEnabled()) {
    inBroadcast = false;
    return;
  }

  auto& ws = WebSocketService::getInstance();
  if (!ws.isInitialized() || _clients.empty()) {
    inBroadcast = false;
    return;
  }

  // Create JSON document for the log message
  // Reduced document size to save RAM; payload contains only level/message/timestamp
  StaticJsonDocument<256> doc;
  doc["type"] = "log";
  doc["level"] = Logger::logLevelToString(level);
  doc["message"] = message;
  doc["timestamp"] =
      logger.isNTPInitialized() ? logger.getSynchronizedTime() : millis();

  String json;
  serializeJson(doc, json);

  int failureCount = 0;
  const int MAX_FAILURES = 3;

  for (auto it = _clients.begin(); it != _clients.end();) {
    if (!ws.clientIsConnected(*it)) {
      // Do not log here to avoid recursion
      it = _clients.erase(it);
      if (_clients.empty()) break;
      continue;
    }
    // Do not log here to avoid recursion
    if (!ws.sendTXT(*it, json)) {
      // Do not log here to avoid recursion
      it = _clients.erase(it);
      failureCount++;
      yield();
      if (failureCount >= MAX_FAILURES) {
        // Do not log here to avoid recursion
        _clients.clear();
        break;
      }
      if (_clients.empty()) break;
    } else {
      // Do not log here to avoid recursion
      ++it;
    }
  }
  inBroadcast = false;
#endif
}

void LogHandler::cleanupAllClients() {
#if USE_WEBSOCKET
  if (!isInitialized()) return;

  auto& ws = WebSocketService::getInstance();

  // Clear the client list
  for (auto clientId : _clients) {
    if (ws.isInitialized() && ws.clientIsConnected(clientId)) {
      ws.sendTXT(clientId, F("{\"type\":\"shutdown\"}"));
    }
  }

  _clients.clear();
  _messageQueue.clear();
  _content.clear();
  _cleaned = false;

  logger.debug(F("LogHandler"), F("All WebSocket clients cleaned up"));
#if USE_WEBSOCKET
  // Unregister logger callback to free std::function memory
  logger.setCallback(nullptr);
#endif
#endif
}

void LogHandler::handleWebSocketEvent(uint8_t num, WStype_t type,
                                      uint8_t* payload, size_t length) {
#if USE_WEBSOCKET
  if (!isInitialized()) return;

  // Check memory and critical operations before any WebSocket operations
  if (ESP.getFreeHeap() < 3000 || ResourceMgr.isInCriticalOperation()) {
    // Too low memory or in critical operation, skip processing
    return;
  }

  auto& ws = WebSocketService::getInstance();

  switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = ws.remoteIP(num);
      if (ConfigMgr.isDebugWebSocket()) {
        logger.debug(F("LogHandler"), "WebSocket client " + String(num) +
                                          " connected from " + ip.toString());
      }
      // Only add if not already present
      if (std::find(_clients.begin(), _clients.end(), num) == _clients.end()) {
        _clients.push_back(num);
      }
      // Send welcome message with minimal memory usage
      StaticJsonDocument<128> doc;  // Reduced size
      doc["type"] = "connected";
      doc["status"] = "ok";
      String json;
      serializeJson(doc, json);
      if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
        ws.sendTXT(num, json);
      }
      // Removed log buffer/history send
      break;
    }
    case WStype_DISCONNECTED: {
      if (ConfigMgr.isDebugWebSocket()) {
        logger.debug(F("LogHandler"),
                     "WebSocket client " + String(num) + " disconnected");
      }
      // Remove only the disconnected client
      _clients.remove(num);
      cleanupClientResources(num);
      break;
    }

    case WStype_TEXT: {
      if (length > WebSocketService::MAX_MESSAGE_SIZE) {
        if (ConfigMgr.isDebugWebSocket()) {
          logger.warning(F("LogHandler"), F("Message too large, ignoring"));
        }
        return;
      }

      // Additional memory check before JSON parsing
      if (ESP.getFreeHeap() < 4000) {
        if (ConfigMgr.isDebugWebSocket()) {
          logger.warning(F("LogHandler"), F("Low memory, skipping message"));
        }
        return;
      }

      // Handle text messages with better error handling
  // Smaller JSON buffer for incoming control messages
  StaticJsonDocument<128> doc;
      DeserializationError error = deserializeJson(doc, (char*)payload);

      if (error) {
        if (ConfigMgr.isDebugWebSocket()) {
          logger.error(F("LogHandler"), "Failed to parse WebSocket message: " +
                                            String(error.c_str()));
        }
        return;
      }

      // Extract type and data with null checks
      const char* typeStr = doc["type"];
      const char* dataStr = doc["data"];

      if (!typeStr) {
        if (ConfigMgr.isDebugWebSocket()) {
          logger.warning(F("LogHandler"), F("Message missing type field"));
        }
        return;
      }

      String type = String(typeStr);
      String data = dataStr ? String(dataStr) : "";

      handleClientMessage(num, type, data);
      break;
    }

    case WStype_ERROR: {
      if (ConfigMgr.isDebugWebSocket()) {
        logger.error(F("LogHandler"),
                     "WebSocket error on client " + String(num));
      }
      cleanupClientResources(num);
      _clients.remove(num);
      break;
    }

    case WStype_PING:
      // Respond to ping with pong
      if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
        ws.sendTXT(num, "{\"type\":\"pong\"}");
      }
      break;

    case WStype_PONG:
      // Simply acknowledge PONG without logging
      break;

    default:
      if (ConfigMgr.isDebugWebSocket()) {
        if (type != WStype_PING) {  // Don't log PING events
          logger.debug(F("LogHandler"),
                       "Unhandled WebSocket event type: " + String(type));
        }
      }
      break;
  }
#endif
}

void LogHandler::cleanupClientResources(uint8_t clientNum) {
#if USE_WEBSOCKET
  _messageQueue.erase(std::remove_if(_messageQueue.begin(), _messageQueue.end(),
                                     [clientNum](const QueuedMessage& msg) {
                                       return msg.clientId == clientNum;
                                     }),
                      _messageQueue.end());
#endif
}

void LogHandler::handleClientMessage(uint8_t clientNum, const String& type,
                                     const String& data) {
#if USE_WEBSOCKET
  if (!isInitialized()) return;

  auto& ws = WebSocketService::getInstance();
  if (!ws.isInitialized()) return;

  // Check memory and critical operations before processing any message
  if (ESP.getFreeHeap() < 5000 || ResourceMgr.isInCriticalOperation()) {
    // Send error response if memory is too low or in critical operation
    StaticJsonDocument<64> response;
    response["type"] = "error";
    response["message"] = ResourceMgr.isInCriticalOperation() ? F("System busy")
                                                              : F("Low memory");
    String jsonResponse;
    serializeJson(response, jsonResponse);
    if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
      ws.sendTXT(clientNum, jsonResponse);
    }
    return;
  }

  if (type == "init" && data == "log_client") {
    if (ESP.getFreeHeap() > 8192) {
      // sendLogBuffer(clientNum); // Removed as per instructions

      // Send current log level to client
      StaticJsonDocument<96> response;
      response["type"] = "log_level_changed";
      response["data"] = Logger::logLevelToString(logger.getLogLevel());
      response["saved"] = true;
      String jsonResponse;
      serializeJson(response, jsonResponse);
      if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
        ws.sendTXT(clientNum, jsonResponse);
      }
    }
  } else if (type == "log_level" && !data.isEmpty()) {
    // Check if logger callback is disabled (indicating critical operation)
    if (!logger.isCallbackEnabled()) {
      StaticJsonDocument<64> response;
      response["type"] = "error";
      response["message"] = F("System busy");
      String jsonResponse;
      serializeJson(response, jsonResponse);
      if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
        ws.sendTXT(clientNum, jsonResponse);
      }
      return;
    }

    // Validate log level before changing
    try {
      Logger::stringToLogLevel(data);
    } catch (const std::exception& e) {
      StaticJsonDocument<64> response;
      response["type"] = "error";
      response["message"] = F("Invalid level");
      String jsonResponse;
      serializeJson(response, jsonResponse);
      if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
        ws.sendTXT(clientNum, jsonResponse);
      }
      return;
    }

    // Send immediate confirmation to prevent timeout
    StaticJsonDocument<96> response;
    response["type"] = "log_level_changed";
    response["data"] = data;
    response["saved"] = true;  // Assume success initially
    String jsonResponse;
    serializeJson(response, jsonResponse);

    // Check if we can safely send WebSocket message
    if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
      ws.sendTXT(clientNum, jsonResponse);
    }

    // Change log level and save config with yield to prevent watchdog
    yield();

    // Additional memory check before config save
    if (ESP.getFreeHeap() < 5000) {
      // Send error response if memory is too low
      StaticJsonDocument<96> errorResponse;
      errorResponse["type"] = "log_level_changed";
      errorResponse["data"] = data;
      errorResponse["saved"] = false;
      String errorJsonResponse;
      serializeJson(errorResponse, errorJsonResponse);
      if (ESP.getFreeHeap() > 4000 && logger.isCallbackEnabled()) {
        ws.sendTXT(clientNum, errorJsonResponse);
      }
      return;
    }

    auto saveResult = ConfigMgr.setLogLevel(data);
    yield();

    // Send final confirmation if different from initial
    if (!saveResult.isSuccess() && ESP.getFreeHeap() > 4096 &&
        logger.isCallbackEnabled()) {
      StaticJsonDocument<96> errorResponse;
      errorResponse["type"] = "log_level_changed";
      errorResponse["data"] = data;
      errorResponse["saved"] = false;
      String errorJsonResponse;
      serializeJson(errorResponse, errorJsonResponse);
      ws.sendTXT(clientNum, errorJsonResponse);
    }
  }
#endif
}

// Remove or comment out any function like sendLogBuffer, and any code that
// sends the buffer on client connect. Only keep real-time log broadcasting
// (e.g., broadcastLog).
#endif
