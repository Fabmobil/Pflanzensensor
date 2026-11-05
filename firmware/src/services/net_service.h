/**
 * @file net_service.h
 * @brief WLAN-Zustandsmaschine (refactor.md 2.9, 5.13)
 * @details
 *   Boot → TrySlot(i) [10 s] → … → StaUp | ApMode
 *   StaUp → (Verlust, Prüfung alle 30 s) Reconnect(rotierend, 10 s je Slot) → StaUp
 *   ApMode → alle 5 min: ApStaProbe(15 s) → StaUp
 *  Nichts blockiert. Ein Slot gilt als konfiguriert, sobald die SSID gesetzt
 *  ist; leere Passwörter (offene Netze) sind erlaubt (B-39).
 */
#pragma once

#include "core/util/event_queue.h"
#include "hal/wifi.h"
#include "services/config_service.h"

namespace ps {

class NetService {
public:
  enum class State : uint8_t { Off, TrySlot, StaUp, Reconnect, ApMode, ApProbe };

  static constexpr int32_t kSlotTimeoutMs = 10000;
  static constexpr int32_t kCheckIntervalMs = 30000;
  static constexpr int32_t kApProbeIntervalMs = 5 * 60 * 1000;
  static constexpr int32_t kApProbeMs = 15000;

  using BootLogFn = void (*)(void* ctx, const char* line);

  NetService(hal::Wifi& wifi, ConfigService& cfg, EventQueue<16>& events)
      : wifi_(wifi), cfg_(cfg), events_(events) {}

  void setBootLog(BootLogFn fn, void* ctx) {
    bootLog_ = fn;
    bootCtx_ = ctx;
  }
  void begin(Instant now);
  /// Alle 100 ms
  void tick(Instant now);
  /// Gerätename oder SSIDs geändert
  void configChanged(Instant now, bool reconnect);

  State state() const { return state_; }
  bool connected() const { return state_ == State::StaUp; }
  bool apActive() const { return state_ == State::ApMode || state_ == State::ApProbe; }
  /// Aktiver Slot (0..2) oder -1
  int8_t activeSlot() const { return state_ == State::StaUp ? slot_ : -1; }
  const char* hostname() const { return host_; }
  void ip(char* out, size_t cap);
  void ssid(char* out, size_t cap);
  int8_t rssi() { return connected() ? wifi_.rssi() : 0; }
  void apSsid(char* out, size_t cap) const;

private:
  int8_t nextSlot(int8_t from) const;
  void trySlot(int8_t s, Instant now, bool keepAp);
  void enterStaUp(Instant now);
  void enterAp(Instant now);
  void log(const char* fmt, ...);
  void updateHostname();

  hal::Wifi& wifi_;
  ConfigService& cfg_;
  EventQueue<16>& events_;
  BootLogFn bootLog_ = nullptr;
  void* bootCtx_ = nullptr;
  State state_ = State::Off;
  int8_t slot_ = -1;
  int8_t triedInRound_ = 0;
  Instant since_;
  Instant lastCheck_;
  char host_[64] = "";
  char lastIp_[16] = "";
  bool mdnsUp_ = false;
};

} // namespace ps
