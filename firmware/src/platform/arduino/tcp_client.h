/**
 * @file tcp_client.h
 * @brief hal::TcpClient auf WiFiClient
 */
#pragma once

#if defined(ARDUINO)

#include "hal/tcp_client.h"

namespace ps {
namespace platform {

class TcpClient : public hal::TcpClient {
public:
  bool connect(const char* host, uint16_t port, uint32_t timeoutMs) override;
  size_t writeBytes(const uint8_t* data, size_t len) override;
  int available() override;
  int read(uint8_t* buf, size_t len) override;
  bool connected() override;
  void stop() override;
};

} // namespace platform
} // namespace ps

#endif
