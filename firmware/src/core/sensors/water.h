/**
 * @file water.h
 * @brief Wasservorrat aus dem Topfgewicht, Gießerkennung, Kalibrierpunkte und
 *        das Lernen eines Bodenfeuchtesensors an der Waage
 * @details Die Waage misst, wie viel Wasser im Topf ist: 100 % beim Gewicht
 *          nach dem Gießen und Abtropfen (wetG), 0 % beim Gewicht, bei dem
 *          die Pflanze Wasser braucht (dryG). Ein Bodenfeuchtesensor misst nur
 *          die Erde um sich herum und ist nicht linear. Hängt er im selben
 *          Topf, sammelt SoilFit seine Rohwerte je 10 % Wasservorrat und
 *          schlägt daraus Rohgrenzen vor.
 */
#pragma once

#include <math.h>
#include <stdint.h>

#include "core/config/sensor_config.h"
#include "core/types.h"

namespace ps {

/// Wasservorrat in Prozent (0..100), NaN ohne gültige Nass-/Trockenpunkte.
inline float waterPercent(float weight, float dryG, float wetG) {
  if (isnan(weight) || isnan(dryG) || isnan(wetG) || !(wetG > dryG))
    return NAN;
  const float p = 100.0f * (weight - dryG) / (wetG - dryG);
  if (p < 0)
    return 0;
  if (p > 100)
    return 100;
  return p;
}

/**
 * @brief Kalibrierpunkt eines Analogkanals auf einen Rohwert setzen
 * @details Welche Grenze 0 % bzw. 100 % ist, hängt von der Invertierung ab.
 *          Die automatische Kalibrierung wird abgeschaltet, sonst wandern die
 *          Grenzen wieder weg.
 * @param percent 0 oder 100
 * @return OutOfRange, wenn der Rohwert auf der falschen Seite der anderen
 *         Grenze liegt (meist stimmt dann die Invertierung nicht)
 */
inline Err analogSetPoint(ChannelConfig& c, int16_t raw, uint8_t percent) {
  if (raw < 0 || (percent != 0 && percent != 100))
    return Err::InvalidArgument;
  const bool upper = (percent == 100) != c.inverted; // rawMax ist gemeint
  if (upper ? raw <= c.rawMin : raw >= c.rawMax)
    return Err::OutOfRange;
  (upper ? c.rawMax : c.rawMin) = raw;
  c.autocal = false;
  return Err::None;
}

/**
 * @brief Nass- oder Trockengewicht der Waage setzen
 * @details Ein Punkt gilt als gesetzt, wenn er nicht 0 ist. Liegt der andere
 *          Punkt schon fest, muss nass schwerer als trocken sein.
 */
inline Err weightSetPoint(SensorConfig& c, float weight, uint8_t percent) {
  if (isnan(weight) || isinf(weight) || (percent != 0 && percent != 100))
    return Err::InvalidArgument;
  if (percent == 100) {
    if (c.dryG != 0 && !(weight > c.dryG))
      return Err::OutOfRange;
    c.wetG = weight;
  } else {
    if (c.wetG != 0 && !(weight < c.wetG))
      return Err::OutOfRange;
    c.dryG = weight;
  }
  return Err::None;
}

/**
 * @brief Gießen am Gewichtsanstieg erkennen
 * @details Vergleicht mit dem kleinsten der letzten fünf Gewichte, damit auch
 *          langsames Gießen über zwei Messungen auffällt. Nach dem Gießen
 *          tropft Wasser ab; erst nach kSettleMs zählt das Gewicht als
 *          neuer Nasspunkt.
 */
class WateringDetector {
public:
  static constexpr uint32_t kSettleMs = 60UL * 60 * 1000;
  static constexpr uint8_t kHistory = 5;

  /// Neues Gewicht. true, wenn gerade gegossen wurde (Anstieg ≥ thresholdG).
  bool feed(float weight, uint16_t thresholdG, Instant now) {
    if (isnan(weight))
      return false;
    bool watered = false;
    if (thresholdG > 0 && n_ > 0) {
      float lo = hist_[0];
      for (uint8_t i = 1; i < n_; i++)
        if (hist_[i] < lo)
          lo = hist_[i];
      if (weight - lo >= thresholdG) {
        watered = true;
        watered_ = true;
        rise_ = weight - lo;
        settling_ = true;
        wateredAt_ = now;
        n_ = 0; // weiteres Gießen zählt erneut und verlängert das Abtropfen
      }
    }
    if (n_ < kHistory) {
      hist_[n_++] = weight;
    } else {
      for (uint8_t i = 1; i < kHistory; i++)
        hist_[i - 1] = hist_[i];
      hist_[kHistory - 1] = weight;
    }
    return watered;
  }

  /// Von vorn beginnen (Tara/Skalierung geändert: Gewichte nicht vergleichbar)
  void restart() {
    n_ = 0;
    settling_ = false;
  }

  /// Abtropfzeit vorbei? Dann einmal true und das Abtropfen ist beendet.
  bool settledNow(Instant now) {
    if (!settling_ || (now - wateredAt_).millis() < static_cast<int32_t>(kSettleMs))
      return false;
    settling_ = false;
    return true;
  }

  bool settling() const { return settling_; }
  /// Seit dem Start gegossen? (wateredAt/lastRise gelten nur dann)
  bool watered() const { return watered_; }
  Instant wateredAt() const { return wateredAt_; }
  float lastRise() const { return rise_; }

private:
  float hist_[kHistory] = {};
  uint8_t n_ = 0;
  bool settling_ = false;
  bool watered_ = false;
  float rise_ = 0;
  Instant wateredAt_;
};

/// Vorschlag für die Rohgrenzen eines Bodenfeuchtesensors
struct FitSuggestion {
  bool ok = false;
  int16_t rawMin = 0;
  int16_t rawMax = 0;
  bool inverted = false;
  float r2 = NAN;   ///< Bestimmtheitsmaß der Geraden (1 = ganz linear)
  uint8_t bins = 0; ///< genutzte Bereiche
};

/**
 * @brief Rohwerte eines Sensors je 10 % Wasservorrat
 * @details Jeder Bereich führt einen gleitenden Mittelwert (wirkt ab
 *          kWindow Messungen wie ein EMA, damit Drift des Sensors nicht ewig
 *          nachhängt). Für den Vorschlag zählt jeder Bereich gleich, egal wie
 *          lange der Topf dort stand.
 */
struct SoilFit {
  static constexpr uint8_t kBins = 10;
  static constexpr uint16_t kWindow = 50;
  static constexpr uint16_t kMinSamples = 3;

  float raw[kBins];
  uint16_t n[kBins];

  SoilFit() { clear(); }
  void clear() {
    for (uint8_t i = 0; i < kBins; i++) {
      raw[i] = 0;
      n[i] = 0;
    }
  }
  bool empty() const {
    for (uint8_t i = 0; i < kBins; i++)
      if (n[i])
        return false;
    return true;
  }

  static uint8_t binOf(float waterPct) {
    if (!(waterPct > 0))
      return 0;
    const int b = static_cast<int>(waterPct / 10.0f);
    return static_cast<uint8_t>(b >= kBins ? kBins - 1 : b);
  }

  void add(float waterPct, int16_t r) {
    if (isnan(waterPct) || r < 0)
      return;
    const uint8_t b = binOf(waterPct);
    if (n[b] < UINT16_MAX)
      n[b]++;
    const uint16_t k = n[b] < kWindow ? n[b] : kWindow;
    raw[b] += (static_cast<float>(r) - raw[b]) / static_cast<float>(k);
  }

  /**
   * @brief Gerade durch die Bereichsmittel, verlängert auf 0 % und 100 %
   * @details Braucht mindestens drei Bereiche mit je kMinSamples Messungen,
   *          die zusammen 30 % Wasservorrat überspannen.
   */
  FitSuggestion suggest() const {
    FitSuggestion s;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    uint8_t k = 0, lo = kBins, hi = 0;
    for (uint8_t i = 0; i < kBins; i++) {
      if (n[i] < kMinSamples)
        continue;
      const double x = 10.0 * i + 5.0;
      sx += x;
      sy += raw[i];
      sxx += x * x;
      sxy += x * raw[i];
      k++;
      if (i < lo)
        lo = i;
      if (i > hi)
        hi = i;
    }
    s.bins = k;
    if (k < 3 || hi - lo < 3)
      return s;
    const double den = k * sxx - sx * sx;
    if (den <= 0)
      return s;
    const double b = (k * sxy - sx * sy) / den;
    const double a = (sy - b * sx) / k;
    if (fabs(b) < 0.05) // weniger als 5 Rohpunkte über den ganzen Bereich
      return s;
    const double my = sy / k;
    double ssTot = 0, ssRes = 0;
    for (uint8_t i = 0; i < kBins; i++) {
      if (n[i] < kMinSamples)
        continue;
      const double x = 10.0 * i + 5.0;
      const double e = raw[i] - (a + b * x);
      ssRes += e * e;
      ssTot += (raw[i] - my) * (raw[i] - my);
    }
    s.r2 = ssTot > 0 ? static_cast<float>(1.0 - ssRes / ssTot) : NAN;
    const double r0 = a, r100 = a + 100.0 * b;
    auto clampRaw = [](double v) {
      return static_cast<int16_t>(v < 0 ? 0 : v > 4095 ? 4095 : lround(v));
    };
    s.inverted = b < 0;
    s.rawMin = clampRaw(s.inverted ? r100 : r0);
    s.rawMax = clampRaw(s.inverted ? r0 : r100);
    s.ok = s.rawMin < s.rawMax;
    return s;
  }
};

} // namespace ps
