/**
 * @file crc.h
 * @brief CRC-32 (IEEE 802.3) und CRC-8 (Polynom 0x07)
 * @details CRC-32 mit Halbbyte-Tabelle (64 Byte statt 1 KB), Ergebnis
 *          identisch zur klassischen Tabellenvariante des Altcodes.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {
namespace crc {

inline uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  static const uint32_t kNibble[16] = {0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
                                       0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
                                       0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
                                       0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    crc = (crc >> 4) ^ kNibble[crc & 0x0F];
    crc = (crc >> 4) ^ kNibble[crc & 0x0F];
  }
  return crc;
}

/// Einmalige Berechnung
inline uint32_t crc32(const uint8_t* data, size_t len) {
  return ~crc32Update(0xFFFFFFFFu, data, len);
}

/// Fortlaufende Berechnung: Start mit crc32Begin(), Ende mit crc32End().
inline uint32_t crc32Begin() { return 0xFFFFFFFFu; }
inline uint32_t crc32End(uint32_t c) { return ~c; }

/// CRC-8/ATM (Polynom 0x07, Start 0), Prüfsumme der Chronikrahmen.
inline uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t c = 0;
  for (size_t i = 0; i < len; i++) {
    c ^= data[i];
    for (uint8_t b = 0; b < 8; b++)
      c = (c & 0x80) ? static_cast<uint8_t>((c << 1) ^ 0x07) : static_cast<uint8_t>(c << 1);
  }
  return c;
}

} // namespace crc
} // namespace ps
