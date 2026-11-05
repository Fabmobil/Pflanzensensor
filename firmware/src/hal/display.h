/**
 * @file display.h
 * @brief Ausgabe eines 128×64-Framebuffers auf das SSD1306
 */
#pragma once

#include <stdint.h>

namespace ps {
namespace hal {

class DisplayBus {
public:
  virtual bool begin(uint8_t address) = 0;
  static constexpr uint8_t kPages = 8;
  /// Seiten [first, first+count) aus fb (1024 Byte im SSD1306-Seitenformat,
  /// 8 Seiten × 128 Spalten) übertragen. Eine Seite dauert über I2C ~4 ms.
  virtual void flushPages(const uint8_t* fb, uint8_t first, uint8_t count) = 0;
  /// Ganzes Bild auf einmal (Boot, OTA); in der Rotation seitenweise
  void flush(const uint8_t* fb) { flushPages(fb, 0, kPages); }
  virtual void power(bool on) = 0;

protected:
  ~DisplayBus() = default;
};

} // namespace hal
} // namespace ps
