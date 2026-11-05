/**
 * @file scheduler.h
 * @brief Zentraler Mess-Scheduler (refactor.md 2.4 und 5.6)
 * @details Kennt alle Sensoren, ihre nächste Fälligkeit und führt
 *          Messaufträge aus: Prepare (begin/wake/Warmup) → Sample (Runden) →
 *          Finish (Median, sleep). Geteilte Hardware wird als Ressourcenmaske
 *          nur während einer Runde belegt, nie beim Aufwärmen. Es gibt genau
 *          eine Frist pro Auftrag; wer sie reißt, wird abgebrochen und gibt
 *          seine Ressourcen frei - einen "verlorenen Slot" gibt es nicht.
 *
 *          Arduino-frei, Speicher statisch (Template-Parameter aus dem
 *          Board-Profil).
 */
#pragma once

#include "core/sensors/driver.h"

namespace ps {

enum class Health : uint8_t { Ok, Degraded, Failed };
enum class MeasureNowResult : uint8_t { Accepted, Busy, NotReady, Disabled, NotFound };

/// Empfänger der Messergebnisse (SensorService).
class MeasurementListener {
public:
  /// Messung fertig; values hat `n` Einträge (NaN = Kanal ungültig).
  virtual void onMeasurement(uint8_t sensor, const Reading* values, uint8_t n, Instant now) = 0;
  /// Auftrag fehlgeschlagen. becameFailed: gerade nach Failed gewechselt.
  virtual void onFailure(uint8_t sensor, Health health, uint8_t failures, bool becameFailed,
                         Instant now) = 0;
  /// Auftrag beginnt (nur fürs Log). priority: Sofortmessung
  virtual void onJobStart(uint8_t sensor, bool priority, Instant now) {
    (void)sensor, (void)priority, (void)now;
  }
  /// Einzelrunden vor dem Median (nur fürs Log): samples[runde * stride + kanal]
  virtual void onRounds(uint8_t sensor, const Reading* samples, uint8_t rounds, uint8_t stride,
                        uint8_t n) {
    (void)sensor, (void)samples, (void)rounds, (void)stride, (void)n;
  }

protected:
  ~MeasurementListener() = default;
};

/// Öffentlich lesbarer Zustand eines Sensors.
struct SensorRunState {
  bool enabled = true;
  bool active = false; ///< Auftrag läuft
  Health health = Health::Ok;
  uint8_t failures = 0;
  uint8_t backoffLevel = 0;
  uint32_t intervalMs = 60000;
  Instant nextDue;
  Instant startWarmupUntil;
  bool startWarmupDone = true;
};

template <uint8_t MaxSensors, uint8_t MaxChannels, uint8_t MaxRounds = 3>
class MeasurementScheduler {
public:
  static constexpr uint8_t kRetryFailures = 5; ///< ab hier Failed
  static constexpr int32_t kRetryDelayMs = 5000;
  static constexpr int32_t kMaxBackoffMs = 30 * 60 * 1000;
  static constexpr uint8_t kMaxBackoffLevel = 8;
  static constexpr int32_t kStartStaggerMs = 2000;
  static constexpr int32_t kDeadlineSlackMs = 5000;

  explicit MeasurementScheduler(MeasurementListener& listener) : listener_(listener) {}

  /// Sensor anmelden (in Reihenfolge der Sensorindizes).
  /// @param channels Kanalplätze laut Layout
  bool add(SensorDriver& d, uint8_t channels, uint32_t intervalMs, bool enabled) {
    if (count_ >= MaxSensors)
      return false;
    Slot& s = slots_[count_++];
    s.driver = &d;
    s.info = d.info();
    s.channels = channels > MaxChannels ? MaxChannels : channels;
    if (s.info.rounds == 0)
      s.info.rounds = 1;
    if (s.info.rounds > MaxRounds)
      s.info.rounds = MaxRounds;
    s.st.intervalMs = intervalMs ? intervalMs : 60000;
    s.st.enabled = enabled;
    return true;
  }

  /// Startzeitpunkt setzen: Sensoren werden um je 2 s versetzt fällig.
  void start(Instant now) {
    for (uint8_t i = 0; i < count_; i++) {
      Slot& s = slots_[i];
      s.st.nextDue = now + Duration::ms(kStartStaggerMs * i);
      s.st.startWarmupUntil = now + Duration::ms(static_cast<int32_t>(s.info.startWarmupMs));
      s.st.startWarmupDone = s.info.startWarmupMs == 0;
      if (!s.st.startWarmupDone && s.st.startWarmupUntil.reached(s.st.nextDue))
        s.st.nextDue = s.st.startWarmupUntil;
      s.needsBegin = true;
    }
    started_ = true;
  }

  uint8_t count() const { return count_; }
  const SensorRunState& state(uint8_t i) const { return slots_[i].st; }
  uint8_t busyResources() const { return busy_; }

  bool inStartWarmup(uint8_t i, Instant now) const {
    if (i >= count_)
      return false;
    const Slot& s = slots_[i];
    return !s.st.startWarmupDone && now.before(s.st.startWarmupUntil);
  }

  /// Restliche Start-Warmup-Zeit in ms (0, wenn keine).
  uint32_t warmupRemainingMs(uint8_t i, Instant now) const {
    if (!inStartWarmup(i, now))
      return 0;
    return static_cast<uint32_t>((slots_[i].st.startWarmupUntil - now).millis());
  }

  /// Sofortmessung: Priorität, kein Abbruch.
  MeasureNowResult measureNow(uint8_t i, Instant now) {
    if (i >= count_)
      return MeasureNowResult::NotFound;
    Slot& s = slots_[i];
    if (!s.st.enabled)
      return MeasureNowResult::Disabled;
    if (s.phase != Phase::Idle)
      return MeasureNowResult::Busy;
    if (inStartWarmup(i, now))
      return MeasureNowResult::NotReady;
    s.priority = true;
    return MeasureNowResult::Accepted;
  }

  void setEnabled(uint8_t i, bool enabled, Instant now) {
    if (i >= count_)
      return;
    Slot& s = slots_[i];
    if (s.st.enabled == enabled)
      return;
    s.st.enabled = enabled;
    if (!enabled) {
      if (s.phase != Phase::Idle)
        abortJob(s);
      s.priority = false;
    } else {
      s.needsBegin = true;
      s.st.health = Health::Ok;
      s.st.failures = 0;
      s.st.backoffLevel = 0;
      s.st.nextDue = now;
    }
  }

  void setInterval(uint8_t i, uint32_t ms, Instant now) {
    if (i >= count_ || ms == 0)
      return;
    Slot& s = slots_[i];
    s.st.intervalMs = ms;
    const Instant latest = now + Duration::ms(static_cast<int32_t>(ms));
    if (s.st.nextDue.before(now) == false && latest.before(s.st.nextDue))
      s.st.nextDue = latest;
  }

  /// Alle Aufträge abbrechen und keine neuen starten (OTA).
  void pause(bool paused) {
    paused_ = paused;
    if (paused)
      for (uint8_t i = 0; i < count_; i++)
        if (slots_[i].phase != Phase::Idle)
          abortJob(slots_[i]);
  }
  bool paused() const { return paused_; }

  /// Alle 100 ms aus der Hauptschleife.
  void tick(Instant now) {
    if (!started_ || paused_)
      return;
    // 1. Fristen
    for (uint8_t i = 0; i < count_; i++) {
      Slot& s = slots_[i];
      if (s.phase != Phase::Idle && now.reached(s.deadline)) {
        abortJob(s);
        fail(i, now);
      }
    }
    // 2. Laufende Aufträge weiterschalten
    for (uint8_t i = 0; i < count_; i++)
      if (slots_[i].phase != Phase::Idle)
        advance(i, now);
    // 3. Neue Aufträge: Priorität vor Fälligkeit
    for (uint8_t pass = 0; pass < 2; pass++) {
      for (;;) {
        int best = -1;
        for (uint8_t i = 0; i < count_; i++) {
          Slot& s = slots_[i];
          if (s.phase != Phase::Idle || !s.st.enabled)
            continue;
          if (inStartWarmup(i, now))
            continue;
          if (!s.st.startWarmupDone)
            s.st.startWarmupDone = true;
          const bool want = pass == 0 ? s.priority : now.reached(s.st.nextDue);
          if (!want)
            continue;
          if (best < 0 || s.st.nextDue.before(slots_[best].st.nextDue))
            best = i;
        }
        if (best < 0)
          break;
        startJob(static_cast<uint8_t>(best), now, pass == 0);
        advance(static_cast<uint8_t>(best), now);
      }
    }
  }

private:
  enum class Phase : uint8_t { Idle, Begin, Wake, Warmup, WaitRound, InRound, Finish };

  struct Slot {
    SensorDriver* driver = nullptr;
    SensorInfo info{};
    SensorRunState st;
    Phase phase = Phase::Idle;
    bool needsBegin = true;
    bool priority = false;
    bool holding = false;
    bool woke = false;
    uint8_t channels = 0;
    uint8_t round = 0;
    Instant scheduled;
    Instant deadline;
    Instant waitUntil;
    Instant roundDeadline;
    Reading samples[MaxRounds * MaxChannels];
  };

  void startJob(uint8_t i, Instant now, bool byPriority) {
    Slot& s = slots_[i];
    s.priority = false;
    s.st.active = true;
    s.scheduled = byPriority ? now : s.st.nextDue;
    const int32_t budget = static_cast<int32_t>(s.info.prepareWarmupMs) +
                           s.info.rounds * (static_cast<int32_t>(s.info.roundDelayMs) +
                                            static_cast<int32_t>(s.info.roundTimeoutMs)) +
                           kDeadlineSlackMs;
    s.deadline = now + Duration::ms(budget);
    s.round = 0;
    s.woke = false;
    for (auto& r : s.samples)
      r = Reading{};
    s.phase = s.needsBegin ? Phase::Begin : Phase::Wake;
    listener_.onJobStart(i, byPriority, now);
  }

  void release(Slot& s) {
    if (s.holding) {
      busy_ = static_cast<uint8_t>(busy_ & ~s.info.resources);
      s.holding = false;
    }
  }

  void abortJob(Slot& s) {
    release(s);
    if (s.woke)
      s.driver->sleep();
    s.phase = Phase::Idle;
    s.st.active = false;
  }

  void nextRound(Slot& s, Instant now) {
    release(s);
    s.round++;
    if (s.round >= s.info.rounds) {
      s.phase = Phase::Finish;
    } else {
      s.waitUntil = now + Duration::ms(s.info.roundDelayMs);
      s.phase = Phase::WaitRound;
    }
  }

  void advance(uint8_t i, Instant now) {
    Slot& s = slots_[i];
    const uint8_t stride = MaxChannels;
    for (uint8_t guard = 0; guard < 8 && s.phase != Phase::Idle; guard++) {
      const Phase before = s.phase;
      switch (s.phase) {
      case Phase::Begin: {
        const Step r = s.driver->begin(now);
        if (r == Step::Pending)
          return;
        if (r == Step::Error) {
          abortJob(s);
          fail(i, now);
          return;
        }
        s.needsBegin = false;
        s.phase = Phase::Wake;
        break;
      }
      case Phase::Wake: {
        const Step r = s.driver->wake(now);
        if (r == Step::Pending)
          return;
        s.woke = true;
        if (r == Step::Error) {
          abortJob(s);
          fail(i, now);
          return;
        }
        s.waitUntil = now + Duration::ms(static_cast<int32_t>(s.info.prepareWarmupMs));
        s.phase = Phase::Warmup;
        break;
      }
      case Phase::Warmup:
        if (!now.reached(s.waitUntil))
          return;
        s.waitUntil = now;
        s.phase = Phase::WaitRound;
        break;
      case Phase::WaitRound: {
        if (!now.reached(s.waitUntil))
          return;
        if (busy_ & s.info.resources)
          return; // Ressource belegt: später
        busy_ = static_cast<uint8_t>(busy_ | s.info.resources);
        s.holding = true;
        const Step r = s.driver->startRound(now);
        if (r == Step::Error) {
          nextRound(s, now);
          break;
        }
        s.roundDeadline = now + Duration::ms(s.info.roundTimeoutMs);
        s.phase = Phase::InRound;
        break;
      }
      case Phase::InRound: {
        Reading* out = &s.samples[s.round * stride];
        const uint8_t n = channels(s);
        const Step r = s.driver->pollRound(now, out, n);
        if (r == Step::Pending) {
          if (!now.reached(s.roundDeadline))
            return;
          for (uint8_t c = 0; c < n; c++)
            out[c] = Reading{};
        } else if (r == Step::Error) {
          for (uint8_t c = 0; c < n; c++)
            out[c] = Reading{};
        }
        nextRound(s, now);
        break;
      }
      case Phase::Finish:
        finish(i, now);
        return;
      case Phase::Idle:
        return;
      }
      if (s.phase == before)
        return;
    }
  }

  uint8_t channels(const Slot& s) const { return s.channels; }

  void finish(uint8_t i, Instant now) {
    Slot& s = slots_[i];
    Reading agg[MaxChannels];
    const uint8_t n = channels(s);
    listener_.onRounds(i, s.samples, s.info.rounds, MaxChannels, n);
    uint8_t valid = 0;
    for (uint8_t c = 0; c < n; c++) {
      agg[c] = aggregateChannel(s.samples, s.info.rounds, MaxChannels, c);
      if (!isnan(agg[c].value))
        valid++;
    }
    s.driver->sleep();
    s.phase = Phase::Idle;
    s.st.active = false;
    if (valid == 0) {
      fail(i, now);
      return;
    }
    s.st.failures = 0;
    s.st.backoffLevel = 0;
    s.st.health = Health::Ok;
    // Taktung ohne Drift; kein Nachholen bei mehr als einem Intervall Verzug
    const Duration iv = Duration::ms(static_cast<int32_t>(s.st.intervalMs));
    Instant next = s.scheduled + iv;
    if ((now - s.scheduled) > iv)
      next = now + iv;
    s.st.nextDue = next;
    listener_.onMeasurement(i, agg, n, now);
  }

  void fail(uint8_t i, Instant now) {
    Slot& s = slots_[i];
    s.phase = Phase::Idle;
    s.st.active = false;
    s.needsBegin = true;
    if (s.st.failures < 255)
      s.st.failures++;
    bool becameFailed = false;
    if (s.st.failures < kRetryFailures) {
      s.st.health = Health::Degraded;
      s.st.nextDue = now + Duration::ms(kRetryDelayMs);
    } else {
      becameFailed = s.st.health != Health::Failed;
      s.st.health = Health::Failed;
      if (s.st.backoffLevel < kMaxBackoffLevel)
        s.st.backoffLevel++;
      int64_t delay = static_cast<int64_t>(s.st.intervalMs) << (s.st.backoffLevel - 1);
      if (delay > kMaxBackoffMs)
        delay = kMaxBackoffMs;
      s.st.nextDue = now + Duration::ms(delay);
    }
    listener_.onFailure(i, s.st.health, s.st.failures, becameFailed, now);
  }

  MeasurementListener& listener_;
  Slot slots_[MaxSensors];
  uint8_t count_ = 0;
  uint8_t busy_ = 0;
  bool started_ = false;
  bool paused_ = false;
};

} // namespace ps
