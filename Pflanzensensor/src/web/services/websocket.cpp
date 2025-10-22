#include "web/services/websocket.h"

#include "logger/logger.h"

// s_sendBuffer size is driven by MAX_MESSAGE_SIZE in the header
char WebSocketService::s_sendBuffer[WebSocketService::MAX_MESSAGE_SIZE];

WebSocketService& WebSocketService::getInstance() {
  static WebSocketService instance;
  return instance;
}

bool WebSocketService::init(uint16_t port, WebSocketEventHandler handler) {
  if (_wsServer) {
    logger.debug(F("Websocket"), F("WebSocket-Server bereits initialisiert"));
    return true;
  }

  try {
    _eventHandler = std::move(handler);
    _wsServer = std::make_unique<WebSocketsServer>(port);

    if (!_wsServer) {
      logger.error(F("Websocket"), F("WebSocket-Server konnte nicht erstellt werden"));
      return false;
    }

    // Enable heartbeat with 15s interval, timeout after 3s, and 2 missed pings
    _wsServer->enableHeartbeat(15000, 3000, 2);

    _wsServer->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
      handleEvent(num, type, payload, length);
    });

    _wsServer->begin();
    logger.info(F("Websocket"), F("WebSocket-Server erfolgreich gestartet"));
    return true;

  } catch (const std::exception& e) {
    logger.error(F("Websocket"),
                 String(F("WebSocket-Initialisierung fehlgeschlagen: ")) + String(e.what()));
    stop();
    return false;
  }
}

void WebSocketService::stop() {
  if (_wsServer) {
    // Notify all clients before closing
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (isClientConnected(i)) {
        _wsServer->sendTXT(i, "{\"type\":\"shutdown\"}");
      }
    }
    _wsServer->close();
    _wsServer.reset();
    m_connectedClients = 0;
    logger.info(F("Websocket"), F("WebSocket-Server gestoppt"));
  }
}

WebSocketService::~WebSocketService() { stop(); }

bool WebSocketService::RingBuffer::write(const char* data, size_t len) {
  if (len > RING_BUFFER_SIZE - ((writePos - readPos) % RING_BUFFER_SIZE)) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    buffer[writePos % RING_BUFFER_SIZE] = data[i];
    writePos++;
  }
  return true;
}

size_t WebSocketService::RingBuffer::read(char* data, size_t maxLen) {
  size_t available = (writePos - readPos) % RING_BUFFER_SIZE;
  size_t len = min(available, maxLen);
  for (size_t i = 0; i < len; i++) {
    data[i] = buffer[readPos % RING_BUFFER_SIZE];
    readPos++;
  }
  return len;
}

void WebSocketService::loop() {
  if (_wsServer) {
    _wsServer->loop();
  }
}

bool WebSocketService::sendTXT(uint8_t num, const String& text) {
  if (!_wsServer || !isClientConnected(num)) {
    return false;
  }

  size_t len = text.length();
  if (len >= MAX_MESSAGE_SIZE) {
    logger.warning(F("Websocket"),
                   String(F("Nachricht zu lang: ")) + String(len) + String(F(" Bytes")));
    return false;
  }

  memcpy(s_sendBuffer, text.c_str(), len);
  s_sendBuffer[len] = '\0';

  return _wsServer->sendTXT(num, s_sendBuffer);
}

bool WebSocketService::sendBIN(uint8_t num, const uint8_t* data, size_t len) {
  if (!_wsServer || !isClientConnected(num)) {
    return false;
  }

  if (len >= MAX_MESSAGE_SIZE) {
    logger.warning(F("Websocket"),
                   String(F("Binärnachricht zu lang: ")) + String(len) + String(F(" Bytes")));
    return false;
  }

  memcpy(s_sendBuffer, data, len);
  return _wsServer->sendBIN(num, (uint8_t*)s_sendBuffer, len);
}

bool WebSocketService::isClientConnected(uint8_t num) const {
  if (num >= 3) // our bitmask only supports up to 3 clients
    return false;
  return (m_connectedClients & (1UL << num)) != 0;
}

void WebSocketService::setClientConnected(uint8_t num, bool connected) {
  if (num >= 3) // our bitmask only supports up to 3 clients
    return;
  if (connected) {
    m_connectedClients |= (1UL << num);
  } else {
    m_connectedClients &= ~(1UL << num);
  }
}

uint8_t WebSocketService::countConnectedClients() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_CLIENTS && i < 32; i++) {
    if (isClientConnected(i))
      count++;
  }
  return count;
}

bool WebSocketService::clientIsConnected(uint8_t num) const { return isClientConnected(num); }

IPAddress WebSocketService::remoteIP(uint8_t num) const {
  if (!_wsServer) {
    return IPAddress(0, 0, 0, 0);
  }
  return _wsServer->remoteIP(num);
}

void WebSocketService::handleEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
  case WStype_CONNECTED: {
    // Validate client id is within supported range
    if (num >= MAX_CLIENTS || countConnectedClients() >= MAX_CLIENTS) {
      logger.warning(F("Websocket"),
                     F("Maximale Anzahl WebSocket-Clients erreicht, Verbindung abgelehnt"));
      _wsServer->disconnect(num);
      return;
    }

    IPAddress ip = remoteIP(num);
    logger.info(F("Websocket"), String(F("WebSocket-Client ")) + String(num) +
                                    String(F(" verbunden von ")) + ip.toString() + String(F(" (")) +
                                    String(countConnectedClients() + 1) + String(F("/")) +
                                    String(MAX_CLIENTS) + String(F(" aktiv)")));

    setClientConnected(num, true);
    if (_eventHandler) {
      _eventHandler(num, type, payload, length);
    }
    break;
  }

  case WStype_DISCONNECTED: {
    if (num < MAX_CLIENTS && isClientConnected(num)) {
      logger.info(F("Websocket"),
                  String(F("WebSocket-Client ")) + String(num) + String(F(" getrennt")));
      setClientConnected(num, false);
      if (_eventHandler) {
        _eventHandler(num, type, payload, length);
      }
    }
    break;
  }

  default: {
    if (isClientConnected(num) && _eventHandler) {
      _eventHandler(num, type, payload, length);
    }
    break;
  }
  }
}
