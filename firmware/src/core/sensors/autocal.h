/**
 * @file autocal.h
 * @brief Automatische Kalibrierung der Analog-Rohgrenzen (EMA)
 * @details Ein Rohwert außerhalb [min,max] verschiebt die Grenze sofort. Sonst
 *          "vergessen" beide Grenzen per EMA langsam in Richtung des Werts:
 *          α = 1 − 0.5^(1/n), n = Halbwertszeit / Intervall. Der Float-Zustand
 *          wird separat gehalten; persistiert wird nur, wenn sich die
 *          gerundete Ganzzahl ändert.
 */
#pragma once

#include <math.h>
#include <stdint.h>

namespace ps {

struct AutoCalState {
  float min = NAN;
  float max = NAN;
  bool valid() const { return !isnan(min) && !isnan(max); }
};

/// α aus Halbwertszeit und Messintervall; 1e-4 bei ungültigen Werten.
inline float autocalAlpha(uint32_t halfLifeS, uint32_t intervalMs) {
  if (halfLifeS == 0 || intervalMs == 0)
    return 1e-4f;
  const double n = static_cast<double>(halfLifeS) * 1000.0 / static_cast<double>(intervalMs);
  if (!(n > 0) || isinf(n))
    return 1e-4f;
  const double a = 1.0 - pow(0.5, 1.0 / n);
  if (!(a > 0) || a >= 1)
    return 1e-4f;
  return static_cast<float>(a);
}

/// Start beim Einschalten: konfiguriertes min/max, erweitert um den letzten Rohwert.
inline AutoCalState autocalStart(int16_t cfgMin, int16_t cfgMax, int16_t lastRaw) {
  AutoCalState s;
  s.min = cfgMin;
  s.max = cfgMax;
  if (s.min > s.max) {
    const float t = s.min;
    s.min = s.max;
    s.max = t;
  }
  if (lastRaw >= 0) {
    if (lastRaw < s.min)
      s.min = lastRaw;
    if (lastRaw > s.max)
      s.max = lastRaw;
  }
  return s;
}

/**
 * @brief Einen Rohwert einarbeiten
 * @return true, wenn sich die gerundeten Grenzen geändert haben
 */
inline bool autocalUpdate(AutoCalState& s, float raw, float alpha, int16_t& outMin,
                          int16_t& outMax) {
  if (!s.valid()) {
    s.min = raw;
    s.max = raw;
  }
  if (raw < s.min) {
    s.min = raw;
  } else {
    s.min += alpha * (raw - s.min);
  }
  if (raw > s.max) {
    s.max = raw;
  } else {
    s.max += alpha * (raw - s.max);
  }
  if (s.min > s.max) {
    const float t = s.min;
    s.min = s.max;
    s.max = t;
  }
  const int16_t nmin = static_cast<int16_t>(lroundf(s.min));
  const int16_t nmax = static_cast<int16_t>(lroundf(s.max));
  const bool changed = nmin != outMin || nmax != outMax;
  outMin = nmin;
  outMax = nmax;
  return changed;
}

} // namespace ps
