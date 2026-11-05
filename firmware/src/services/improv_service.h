/**
 * @file improv_service.h
 * @brief WLAN-Einrichtung über USB nach dem Flashen (Improv Serial)
 * @details Der Webflasher fragt nach dem Flashen Zustand und Geräteinfo ab
 *          und kann - wenn man es dort auswählt - ein WLAN schicken. Das landet
 *          in Slot 1; danach wird neu verbunden und die Adresse des Geräts
 *          zurückgemeldet. Ohne Anfrage über USB passiert nichts, die
 *          bestehenden Einstellungen bleiben.
 */
#pragma once

#include "core/net/improv.h"
#include "core/types.h"

namespace ps {

class ConfigService;
class NetService;

class ImprovService {
public:
  static constexpr int32_t kConnectTimeoutMs = 30000;

  ImprovService(ConfigService& cfg, NetService& net) : cfg_(cfg), net_(net) {}
  void tick(Instant now);

private:
  void handle(const improv::Command& c, Instant now);
  void send(const uint8_t* p, size_t n);
  void sendState(improv::State s);
  void sendError(improv::Error e);
  void sendUrl(improv::Cmd cmd);

  ConfigService& cfg_;
  NetService& net_;
  improv::Parser parser_;
  bool provisioning_ = false;
  Instant provisionStart_;
};

} // namespace ps
