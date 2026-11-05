/**
 * @file wifi_port.cpp
 */
#if defined(ARDUINO)

#include "platform/arduino/wifi_port.h"

#include <DNSServer.h>

#include "board.h"

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#include <WiFi.h>
#endif

namespace ps {
namespace platform {

namespace {
DNSServer g_dns;
const IPAddress kApIp(192, 168, 4, 1);
const IPAddress kApMask(255, 255, 255, 0);

void ipToStr(const IPAddress& ip, char* out, size_t cap) {
  snprintf(out, cap, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}
IPAddress fromArr(const uint8_t a[4]) { return IPAddress(a[0], a[1], a[2], a[3]); }
} // namespace

void WifiPort::init() {
  if (init_)
    return;
  init_ = true;
  WiFi.persistent(false); // keine Flash-Schreibzugriffe durch das SDK
  WiFi.setAutoReconnect(false);
}

void WifiPort::setHostname(const char* host) {
  init();
#if defined(ARDUINO_ARCH_ESP8266)
  WiFi.hostname(host);
#else
  WiFi.setHostname(host);
#endif
}

void WifiPort::beginSta(const char* ssid, const char* password) {
  init();
  WiFi.mode(apOn_ ? WIFI_AP_STA : WIFI_STA);
  if (board::hardware().staticIp.enabled) {
    WiFi.config(fromArr(board::hardware().staticIp.ip), fromArr(board::hardware().staticIp.gateway),
                fromArr(board::hardware().staticIp.subnet),
                fromArr(board::hardware().staticIp.dns1), fromArr(board::hardware().staticIp.dns2));
  }
  WiFi.begin(ssid, password && *password ? password : nullptr);
}

hal::StaStatus WifiPort::staStatus() {
  switch (WiFi.status()) {
  case WL_CONNECTED:
    return hal::StaStatus::Connected;
  case WL_CONNECT_FAILED:
  case WL_NO_SSID_AVAIL:
#if defined(ARDUINO_ARCH_ESP8266)
  case WL_WRONG_PASSWORD:
#endif
    return hal::StaStatus::Failed;
  case WL_IDLE_STATUS:
  case WL_DISCONNECTED:
    return hal::StaStatus::Connecting;
  default:
    return hal::StaStatus::Idle;
  }
}

void WifiPort::disconnectSta() { WiFi.disconnect(false); }

bool WifiPort::startAp(const char* ssid, bool keepSta) {
  init();
  WiFi.mode(keepSta ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(kApIp, kApIp, kApMask);
  apOn_ = WiFi.softAP(ssid, nullptr, 1); // offen, Kanal 1 (E9)
  return apOn_;
}

void WifiPort::stopAp() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apOn_ = false;
}

void WifiPort::staIp(char* out, size_t cap) { ipToStr(WiFi.localIP(), out, cap); }
void WifiPort::apIp(char* out, size_t cap) { ipToStr(WiFi.softAPIP(), out, cap); }

void WifiPort::ssid(char* out, size_t cap) {
  const String s = WiFi.SSID();
  strncpy(out, s.c_str(), cap - 1);
  out[cap - 1] = '\0';
}

int8_t WifiPort::rssi() { return static_cast<int8_t>(WiFi.RSSI()); }

void WifiPort::dnsStart() {
  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  dnsOn_ = g_dns.start(53, "*", kApIp);
}
void WifiPort::dnsLoop() {
  if (dnsOn_)
    g_dns.processNextRequest();
}
void WifiPort::dnsStop() {
  if (dnsOn_)
    g_dns.stop();
  dnsOn_ = false;
}

bool WifiPort::mdnsBegin(const char* host) {
  mdnsOn_ = MDNS.begin(host);
  if (mdnsOn_)
    MDNS.addService("http", "tcp", 80);
  return mdnsOn_;
}
void WifiPort::mdnsLoop() {
#if defined(ARDUINO_ARCH_ESP8266)
  if (mdnsOn_)
    MDNS.update();
#endif
}
void WifiPort::mdnsEnd() {
  if (mdnsOn_)
    MDNS.end();
  mdnsOn_ = false;
}

} // namespace platform
} // namespace ps

#endif
