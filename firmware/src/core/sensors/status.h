/**
 * @file status.h
 * @brief Bewertung eines Messwerts gegen seine Schwellen
 * @details Regeln aus test_messzustand des Altcodes (refactor.md 2.4 Punkt 11).
 */
#pragma once

#include <math.h>

#include "core/sensors/catalog.h"

namespace ps {

enum class ChannelStatus : uint8_t { Green = 0, Yellow, Red, Error, Unknown, Warmup };

/// Statustext für JSON/API ("green" …), im Flash.
FStr statusName(ChannelStatus s);
/// Status aus Text; Unknown, wenn unbekannt.
ChannelStatus statusFromName(const char* s);

/**
 * @brief Wert bewerten
 * @param oneSided nur obere Grenzen (gh/yh) zählen
 * @return Error bei NaN/Inf
 */
inline ChannelStatus evaluate(float v, const Limits& l, bool oneSided) {
  if (isnan(v) || isinf(v))
    return ChannelStatus::Error;
  if (oneSided) {
    if (v <= l.greenHigh)
      return ChannelStatus::Green;
    if (v <= l.yellowHigh)
      return ChannelStatus::Yellow;
    return ChannelStatus::Red;
  }
  if (v < l.yellowLow || v > l.yellowHigh)
    return ChannelStatus::Red;
  if (v < l.greenLow || v > l.greenHigh)
    return ChannelStatus::Yellow;
  return ChannelStatus::Green;
}

/// Sind die Schwellen geordnet? Einseitig nur gh ≤ yh (Befund B-26).
inline bool limitsOrdered(const Limits& l, bool oneSided) {
  if (isnan(l.yellowLow) || isnan(l.greenLow) || isnan(l.greenHigh) || isnan(l.yellowHigh))
    return false;
  if (oneSided)
    return l.greenHigh <= l.yellowHigh;
  return l.yellowLow <= l.greenLow && l.greenLow <= l.greenHigh && l.greenHigh <= l.yellowHigh;
}

} // namespace ps
