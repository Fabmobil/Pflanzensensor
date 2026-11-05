/**
 * @file aggregate.h
 * @brief Zusammenfassen der Runden einer Messung je Kanal
 * @details Median der gültigen Runden; bei zwei gültigen der Mittelwert, bei
 *          einer dieser Wert, bei keiner NaN (refactor.md 2.4 Punkt 4).
 */
#pragma once

#include <math.h>
#include <stdint.h>

namespace ps {

/// Ein Rundenwert. NaN heißt ungültig; raw < 0 heißt kein Rohwert.
struct Reading {
  float value = NAN;
  int16_t raw = -1;
};

/// Median gültiger Werte. values wird dabei sortiert. NaN, wenn n == 0.
inline float medianOf(float* values, uint8_t n) {
  if (n == 0)
    return NAN;
  for (uint8_t i = 1; i < n; i++) { // Insertion-Sort, n ist klein
    const float v = values[i];
    int8_t j = static_cast<int8_t>(i - 1);
    while (j >= 0 && values[j] > v) {
      values[j + 1] = values[j];
      j--;
    }
    values[j + 1] = v;
  }
  if (n % 2 == 1)
    return values[n / 2];
  return (values[n / 2 - 1] + values[n / 2]) / 2.0f;
}

/**
 * @brief Kanal aggregieren
 * @param rounds Runden-Array, Kanal ch steht bei rounds[r * stride + ch]
 * @return Reading mit Median-Wert und Median-Rohwert (gerundet)
 */
inline Reading aggregateChannel(const Reading* rounds, uint8_t nRounds, uint8_t stride,
                                uint8_t ch) {
  float vals[8];
  float raws[8];
  uint8_t nv = 0, nr = 0;
  for (uint8_t r = 0; r < nRounds && r < 8; r++) {
    const Reading& rd = rounds[r * stride + ch];
    if (isnan(rd.value) || isinf(rd.value))
      continue;
    vals[nv++] = rd.value;
    if (rd.raw >= 0)
      raws[nr++] = rd.raw;
  }
  Reading out;
  out.value = medianOf(vals, nv);
  if (nr > 0)
    out.raw = static_cast<int16_t>(lroundf(medianOf(raws, nr)));
  return out;
}

} // namespace ps
