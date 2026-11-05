/**
 * @file improv_service.cpp
 */
#include "services/improv_service.h"

#include <stdio.h>

#include "board.h"
#include "core/version.h"
#include "hal/system.h"
#include "services/config_service.h"
#include "services/log.h"
#include "services/net_service.h"

namespace ps {

void ImprovService::send(const uint8_t* p, size_t n) {
  if (n)
    hal::consoleWrite(reinterpret_cast<const char*>(p), n);
}

void ImprovService::sendState(improv::State s) {
  uint8_t p[16];
  send(p, improv::encodeState(p, sizeof(p), s));
}

void ImprovService::sendError(improv::Error e) {
  uint8_t p[16];
  send(p, improv::encodeError(p, sizeof(p), e));
}

void ImprovService::sendUrl(improv::Cmd cmd) {
  char ip[16], url[24];
  net_.ip(ip, sizeof(ip));
  snprintf(url, sizeof(url), "http://%s", ip);
  const char* s[] = {url};
  uint8_t p[64];
  send(p, improv::encodeResult(p, sizeof(p), cmd, s, 1));
}

void ImprovService::handle(const improv::Command& c, Instant now) {
  switch (c.cmd) {
  case improv::Cmd::GetState:
    if (net_.connected()) {
      sendState(improv::State::Provisioned);
      sendUrl(improv::Cmd::GetState);
    } else {
      sendState(provisioning_ ? improv::State::Provisioning : improv::State::Ready);
    }
    break;
  case improv::Cmd::GetInfo: {
    const char* s[] = {"Pflanzensensor", kVersion, board::kPlatformName,
                       cfg_.device().deviceName.c_str()};
    uint8_t p[improv::kMaxPacket + 64];
    send(p, improv::encodeResult(p, sizeof(p), improv::Cmd::GetInfo, s, 4));
    break;
  }
  case improv::Cmd::WifiSettings:
    if (cfg_.setWifi(0, c.ssid, c.password, false) != Err::None) {
      sendError(improv::Error::InvalidRpc);
      break;
    }
    LOGI(Mod::Wifi, "WLAN 1 über USB eingerichtet: %s", c.ssid);
    sendError(improv::Error::None);
    sendState(improv::State::Provisioning);
    provisioning_ = true;
    provisionStart_ = now;
    net_.configChanged(now, true);
    break;
  case improv::Cmd::GetNetworks:
  default:
    // Keine Netzliste: der Webflasher bietet dann ein Eingabefeld an
    sendError(improv::Error::UnknownRpc);
    break;
  }
}

void ImprovService::tick(Instant now) {
  uint8_t buf[32];
  size_t n;
  while ((n = hal::consoleRead(buf, sizeof(buf))) > 0) {
    for (size_t i = 0; i < n; i++) {
      improv::Command c;
      switch (parser_.feed(buf[i], c)) {
      case improv::Parser::Result::Command:
        handle(c, now);
        break;
      case improv::Parser::Result::Invalid:
        sendError(improv::Error::InvalidRpc);
        break;
      case improv::Parser::Result::Unknown:
        sendError(improv::Error::UnknownRpc);
        break;
      default:
        break;
      }
    }
  }
  if (!provisioning_)
    return;
  if (net_.connected() && net_.activeSlot() == 0) {
    provisioning_ = false;
    sendState(improv::State::Provisioned);
    sendUrl(improv::Cmd::WifiSettings);
  } else if ((now - provisionStart_).millis() > kConnectTimeoutMs) {
    provisioning_ = false;
    LOGW(Mod::Wifi, "WLAN 1 über USB: keine Verbindung");
    sendError(improv::Error::UnableToConnect);
    sendState(improv::State::Ready);
  }
}

} // namespace ps
