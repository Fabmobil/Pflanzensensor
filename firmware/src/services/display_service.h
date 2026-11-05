/**
 * @file display_service.h
 * @brief Display-Rotation, Bootlog und OTA-Fortschritt (refactor.md 2.7)
 * @details Ohne Display-Hardware läuft alles weiter; die Uhr wird ohne
 *          gültige Zeit übersprungen, ohne einen Rotationsschritt zu
 *          verbrauchen (B-34). 12h-Format zeigt AM/PM (B-35).
 */
#pragma once

#include "core/display/canvas.h"
#include "hal/display.h"
#include "services/config_service.h"
#include "services/net_service.h"
#include "services/sensor_service.h"

namespace ps {

class DisplayService {
public:
  DisplayService(hal::DisplayBus& bus, ConfigService& cfg, SensorService& sensors, NetService& net)
      : bus_(bus), cfg_(cfg), sensors_(sensors), net_(net) {}

  void begin();
  bool present() const { return present_; }
  /// Statuszeile für den Boot-/Update-Bildschirm
  void bootLine(const char* line);
  void bootHeader(bool update);
  void endBoot();
  void otaProgress(uint8_t percent);
  /// Alle 100 ms
  void tick(Instant now);
  /// Kanal des aktuellen Bildschirms (-1: keiner) – für LED-Modus 1
  int currentSlot() const { return currentSlot_; }
  /// Konfiguration geändert: sofort neu zeichnen
  void refresh() {
    lastSwitch_ = Instant();
    forceNext_ = true;
  }

private:
  enum Screen : uint8_t { Info = 0, Clock, Qr, Flower, Fabmobil, FirstChannel };
  bool emptyLogged_ = false;
  bool screenAvailable(uint8_t s) const;
  void render(uint8_t s);
  /// Gerendertes Bild in den folgenden Ticks seitenweise übertragen
  void startPush() { pushPage_ = 0; }
  void renderBoot();

  hal::DisplayBus& bus_;
  ConfigService& cfg_;
  SensorService& sensors_;
  NetService& net_;
  Canvas canvas_;
  bool present_ = false;
  bool boot_ = true;
  bool ota_ = false;
  bool forceNext_ = true;
  char header_[48] = "";
  char lines_[6][24] = {};
  uint8_t lineCount_ = 0;
  uint8_t screen_ = 0xFF;
  int currentSlot_ = -1;
  Instant lastSwitch_;
  /// Nächste zu übertragende Seite; kPages = nichts offen. Ein ganzes Bild
  /// dauert über I2C ~35 ms - zu lang für einen Schritt (Ziel ≤ 20 ms).
  uint8_t pushPage_ = hal::DisplayBus::kPages;
  static constexpr uint8_t kPagesPerTick = 4;
};

} // namespace ps
