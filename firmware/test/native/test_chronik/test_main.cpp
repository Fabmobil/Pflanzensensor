/**
 * @file test_main.cpp
 * @brief Chronik v2: Format, Vektoren, Budget, Segmente, CSV, Dienst mit RAM-Dateisystem
 */
#include "core/chronik/budget.h"
#include "core/chronik/csv.h"
#include "core/chronik/format.h"
#include "core/chronik/segments.h"
#include "services/chronik_service.h"
#include "test_support.h"

using namespace ps;
using namespace ps::chronik;

void setUp() { native::setMillis(1000); }
void tearDown() {}

static std::vector<uint8_t> fromHex(const char* h) {
  std::vector<uint8_t> v;
  for (size_t i = 0; h[i] && h[i + 1]; i += 2)
    v.push_back(static_cast<uint8_t>(strtoul(std::string(h + i, 2).c_str(), nullptr, 16)));
  return v;
}

void test_vectors() {
  JsonDocument d;
  TEST_ASSERT_TRUE(pstest::loadVector("chronik_frames.json", d));
  const auto table = fromHex(d["table"]);
  const auto sample = fromHex(d["sample"]);
  EncodingMap map;
  TEST_ASSERT_EQUAL(table.size(), readTable(table.data(), table.size(), nullptr, nullptr, &map));
  SampleFrame f;
  TEST_ASSERT_EQUAL(sample.size(), readSample(sample.data(), sample.size(), map, f));
  TEST_ASSERT_EQUAL_UINT32(d["sampleDecoded"]["epoch"].as<uint32_t>(), f.epoch);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 42.5f, f.values[0].value);
  TEST_ASSERT_EQUAL(612, f.values[0].raw);
  TEST_ASSERT_FLOAT_WITHIN(0.06f, 1013.2f, f.values[1].value); // int16 mit Skala 0,1
  TEST_ASSERT_TRUE(isnan(f.values[2].value));
  for (JsonObject h : d["half"].as<JsonArray>())
    TEST_ASSERT_EQUAL_UINT16(h["bits"].as<uint16_t>(), halfFromFloat(h["value"].as<float>()));
  // Ohne Tabelle nicht dekodierbar, aber abgrenzbar
  EncodingMap empty;
  TEST_ASSERT_EQUAL(0, readSample(sample.data(), sample.size(), empty, f));
  TEST_ASSERT_EQUAL(sample.size(), frameLength(sample.data(), sample.size()));
  // Beschädigt
  auto bad = sample;
  bad[9] ^= 0xFF;
  TEST_ASSERT_EQUAL(0, frameLength(bad.data(), bad.size()));
  // Präfix
  std::vector<uint8_t> all = table;
  all.insert(all.end(), sample.begin(), sample.end());
  all.insert(all.end(), sample.begin(), sample.begin() + 5);
  TEST_ASSERT_EQUAL(table.size() + sample.size(), validPrefixLength(all.data(), all.size()));
}

void test_budget_segments() {
  BudgetInput in;
  in.freeBytes = 80000;
  TEST_ASSERT_EQUAL(0, targetSegments(in));
  in.freeBytes = 950 * 1024;
  TEST_ASSERT_EQUAL(kMaxSegments, targetSegments(in));
  in.freeBytes = reserveBytes(false) + 3 * kSegmentSize + 10;
  TEST_ASSERT_EQUAL(3, targetSegments(in));
  in.fileLogEnabled = true;
  TEST_ASSERT_EQUAL(1, targetSegments(in));
  char n[kNameBuffer];
  nameFromIndex(42, n);
  TEST_ASSERT_EQUAL_STRING("/chr_0000002a.dat", n);
  TEST_ASSERT_EQUAL(42, indexFromName(n));
  TEST_ASSERT_EQUAL(42, indexFromName("chr_0000002a.dat"));
  TEST_ASSERT_EQUAL(-1, indexFromName("/chr_0000002A.dat"));
  TEST_ASSERT_EQUAL(-1, indexFromName("/log.txt"));
}

void test_csv_stream_split() {
  JsonDocument d;
  pstest::loadVector("chronik_frames.json", d);
  auto stream = fromHex(d["table"]);
  const auto sample = fromHex(d["sample"]);
  stream.insert(stream.end(), {0x00, 0x13, 0x37}); // Störstelle
  stream.insert(stream.end(), sample.begin(), sample.end());
  pstest::StdSink out;
  CsvConverter* c = new CsvConverter(out);
  c->header();
  for (size_t i = 0; i < stream.size(); i += 7)
    c->feed(stream.data() + i, std::min<size_t>(7, stream.size() - i));
  c->finish();
  delete c;
  TEST_ASSERT_NOT_NULL(strstr(out.s.c_str(), "1700000060,0,ANALOG_0,42.5,612,green\n"));
  TEST_ASSERT_NOT_NULL(strstr(out.s.c_str(), ",1,BMP280_1,1013.2,,yellow\n"));
  TEST_ASSERT_NOT_NULL(strstr(out.s.c_str(), ",2,SDS011_0,,,red\n"));
}

void test_service_record_flush_stream() {
  native::MemFs fs;
  TemplateStore tpl(fs);
  StateStore st(fs);
  ConfigService cfg(fs, tpl, st);
  cfg.load();
  Events ev;
  SensorService sensors(cfg, st, ev);
  ChronikService ch(fs, cfg, sensors);
  ch.begin(false);
  TEST_ASSERT_TRUE(ch.enabled());
  TEST_ASSERT_TRUE(ch.targetSegments() > 0);
  st.channel(0).value = 12.5f;
  st.channel(0).raw = 300;
  st.channel(0).status = ChannelStatus::Green;
  st.channel(1).value = 70;
  // ohne gültige Uhr verworfen
  native::setEpoch(0);
  ch.record(0, 0, hal::now());
  TEST_ASSERT_EQUAL(1, ch.dropped());
  native::setEpoch(1700000000);
  for (int i = 0; i < 30; i++) {
    ch.record(0, 1700000000 + i * 60, hal::now());
    ch.tick(hal::now(), false); // Flush ab Schwelle
  }
  TEST_ASSERT_EQUAL(1, ch.segmentCount());
  pstest::StdSink out;
  ch.stream(0, out);
  CsvConverter* c = new CsvConverter(out);
  pstest::StdSink csv;
  CsvConverter conv(csv);
  conv.feed(reinterpret_cast<const uint8_t*>(out.s.data()), out.s.size());
  conv.finish();
  delete c;
  TEST_ASSERT_NOT_NULL(strstr(csv.s.c_str(), "1700000000,0,ANALOG_0,12.5,300,green"));
  TEST_ASSERT_NOT_NULL(strstr(csv.s.c_str(), "1700001740,1,ANALOG_1,70,"));
  // Startreparatur: angeschnittenen Rahmen entfernen
  char name[kNameBuffer];
  nameFromIndex(0, name);
  auto& data = fs.files_[name];
  const size_t good = data.size();
  data.push_back(0xA5);
  data.push_back(0xC6);
  data.push_back(0x01);
  ChronikService again(fs, cfg, sensors);
  again.begin(false);
  TEST_ASSERT_EQUAL(good, fs.files_[name].size());
  // Boot-Schleife: nichts anfassen
  ChronikService blocked(fs, cfg, sensors);
  blocked.begin(true);
  TEST_ASSERT_FALSE(blocked.enabled());
  TEST_ASSERT_TRUE(blocked.disabledForBoot());
  // Infotabelle als gültiges JSON
  pstest::StdSink info;
  JsonWriter w(info);
  ch.writeInfo(w);
  JsonDocument doc;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(doc, info.s).code());
  TEST_ASSERT_TRUE(doc["skippedChannels"].as<int>() > 0); // Testprofil hat > 16 Kanäle
}

void test_rotation() {
  native::MemFs fs;
  fs.total = reserveBytes(false) + 2 * kSegmentSize + 4000;
  TemplateStore tpl(fs);
  StateStore st(fs);
  ConfigService cfg(fs, tpl, st);
  cfg.load();
  Events ev;
  SensorService sensors(cfg, st, ev);
  ChronikService ch(fs, cfg, sensors);
  ch.begin(false);
  native::setEpoch(1700000000);
  for (int i = 0; i < 3000; i++) {
    ch.record(0, 1700000000 + i * 60, hal::now());
    native::advance(1000);
    ch.tick(hal::now(), false);
  }
  TEST_ASSERT_TRUE(ch.segmentCount() <= ch.targetSegments());
  TEST_ASSERT_TRUE(ch.segmentCount() >= 1);
  char first[kNameBuffer];
  nameFromIndex(0, first);
  TEST_ASSERT_FALSE(fs.exists(first)); // ältestes gelöscht
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_vectors);
  RUN_TEST(test_budget_segments);
  RUN_TEST(test_csv_stream_split);
  RUN_TEST(test_service_record_flush_stream);
  RUN_TEST(test_rotation);
  return UNITY_END();
}
