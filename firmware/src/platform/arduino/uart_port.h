/**
 * @file uart_port.h
 * @brief Serielle Schnittstelle für Sensoren
 * @details ESP8266: SoftwareSerial (EspSoftwareSerial des Cores);
 *          ESP32: HardwareSerial 1 bzw. 2 auf beliebigen Pins.
 */
#pragma once

#if defined(ARDUINO)

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <SoftwareSerial.h>
#pragma GCC diagnostic pop
#endif

namespace ps {
namespace platform {

class UartPort {
public:
  explicit UartPort(uint8_t unit) : unit_(unit) {}
  bool begin(uint32_t baud, int8_t rx, int8_t tx) {
#if defined(ARDUINO_ARCH_ESP8266)
    sw_.begin(baud, SWSERIAL_8N1, rx, tx, false);
    return sw_.isListening() || true;
#else
    hw_ = unit_ == 1 ? &Serial1 : &Serial2;
    hw_->begin(baud, SERIAL_8N1, rx, tx);
    return true;
#endif
  }
  int available() { return port().available(); }
  int read() { return port().read(); }
  size_t write(const uint8_t* d, size_t n) { return port().write(d, n); }
  void flushInput() {
    while (port().available() > 0)
      port().read();
  }

private:
  Stream& port() {
#if defined(ARDUINO_ARCH_ESP8266)
    return sw_;
#else
    return *hw_;
#endif
  }
  uint8_t unit_;
#if defined(ARDUINO_ARCH_ESP8266)
  SoftwareSerial sw_;
#else
  HardwareSerial* hw_ = nullptr;
#endif
};

} // namespace platform
} // namespace ps

#endif
