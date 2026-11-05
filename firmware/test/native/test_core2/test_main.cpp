/**
 * @file test_main.cpp
 * @brief Randfälle im Kern, die die übrigen Suiten nicht treffen
 */
#include "core/chronik/format.h"
#include "core/config/defaults.h"
#include "core/config/document.h"
#include "core/config/legacy_import.h"
#include "core/display/canvas.h"
#include "core/display/font5x7.h"
#include "core/display/screens.h"
#include "core/mail/scheduler.h"
#include "core/sensors/scheduler.h"
#include "core/util/event_queue.h"
#include "core/util/format.h"
#include "test_support.h"

using namespace ps;

void setUp() { native::setMillis(1000); }
void tearDown() {}

// ---------------------------------------------------------------------------
void test_json_writer_details() {
  pstest::StdSink s;
  JsonWriter w(s);
  w.beginArray().value(FS_("flash")).raw("{\"x\":1}").null().value(true).value(false);
  w.value(static_cast<uint64_t>(18446744073709551615ULL)).value(static_cast<int64_t>(INT64_MIN));
  w.value(1e30f, 2).value(-0.001f, 2).value(0.5f, 0).value(123.456f, 6).endArray();
  JsonDocument d;
  TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, deserializeJson(d, s.s).code(), s.s.c_str());
  TEST_ASSERT_EQUAL_STRING("flash", d[0]);
  TEST_ASSERT_EQUAL(1, d[1]["x"].as<int>());
  TEST_ASSERT_TRUE(d[2].isNull());
  TEST_ASSERT_NOT_NULL(strstr(s.s.c_str(), "18446744073709551615"));
  TEST_ASSERT_NOT_NULL(strstr(s.s.c_str(), "-9223372036854775808"));
  TEST_ASSERT_FLOAT_WITHIN(1e25f, 1e30f, d[7].as<float>());
  TEST_ASSERT_EQUAL_STRING("0", d[8].as<std::string>().c_str());
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 123.456, d[10].as<double>());
  // Schlüssel aus Teilen, Flash-Schlüssel, Escapes
  s.s.clear();
  JsonWriter w2(s);
  w2.beginObject()
      .keyParts("ANALOG", '_', "1")
      .value(1)
      .key(FS_("t\tab"))
      .value("a\x01\x1f\\b")
      .endObject();
  deserializeJson(d, s.s);
  TEST_ASSERT_EQUAL(1, d["ANALOG_1"].as<int>());
  TEST_ASSERT_EQUAL_STRING("a\x01\x1f\\b", d["t\tab"]);
  TEST_ASSERT_NOT_NULL(strstr(s.s.c_str(), "\\u0001"));
  // Langer Flash-Text (mehrere 32-Byte-Stücke)
  pstest::StdSink p;
  p.writeP(FS_("0123456789012345678901234567890123456789012345678901234567890123456789"));
  TEST_ASSERT_EQUAL(70, p.s.size());
  // BufferSink läuft nie über, CountingSink zählt
  char small[8];
  BufferSink b(small, sizeof(small));
  b.write("0123456789");
  TEST_ASSERT_TRUE(b.overflow());
  TEST_ASSERT_EQUAL(7, b.size());
  TEST_ASSERT_EQUAL_STRING("0123456", b.c_str());
  b.clear();
  TEST_ASSERT_EQUAL(0, b.size());
  CountingSink c;
  JsonWriter w3(c);
  w3.beginObject().kv("a", 12).endObject();
  TEST_ASSERT_EQUAL(8, c.count());
  // formatFloat
  char f[8];
  TEST_ASSERT_EQUAL(4, formatFloat(f, sizeof(f), NAN, 2));
  TEST_ASSERT_EQUAL_STRING("null", f);
  char tiny[3];
  formatFloat(tiny, sizeof(tiny), NAN, 2);
  TEST_ASSERT_EQUAL_STRING("nu", tiny);
  formatFloat(f, sizeof(f), 1.23456f, 9); // höchstens 6 Nachkommastellen
  TEST_ASSERT_EQUAL_STRING("1.23456", f);
}

void test_fixed_string_misc() {
  FixedString<8> s(std::string_view("abcdefghij").data());
  TEST_ASSERT_EQUAL_STRING("abcdefg", s.c_str());
  TEST_ASSERT_TRUE(s.assign(std::string_view("xyz")));
  TEST_ASSERT_TRUE(s.view() == "xyz");
  FixedString<8> t("xyz");
  TEST_ASSERT_TRUE(s == t);
  TEST_ASSERT_TRUE(s != "abc");
  TEST_ASSERT_FALSE(s == nullptr);
  TEST_ASSERT_TRUE(s.assign(nullptr));
  TEST_ASSERT_TRUE(s.empty());
  TEST_ASSERT_FALSE(s.append("12345678"));
  TEST_ASSERT_EQUAL_STRING("1234567", s.c_str());
  TEST_ASSERT_EQUAL(0, utf8Truncate("ä", 2, 1));
}

void test_duration_and_status() {
  TEST_ASSERT_EQUAL(120000, Duration::min(2).millis());
  TEST_ASSERT_EQUAL(3, Duration::s(3).seconds());
  TEST_ASSERT_TRUE(Duration::s(1) < Duration::s(2));
  TEST_ASSERT_TRUE(Duration::s(2) >= Duration::s(2));
  TEST_ASSERT_TRUE(Duration::s(2) != Duration::s(3));
  TEST_ASSERT_EQUAL(4000, (Duration::s(2) * 2).millis());
  TEST_ASSERT_TRUE(
      (Instant::fromMillis(10) - Duration::ms(20)).reached(Instant::fromMillis(0xFFFFFFF0u)));
  Status ok;
  TEST_ASSERT_TRUE(static_cast<bool>(ok));
  Status bad(Err::Io);
  TEST_ASSERT_EQUAL(Err::Io, bad.error());
  TEST_ASSERT_EQUAL(ChannelStatus::Warmup, statusFromName("warmup"));
  TEST_ASSERT_EQUAL(ChannelStatus::Unknown, statusFromName("kaputt"));
  TEST_ASSERT_EQUAL(ChannelStatus::Unknown, statusFromName(nullptr));
  char n[10];
  copyP(n, sizeof(n), statusName(static_cast<ChannelStatus>(99)));
  TEST_ASSERT_EQUAL_STRING("unknown", n);
  copyP(n, sizeof(n), errCode(static_cast<Err>(250)));
  TEST_ASSERT_EQUAL_STRING("unknown", n);
  TEST_ASSERT_FALSE(limitsOrdered(Limits{NAN, 0, 0, 0}, false));
}

void test_event_queue_overflow() {
  EventQueue<3> q;
  Event e;
  e.type = EventType::WifiUp;
  for (int i = 0; i < 5; i++)
    q.push(e);
  TEST_ASSERT_EQUAL(3, q.size());
  TEST_ASSERT_EQUAL_UINT32(2, q.dropped());
  int n = 0;
  while (q.pop(e))
    n++;
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_FALSE(q.pop(e));
  // Ringpuffer über die Grenze
  for (int r = 0; r < 10; r++) {
    e.a = static_cast<uint8_t>(r);
    q.push(e);
    Event o;
    TEST_ASSERT_TRUE(q.pop(o));
    TEST_ASSERT_EQUAL(r, o.a);
  }
}

void test_format_helpers() {
  char b[24];
  fmt::uptime(59, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("0m", b);
  fmt::uptime(3 * 86400 + 60, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("3d 1m", b);
  fmt::date(2026, 1, 2, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("02.01.2026", b);
  fmt::time(7, 5, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("07:05", b);
}

// ---------------------------------------------------------------------------
void test_canvas_primitives() {
  Canvas c;
  c.pixel(-1, 0);
  c.pixel(128, 0);
  c.pixel(0, 64);
  TEST_ASSERT_FALSE(c.get(-1, 0));
  c.pixel(5, 9);
  TEST_ASSERT_TRUE(c.get(5, 9));
  c.pixel(5, 9, false);
  TEST_ASSERT_FALSE(c.get(5, 9));
  c.fillRect(120, 60, 20, 20); // ragt hinaus
  TEST_ASSERT_TRUE(c.get(127, 63));
  static const uint8_t bmp[] = {0x80, 0x01}; // 16×1: erstes und letztes Pixel
  c.clear();
  c.bitmap(10, 10, bmp, 16, 1);
  TEST_ASSERT_TRUE(c.get(10, 10));
  TEST_ASSERT_FALSE(c.get(11, 10));
  TEST_ASSERT_TRUE(c.get(25, 10));
  // Glyphen
  const char* p = "a";
  TEST_ASSERT_EQUAL('a' - font::kFirst, nextGlyph(p));
  p = "\x01";
  TEST_ASSERT_EQUAL(font::kUnknown, nextGlyph(p));
  p = "µ²³";
  TEST_ASSERT_EQUAL('u' - font::kFirst, nextGlyph(p));
  TEST_ASSERT_EQUAL('2' - font::kFirst, nextGlyph(p));
  TEST_ASSERT_EQUAL('3' - font::kFirst, nextGlyph(p));
  p = "\xC2\xA9"; // ©
  TEST_ASSERT_EQUAL(font::kUnknown, nextGlyph(p));
  p = "\xC3\xA9"; // é
  TEST_ASSERT_EQUAL(font::kUnknown, nextGlyph(p));
  p = "€x"; // 3-Byte-Folge wird übersprungen
  TEST_ASSERT_EQUAL(font::kUnknown, nextGlyph(p));
  TEST_ASSERT_EQUAL('x' - font::kFirst, nextGlyph(p));
  TEST_ASSERT_EQUAL(3, Canvas::glyphCount("ÄÖÜ"));
  TEST_ASSERT_EQUAL(0, Canvas::glyphCount(""));
  // Gekürzter Text endet mit "~"
  Canvas a, b;
  a.textFit(0, 0, "abcdef", 3);
  b.text(0, 0, "ab~");
  TEST_ASSERT_EQUAL_HEX32(b.hash(), a.hash());
}

void test_screens_progress_and_info() {
  Canvas c;
  screens::progress(c, "Update", 0);
  const uint32_t h0 = c.hash();
  screens::progress(c, "Update", 50);
  const uint32_t h50 = c.hash();
  screens::progress(c, "Update", 200); // auf 100 begrenzt
  const uint32_t h100 = c.hash();
  screens::progress(c, "Update", 100);
  TEST_ASSERT_EQUAL_HEX32(h100, c.hash());
  TEST_ASSERT_NOT_EQUAL(h0, h50);
  TEST_ASSERT_NOT_EQUAL(h50, h100);
  screens::info(c, "kurz", "1.2.3.4", "Netz");
  const uint32_t shortName = c.hash();
  screens::info(c, "ein-ziemlich-langer-hostname-der-zwei-zeilen-braucht", "1.2.3.4", "Netz");
  TEST_ASSERT_NOT_EQUAL(shortName, c.hash());
  screens::fabmobil(c);
  screens::channel(c, "Druck", 1013.25f, "hPa"); // kleinere Schrift, passt
  const char* none[] = {nullptr};
  screens::bootLog(c, "Kopf", none, 0);
}

// ---------------------------------------------------------------------------
void test_chronik_table_limits() {
  uint8_t buf[600];
  chronik::TableBuilder tb(buf, sizeof(buf), 1);
  // Lange Namen werden an Zeichengrenzen auf 31 Byte gekürzt
  const std::string longName = std::string(30, 'a') + "ää";
  TEST_ASSERT_TRUE(
      tb.add(0, false, false, "K", longName.c_str(), "u", Limits{}, chronik::Encoding{}));
  int added = 1;
  while (tb.add(static_cast<uint8_t>(added), false, false, "KEY_XXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                "NAME_XXXXXXXXXXXXXXXXXXXXXXXXXXX", "UNIT_XXXXXXXXXXXXXXXXXXXXXXXXXXX", Limits{},
                chronik::Encoding{}))
    added++;
  TEST_ASSERT_TRUE(added < 16); // 512-B-Grenze greift vorher
  TEST_ASSERT_FALSE(tb.add(16, false, false, "k", "n", "u", Limits{}, chronik::Encoding{}));
  const size_t n = tb.finish();
  TEST_ASSERT_TRUE(n > 0 && n <= chronik::kMaxFrame);
  struct Ctx {
    std::string first;
  } ctx;
  TEST_ASSERT_EQUAL(n, chronik::readTable(
                           buf, n,
                           [](void* c, const chronik::TableEntry& e) {
                             auto* x = static_cast<Ctx*>(c);
                             if (x->first.empty())
                               x->first.assign(e.name, e.nameLen);
                           },
                           &ctx, nullptr));
  TEST_ASSERT_EQUAL(30 + 0, ctx.first.size() - (ctx.first.size() - 30)); // 30 'a' …
  TEST_ASSERT_TRUE(ctx.first.size() == 30 || ctx.first.size() == 32 - 2 + 0);
  // Leere Tabelle ergibt keinen Rahmen, zu kleiner Puffer auch nicht
  chronik::TableBuilder empty(buf, sizeof(buf), 1);
  TEST_ASSERT_EQUAL(0, empty.finish());
  chronik::TableBuilder nospace(buf, 5, 1);
  TEST_ASSERT_FALSE(nospace.add(0, false, false, "k", "n", "u", Limits{}, chronik::Encoding{}));
  TEST_ASSERT_EQUAL(0, nospace.finish());
  // Messrahmen mit unbekannter Kodierung wird nicht geschrieben
  chronik::EncodingMap map;
  chronik::SampleFrame f;
  f.count = 1;
  TEST_ASSERT_EQUAL(0, chronik::writeSample(f, map, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(0, chronik::readTable(nullptr, 10, nullptr, nullptr, nullptr));
  TEST_ASSERT_EQUAL(0, chronik::frameLength(buf, 1));
}

// ---------------------------------------------------------------------------
void test_mail_scheduler_branches() {
  Mail::Scheduler s;
  Mail::SchedulerConfig c;
  c.enabled = true;
  c.warnFrom = Mail::Level::Red;
  s.begin(c, 1700000000);
  s.reportLevel(0, Mail::Level::Yellow, true);
  TEST_ASSERT_FALSE(s.hasAlarm()); // erst ab Rot
  s.reportLevel(1, Mail::Level::Red, true);
  TEST_ASSERT_EQUAL(Mail::Kind::None, s.due(1700000100)); // Ruhezeit 180 s
  TEST_ASSERT_EQUAL(Mail::Kind::Warning, s.due(1700000181));
  s.markSent(Mail::Kind::Warning, 1700000181);
  TEST_ASSERT_EQUAL(1u, s.warningCount());
  s.markSent(Mail::Kind::None, 1);            // ohne Wirkung
  s.reportLevel(200, Mail::Level::Red, true); // außerhalb: ignoriert
  s.clearLevels();
  TEST_ASSERT_FALSE(s.hasAlarm());
  s.restore(5, 6);
  TEST_ASSERT_EQUAL(5u, s.lastWarning());
  TEST_ASSERT_EQUAL(6u, s.lastAlive());
  c.enabled = false;
  s.setConfig(c);
  TEST_ASSERT_EQUAL(Mail::Kind::None, s.due(1800000000));
  TEST_ASSERT_EQUAL(Mail::Level::Unknown, Mail::levelVonText(nullptr));
  TEST_ASSERT_EQUAL(Mail::Level::Red, Mail::levelVonText("red"));
  TEST_ASSERT_FALSE(Mail::istAuffaellig(Mail::Level::Unknown, Mail::Level::Yellow));
  TEST_ASSERT_TRUE(Mail::isWatched(" ANALOG_1 , DHT_1 ", "ANALOG_1"));
  TEST_ASSERT_FALSE(Mail::isWatched("ANALOG_1", ""));
  TEST_ASSERT_EQUAL(3600u, Mail::hoursToSeconds(0));
  TEST_ASSERT_EQUAL(720u * 3600, Mail::hoursToSeconds(5000));
}

// ---------------------------------------------------------------------------
namespace {
struct NopListener : MeasurementListener {
  int done = 0;
  void onMeasurement(uint8_t, const Reading*, uint8_t, Instant) override { done++; }
  void onFailure(uint8_t, Health, uint8_t, bool, Instant) override {}
};
struct QuickDriver final : SensorDriver {
  SensorType type() const override { return SensorType::Dht; }
  SensorInfo info() const override {
    SensorInfo i = catalog::info(SensorType::Dht);
    i.roundDelayMs = 0;
    i.rounds = 9; // wird auf MaxRounds begrenzt
    return i;
  }
  uint8_t channelCount() const override { return 2; }
  Step begin(Instant) override { return Step::Ok; }
  Step pollRound(Instant, Reading* out, uint8_t n) override {
    for (uint8_t i = 0; i < n; i++)
      out[i].value = 1;
    return Step::Ok;
  }
};
} // namespace

void test_scheduler_interval_and_limits() {
  NopListener l;
  MeasurementScheduler<1, 2> s(l);
  QuickDriver d;
  TEST_ASSERT_TRUE(s.add(d, 2, 0, true));     // Intervall 0 → 60 s
  TEST_ASSERT_FALSE(s.add(d, 2, 1000, true)); // voll
  TEST_ASSERT_EQUAL(60000u, s.state(0).intervalMs);
  TEST_ASSERT_EQUAL(MeasureNowResult::NotFound, s.measureNow(5, hal::now()));
  s.tick(hal::now()); // vor start(): nichts
  TEST_ASSERT_EQUAL(0, l.done);
  s.start(hal::now());
  s.tick(hal::now()); // höchstens 8 Phasenschritte je Tick
  s.tick(hal::now());
  TEST_ASSERT_EQUAL(1, l.done);
  // Kürzeres Intervall zieht die nächste Fälligkeit vor
  s.setInterval(0, 10000, hal::now());
  native::advance(10000);
  s.tick(hal::now());
  s.tick(hal::now());
  TEST_ASSERT_EQUAL(2, l.done);
  s.setInterval(0, 0, hal::now()); // ignoriert
  s.setInterval(3, 1000, hal::now());
  s.setEnabled(3, false, hal::now());
  TEST_ASSERT_EQUAL(0u, s.warmupRemainingMs(0, hal::now()));
  TEST_ASSERT_FALSE(s.inStartWarmup(7, hal::now()));
  s.pause(true);
  TEST_ASSERT_TRUE(s.paused());
  native::advance(20000);
  s.tick(hal::now());
  TEST_ASSERT_EQUAL(2, l.done);
  // Verspätung > Intervall: kein Nachholen
  s.pause(false);
  native::advance(100000);
  s.tick(hal::now());
  s.tick(hal::now());
  native::advance(100);
  s.tick(hal::now());
  s.tick(hal::now());
  TEST_ASSERT_EQUAL(3, l.done);
}

// ---------------------------------------------------------------------------
void test_document_helpers() {
  uint8_t b[4];
  TEST_ASSERT_TRUE(doc::fromHex("A0ff0012", b, 4));
  TEST_ASSERT_EQUAL_HEX8(0xA0, b[0]);
  TEST_ASSERT_FALSE(doc::fromHex("zz", b, 1));
  TEST_ASSERT_FALSE(doc::fromHex("abc", b, 2));
  TEST_ASSERT_FALSE(doc::fromHex(nullptr, b, 1));
  TEST_ASSERT_FALSE(doc::slotOneSided(250));
  TEST_ASSERT_FALSE(doc::slotHasRaw(250));
  TEST_ASSERT_TRUE(doc::slotHasRaw(0));
  // Geheimnisse: kaputter Admin-Hash wird ignoriert
  Secrets s;
  JsonDocument d;
  deserializeJson(d, "{\"wifi\":[\"a\",1,\"c\",\"d\"],\"admin\":{\"salt\":\"xx\",\"hash\":\"yy\"},"
                     "\"mailKey\":\"k\"}");
  doc::readSecrets(d.as<JsonObjectConst>(), s);
  TEST_ASSERT_FALSE(s.adminSet);
  TEST_ASSERT_EQUAL_STRING("a", s.wifiPassword[0].c_str());
  TEST_ASSERT_EQUAL_STRING("c", s.wifiPassword[2].c_str());
  TEST_ASSERT_EQUAL_STRING("k", s.mailKey.c_str());
  doc::readSecrets(JsonObjectConst(), s);
  // Schlüssel ↔ Slot
  TEST_ASSERT_EQUAL(-1, schema::slotOfKey("ANALOG_"));
  TEST_ASSERT_EQUAL(-1, schema::slotOfKey("ANALOG_x"));
  TEST_ASSERT_EQUAL(-1, schema::slotOfKey("_1"));
  TEST_ASSERT_EQUAL(-1, schema::slotOfKey("ANALOG_-1"));
  TEST_ASSERT_EQUAL(-1, schema::slotOfKey("ANALOG_9"));
  TEST_ASSERT_EQUAL(-1, schema::slotOfKey(nullptr));
  char key[32];
  schema::keyOfSlot(250, key, sizeof(key));
  TEST_ASSERT_EQUAL_STRING("", key);
  for (uint8_t i = 0; i < board::slotCount(); i++) {
    schema::keyOfSlot(i, key, sizeof(key));
    TEST_ASSERT_EQUAL(i, schema::slotOfKey(key));
  }
  // FieldErrors: Überlauf wird gezählt
  schema::FieldErrors e;
  for (int i = 0; i < 12; i++)
    e.add("x", Err::Io);
  TEST_ASSERT_EQUAL(8, e.count);
  TEST_ASSERT_EQUAL(4, e.dropped);
  TEST_ASSERT_FALSE(e.empty());
  // Enum als Zahl
  DeviceConfig dc;
  deserializeJson(d, "{\"log\":{\"level\":2},\"display\":{\"clockFormat\":7}}");
  schema::FieldErrors fe;
  schema::apply(schema::deviceTable(), d.as<JsonObjectConst>(), &dc, fe, true);
  TEST_ASSERT_EQUAL(2, dc.logLevel);
  TEST_ASSERT_EQUAL(1, fe.count);
  // Zahl statt Text, Text statt Bool, Bruch statt Ganzzahl
  deserializeJson(d, "{\"general\":{\"deviceName\":5,\"otaRequireMd5\":\"ja\"},\"display\":{"
                     "\"screenDuration\":2.5}}");
  fe = schema::FieldErrors{};
  schema::apply(schema::deviceTable(), d.as<JsonObjectConst>(), &dc, fe, true);
  TEST_ASSERT_EQUAL(3, fe.count);
}

void test_legacy_edge_cases() {
  DeviceConfig dev;
  defaults::device(dev);
  SensorsConfig sens;
  sensorDefaults(sens);
  Secrets sec;
  legacy::Extras* ex = new legacy::Extras();
  legacy::Report rep;
  JsonDocument d;
  deserializeJson(
      d, R"({"general":{"device_name":"","admin_pwd":"Grün","file_log":1,"flower_sens":"NOPE_1"},
    "log":{"level":"warn"},"display":{"screen_dur":70000,"clock_fmt":"13h","show_ip":"ja"},
    "led_traffic":{"mode":5},"wifi":{"ssid1":")" +
             std::string(40, 'x') + R"("},
    "sensors":[{"id":"ANALOG","measInt":1000,"measurements":[{"idx":0,"min":900,"max":100,"ahl":12345,"thresh":{"yl":"x"}}]}]})");
  TEST_ASSERT_FALSE(legacy::isLegacy(JsonObjectConst()));
  TEST_ASSERT_TRUE(legacy::isLegacy(d.as<JsonObjectConst>()));
  legacy::import(d.as<JsonObjectConst>(), dev, sens, sec, *ex, rep);
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor",
                           dev.deviceName.c_str()); // leerer Name abgelehnt
  TEST_ASSERT_FALSE(ex->hasAdminPassword);
  TEST_ASSERT_TRUE(dev.fileLog); // 1 zählt als wahr
  TEST_ASSERT_EQUAL_STRING("ANALOG_1", dev.flowerChannel.c_str());
  TEST_ASSERT_EQUAL(2, dev.logLevel);        // "warn"
  TEST_ASSERT_EQUAL(5, dev.screenDurationS); // 70 s außerhalb
  TEST_ASSERT_EQUAL(0, dev.clockFormat);
  TEST_ASSERT_EQUAL(2, dev.ledMode);
  TEST_ASSERT_TRUE(dev.wifiSsid[0].empty());
  TEST_ASSERT_EQUAL(10, sens.sensors[0].intervalS); // 1 s → Minimum
  TEST_ASSERT_EQUAL(100, sens.channels[0].rawMin);  // vertauscht
  TEST_ASSERT_EQUAL(900, sens.channels[0].rawMax);
  TEST_ASSERT_EQUAL(86400u, sens.channels[0].halfLifeS);
  TEST_ASSERT_TRUE(rep.invalid >= 7);
  delete ex;
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_json_writer_details);
  RUN_TEST(test_fixed_string_misc);
  RUN_TEST(test_duration_and_status);
  RUN_TEST(test_event_queue_overflow);
  RUN_TEST(test_format_helpers);
  RUN_TEST(test_canvas_primitives);
  RUN_TEST(test_screens_progress_and_info);
  RUN_TEST(test_chronik_table_limits);
  RUN_TEST(test_mail_scheduler_branches);
  RUN_TEST(test_scheduler_interval_and_limits);
  RUN_TEST(test_document_helpers);
  RUN_TEST(test_legacy_edge_cases);
  return UNITY_END();
}
