/**
 * @file sensor_service.cpp
 */
#include "services/sensor_service.h"

#include <stdio.h>

#include "core/sensors/analog_mapping.h"
#include "services/log.h"

namespace ps {

namespace {
/// Statustexte fürs Log, Reihenfolge wie ChannelStatus
const char kStatusDe[6][12] PS_PROGMEM = {"OK",     "Warnung",   "Kritisch",
                                          "Fehler", "unbekannt", "Aufwärmen"};

struct StatusText {
  char s[12];
  explicit StatusText(ChannelStatus st) {
    const uint8_t i = static_cast<uint8_t>(st);
    ps_memcpy_P(s, kStatusDe[i < 6 ? i : 4], sizeof(s));
  }
};

/// "17.2 %" oder "--"
void formatValue(char* out, size_t cap, float v, const char* unit) {
  if (isnan(v))
    snprintf(out, cap, "--");
  else
    snprintf(out, cap, "%.1f %s", static_cast<double>(v), unit);
}

struct SensorId {
  char s[16];
  explicit SensorId(uint8_t sensor) {
    copyP(s, sizeof(s), catalog::id(board::sensor(sensor).type));
  }
};
} // namespace

void SensorService::onJobStart(uint8_t sensor, bool priority, Instant now) {
  jobStart_[sensor] = now;
  if (!log::enabled(LogLevel::Debug, Mod::Cycle))
    return;
  SensorId id(sensor);
  LOGD(Mod::Cycle, "%s: Messung startet%s", id.s, priority ? " (Sofortmessung)" : "");
}

void SensorService::onRounds(uint8_t sensor, const Reading* samples, uint8_t rounds, uint8_t stride,
                             uint8_t n) {
  if (rounds < 2 || !log::enabled(LogLevel::Debug, Mod::Cycle))
    return;
  const auto& se = board::sensor(sensor);
  const SensorsConfig& sc = cfg_.sensors();
  for (uint8_t ch = 0; ch < n && ch < se.channels; ch++) {
    const uint8_t slot = static_cast<uint8_t>(se.firstSlot + ch);
    const ChannelConfig& cc = sc.channels[slot];
    if (!cc.enabled)
      continue;
    const bool raw = catalog::channel(se.type, ch).hasRaw;
    char buf[72];
    size_t len = 0;
    for (uint8_t r = 0; r < rounds && len < sizeof(buf) - 12; r++) {
      const Reading& x = samples[r * stride + ch];
      const char* sep = r ? " | " : "";
      int w;
      if (raw ? x.raw < 0 : isnan(x.value))
        w = snprintf(buf + len, sizeof(buf) - len, "%s--", sep);
      else if (raw)
        w = snprintf(buf + len, sizeof(buf) - len, "%s%d", sep, x.raw);
      else
        w = snprintf(buf + len, sizeof(buf) - len, "%s%.1f", sep, static_cast<double>(x.value));
      if (w > 0)
        len += static_cast<size_t>(w);
    }
    LOGD(Mod::Cycle, "%s: %s aus %u Runden %s (Median zählt)", cc.name.c_str(),
         raw ? "Rohwerte" : "Werte", rounds, buf);
  }
}

void SensorService::logResult(uint8_t slot, float value, int16_t raw, ChannelStatus before,
                              ChannelStatus after, bool newMin, bool newMax) {
  const ChannelConfig& cc = cfg_.sensors().channels[slot];
  const char* name = cc.name.c_str();
  char val[24];
  formatValue(val, sizeof(val), value, cc.unit.c_str());
  const StatusText now(after);
  if (log::enabled(LogLevel::Debug, Mod::Sensor)) {
    if (raw >= 0)
      LOGD(Mod::Sensor, "%s = %s (roh %d, Grenzen %d..%d%s) -> %s", name, val, raw, cc.rawMin,
           cc.rawMax, cc.inverted ? ", invertiert" : "", now.s);
    else
      LOGD(Mod::Sensor, "%s = %s -> %s", name, val, now.s);
  }
  if (before != after) {
    const StatusText was(before);
    if (after == ChannelStatus::Error)
      LOGW(Mod::Sensor, "%s: kein gültiger Messwert (vorher %s)", name, was.s);
    else if (before == ChannelStatus::Unknown || before == ChannelStatus::Warmup)
      LOGI(Mod::Sensor, "%s: erster Wert %s (%s)", name, val, now.s);
    else if (before == ChannelStatus::Error)
      LOGI(Mod::Sensor, "%s: wieder gültig, %s (%s)", name, val, now.s);
    else
      LOGI(Mod::Sensor, "%s: %s -> %s (%s)", name, was.s, now.s, val);
  }
  if (before != ChannelStatus::Unknown && (newMin || newMax))
    LOGD(Mod::Sensor, "%s: neues %s %s", name, newMin ? "Minimum" : "Maximum", val);
}

void SensorService::begin(SensorDriver* const* drivers, Instant now) {
  const SensorsConfig& c = cfg_.sensors();
  for (uint8_t i = 0; i < board::sensorCount(); i++) {
    drivers_[i] = drivers[i];
    drivers_[i]->configure(c.sensors[i]);
    sched_.add(*drivers_[i], board::sensor(i).channels, c.sensors[i].intervalS * 1000UL,
               c.sensors[i].enabled);
  }
  // Autokalibrierung: Float-Zustand aus gespeichertem Zustand oder Startwerten
  for (uint8_t s = 0; s < board::slotCount(); s++) {
    const ChannelConfig& cc = c.channels[s];
    ChannelState& st = state_.channel(s);
    if (cc.autocal && !st.cal.valid())
      st.cal = autocalStart(cc.rawMin, cc.rawMax, st.raw);
  }
  sched_.start(now);
}

int SensorService::sensorByName(const char* id) {
  if (!id)
    return -1;
  const SensorType t = catalog::typeFromId(id, strlen(id));
  if (t == SensorType::Count)
    return -1;
  return board::sensorIndexOf(t);
}

MeasureResult SensorService::measureNow(uint8_t sensor, Instant now, bool publicRequest,
                                        uint32_t& waitMs) {
  waitMs = 0;
  if (sensor >= board::sensorCount())
    return MeasureResult::NotFound;
  if (publicRequest && !publicThrottle_.tryAcquire(now.millis())) {
    waitMs = publicThrottle_.remaining(now.millis());
    return MeasureResult::Throttled;
  }
  switch (sched_.measureNow(sensor, now)) {
  case MeasureNowResult::Accepted:
    LOGD(Mod::Cycle, "%s: Sofortmessung angenommen", SensorId(sensor).s);
    return MeasureResult::Accepted;
  case MeasureNowResult::Busy:
    return MeasureResult::Busy;
  case MeasureNowResult::NotReady:
    waitMs = sched_.warmupRemainingMs(sensor, now);
    LOGI(Mod::Cycle, "%s: Sofortmessung abgelehnt, wärmt noch %lu s auf", SensorId(sensor).s,
         static_cast<unsigned long>(waitMs / 1000));
    return MeasureResult::NotReady;
  case MeasureNowResult::Disabled:
    return MeasureResult::Disabled;
  default:
    return MeasureResult::NotFound;
  }
}

void SensorService::applyConfig(uint32_t changed, Instant now) {
  const SensorsConfig& c = cfg_.sensors();
  for (uint8_t i = 0; i < board::sensorCount(); i++) {
    if (!(changed & (1u << i)))
      continue;
    if (drivers_[i])
      drivers_[i]->configure(c.sensors[i]);
    {
      const SensorRunState& rs = sched_.state(i);
      const uint32_t iv = c.sensors[i].intervalS * 1000UL;
      if (rs.enabled != c.sensors[i].enabled)
        LOGI(Mod::Sensor, "%s %s", SensorId(i).s,
             c.sensors[i].enabled ? "aktiviert" : "deaktiviert");
      if (rs.intervalMs != iv)
        LOGI(Mod::Sensor, "%s: Messintervall %lu s -> %lu s", SensorId(i).s,
             static_cast<unsigned long>(rs.intervalMs / 1000),
             static_cast<unsigned long>(iv / 1000));
    }
    sched_.setEnabled(i, c.sensors[i].enabled, now);
    sched_.setInterval(i, c.sensors[i].intervalS * 1000UL, now);
    // Autokalibrierung neu eingeschaltet: Start aus konfigurierten Grenzen
    const auto& se = board::sensor(i);
    for (uint8_t ch = 0; ch < se.channels; ch++) {
      const uint8_t slot = static_cast<uint8_t>(se.firstSlot + ch);
      ChannelState& st = state_.channel(slot);
      const ChannelConfig& cc = c.channels[slot];
      if (cc.autocal && !st.cal.valid())
        st.cal = autocalStart(cc.rawMin, cc.rawMax, st.raw);
      if (!cc.autocal)
        st.cal = AutoCalState{};
      if (!cc.enabled)
        st.status = ChannelStatus::Unknown;
    }
  }
}

void SensorService::resetExtrema(uint8_t slot, bool raw, Instant now) {
  if (slot >= board::slotCount())
    return;
  ChannelState& st = state_.channel(slot);
  if (raw)
    st.resetRawObs();
  else
    st.resetObs();
  state_.markImportant(now);
}

Err SensorService::requestScaleAction(uint8_t sensor, Instant now) {
  pendingAt_ = now;
  uint32_t wait;
  // Busy: die laufende Messung begann vorher und zählt nicht; afterScale
  // fordert danach eine neue an.
  if (measureNow(sensor, now, false, wait) == MeasureResult::Disabled)
    return Err::Disabled;
  return Err::None;
}

Err SensorService::tare(uint8_t sensor, Instant now) {
  if (sensor >= board::sensorCount() || board::sensor(sensor).type != SensorType::Hx711)
    return Err::Unsupported;
  const Err e = requestScaleAction(sensor, now);
  if (e == Err::None)
    pendingTare_ |= (1u << sensor);
  return e;
}

Err SensorService::scaleReference(uint8_t sensor, Instant now) {
  if (sensor >= board::sensorCount() || board::sensor(sensor).type != SensorType::Hx711)
    return Err::Unsupported;
  const Err e = requestScaleAction(sensor, now);
  if (e == Err::None) {
    pendingRef_ = true;
    hasRef_ = false;
  }
  return e;
}

Err SensorService::scaleCalibrate(uint8_t sensor, float grams, Instant now) {
  if (sensor >= board::sensorCount() || board::sensor(sensor).type != SensorType::Hx711)
    return Err::Unsupported;
  if (!(grams > 0) || grams > 1e6f)
    return Err::OutOfRange;
  if (pendingRef_)
    return Err::Busy; // Ausgangswert noch nicht gemessen
  const Err e = requestScaleAction(sensor, now);
  if (e == Err::None)
    pendingGrams_ = grams;
  return e;
}

Err SensorService::calibratePoint(uint8_t slot, uint8_t percent, Instant now) {
  if (slot >= board::slotCount())
    return Err::NotFound;
  if (percent != 0 && percent != 100)
    return Err::InvalidArgument;
  const auto& sl = board::slot(slot);
  const auto& se = board::sensor(sl.sensor);
  const ChannelDef def = catalog::channel(se.type, sl.channel);
  SensorsConfig& sc = cfg_.sensorsMutable();
  ChannelConfig& cc = sc.channels[slot];
  if (def.hasRaw) {
    const ChannelState& st = state_.channel(slot);
    if (!st.hasOk || st.raw < 0)
      return Err::NotReady;
    ChannelConfig copy = cc;
    const Err e = analogSetPoint(copy, st.raw, percent);
    if (e != Err::None)
      return e;
    cc = copy;
    LOGI(Mod::Sensor, "%s: Rohwert %d ist jetzt %u %% (Grenzen %d..%d)", cc.name.c_str(), st.raw,
         percent, cc.rawMin, cc.rawMax);
  } else if (se.type == SensorType::Hx711 && sl.channel == 1) {
    const ChannelState& w = state_.channel(se.firstSlot);
    if (!w.hasOk || isnan(w.value))
      return Err::NotReady;
    SensorConfig copy = sc.sensors[sl.sensor];
    const Err e = weightSetPoint(copy, w.value, percent);
    if (e != Err::None)
      return e;
    sc.sensors[sl.sensor] = copy;
    cc.enabled = true;
    LOGI(Mod::Sensor, "%s: %s = %.0f g", cc.name.c_str(), percent ? "nass" : "trocken",
         static_cast<double>(w.value));
  } else {
    return Err::Unsupported;
  }
  cfg_.markSensorsDirty(now);
  applyConfig(1u << sl.sensor, now);
  uint32_t wait;
  measureNow(sl.sensor, now, false, wait); // neuen Prozentwert gleich zeigen
  return Err::None;
}

const SoilFit* SensorService::soilFit(uint8_t slot) const {
  if (slot >= board::slotCount())
    return nullptr;
  const auto& sl = board::slot(slot);
  if (board::sensor(sl.sensor).type != SensorType::Analog)
    return nullptr;
  return &state_.soilFit(sl.channel);
}

Err SensorService::applySoilFit(uint8_t slot, Instant now) {
  const SoilFit* f = soilFit(slot);
  if (!f)
    return Err::Unsupported;
  const FitSuggestion sug = f->suggest();
  if (!sug.ok)
    return Err::NotReady;
  ChannelConfig& cc = cfg_.sensorsMutable().channels[slot];
  cc.rawMin = sug.rawMin;
  cc.rawMax = sug.rawMax;
  cc.inverted = sug.inverted;
  cc.autocal = false;
  LOGI(Mod::Sensor, "%s: Rohgrenzen aus der Waage %d..%d%s", cc.name.c_str(), cc.rawMin, cc.rawMax,
       cc.inverted ? ", invertiert" : "");
  cfg_.markSensorsDirty(now);
  applyConfig(1u << board::slot(slot).sensor, now);
  return Err::None;
}

Err SensorService::resetSoilFit(uint8_t slot, Instant now) {
  if (!soilFit(slot))
    return Err::Unsupported;
  state_.soilFit(board::slot(slot).channel).clear();
  state_.markImportant(now);
  return Err::None;
}

float SensorService::currentWater(Instant now) const {
  const int hx = board::sensorIndexOf(SensorType::Hx711);
  if (hx < 0 || board::sensor(hx).channels < 2 || water_.settling())
    return NAN;
  const uint8_t slot = static_cast<uint8_t>(board::sensor(hx).firstSlot + 1);
  const ChannelState& w = state_.channel(slot);
  if (!cfg_.sensors().channels[slot].enabled || !w.hasOk || isnan(w.value))
    return NAN;
  // Höchstens zwei Messintervalle der Waage alt, mindestens 10 min
  uint32_t maxAge = cfg_.sensors().sensors[hx].intervalS * 2000UL;
  if (maxAge < 600000UL)
    maxAge = 600000UL;
  if ((now - w.lastOk).millis() > static_cast<int32_t>(maxAge))
    return NAN;
  return w.value;
}

bool SensorService::afterScale(uint8_t sensor, float weight, Instant now) {
  SensorConfig& hc = cfg_.sensorsMutable().sensors[sensor];
  bool changed = false;
  if (hc.scale != seenScale_ || hc.tare != seenTare_) {
    water_.restart(); // ein Sprung durch Tara/Kalibrieren ist kein Gießen
    seenScale_ = hc.scale;
    seenTare_ = hc.tare;
  }
  if (water_.feed(weight, hc.wateringG, now))
    LOGI(Mod::Sensor, "Gegossen: +%.0f g, Nasspunkt wird nach dem Abtropfen geprüft",
         static_cast<double>(water_.lastRise()));
  if (water_.settledNow(now) && !isnan(weight)) {
    if (hc.autoWet && hc.wetG > hc.dryG && weight > hc.wetG) {
      LOGI(Mod::Sensor, "Abgetropft: Nasspunkt %.0f g -> %.0f g", static_cast<double>(hc.wetG),
           static_cast<double>(weight));
      hc.wetG = weight;
      changed = true;
    } else {
      LOGI(Mod::Sensor, "Abgetropft: %.0f g (Nasspunkt %.0f g)", static_cast<double>(weight),
           static_cast<double>(hc.wetG));
    }
  }

  const bool tare = pendingTare_ & (1u << sensor);
  if (!(tare || pendingRef_ || !isnan(pendingGrams_)) || !drivers_[sensor])
    return changed;
  if (!pendingAt_.before(jobStart_[sensor])) {
    // Messung lief schon, als die Aktion kam (Gewicht evtl. noch nicht drauf)
    uint32_t wait;
    measureNow(sensor, now, false, wait);
    return changed;
  }
  int32_t counts;
  if (!drivers_[sensor]->lastRawCounts(counts))
    return changed;
  bool remeasure = false;
  if (tare) {
    hc.tare = counts;
    LOGI(Mod::Sensor, "Tariert: Nullpunkt %ld", static_cast<long>(counts));
    changed = remeasure = true;
  }
  if (pendingRef_) {
    refCounts_ = counts;
    hasRef_ = true;
    LOGI(Mod::Sensor, "Waage: Ausgangswert %ld gemerkt, jetzt Gewicht auflegen",
         static_cast<long>(counts));
  } else if (!isnan(pendingGrams_)) {
    const int32_t base = hasRef_ ? refCounts_ : hc.tare;
    const float delta = static_cast<float>(static_cast<int64_t>(counts) - base);
    if (fabsf(delta) < 50.0f) {
      LOGW(Mod::Sensor, "Waage: Rohwert ändert sich kaum (%.0f) - liegt das Gewicht auf?",
           static_cast<double>(delta));
    } else {
      const float scale = delta / pendingGrams_;
      LOGI(Mod::Sensor, "Waage kalibriert mit %.0f g: Skalierung %.3f -> %.3f",
           static_cast<double>(pendingGrams_), static_cast<double>(hc.scale),
           static_cast<double>(scale));
      hc.scale = scale;
      changed = remeasure = true;
    }
    hasRef_ = false;
  }
  pendingTare_ &= ~(1u << sensor);
  pendingRef_ = false;
  pendingGrams_ = NAN;
  if (remeasure) {
    drivers_[sensor]->configure(hc);
    uint32_t wait;
    measureNow(sensor, now, false, wait); // Wert mit neuer Einstellung zeigen
  }
  return changed;
}

ChannelStatus SensorService::status(uint8_t slot, Instant now) const {
  if (slot >= board::slotCount())
    return ChannelStatus::Unknown;
  const uint8_t sensor = board::slot(slot).sensor;
  if (sched_.inStartWarmup(sensor, now))
    return ChannelStatus::Warmup;
  return state_.channel(slot).status;
}

bool SensorService::allMeasuredOnce() const {
  for (uint8_t i = 0; i < board::sensorCount(); i++)
    if (sched_.state(i).enabled && !lastOk_[i].ok)
      return false;
  return true;
}

uint8_t SensorService::processedSensors() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < board::sensorCount(); i++)
    if (sched_.state(i).enabled && lastOk_[i].ok)
      n++;
  return n;
}

void SensorService::onMeasurement(uint8_t sensor, const Reading* values, uint8_t n, Instant now) {
  SensorsConfig& sc = cfg_.sensorsMutable();
  const auto& se = board::sensor(sensor);
  const uint32_t intervalMs = sc.sensors[sensor].intervalS * 1000UL;
  bool configChanged = false;
  bool important = false;
  uint8_t valid = 0;

  for (uint8_t ch = 0; ch < n && ch < se.channels; ch++) {
    const uint8_t slot = static_cast<uint8_t>(se.firstSlot + ch);
    ChannelConfig& cc = sc.channels[slot];
    ChannelState& st = state_.channel(slot);
    if (!cc.enabled)
      continue;
    const ChannelDef def = catalog::channel(se.type, ch);
    float value = values[ch].value;
    const ChannelStatus before = st.status;

    if (se.type == SensorType::Hx711 && ch == 1) {
      // Wasservorrat aus dem Gewicht dieser Messung
      const SensorConfig& hc = sc.sensors[sensor];
      value = waterPercent(values[0].value, hc.dryG, hc.wetG);
    }

    if (def.hasRaw) {
      const int16_t raw = values[ch].raw;
      if (raw < 0 || isnan(value)) {
        value = NAN;
      } else {
        st.raw = raw;
        const int16_t oldMin = st.rawMin, oldMax = st.rawMax;
        st.observeRaw(raw);
        if (st.rawMin != oldMin || st.rawMax != oldMax)
          important = true;
        if (cc.autocal) {
          if (!st.cal.valid())
            st.cal = autocalStart(cc.rawMin, cc.rawMax, st.raw);
          const float alpha = autocalAlpha(cc.halfLifeS, intervalMs);
          if (autocalUpdate(st.cal, raw, alpha, cc.rawMin, cc.rawMax)) {
            // Nur bei geänderter Ganzzahl speichern; der Float-Zustand
            // folgt mit dem normalen 15-min-Takt.
            configChanged = true;
            important = true;
            LOGD(Mod::Sensor, "Autokalibrierung Kanal %u: %d..%d", slot, cc.rawMin, cc.rawMax);
          }
        }
        if (cc.learn && se.type == SensorType::Analog) {
          const float water = currentWater(now);
          if (!isnan(water)) {
            state_.soilFit(ch).add(water, raw);
            LOGD(Mod::Sensor, "%s lernt: Rohwert %d bei %.0f %% Wasservorrat", cc.name.c_str(), raw,
                 static_cast<double>(water));
          }
        }
        bool clamped = false;
        value = analogPercent(raw, cc.rawMin, cc.rawMax, cc.inverted, &clamped);
        // Einmal warnen, bis der Wert wieder im Bereich liegt: ein Lichtsensor
        // liegt nachts stundenlang unter rawMin, das ist kein neuer Befund
        const uint32_t bit = 1UL << slot;
        if (clamped && !cc.autocal) {
          if (!(clampWarned_ & bit))
            LOGW(Mod::Sensor, "Rohwert %d außerhalb %d..%d (Kanal %u) - begrenzt", raw, cc.rawMin,
                 cc.rawMax, slot);
          clampWarned_ |= bit;
        } else {
          clampWarned_ &= ~bit;
        }
      }
    }

    if (isnan(value) || isinf(value) || value < def.validMin || value > def.validMax) {
      st.value = NAN; // nie 0 als Ersatz (B-08)
      st.status = ChannelStatus::Error;
      logResult(slot, NAN, def.hasRaw ? values[ch].raw : -1, before, st.status, false, false);
      continue;
    }
    valid++;
    st.value = value;
    st.status = evaluate(value, cc.limits, def.oneSided);
    const float oldMin = st.obsMin, oldMax = st.obsMax;
    st.observe(value);
    if (st.obsMin != oldMin || st.obsMax != oldMax)
      important = true;
    st.hasOk = true;
    st.lastOk = now;
    logResult(slot, value, def.hasRaw ? st.raw : -1, before, st.status,
              !isnan(oldMin) && st.obsMin < oldMin, !isnan(oldMax) && st.obsMax > oldMax);
  }

  if (valid > 0) {
    lastOk_[sensor].ok = true;
    lastOk_[sensor].at = now;
  }
  if (important)
    state_.markImportant(now);
  else
    state_.markDirty(now);
  if (configChanged)
    cfg_.markSensorsDirty(now);
  // Konfiguration (Tara, Skalierung, Nasspunkt), kein Kanalzustand
  if (se.type == SensorType::Hx711 && afterScale(sensor, n > 0 ? values[0].value : NAN, now))
    cfg_.markSensorsDirty(now);

  if (failuresBefore_[sensor] > 0) {
    LOGI(Mod::Sensor, "%s misst wieder (nach %u Fehlschlägen)", SensorId(sensor).s,
         failuresBefore_[sensor]);
    failuresBefore_[sensor] = 0;
  }
  if (log::enabled(LogLevel::Debug, Mod::Cycle)) {
    uint8_t active = 0;
    for (uint8_t ch = 0; ch < n && ch < se.channels; ch++)
      if (sc.channels[se.firstSlot + ch].enabled)
        active++;
    const int32_t nextS = (sched_.state(sensor).nextDue - now).millis() / 1000;
    LOGD(Mod::Cycle, "%s: fertig in %ld ms, %u von %u Kanälen gültig, nächste Messung in %ld s",
         SensorId(sensor).s, static_cast<long>((now - jobStart_[sensor]).millis()), valid, active,
         static_cast<long>(nextS));
  }
  Event e;
  e.type = EventType::MeasurementDone;
  e.a = sensor;
  events_.push(e);
}

void SensorService::onFailure(uint8_t sensor, Health health, uint8_t failures, bool becameFailed,
                              Instant now) {
  char id[16];
  copyP(id, sizeof(id), catalog::id(board::sensor(sensor).type));
  failuresBefore_[sensor] = failures;
  if (health == Health::Failed) {
    const auto& se = board::sensor(sensor);
    for (uint8_t ch = 0; ch < se.channels; ch++) {
      ChannelState& st = state_.channel(static_cast<uint8_t>(se.firstSlot + ch));
      st.status = ChannelStatus::Error;
      st.value = NAN;
    }
    if (becameFailed) {
      LOGE(Mod::Sensor, "%s dauerhaft fehlerhaft (%u Fehlschläge in Folge)", id, failures);
      Event e;
      e.type = EventType::SensorFailed;
      e.a = sensor;
      events_.push(e);
      if (board::sensor(sensor).type == SensorType::Ds18b20 && restartGate_ &&
          restartGate_(restartCtx_)) {
        LOGW(Mod::Sensor, "DS18B20 hängt - Gerät wird neu gestartet");
        Event r;
        r.type = EventType::RebootRequested;
        r.a = 2;
        events_.push(r);
      }
    }
  } else {
    LOGW(Mod::Sensor, "%s: Messung fehlgeschlagen (%u), neuer Versuch in 5 s", id, failures);
  }
  (void)now;
}

} // namespace ps
