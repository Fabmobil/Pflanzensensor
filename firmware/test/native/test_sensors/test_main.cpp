/**
 * @file test_main.cpp
 * @brief Mess-Scheduler, Aggregation, Autokalibrierung, Analog-Mapping,
 *        SensorService (refactor.md 5.6, AP-3)
 */
#include <vector>

#include "core/sensors/analog_mapping.h"
#include "core/sensors/autocal.h"
#include "core/sensors/protocols.h"
#include "core/sensors/scheduler.h"
#include "core/sensors/water.h"
#include "services/sensor_service.h"
#include "test_support.h"

using namespace ps;

namespace {

/// Steuerbarer Treiber
class FakeDriver final : public SensorDriver {
public:
  FakeDriver(SensorType t, uint8_t res, uint16_t roundDelay, uint32_t startWarmup = 0,
             uint32_t prepareWarmup = 0, uint8_t ch = 2)
      : t_(t), res_(res), delay_(roundDelay), sw_(startWarmup), pw_(prepareWarmup), ch_(ch) {}
  SensorType type() const override { return t_; }
  SensorInfo info() const override {
    SensorInfo i = catalog::info(t_);
    i.resources = res_;
    i.roundDelayMs = delay_;
    i.roundTimeoutMs = 500;
    i.startWarmupMs = sw_;
    i.prepareWarmupMs = pw_;
    i.rounds = 3;
    return i;
  }
  uint8_t channelCount() const override { return ch_; }
  Step begin(Instant) override {
    begins++;
    return failBegin ? Step::Error : Step::Ok;
  }
  Step wake(Instant) override {
    wakes++;
    return Step::Ok;
  }
  Step startRound(Instant now) override {
    roundStarts.push_back(now.millis());
    roundStartedAt = now;
    return Step::Ok;
  }
  Step pollRound(Instant now, Reading* out, uint8_t n) override {
    if (hang)
      return Step::Pending;
    if ((now - roundStartedAt).millis() < roundTime)
      return Step::Pending;
    polls++;
    for (uint8_t i = 0; i < n; i++) {
      if (i == deadChannel)
        continue;
      out[i].value = values[i % 4] + static_cast<float>(polls % 3);
      out[i].raw = static_cast<int16_t>(values[i % 4]);
    }
    if (allInvalid)
      for (uint8_t i = 0; i < n; i++)
        out[i] = Reading{};
    return Step::Ok;
  }
  void sleep() override { sleeps++; }

  SensorType t_;
  uint8_t res_;
  uint16_t delay_;
  uint32_t sw_, pw_;
  uint8_t ch_;
  int begins = 0, wakes = 0, sleeps = 0, polls = 0;
  bool failBegin = false, hang = false, allInvalid = false;
  int deadChannel = -1;
  int32_t roundTime = 0;
  float values[4] = {10, 20, 30, 40};
  Instant roundStartedAt;
  std::vector<uint32_t> roundStarts;
};

struct Listener : MeasurementListener {
  void onMeasurement(uint8_t s, const Reading* v, uint8_t n, Instant now) override {
    done.push_back(s);
    doneAt.push_back(now.millis());
    last.assign(v, v + n);
  }
  void onFailure(uint8_t s, Health h, uint8_t f, bool became, Instant) override {
    failures.push_back(s);
    lastHealth = h;
    if (became)
      becameFailed++;
  }
  std::vector<uint8_t> done, failures;
  std::vector<uint32_t> doneAt;
  std::vector<Reading> last;
  Health lastHealth = Health::Ok;
  int becameFailed = 0;
};

using Sched = MeasurementScheduler<3, 4>;

void runFor(Sched& s, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 100) {
    native::advance(100);
    s.tick(hal::now());
  }
}

} // namespace

void setUp() { native::setMillis(1000); }
void tearDown() {}

void test_median() {
  float a[] = {5, 1, 3};
  TEST_ASSERT_EQUAL_FLOAT(3, medianOf(a, 3));
  float b[] = {4, 2};
  TEST_ASSERT_EQUAL_FLOAT(3, medianOf(b, 2));
  Reading r[3 * 2];
  r[0].value = 1;
  r[2].value = 3; // Runde 1, Kanal 0
  r[4].value = 2; // Runde 2, Kanal 0
  TEST_ASSERT_EQUAL_FLOAT(2, aggregateChannel(r, 3, 2, 0).value);
  TEST_ASSERT_TRUE(isnan(aggregateChannel(r, 3, 2, 1).value));
}

void test_analog_mapping() {
  TEST_ASSERT_EQUAL_FLOAT(50, analogPercent(500, 250, 750, false));
  TEST_ASSERT_EQUAL_FLOAT(50, analogPercent(500, 250, 750, true));
  TEST_ASSERT_EQUAL_FLOAT(100, analogPercent(250, 250, 750, true));
  bool clamped = false;
  TEST_ASSERT_EQUAL_FLOAT(100, analogPercent(900, 250, 750, false, &clamped));
  TEST_ASSERT_TRUE(clamped);
  TEST_ASSERT_EQUAL_FLOAT(0, analogPercent(300, 300, 300, false));
}

void test_autocal() {
  AutoCalState s = autocalStart(250, 750, 800);
  TEST_ASSERT_EQUAL_FLOAT(800, s.max);
  int16_t mn = 250, mx = 750;
  TEST_ASSERT_TRUE(autocalUpdate(s, 100, 0.0f, mn, mx)); // sofort erweitert
  TEST_ASSERT_EQUAL(100, mn);
  TEST_ASSERT_EQUAL(800, mx);
  // Halbwertszeit: nach n Schritten halber Abstand
  const float a = autocalAlpha(3600, 60000); // n = 60
  AutoCalState h;
  h.min = 0;
  h.max = 1000;
  for (int i = 0; i < 60; i++)
    autocalUpdate(h, 500, a, mn, mx);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 250, h.min);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 750, h.max);
  TEST_ASSERT_EQUAL_FLOAT(1e-4f, autocalAlpha(0, 60000));
}

void test_protocols() {
  uint8_t cmd[19];
  proto::sdsQuery(cmd);
  TEST_ASSERT_EQUAL_HEX8(0xAA, cmd[0]);
  TEST_ASSERT_EQUAL_HEX8(0x04, cmd[2]);
  TEST_ASSERT_EQUAL_HEX8(0x02, cmd[17]); // 0x04 + 0xFF + 0xFF = 0x202
  TEST_ASSERT_EQUAL_HEX8(0xAB, cmd[18]);
  proto::SdsParser p;
  const uint8_t frame[] = {0x00, 0xAA, 0xC0, 0xD4, 0x04, 0x3A, 0x0A, 0xA1, 0x60, 0x1D, 0xAB};
  bool got = false;
  for (uint8_t b : frame)
    got = p.feed(b) || got;
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL_FLOAT(123.6f, p.pm25());
  TEST_ASSERT_EQUAL_FLOAT(261.8f, p.pm10());
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 2000.0f, proto::mhz19Ppm(402000, 602000, 5000));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 800.0f, proto::mhz19Ppm(402000, 602000, 2000));
  TEST_ASSERT_TRUE(isnan(proto::mhz19Ppm(1000, 1000, 5000)));
  float v;
  TEST_ASSERT_TRUE(proto::parseValueLine(" 12.5\r", v));
  TEST_ASSERT_EQUAL_FLOAT(12.5f, v);
  TEST_ASSERT_FALSE(proto::parseValueLine("abc", v));
  TEST_ASSERT_EQUAL(7, proto::muxAddress(0));
  TEST_ASSERT_EQUAL(0, proto::muxAddress(7));
}

void test_dht_decode() {
  float t = 0, h = 0;
  // DHT11: 59,5 % / 18,5 °C (Werte vom Gerät), Prüfsumme 59+5+18+5 = 87
  const uint8_t d11[5] = {59, 5, 18, 5, 87};
  TEST_ASSERT_TRUE(proto::dhtDecode(d11, false, t, h));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 59.5f, h);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.5f, t);
  // DHT11 negativ: Bit 7 im Nachkommabyte
  const uint8_t d11n[5] = {40, 0, 3, 0x82, static_cast<uint8_t>(40 + 3 + 0x82)};
  TEST_ASSERT_TRUE(proto::dhtDecode(d11n, false, t, h));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -3.2f, t);
  // DHT22: 65,2 % = 0x028C, 35,1 °C = 0x015F
  const uint8_t d22[5] = {0x02, 0x8C, 0x01, 0x5F, static_cast<uint8_t>(0x02 + 0x8C + 0x01 + 0x5F)};
  TEST_ASSERT_TRUE(proto::dhtDecode(d22, true, t, h));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.2f, h);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 35.1f, t);
  // DHT22 negativ: -10,1 °C = 0x8065
  const uint8_t d22n[5] = {0x02, 0x8C, 0x80, 0x65, static_cast<uint8_t>(0x02 + 0x8C + 0x80 + 0x65)};
  TEST_ASSERT_TRUE(proto::dhtDecode(d22n, true, t, h));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.1f, t);
  // Prüfsumme falsch, Leitung tot (alles 0), Überlauf der Summe
  const uint8_t bad[5] = {59, 5, 18, 5, 88};
  TEST_ASSERT_FALSE(proto::dhtDecode(bad, false, t, h));
  const uint8_t zero[5] = {0, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(proto::dhtDecode(zero, false, t, h));
  const uint8_t wrap[5] = {200, 0, 100, 0, static_cast<uint8_t>(300 & 0xFF)};
  TEST_ASSERT_TRUE(proto::dhtDecode(wrap, false, t, h));
}

void test_parallel_no_conflict() {
  Listener l;
  Sched s(l);
  FakeDriver analog(SensorType::Analog, ResAdc, 300);
  FakeDriver dht(SensorType::Dht, ResNone, 1000);
  FakeDriver analog2(SensorType::Analog, ResAdc, 300);
  analog.roundTime = 50;
  analog2.roundTime = 50;
  s.add(analog, 2, 60000, true);
  s.add(dht, 2, 60000, true);
  s.add(analog2, 2, 60000, true);
  s.start(hal::now());
  // alle sofort fällig machen
  uint32_t w;
  (void)w;
  s.measureNow(0, hal::now());
  s.measureNow(1, hal::now());
  s.measureNow(2, hal::now());
  runFor(s, 5000);
  TEST_ASSERT_EQUAL(3, l.done.size());
  // Zwei Adc-Sensoren nie gleichzeitig in einer Runde
  for (uint32_t a : analog.roundStarts)
    for (uint32_t b : analog2.roundStarts)
      TEST_ASSERT_TRUE(a + 50 <= b || b + 50 <= a);
  TEST_ASSERT_EQUAL(0, s.busyResources());
}

void test_warmup_does_not_block() {
  Listener l;
  Sched s(l);
  FakeDriver sds(SensorType::Sds011, ResSoftUart, 1000, 0, 30000);
  FakeDriver ser(SensorType::SerialReceiver, ResSoftUart, 0);
  s.add(sds, 2, 600000, true);
  s.add(ser, 2, 60000, true);
  s.start(hal::now());
  s.measureNow(0, hal::now());
  runFor(s, 200);
  s.measureNow(1, hal::now());
  runFor(s, 2000);
  // Serial Receiver ist fertig, obwohl SDS011 noch aufwärmt (gleiche Ressource)
  TEST_ASSERT_EQUAL(1, l.done.size());
  TEST_ASSERT_EQUAL(1, l.done[0]);
  runFor(s, 40000);
  TEST_ASSERT_EQUAL(2, l.done.size());
  TEST_ASSERT_EQUAL(1, sds.sleeps);
  TEST_ASSERT_EQUAL(1, sds.wakes);
}

void test_deadline_releases_resources() {
  Listener l;
  Sched s(l);
  FakeDriver a(SensorType::Analog, ResAdc, 300);
  a.hang = true;
  s.add(a, 2, 60000, true);
  s.start(hal::now());
  s.measureNow(0, hal::now());
  runFor(s, 300);
  TEST_ASSERT_NOT_EQUAL(0, s.busyResources() | 1);
  runFor(s, 10000);
  TEST_ASSERT_EQUAL(0, s.busyResources());
  TEST_ASSERT_TRUE(l.failures.size() >= 1);
  TEST_ASSERT_TRUE(a.sleeps >= 1);
}

void test_measure_now_busy_and_warmup() {
  Listener l;
  Sched s(l);
  FakeDriver co2(SensorType::Mhz19, ResNone, 2000, 180000);
  FakeDriver dht(SensorType::Dht, ResNone, 1000);
  s.add(co2, 1, 60000, true);
  s.add(dht, 2, 60000, true);
  s.start(hal::now());
  TEST_ASSERT_EQUAL(MeasureNowResult::NotReady, s.measureNow(0, hal::now()));
  TEST_ASSERT_TRUE(s.warmupRemainingMs(0, hal::now()) > 170000);
  TEST_ASSERT_EQUAL(MeasureNowResult::Accepted, s.measureNow(1, hal::now()));
  runFor(s, 100);
  TEST_ASSERT_EQUAL(MeasureNowResult::Busy, s.measureNow(1, hal::now()));
  runFor(s, 5000);
  TEST_ASSERT_EQUAL(MeasureNowResult::Accepted, s.measureNow(1, hal::now()));
  // kein Selbstblockieren: die Sofortmessung kommt durch
  runFor(s, 5000);
  TEST_ASSERT_TRUE(l.done.size() >= 2);
  // nach dem Start-Warmup misst der MH-Z19
  runFor(s, 180000);
  bool co2Done = false;
  for (auto d : l.done)
    co2Done |= d == 0;
  TEST_ASSERT_TRUE(co2Done);
}

void test_backoff_and_recovery() {
  Listener l;
  Sched s(l);
  FakeDriver d(SensorType::Dht, ResNone, 1000);
  d.failBegin = true;
  s.add(d, 2, 60000, true);
  s.start(hal::now());
  runFor(s, 4 * 5000 + 1000);
  TEST_ASSERT_EQUAL(5, d.begins); // Start + 4 Wiederholungen nach je 5 s
  TEST_ASSERT_EQUAL(Health::Failed, s.state(0).health);
  TEST_ASSERT_EQUAL(1, l.becameFailed);
  // Backoff: nächster Versuch nach Intervall × 2^0 = 60 s
  const int before = d.begins;
  runFor(s, 59000);
  TEST_ASSERT_EQUAL(before, d.begins);
  runFor(s, 2000);
  TEST_ASSERT_EQUAL(before + 1, d.begins);
  TEST_ASSERT_EQUAL(2, s.state(0).backoffLevel);
  // Erholung
  d.failBegin = false;
  runFor(s, 125000);
  TEST_ASSERT_EQUAL(Health::Ok, s.state(0).health);
  TEST_ASSERT_EQUAL(0, s.state(0).failures);
}

void test_channel_failure_keeps_others() {
  Listener l;
  Sched s(l);
  FakeDriver d(SensorType::Dht, ResNone, 100);
  d.deadChannel = 0;
  s.add(d, 2, 60000, true);
  s.start(hal::now());
  s.measureNow(0, hal::now());
  runFor(s, 2000);
  TEST_ASSERT_EQUAL(1, l.done.size());
  TEST_ASSERT_TRUE(isnan(l.last[0].value));
  TEST_ASSERT_FALSE(isnan(l.last[1].value)); // Index verschiebt sich nicht (B-09)
}

void test_no_drift_and_wrap() {
  native::setMillis(0xFFFFFFFFu - 5UL * 60000); // Überlauf mitten im Lauf
  Listener l;
  Sched s(l);
  FakeDriver d(SensorType::Dht, ResNone, 100);
  d.roundTime = 30;
  s.add(d, 2, 10000, true);
  s.start(hal::now());
  const uint32_t start = hal::now().millis();
  runFor(s, 1000 * 10000 + 500);
  TEST_ASSERT_TRUE(l.done.size() >= 999 && l.done.size() <= 1001);
  // Start der letzten Messung liegt auf dem Raster (keine Drift)
  const uint32_t lastStart = d.roundStarts[d.roundStarts.size() - 3];
  TEST_ASSERT_EQUAL_UINT32(0, (lastStart - start) % 10000 / 100);
}

void test_disable_enable_runtime() {
  Listener l;
  Sched s(l);
  FakeDriver d(SensorType::Dht, ResNone, 100);
  s.add(d, 2, 60000, false); // beim Start aus
  s.start(hal::now());
  runFor(s, 5000);
  TEST_ASSERT_EQUAL(0, l.done.size());
  TEST_ASSERT_EQUAL(MeasureNowResult::Disabled, s.measureNow(0, hal::now()));
  s.setEnabled(0, true, hal::now()); // zur Laufzeit an (B-11)
  runFor(s, 2000);
  TEST_ASSERT_EQUAL(1, l.done.size());
  s.measureNow(0, hal::now());
  runFor(s, 100);
  s.setEnabled(0, false, hal::now()); // laufender Auftrag sauber beendet
  TEST_ASSERT_FALSE(s.state(0).active);
  TEST_ASSERT_TRUE(d.sleeps >= 2);
}

void test_all_invalid_is_failure() {
  Listener l;
  Sched s(l);
  FakeDriver d(SensorType::Dht, ResNone, 100);
  d.allInvalid = true;
  s.add(d, 2, 60000, true);
  s.start(hal::now());
  s.measureNow(0, hal::now());
  runFor(s, 2000);
  TEST_ASSERT_EQUAL(0, l.done.size());
  TEST_ASSERT_EQUAL(1, l.failures.size());
}

// ---------------------------------------------------------------------------
// SensorService mit allen Sensoren des Testprofils
// ---------------------------------------------------------------------------
void test_sensor_service() {
  native::MemFs fs;
  TemplateStore tpl(fs);
  StateStore state(fs);
  ConfigService cfg(fs, tpl, state);
  cfg.load();
  Events ev;
  SensorService svc(cfg, state, ev);
  std::vector<FakeDriver*> fakes;
  SensorDriver* drivers[board::sensorCount()];
  for (uint8_t i = 0; i < board::sensorCount(); i++) {
    const SensorType t = board::sensor(i).type;
    auto* f = new FakeDriver(t, ResNone, 100, 0, 0, board::sensor(i).channels);
    if (t == SensorType::Analog) {
      f->values[0] = 500;
      f->values[1] = 1023;
    }
    if (t == SensorType::Dht)
      f->failBegin = true; // DHT steckt nicht (B-10)
    fakes.push_back(f);
    drivers[i] = f;
  }
  svc.begin(drivers, hal::now());
  const int a = SensorService::sensorByName("ANALOG");
  const int dht = SensorService::sensorByName("DHT");
  uint32_t wait;
  TEST_ASSERT_EQUAL(MeasureResult::Accepted,
                    svc.measureNow(static_cast<uint8_t>(a), hal::now(), true, wait));
  TEST_ASSERT_EQUAL(MeasureResult::Throttled,
                    svc.measureNow(static_cast<uint8_t>(dht), hal::now(), true, wait));
  for (int i = 0; i < 30; i++) {
    native::advance(100);
    svc.tick(hal::now());
  }
  const uint8_t s0 = board::sensor(a).firstSlot;
  // Lichtstärke: Roh 500 → (500-5)/(1018)·100; Bodenfeuchte invertiert 1023 → begrenzt → 0 %
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 48.6f, svc.channel(s0).value);
  TEST_ASSERT_EQUAL_FLOAT(0, svc.channel(s0 + 1).value);
  TEST_ASSERT_EQUAL(ChannelStatus::Red, svc.status(s0 + 1, hal::now()));
  TEST_ASSERT_EQUAL(500, svc.channel(s0).raw);
  TEST_ASSERT_TRUE(svc.measuredOnce(static_cast<uint8_t>(a)));
  // DHT bleibt im Plan und hat Fehlerzählung, aber keinen Wert (≠ 0, B-08)
  TEST_ASSERT_FALSE(svc.measuredOnce(static_cast<uint8_t>(dht)));
  TEST_ASSERT_TRUE(isnan(svc.channel(board::sensor(dht).firstSlot).value));
  TEST_ASSERT_TRUE(svc.run(static_cast<uint8_t>(dht)).failures >= 1);
  Event e;
  bool done = false;
  while (ev.pop(e))
    done |= e.type == EventType::MeasurementDone;
  TEST_ASSERT_TRUE(done);
  // Extrema zurücksetzen
  TEST_ASSERT_TRUE(svc.channel(s0).hasObs());
  svc.resetExtrema(s0, false, hal::now());
  TEST_ASSERT_FALSE(svc.channel(s0).hasObs());
  for (auto* f : fakes)
    delete f;
}

// ---------------------------------------------------------------------------
// Wasservorrat, Kalibrierpunkte, Gießerkennung, Lernen an der Waage
// ---------------------------------------------------------------------------
void test_water_percent() {
  TEST_ASSERT_EQUAL_FLOAT(50, waterPercent(2500, 2000, 3000));
  TEST_ASSERT_EQUAL_FLOAT(0, waterPercent(1500, 2000, 3000));   // unter trocken
  TEST_ASSERT_EQUAL_FLOAT(100, waterPercent(3300, 2000, 3000)); // frisch gegossen
  TEST_ASSERT_TRUE(isnan(waterPercent(2500, 0, 0)));            // nicht eingerichtet
  TEST_ASSERT_TRUE(isnan(waterPercent(2500, 3000, 2000)));      // vertauscht
  TEST_ASSERT_TRUE(isnan(waterPercent(NAN, 2000, 3000)));
}

void test_analog_set_point() {
  ChannelConfig c;
  c.rawMin = 250;
  c.rawMax = 750;
  c.inverted = true; // Bodenfeuchte: nass = kleiner Rohwert
  c.autocal = true;
  TEST_ASSERT_EQUAL(Err::None, analogSetPoint(c, 300, 100));
  TEST_ASSERT_EQUAL(300, c.rawMin);
  TEST_ASSERT_FALSE(c.autocal);
  TEST_ASSERT_EQUAL(Err::None, analogSetPoint(c, 820, 0));
  TEST_ASSERT_EQUAL(820, c.rawMax);
  // Nass mit hohem Rohwert passt nicht zur Invertierung
  TEST_ASSERT_EQUAL(Err::OutOfRange, analogSetPoint(c, 900, 100));
  TEST_ASSERT_EQUAL(Err::OutOfRange, analogSetPoint(c, 300, 0)); // gleich der anderen Grenze
  TEST_ASSERT_EQUAL(300, c.rawMin);
  TEST_ASSERT_EQUAL(Err::InvalidArgument, analogSetPoint(c, 500, 50));
  TEST_ASSERT_EQUAL(Err::InvalidArgument, analogSetPoint(c, -1, 0));
  // Nicht invertiert (Licht): 100 % ist rawMax
  ChannelConfig l;
  l.rawMin = 5;
  l.rawMax = 1023;
  TEST_ASSERT_EQUAL(Err::None, analogSetPoint(l, 900, 100));
  TEST_ASSERT_EQUAL(900, l.rawMax);
  TEST_ASSERT_EQUAL(Err::None, analogSetPoint(l, 40, 0));
  TEST_ASSERT_EQUAL(40, l.rawMin);
}

void test_weight_set_point() {
  SensorConfig c;
  TEST_ASSERT_EQUAL(Err::None, weightSetPoint(c, 3000, 100)); // nass zuerst
  TEST_ASSERT_EQUAL_FLOAT(3000, c.wetG);
  TEST_ASSERT_EQUAL(Err::OutOfRange, weightSetPoint(c, 3100, 0));
  TEST_ASSERT_EQUAL(Err::None, weightSetPoint(c, 2100, 0));
  TEST_ASSERT_EQUAL_FLOAT(2100, c.dryG);
  TEST_ASSERT_EQUAL(Err::OutOfRange, weightSetPoint(c, 2000, 100));
  TEST_ASSERT_EQUAL(Err::InvalidArgument, weightSetPoint(c, NAN, 100));
  // Trocken zuerst geht auch
  SensorConfig d;
  TEST_ASSERT_EQUAL(Err::None, weightSetPoint(d, 2100, 0));
  TEST_ASSERT_EQUAL(Err::None, weightSetPoint(d, 3000, 100));
}

void test_watering_detector() {
  WateringDetector w;
  Instant t = Instant::fromMillis(1000);
  const Duration minute = Duration::ms(60000);
  // Austrocknen: kein Gießen
  for (int i = 0; i < 10; i++) {
    TEST_ASSERT_FALSE(w.feed(3000.0f - 5 * i, 50, t));
    t = t + minute;
  }
  TEST_ASSERT_FALSE(w.watered());
  // Langsam gegossen: zweimal +30 g, zusammen über der Schwelle
  TEST_ASSERT_FALSE(w.feed(2985, 50, t));
  TEST_ASSERT_TRUE(w.feed(3015, 50, t + minute));
  TEST_ASSERT_TRUE(w.settling());
  TEST_ASSERT_TRUE(w.watered());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 60, w.lastRise()); // gegen das Minimum 2955
  // Nachgießen verlängert das Abtropfen
  t = t + Duration::ms(30 * 60000);
  TEST_ASSERT_TRUE(w.feed(3200, 50, t));
  TEST_ASSERT_FALSE(w.settledNow(t + Duration::ms(59 * 60000)));
  TEST_ASSERT_TRUE(w.settledNow(t + Duration::ms(60 * 60000)));
  TEST_ASSERT_FALSE(w.settling());
  TEST_ASSERT_FALSE(w.settledNow(t + Duration::ms(61 * 60000))); // nur einmal
  // Neustart vergisst die Historie und das Abtropfen
  TEST_ASSERT_TRUE(w.feed(3500, 50, t));
  w.restart();
  TEST_ASSERT_FALSE(w.settling());
  TEST_ASSERT_FALSE(w.feed(9000, 50, t));
  // Schwelle 0 = aus
  WateringDetector off;
  off.feed(1000, 0, t);
  TEST_ASSERT_FALSE(off.feed(5000, 0, t));
  TEST_ASSERT_FALSE(off.feed(NAN, 50, t));
}

void test_soil_fit_linear_and_curved() {
  SoilFit f;
  TEST_ASSERT_TRUE(f.empty());
  TEST_ASSERT_FALSE(f.suggest().ok);
  // Sensor: 0 % Wasser = 700, 100 % = 300 (invertiert), nur 30..90 % beobachtet
  for (int rep = 0; rep < 5; rep++)
    for (int w = 35; w <= 95; w += 10)
      f.add(static_cast<float>(w), static_cast<int16_t>(700 - 4 * w));
  FitSuggestion s = f.suggest();
  TEST_ASSERT_TRUE(s.ok);
  TEST_ASSERT_TRUE(s.inverted);
  TEST_ASSERT_INT_WITHIN(1, 300, s.rawMin);
  TEST_ASSERT_INT_WITHIN(1, 700, s.rawMax);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, s.r2);
  TEST_ASSERT_EQUAL(7, s.bins);
  // Gekrümmt: Bestimmtheitsmaß sinkt
  SoilFit g;
  for (int rep = 0; rep < 5; rep++)
    for (int w = 5; w <= 95; w += 10)
      g.add(static_cast<float>(w), static_cast<int16_t>(300 + w * w / 20));
  s = g.suggest();
  TEST_ASSERT_TRUE(s.ok);
  TEST_ASSERT_FALSE(s.inverted);
  TEST_ASSERT_TRUE(s.r2 < 0.98f);
  // Zu wenig Spanne (zwei Bereiche) bzw. zu wenige Messungen
  SoilFit h;
  for (int rep = 0; rep < 5; rep++) {
    h.add(55, 500);
    h.add(65, 480);
  }
  h.add(85, 400); // nur eine Messung: zählt nicht
  TEST_ASSERT_FALSE(h.suggest().ok);
  TEST_ASSERT_EQUAL(2, h.suggest().bins);
  // Sensor reagiert nicht: kein Vorschlag
  SoilFit flat;
  for (int rep = 0; rep < 5; rep++)
    for (int w = 5; w <= 95; w += 10)
      flat.add(static_cast<float>(w), 512);
  TEST_ASSERT_FALSE(flat.suggest().ok);
  // Ränder und Gleitmittel
  TEST_ASSERT_EQUAL(0, SoilFit::binOf(-3));
  TEST_ASSERT_EQUAL(9, SoilFit::binOf(100));
  SoilFit e;
  for (int i = 0; i < 200; i++)
    e.add(50, 400);
  for (int i = 0; i < 200; i++)
    e.add(50, 600);
  TEST_ASSERT_FLOAT_WITHIN(5, 600, e.raw[5]); // folgt nach kWindow Messungen dem neuen Wert
  e.clear();
  TEST_ASSERT_TRUE(e.empty());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_median);
  RUN_TEST(test_analog_mapping);
  RUN_TEST(test_autocal);
  RUN_TEST(test_protocols);
  RUN_TEST(test_parallel_no_conflict);
  RUN_TEST(test_warmup_does_not_block);
  RUN_TEST(test_deadline_releases_resources);
  RUN_TEST(test_measure_now_busy_and_warmup);
  RUN_TEST(test_backoff_and_recovery);
  RUN_TEST(test_channel_failure_keeps_others);
  RUN_TEST(test_no_drift_and_wrap);
  RUN_TEST(test_disable_enable_runtime);
  RUN_TEST(test_all_invalid_is_failure);
  RUN_TEST(test_sensor_service);
  RUN_TEST(test_water_percent);
  RUN_TEST(test_analog_set_point);
  RUN_TEST(test_weight_set_point);
  RUN_TEST(test_watering_detector);
  RUN_TEST(test_soil_fit_linear_and_curved);
  RUN_TEST(test_dht_decode);
  return UNITY_END();
}
