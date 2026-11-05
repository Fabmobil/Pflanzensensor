/**
 * @file led_service.h
 * @brief LED-Ampel (refactor.md 2.8)
 * @details Modus 0 aus (auch beim Wechsel, B-37), 1 folgt dem Display,
 *          2 fester Kanal. "Nur Rot": Gelb und Grün schalten alles aus.
 *          Unbekannt, Fehler und Warmup: aus.
 */
#pragma once

#include "core/sensors/status.h"
#include "services/config_service.h"
#include "services/sensor_service.h"

namespace ps {

class LedPort {
public:
  virtual void set(bool green, bool yellow, bool red) = 0;

protected:
  ~LedPort() = default;
};

class LedService {
public:
  /// Welchen Kanal zeigt das Display gerade? (-1: keinen)
  using DisplaySlotFn = int (*)(void* ctx);

  LedService(LedPort& port, ConfigService& cfg, SensorService& sensors)
      : port_(port), cfg_(cfg), sensors_(sensors) {}
  void setDisplaySource(DisplaySlotFn fn, void* ctx) {
    displayFn_ = fn;
    displayCtx_ = ctx;
  }
  /// Sekündlich und nach Konfigurationsänderung
  void tick(Instant now);
  /// Reine Logik (nativ getestet)
  static void lights(ChannelStatus s, bool onlyRed, bool& g, bool& y, bool& r);

private:
  uint8_t lastBits_ = 0xFF; ///< zuletzt gemeldete LEDs (fürs Log)
  LedPort& port_;
  ConfigService& cfg_;
  SensorService& sensors_;
  DisplaySlotFn displayFn_ = nullptr;
  void* displayCtx_ = nullptr;
};

} // namespace ps
