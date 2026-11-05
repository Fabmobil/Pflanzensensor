/**
 * @file tcp_client.cpp
 * @details Der WiFiClient lebt nur während eines Versands (statischer Speicher,
 *          die Puffer allokiert lwIP).
 */
#if defined(ARDUINO)

#include "platform/arduino/tcp_client.h"

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace ps {
namespace platform {

namespace {
WiFiClient g_client;
}

bool TcpClient::connect(const char* host, uint16_t port, uint32_t timeoutMs) {
  g_client.setTimeout(timeoutMs);
#if defined(ARDUINO_ARCH_ESP32)
  return g_client.connect(host, port, static_cast<int32_t>(timeoutMs)) == 1;
#else
  return g_client.connect(host, port) == 1;
#endif
}

size_t TcpClient::writeBytes(const uint8_t* data, size_t len) { return g_client.write(data, len); }
int TcpClient::available() { return g_client.available(); }
int TcpClient::read(uint8_t* buf, size_t len) { return g_client.read(buf, len); }
bool TcpClient::connected() { return g_client.connected(); }
void TcpClient::stop() { g_client.stop(); }

} // namespace platform
} // namespace ps

#endif
