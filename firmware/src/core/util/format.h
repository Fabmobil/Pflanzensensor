/**
 * @file format.h
 * @brief Kleine Formatierer für Laufzeit, Datum und Uhrzeit (wie im Altcode)
 */
#pragma once

#include <stdint.h>
#include <stdio.h>

namespace ps {
namespace fmt {

/// "2d 3h 4m" (Tage/Stunden nur wenn > 0)
inline void uptime(uint32_t s, char* out, size_t cap) {
  const uint32_t d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
  int n = 0;
  out[0] = '\0';
  if (d)
    n += snprintf(out + n, cap - n, "%lud ", static_cast<unsigned long>(d));
  if (h && static_cast<size_t>(n) < cap)
    n += snprintf(out + n, cap - n, "%luh ", static_cast<unsigned long>(h));
  if (static_cast<size_t>(n) < cap)
    snprintf(out + n, cap - n, "%lum", static_cast<unsigned long>(m));
}

inline void date(uint16_t y, uint8_t mo, uint8_t d, char* out, size_t cap) {
  snprintf(out, cap, "%02u.%02u.%04u", d, mo, y);
}

inline void time(uint8_t h, uint8_t m, char* out, size_t cap) {
  snprintf(out, cap, "%02u:%02u", h, m);
}

} // namespace fmt
} // namespace ps
