/**
 * @file ssd1306.cpp
 */
#if defined(ARDUINO)

#include "drivers/ssd1306.h"

#include <Wire.h>

#include "board.h"

namespace ps {
namespace drivers {

void Ssd1306::command(uint8_t c) {
  Wire.beginTransmission(addr_);
  Wire.write(0x00);
  Wire.write(c);
  Wire.endTransmission();
}

bool Ssd1306::begin(uint8_t address) {
  addr_ = address;
  Wire.begin(board::kPins.sda, board::kPins.scl);
  Wire.setClock(400000);
  Wire.beginTransmission(addr_);
  if (Wire.endTransmission() != 0)
    return false;
  static const uint8_t kInit[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D,
                                  0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF,
                                  0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x2E, 0xAF};
  for (uint8_t c : kInit)
    command(c);
  ok_ = true;
  return true;
}

void Ssd1306::flushPages(const uint8_t* fb, uint8_t first, uint8_t count) {
  if (!ok_ || first >= kPages || count == 0)
    return;
  if (first + count > kPages)
    count = static_cast<uint8_t>(kPages - first);
  command(0x21); // Spalten 0..127
  command(0);
  command(127);
  command(0x22); // Seiten first..last
  command(first);
  command(static_cast<uint8_t>(first + count - 1));
  // 32 Byte je Transfer (+1 Steuerbyte) passen in den Wire-Puffer beider Cores
  const uint16_t from = static_cast<uint16_t>(first) * 128;
  const uint16_t to = static_cast<uint16_t>(from + count * 128);
  for (uint16_t i = from; i < to; i += 32) {
    Wire.beginTransmission(addr_);
    Wire.write(0x40);
    Wire.write(fb + i, 32);
    Wire.endTransmission();
  }
}

void Ssd1306::power(bool on) {
  if (ok_)
    command(on ? 0xAF : 0xAE);
}

} // namespace drivers
} // namespace ps

#endif
