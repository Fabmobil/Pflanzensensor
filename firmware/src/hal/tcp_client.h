/**
 * @file tcp_client.h
 * @brief TCP-Verbindung für den Mailversand (HTTP von Hand, gestreamt)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/json_writer.h"

namespace ps {
namespace hal {

class TcpClient : public Sink {
public:
  /// Verbindungsaufbau; blockiert höchstens timeoutMs (dokumentierte Ausnahme)
  virtual bool connect(const char* host, uint16_t port, uint32_t timeoutMs) = 0;
  virtual size_t writeBytes(const uint8_t* data, size_t len) = 0;
  virtual int available() = 0;
  virtual int read(uint8_t* buf, size_t len) = 0;
  virtual bool connected() = 0;
  virtual void stop() = 0;
  void write(const char* d, size_t n) override {
    if (writeBytes(reinterpret_cast<const uint8_t*>(d), n) != n)
      failed_ = true;
  }
  using Sink::write;
  bool failed() const { return failed_; }
  void resetFailed() { failed_ = false; }

protected:
  ~TcpClient() = default;
  bool failed_ = false;
};

} // namespace hal
} // namespace ps
