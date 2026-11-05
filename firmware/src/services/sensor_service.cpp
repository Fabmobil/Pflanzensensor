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
    copyP(s, sizeof(s), catalog::id(board::kSensors[sensor].type));
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
  const auto& se = board::kSensors[sensor];
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
  for (uint8_t i = 0; i < board::kSensorCount; i++) {
    drivers_[i] = drivers[i];
    drivers_[i]->configure(c.sensors[i]);
    sched_.add(*drivers_[i], board::kSensors[i].channels, c.sensors[i].intervalS * 1000UL,
               c.sensors[i].enabled);
  }
  // Autokalibrierung: Float-Zustand aus gespeichertem Zustand oder Startwerten
  for (uint8_t s = 0; s < board::kSlotCount; s++) {
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
  if (sensor >= board::kSensorCount)
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
  for (uint8_t i = 0; i < board::kSensorCount; i++) {
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
    const auto& se = board::kSensors[i];
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
  if (slot >= board::kSlotCount)
    return;
  ChannelState& st = state_.channel(slot);
  if (raw)
    st.resetRawObs();
  else
    st.resetObs();
  state_.markImportant(now);
}

Err SensorService::tare(uint8_t sensor, Instant now) {
  if (sensor >= board::kSensorCount || board::kSensors[sensor].type != SensorType::Hx711)
    return Err::Unsupported;
  pendingTare_ |= (1u << sensor);
  uint32_t wait;
  const MeasureResult r = measureNow(sensor, now, false, wait);
  if (r == MeasureResult::Disabled) {
    pendingTare_ &= ~(1u << sensor);
    return Err::Disabled;
  }
  return Err::None; // Busy: die laufende Messung übernimmt den Nullpunkt
}

ChannelStatus SensorService::status(uint8_t slot, Instant now) const {
  if (slot >= board::kSlotCount)
    return ChannelStatus::Unknown;
  const uint8_t sensor = board::kSlots[slot].sensor;
  if (sched_.inStartWarmup(sensor, now))
    return ChannelStatus::Warmup;
  return state_.channel(slot).status;
}

bool SensorService::allMeasuredOnce() const {
  for (uint8_t i = 0; i < board::kSensorCount; i++)
    if (sched_.state(i).enabled && !lastOk_[i].ok)
      return false;
  return true;
}

uint8_t SensorService::processedSensors() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < board::kSensorCount; i++)
    if (sched_.state(i).enabled && lastOk_[i].ok)
      n++;
  return n;
}

void SensorService::onMeasurement(uint8_t sensor, const Reading* values, uint8_t n, Instant now) {
  SensorsConfig& sc = cfg_.sensorsMutable();
  const auto& se = board::kSensors[sensor];
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

  if ((pendingTare_ & (1u << sensor)) && drivers_[sensor]) {
    int32_t rawCounts;
    if (drivers_[sensor]->lastRawCounts(rawCounts)) {
      sc.sensors[sensor].tare = rawCounts;
      drivers_[sensor]->configure(sc.sensors[sensor]);
      cfg_.markSensorsDirty(now);
      LOGI(Mod::Sensor, "Tariert: Nullpunkt %ld", static_cast<long>(rawCounts));
    }
    pendingTare_ &= ~(1u << sensor);
  }

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
  copyP(id, sizeof(id), catalog::id(board::kSensors[sensor].type));
  failuresBefore_[sensor] = failures;
  if (health == Health::Failed) {
    const auto& se = board::kSensors[sensor];
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
      if (board::kSensors[sensor].type == SensorType::Ds18b20 && restartGate_ &&
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
