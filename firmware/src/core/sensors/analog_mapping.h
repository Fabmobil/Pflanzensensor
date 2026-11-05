/**
 * @file analog_mapping.h
 * @brief Rohwert eines Analogkanals in Prozent
 */
#pragma once

#include <stdint.h>

namespace ps {

/**
 * @brief Rohwert auf [min,max] begrenzen und in Prozent umrechnen
 * @param clamped erhält true, wenn begrenzt werden musste
 * @return 0..100; max == min ergibt 0
 */
inline float analogPercent(int32_t raw, int32_t min, int32_t max, bool inverted,
                           bool* clamped = nullptr) {
  if (min > max) {
    const int32_t t = min;
    min = max;
    max = t;
  }
  bool c = false;
  if (raw < min) {
    raw = min;
    c = true;
  } else if (raw > max) {
    raw = max;
    c = true;
  }
  if (clamped)
    *clamped = c;
  if (max == min)
    return 0.0f;
  const float span = static_cast<float>(max - min);
  if (inverted)
    return 100.0f * static_cast<float>(max - raw) / span;
  return 100.0f * static_cast<float>(raw - min) / span;
}

} // namespace ps
