/**
 * @file websocket.h
 * @brief WebSocket service that can be initialized on demand with memory
 * optimizations
 */

#pragma once

#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

#include <functional>

#include "logger/logger.h"            // For logger
#include "managers/manager_config.h"  // For ConfigMgr

class WebSocketService {
 public:
  static constexpr size_t MAX_CLIENTS = 1;
  static constexpr size_t RING_BUFFER_SIZE = 1024;
  static constexpr size_t MAX_MESSAGE_SIZE = 512;

  using WebSocketEventHandler = std::function<void(
      uint8_t num, WStype_t type, uint8_t* payload, size_t length)>;

  static WebSocketService& getInstance();
  bool init(uint16_t port, WebSocketEventHandler handler);
  void stop();
  void loop();
  bool sendTXT(uint8_t num, const String& text);
  bool sendBIN(uint8_t num, const uint8_t* data, size_t len);
  bool clientIsConnected(uint8_t num) const;
  IPAddress remoteIP(uint8_t num) const;
  bool isInitialized() const { return _wsServer != nullptr; }

  /**
   * @brief Updates the event handler for the WebSocket server
   * @param handler New event handler function
   */
  void setEventHandler(WebSocketEventHandler handler) {
    _eventHandler = std::move(handler);
    if (_wsServer) {
      _wsServer->onEvent(
          [this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
            handleEvent(num, type, payload, length);
          });
    }
  }

 private:
  WebSocketService() = default;
  ~WebSocketService();

  // Prevent copying
  WebSocketService(const WebSocketService&) = delete;
  WebSocketService& operator=(const WebSocketService&) = delete;

  struct RingBuffer {
    char buffer[RING_BUFFER_SIZE];
    size_t readPos = 0;
    size_t writePos = 0;

    bool write(const char* data, size_t len);
    size_t read(char* data, size_t maxLen);
  };

  std::unique_ptr<WebSocketsServer> _wsServer;
  WebSocketEventHandler _eventHandler;
  RingBuffer m_ringBuffer;
  uint8_t m_connectedClients = 0;
  static char s_sendBuffer[MAX_MESSAGE_SIZE];

  void handleEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
  bool isClientConnected(uint8_t num) const;
  void setClientConnected(uint8_t num, bool connected);
  uint8_t countConnectedClients() const;
};
// Define static buffer
