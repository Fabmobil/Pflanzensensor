/**
 * @file wifi_port.h
 * @brief hal::Wifi für ESP8266 und ESP32 (WLAN, AP, Captive-DNS, mDNS)
 */
#pragma once

#if defined(ARDUINO)

#include "hal/wifi.h"

namespace ps {
namespace platform {

class WifiPort : public hal::Wifi {
public:
  void setHostname(const char* host) override;
  void beginSta(const char* ssid, const char* password) override;
  hal::StaStatus staStatus() override;
  void disconnectSta() override;
  bool startAp(const char* ssid, bool keepSta) override;
  void stopAp() override;
  void staIp(char* out, size_t cap) override;
  void apIp(char* out, size_t cap) override;
  void ssid(char* out, size_t cap) override;
  int8_t rssi() override;
  void dnsStart() override;
  void dnsLoop() override;
  void dnsStop() override;
  bool mdnsBegin(const char* host) override;
  void mdnsLoop() override;
  void mdnsEnd() override;

private:
  bool apOn_ = false;
  bool dnsOn_ = false;
  bool mdnsOn_ = false;
  bool init_ = false;
  void init();
};

} // namespace platform
} // namespace ps

#endif
