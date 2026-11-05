/**
 * @file channel.h
 * @brief Laufzeitzustand eines Kanals
 * @details Liegt im StateStore, nicht im Treiber (refactor.md 5.6).
 */
#pragma once

#include <math.h>
#include <stdint.h>

#include "core/sensors/autocal.h"
#include "core/sensors/status.h"
#include "core/types.h"

namespace ps {

struct ChannelState {
  // persistiert (state.bin)
  float value = NAN;
  int16_t raw = -1;
  float obsMin = INFINITY;
  float obsMax = -INFINITY;
  int16_t rawMin = INT16_MAX;
  int16_t rawMax = INT16_MIN;
  AutoCalState cal;
  // nur zur Laufzeit
  ChannelStatus status = ChannelStatus::Unknown;
  bool hasOk = false; ///< seit dem Start erfolgreich gemessen
  Instant lastOk;     ///< millis() der letzten erfolgreichen Messung

  bool hasObs() const { return !isinf(obsMin) && !isinf(obsMax); }
  bool hasRawObs() const { return rawMin <= rawMax; }
  void observe(float v) {
    if (isnan(v))
      return;
    if (v < obsMin)
      obsMin = v;
    if (v > obsMax)
      obsMax = v;
  }
  void observeRaw(int16_t r) {
    if (r < 0)
      return;
    if (r < rawMin)
      rawMin = r;
    if (r > rawMax)
      rawMax = r;
  }
  void resetObs() {
    obsMin = INFINITY;
    obsMax = -INFINITY;
  }
  void resetRawObs() {
    rawMin = INT16_MAX;
    rawMax = INT16_MIN;
  }
};

} // namespace ps
