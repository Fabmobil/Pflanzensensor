/**
 * @file net_service.cpp
 */
#include "services/net_service.h"

#include <stdarg.h>
#include <stdio.h>

#include "core/util/mdns_name.h"
#include "services/log.h"

namespace ps {

void NetService::log(const char* fmt, ...) {
  char line[64];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  LOGI(Mod::Wifi, "%s", line);
  if (bootLog_)
    bootLog_(bootCtx_, line);
}

void NetService::updateHostname() {
  mdns::hostnameFrom(cfg_.device().deviceName.c_str(), host_, sizeof(host_));
  wifi_.setHostname(host_);
}

void NetService::apSsid(char* out, size_t cap) const {
  const char* name = cfg_.device().deviceName.c_str();
  const size_t n = utf8Truncate(name, strlen(name), cap - 1 < 32 ? cap - 1 : 32);
  memcpy(out, name, n);
  out[n] = '\0';
}

int8_t NetService::nextSlot(int8_t from) const {
  for (int8_t i = 1; i <= 3; i++) {
    const int8_t s = static_cast<int8_t>((from + i + 3) % 3);
    if (!cfg_.device().wifiSsid[s].empty())
      return s;
  }
  return -1;
}

void NetService::trySlot(int8_t s, Instant now, bool keepAp) {
  slot_ = s;
  since_ = now;
  if (!keepAp)
    wifi_.disconnectSta();
  wifi_.beginSta(cfg_.device().wifiSsid[s].c_str(), cfg_.secrets().wifiPassword[s].c_str());
  log("WLAN %d: %s ...", s + 1, cfg_.device().wifiSsid[s].c_str());
}

void NetService::begin(Instant now) {
  updateHostname();
  triedInRound_ = 0;
  const int8_t first = nextSlot(-1);
  if (first < 0) {
    log("Kein WLAN eingerichtet");
    enterAp(now);
    return;
  }
  state_ = State::TrySlot;
  triedInRound_ = 1;
  trySlot(first, now, false);
}

void NetService::enterStaUp(Instant now) {
  state_ = State::StaUp;
  lastCheck_ = now;
  char ipStr[16];
  wifi_.staIp(ipStr, sizeof(ipStr));
  log("Verbunden: %s", ipStr);
  strncpy(lastIp_, ipStr, sizeof(lastIp_) - 1);
  mdnsUp_ = wifi_.mdnsBegin(host_);
  if (mdnsUp_)
    LOGI(Mod::Wifi, "mDNS: %s.local", host_);
  Event e;
  e.type = EventType::WifiUp;
  events_.push(e);
}

void NetService::enterAp(Instant now) {
  char ssid[33];
  apSsid(ssid, sizeof(ssid));
  wifi_.disconnectSta();
  if (!wifi_.startAp(ssid, false))
    LOGE(Mod::Wifi, "AP konnte nicht gestartet werden");
  wifi_.dnsStart();
  state_ = State::ApMode;
  since_ = now;
  char ipStr[16];
  wifi_.apIp(ipStr, sizeof(ipStr));
  log("AP: %s", ssid);
  log("1. Verbinde mit AP");
  log("2. Browser: %s", ipStr);
  Event e;
  e.type = EventType::ApStarted;
  events_.push(e);
}

void NetService::tick(Instant now) {
  switch (state_) {
  case State::Off:
    return;
  case State::TrySlot: {
    const hal::StaStatus st = wifi_.staStatus();
    if (st == hal::StaStatus::Connected) {
      enterStaUp(now);
      return;
    }
    if (st == hal::StaStatus::Failed || (now - since_).millis() >= kSlotTimeoutMs) {
      const int8_t next = nextSlot(slot_);
      if (next < 0 || triedInRound_ >= 3 || next <= slot_) {
        log("Kein WLAN erreichbar");
        enterAp(now);
        return;
      }
      triedInRound_++;
      trySlot(next, now, false);
    }
    return;
  }
  case State::StaUp: {
    if (mdnsUp_)
      wifi_.mdnsLoop();
    if ((now - lastCheck_).millis() < kCheckIntervalMs)
      return;
    lastCheck_ = now;
    if (wifi_.staStatus() == hal::StaStatus::Connected) {
      char ipStr[16];
      wifi_.staIp(ipStr, sizeof(ipStr));
      if (strcmp(ipStr, lastIp_) != 0) { // IP gewechselt: mDNS neu ansagen
        strncpy(lastIp_, ipStr, sizeof(lastIp_) - 1);
        wifi_.mdnsEnd();
        mdnsUp_ = wifi_.mdnsBegin(host_);
      }
      return;
    }
    LOGW(Mod::Wifi, "Verbindung verloren - versuche neu");
    Event e;
    e.type = EventType::WifiDown;
    events_.push(e);
    wifi_.mdnsEnd();
    mdnsUp_ = false;
    state_ = State::Reconnect;
    const int8_t s = nextSlot(slot_ - 1); // zuerst derselbe Slot
    if (s >= 0)
      trySlot(s, now, false);
    return;
  }
  case State::Reconnect: {
    const hal::StaStatus st = wifi_.staStatus();
    if (st == hal::StaStatus::Connected) {
      enterStaUp(now);
      return;
    }
    if (st == hal::StaStatus::Failed || (now - since_).millis() >= kSlotTimeoutMs) {
      const int8_t s = nextSlot(slot_);
      if (s >= 0)
        trySlot(s, now, false);
      else
        since_ = now;
    }
    return;
  }
  case State::ApMode:
    wifi_.dnsLoop();
    if ((now - since_).millis() >= kApProbeIntervalMs) {
      const int8_t s = nextSlot(slot_);
      if (s < 0) {
        since_ = now;
        return;
      }
      state_ = State::ApProbe;
      trySlot(s, now, true);
    }
    return;
  case State::ApProbe:
    wifi_.dnsLoop();
    if (wifi_.staStatus() == hal::StaStatus::Connected) {
      wifi_.dnsStop();
      wifi_.stopAp();
      LOGI(Mod::Wifi, "WLAN gefunden - AP wird beendet");
      enterStaUp(now);
      return;
    }
    if ((now - since_).millis() >= kApProbeMs) {
      wifi_.disconnectSta();
      state_ = State::ApMode;
      since_ = now;
    }
    return;
  }
}

void NetService::configChanged(Instant now, bool reconnect) {
  char old[64];
  strncpy(old, host_, sizeof(old));
  updateHostname();
  if (strcmp(old, host_) != 0 && state_ == State::StaUp) {
    wifi_.mdnsEnd();
    mdnsUp_ = wifi_.mdnsBegin(host_);
    LOGI(Mod::Wifi, "mDNS neu: %s.local", host_);
  }
  if (!reconnect) {
    // Im AP-Modus mit neu eingerichtetem Slot: gleich probieren
    if (state_ == State::ApMode && nextSlot(-1) >= 0)
      since_ = now - Duration::ms(kApProbeIntervalMs);
    return;
  }
  if (state_ == State::ApMode || state_ == State::ApProbe) {
    since_ = now - Duration::ms(kApProbeIntervalMs);
    state_ = State::ApMode;
    return;
  }
  wifi_.mdnsEnd();
  mdnsUp_ = false;
  begin(now);
}

void NetService::ip(char* out, size_t cap) {
  if (apActive() && state_ != State::StaUp)
    wifi_.apIp(out, cap);
  else
    wifi_.staIp(out, cap);
}

void NetService::ssid(char* out, size_t cap) {
  if (state_ == State::StaUp)
    wifi_.ssid(out, cap);
  else if (apActive())
    apSsid(out, cap);
  else if (cap)
    out[0] = '\0';
}

} // namespace ps
