/**
 * @file wifi.h
 * @brief WLAN, Access Point, Captive-DNS und mDNS
 * @details Die Zustandsmaschine (NetService) ist plattformfrei und wird
 *          nativ gegen eine Fake-Implementierung getestet.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {
namespace hal {

enum class StaStatus : uint8_t { Idle, Connecting, Connected, Failed };

class Wifi {
public:
  virtual void setHostname(const char* host) = 0;
  /// STA-Verbindungsversuch starten (nicht blockierend)
  virtual void beginSta(const char* ssid, const char* password) = 0;
  virtual StaStatus staStatus() = 0;
  virtual void disconnectSta() = 0;
  /// AP öffnen (offen, Kanal 1, 192.168.4.1/24); keepSta: AP+STA
  virtual bool startAp(const char* ssid, bool keepSta) = 0;
  virtual void stopAp() = 0;
  virtual void staIp(char* out, size_t cap) = 0;
  virtual void apIp(char* out, size_t cap) = 0;
  virtual void ssid(char* out, size_t cap) = 0;
  virtual int8_t rssi() = 0;
  virtual void dnsStart() = 0;
  virtual void dnsLoop() = 0;
  virtual void dnsStop() = 0;
  virtual bool mdnsBegin(const char* host) = 0;
  virtual void mdnsLoop() = 0;
  virtual void mdnsEnd() = 0;

protected:
  ~Wifi() = default;
};

} // namespace hal
} // namespace ps
