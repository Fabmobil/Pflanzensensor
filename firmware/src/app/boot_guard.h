/**
 * @file boot_guard.h
 * @brief Boot-Schleifen-Erkennung und begrenzte Sensor-Neustarts (refactor.md 2.2)
 * @details Zähler im RTC-RAM statt in einer Flash-Datei (B-01). Ein Kaltstart
 *          (Power-on) gilt nicht als Schleife. Starts innerhalb der ersten
 *          60 s Laufzeit zählen; nach 60 s stabiler Laufzeit wird gelöscht.
 *          Ab 2 Starts zeichnet die Chronik nicht auf, ab 3 wird das
 *          Datei-Log abgeschaltet und gelöscht.
 *          DS18B20 darf höchstens dreimal in Folge neu starten; der Zähler
 *          wird nach 10 min stabiler Laufzeit gelöscht.
 */
#pragma once

#include "core/types.h"
#include "hal/system.h"

namespace ps {

class BootGuard {
public:
  static constexpr uint32_t kStableMs = 60000;
  static constexpr uint32_t kSensorStableMs = 600000;
  static constexpr uint8_t kMaxSensorRestarts = 3;

  /// Beim Start: Zähler lesen, Reset-Grund auswerten, erhöhen
  void begin(hal::ResetReason reason);
  /// Sekündlich
  void tick(Instant now);

  uint8_t count() const { return st_.bootCount; }
  bool chronikBlocked() const { return st_.bootCount >= 2; }
  bool fileLogBlocked() const { return st_.bootCount >= 3; }
  /// Darf ein Sensor das Gerät neu starten? Zählt mit, wenn ja.
  bool allowSensorRestart();
  static bool sensorRestartGate(void* self) {
    return static_cast<BootGuard*>(self)->allowSensorRestart();
  }

  /// Reine Logik (nativ getestet): neuer Zählerstand nach einem Start
  static uint8_t nextCount(uint8_t stored, bool valid, hal::ResetReason reason);

private:
  struct Rtc {
    uint32_t magic;
    uint8_t bootCount;
    uint8_t sensorRestarts;
    uint8_t pad[2];
    uint32_t check;
  };
  static uint32_t checksum(const Rtc& r) {
    return r.magic ^ (uint32_t(r.bootCount) << 8) ^ (uint32_t(r.sensorRestarts) << 16) ^
           0xA5A55A5Au;
  }
  void save();
  Rtc st_{};
  bool bootCleared_ = false;
  bool sensorCleared_ = false;
};

} // namespace ps
