/**
 * @file ssd1306.h
 * @brief Schlanker SSD1306-Treiber (128×64, I²C) für hal::DisplayBus
 * @details Entscheidung O5: eigener Treiber + eigener Font statt
 *          Adafruit_SSD1306/GFX. Der Framebuffer (1 KB) gehört dem
 *          DisplayService; hier wird nur übertragen.
 */
#pragma once

#include "hal/display.h"

namespace ps {
namespace drivers {

class Ssd1306 : public hal::DisplayBus {
public:
  bool begin(uint8_t address) override;
  void flushPages(const uint8_t* fb, uint8_t first, uint8_t count) override;
  void power(bool on) override;

private:
  void command(uint8_t c);
  uint8_t addr_ = 0x3C;
  bool ok_ = false;
};

} // namespace drivers
} // namespace ps
