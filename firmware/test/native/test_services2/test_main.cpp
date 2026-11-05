/**
 * @file test_main.cpp
 * @brief Weitere Dienste: Display, OTA, Zeit, Task-Takt, LED, Systeminfo,
 *        Sensordienst (Autokal, Tara, Neustartsperre), Netz, Chronik
 */
#include "app/boot_guard.h"
#include "app/scheduler.h"
#include "core/chronik/segments.h"
#include "core/display/canvas.h"
#include "services/display_service.h"
#include "services/ota_service.h"
#include "services/storage.h"
#include "services/time_service.h"
#include "web_rig.h"

using namespace ps;
using namespace pstest;

void setUp() {
  native::setMillis(1000);
  native::setEpoch(0);
}
void tearDown() {}

namespace {
struct DisplayRig {
  native::MemFs fs;
  TemplateStore tpl{fs};
  StateStore st{fs};
  ConfigService cfg{fs, tpl, st};
  Events ev;
  SensorService sensors{cfg, st, ev};
  native::FakeWifi wifi;
  NetService net{wifi, cfg, ev};
  FakeBus bus;
  DisplayService display{bus, cfg, sensors, net};
  DisplayRig() { cfg.load(); }
  void patch(const char* json) {
    JsonDocument d;
    deserializeJson(d, json);
    schema::FieldErrors e;
    uint8_t c;
    TEST_ASSERT_EQUAL(Err::None, cfg.patchDevice(d.as<JsonObjectConst>(), e, c));
  }
  int run(uint32_t ms) {
    const int before = bus.flushes;
    for (uint32_t t = 0; t < ms; t += 100) {
      native::advance(100);
      display.tick(hal::now());
    }
    return bus.flushes - before;
  }
};
} // namespace

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
void test_display_absent() {
  DisplayRig r;
  r.bus.present = false;
  r.display.begin();
  TEST_ASSERT_FALSE(r.display.present());
  r.display.bootLine("x");
  r.display.endBoot();
  TEST_ASSERT_EQUAL(0, r.run(10000));
  r.display.otaProgress(50);
  TEST_ASSERT_EQUAL(0, r.bus.flushes);
}

void test_display_boot_and_rotation() {
  DisplayRig r;
  r.display.begin();
  TEST_ASSERT_TRUE(r.display.present());
  TEST_ASSERT_EQUAL(1, r.bus.flushes); // Bootbildschirm
  for (int i = 0; i < 8; i++)
    r.display.bootLine("Zeile"); // mehr als 6: rollt
  TEST_ASSERT_EQUAL(9, r.bus.flushes);
  TEST_ASSERT_EQUAL(0, r.run(10000)); // im Bootmodus keine Rotation
  r.display.endBoot();
  // Standard: Info, (Uhr ohne Zeit übersprungen), Blume, Fabmobil, dann Kanäle
  r.run(100);
  TEST_ASSERT_EQUAL(-1, r.display.currentSlot());
  const int flushes = r.run(5 * 1000 * 4);
  TEST_ASSERT_EQUAL(4, flushes); // 5 s je Bildschirm
  // Irgendwann wird ein Kanal gezeigt
  bool sawChannel = false;
  for (int i = 0; i < 40 && !sawChannel; i++) {
    r.run(5000);
    sawChannel = r.display.currentSlot() >= 0;
  }
  TEST_ASSERT_TRUE(sawChannel);
}

void test_display_rotation_pushes_in_page_steps() {
  DisplayRig r;
  r.display.begin();
  r.display.endBoot();
  r.bus.maxPagesPerCall = 0;
  r.run(30000);
  // Rotation: nie ein ganzes Bild in einem Schritt (I2C ~35 ms)
  TEST_ASSERT_EQUAL(4, r.bus.maxPagesPerCall);
  // OTA-Fortschritt darf weiter ganz übertragen
  r.display.otaProgress(10);
  TEST_ASSERT_EQUAL(8, r.bus.maxPagesPerCall);
}

void test_display_clock_skipped_without_time_and_ampm() {
  DisplayRig r;
  r.patch("{\"display\":{\"showIp\":false,\"showFlower\":false,\"showFabmobil\":false,"
          "\"showClock\":true,\"screenDuration\":1}}");
  // Alle Kanäle aus: ohne Uhr bleibt nur der leere Bildschirm
  JsonDocument d;
  std::string all = "{";
  for (uint8_t i = 0; i < board::sensorCount(); i++) {
    char id[16];
    copyP(id, sizeof(id), catalog::id(board::sensor(i).type));
    all += std::string(i ? "," : "") + "\"" + id + "\":{\"enabled\":false}";
  }
  all += "}";
  deserializeJson(d, all);
  schema::FieldErrors e;
  uint32_t ch;
  r.cfg.patchSensors(d.as<JsonObjectConst>(), e, ch);
  r.display.begin();
  r.display.endBoot();
  r.run(400); // rendern + zwei Ticks seitenweise übertragen
  Canvas empty;
  TEST_ASSERT_EQUAL_MEMORY(empty.data(), r.bus.last, 1024); // nichts verfügbar → leer
  native::setEpoch(1700000000);
  r.display.refresh();
  r.run(400); // rendern + zwei Ticks seitenweise übertragen
  TEST_ASSERT_FALSE(memcmp(empty.data(), r.bus.last, 1024) == 0); // Uhr
  // 12-h-Format zeigt AM/PM: anderes Bild als 24 h
  uint8_t h24[1024];
  memcpy(h24, r.bus.last, 1024);
  r.patch("{\"display\":{\"clockFormat\":\"12h\"}}");
  r.display.refresh();
  r.run(400); // rendern + zwei Ticks seitenweise übertragen
  TEST_ASSERT_FALSE(memcmp(h24, r.bus.last, 1024) == 0);
}

void test_display_qr_and_ota() {
  DisplayRig r;
  r.patch("{\"display\":{\"showIp\":false,\"showFlower\":false,\"showFabmobil\":false,"
          "\"showClock\":false,\"showQr\":true}}");
  r.display.begin();
  r.display.endBoot();
  r.run(400); // rendern + zwei Ticks seitenweise übertragen
  int set = 0;
  for (int i = 0; i < 1024; i++)
    set += __builtin_popcount(r.bus.last[i]);
  TEST_ASSERT_TRUE(set > 300);
  r.display.otaProgress(42);
  const int f = r.bus.flushes;
  TEST_ASSERT_EQUAL(0, r.run(20000)); // während OTA keine Rotation
  TEST_ASSERT_EQUAL(f, r.bus.flushes);
}

// ---------------------------------------------------------------------------
// OTA-Dienst direkt
// ---------------------------------------------------------------------------
namespace {
struct OtaRig {
  native::MemFs fs;
  TemplateStore tpl{fs};
  StateStore st{fs};
  ConfigService cfg{fs, tpl, st};
  Events ev;
  FakeOta port;
  OtaService ota{port, cfg, ev};
  int prepares = 0, resumes = 0;
  std::vector<int> progress;
  OtaRig() {
    cfg.load();
    ota.setHooks(
        [](void* c, bool s) {
          (s ? static_cast<OtaRig*>(c)->prepares : static_cast<OtaRig*>(c)->resumes)++;
        },
        [](void* c, uint8_t p) { static_cast<OtaRig*>(c)->progress.push_back(p); }, this);
  }
};
} // namespace

void test_ota_service_paths() {
  OtaRig r;
  TEST_ASSERT_EQUAL(Err::None, r.ota.start(100, "", hal::now()));
  TEST_ASSERT_EQUAL(1, r.prepares);
  std::vector<uint8_t> img(100, 7);
  img[0] = 0xE9;
  for (int i = 0; i < 100; i += 10)
    r.ota.data(img.data() + i, 10, hal::now());
  TEST_ASSERT_TRUE(r.ota.finish(hal::now()));
  TEST_ASSERT_TRUE(r.ota.succeeded());
  TEST_ASSERT_EQUAL(100, r.progress.back());
  for (size_t i = 1; i < r.progress.size(); i++)
    TEST_ASSERT_TRUE(r.progress[i] > r.progress[i - 1]);
  pstest::StdSink s;
  JsonWriter w(s);
  r.ota.writeStatus(w);
  TEST_ASSERT_NOT_NULL(strstr(s.s.c_str(), "\"written\":100"));
  // Schreibfehler
  r.port.writeLimit = 3;
  TEST_ASSERT_EQUAL(Err::None, r.ota.start(0, nullptr, hal::now()));
  r.ota.data(img.data(), 10, hal::now());
  TEST_ASSERT_FALSE(r.ota.active());
  TEST_ASSERT_TRUE(r.port.aborted);
  TEST_ASSERT_EQUAL_STRING("Fake-Fehler", r.ota.error());
  TEST_ASSERT_EQUAL(1, r.resumes);
  // end() schlägt fehl (z.B. MD5 falsch)
  r.port.writeLimit = SIZE_MAX;
  r.port.endOk = false;
  r.ota.start(10, "0123456789abcdef0123456789abcdef", hal::now());
  r.ota.data(img.data(), 10, hal::now());
  TEST_ASSERT_FALSE(r.ota.finish(hal::now()));
  TEST_ASSERT_FALSE(r.ota.succeeded());
  // begin() schlägt fehl
  r.port.beginOk = false;
  TEST_ASSERT_EQUAL(Err::Io, r.ota.start(10, nullptr, hal::now()));
  TEST_ASSERT_FALSE(r.ota.active());
  // MD5-Format
  r.port.beginOk = true;
  TEST_ASSERT_EQUAL(Err::InvalidArgument, r.ota.start(10, "abc", hal::now()));
  // Leerlauf-Timeout
  r.ota.start(10, nullptr, hal::now());
  native::advance(59000);
  r.ota.tick(hal::now());
  TEST_ASSERT_TRUE(r.ota.active());
  native::advance(2000);
  r.ota.tick(hal::now());
  TEST_ASSERT_FALSE(r.ota.active());
  TEST_ASSERT_NOT_NULL(strstr(r.ota.error(), "60 s"));
  // Neuer Upload bricht einen laufenden ab
  r.ota.start(10, nullptr, hal::now());
  r.ota.start(10, nullptr, hal::now());
  TEST_ASSERT_TRUE(r.ota.active());
  // Ereignisse
  Event e;
  int started = 0, finished = 0;
  while (r.ev.pop(e)) {
    started += e.type == EventType::OtaStarted;
    finished += e.type == EventType::OtaFinished;
  }
  TEST_ASSERT_TRUE(started >= 5);
  TEST_ASSERT_TRUE(finished >= 3);
}

// ---------------------------------------------------------------------------
// Zeit, Task-Takt, Systeminfo, LED
// ---------------------------------------------------------------------------
void test_time_service() {
  EventQueue<16> ev;
  TimeService t(ev);
  t.begin();
  t.tick();
  TEST_ASSERT_FALSE(t.valid());
  native::setEpoch(1700000000);
  t.tick();
  TEST_ASSERT_TRUE(t.valid());
  t.tick();
  Event e;
  int n = 0;
  while (ev.pop(e))
    n += e.type == EventType::ClockValid;
  TEST_ASSERT_EQUAL(1, n); // nur einmal
  native::setEpoch(5);
  t.tick();
  TEST_ASSERT_FALSE(t.valid());
}

namespace {
int g_calls[3];
Instant fakeClock() { return hal::now(); }
} // namespace

void test_task_scheduler() {
  g_calls[0] = g_calls[1] = g_calls[2] = 0;
  Task tasks[] = {
      {[](void*, Instant) { g_calls[0]++; }, nullptr, 0, Instant(), "immer"},
      {[](void*, Instant) { g_calls[1]++; }, nullptr, 100, Instant(), "100"},
      {[](void*, Instant) {
         g_calls[2]++;
         native::advance(80); // langsamer Task
       },
       nullptr, 1000, Instant(), "langsam"},
  };
  TaskScheduler s(tasks, 3);
  s.start(hal::now());
  for (int i = 0; i < 100; i++) {
    s.run(fakeClock);
    native::advance(20);
  }
  TEST_ASSERT_EQUAL(100, g_calls[0]);
  TEST_ASSERT_TRUE(g_calls[1] >= 15 && g_calls[1] <= 30);
  TEST_ASSERT_TRUE(g_calls[2] >= 2 && g_calls[2] <= 5);
  TEST_ASSERT_TRUE(s.overruns() >= 2);
  TEST_ASSERT_TRUE(s.maxLoopMs() >= 80);
}

void test_system_info() {
  native::MemFs fs;
  {
    SystemInfo a(fs);
    a.begin();
    TEST_ASSERT_EQUAL(1u, a.rebootCount());
    a.persist();
    a.persist(); // nur einmal geschrieben
  }
  SystemInfo b(fs);
  b.begin();
  TEST_ASSERT_EQUAL(2u, b.rebootCount());
  TEST_ASSERT_EQUAL(30000u, b.heap().free);
  TEST_ASSERT_EQUAL(30000u, b.minFreeHeap());
  TEST_ASSERT_TRUE(b.fsInfo().total > 0);
  // Laufzeit über den millis()-Überlauf
  native::setMillis(0xFFFFF000u);
  b.tick(hal::now());
  native::setMillis(1000);
  b.tick(hal::now());
  TEST_ASSERT_TRUE(b.uptimeS() > 4294967u);
  pstest::StdSink s;
  JsonWriter w(s);
  w.beginObject();
  b.writeHeap(w);
  w.endObject();
  TEST_ASSERT_NOT_NULL(strstr(s.s.c_str(), "\"minFreeHeap\":30000"));
}

void test_led_service_modes() {
  Rig r;
  r.measureAll(5000);
  FakeLeds leds;
  LedService led(leds, r.cfg, r.sensors);
  // Modus 2, Kanal ANALOG_1 (invertiert, Roh 500 von 250..750 → 50 % → grün)
  led.tick(hal::now());
  TEST_ASSERT_TRUE(leds.g && !leds.y && !leds.r);
  r.admin(hal::HttpMethod::Patch, "/api/v1/config", "{\"led\":{\"onlyRed\":true}}");
  led.tick(hal::now());
  TEST_ASSERT_TRUE(!leds.g && !leds.y && !leds.r);
  // Modus 0: sofort aus (B-37)
  r.admin(hal::HttpMethod::Patch, "/api/v1/config", "{\"led\":{\"mode\":0,\"onlyRed\":false}}");
  led.tick(hal::now());
  TEST_ASSERT_TRUE(!leds.g && !leds.y && !leds.r);
  // Modus 1: folgt dem Display
  int slot = -1;
  led.setDisplaySource([](void* c) { return *static_cast<int*>(c); }, &slot);
  r.admin(hal::HttpMethod::Patch, "/api/v1/config", "{\"led\":{\"mode\":1}}");
  led.tick(hal::now());
  TEST_ASSERT_TRUE(!leds.g && !leds.y && !leds.r); // kein Kanal-Bildschirm
  slot = 1;
  led.tick(hal::now());
  TEST_ASSERT_TRUE(leds.g);
  // Kanal deaktiviert: aus
  r.admin(hal::HttpMethod::Patch, "/api/v1/sensors",
          "{\"ANALOG\":{\"channels\":{\"1\":{\"enabled\":false}}}}");
  led.tick(hal::now());
  TEST_ASSERT_FALSE(leds.g);
}

// ---------------------------------------------------------------------------
// Sensordienst
// ---------------------------------------------------------------------------
void test_clamp_warning_once_until_back_in_range() {
  Rig r;
  log::attach(&r.log);
  auto count = [&r]() {
    pstest::StdSink out;
    JsonWriter w(out);
    r.log.writeSince(w, 0, 200);
    size_t n = 0;
    for (size_t at = out.s.find("außerhalb"); at != std::string::npos;
         at = out.s.find("außerhalb", at + 1))
      n++;
    return n;
  };
  r.admin(hal::HttpMethod::Patch, "/api/v1/sensors", "{\"ANALOG\":{\"interval\":10}}");
  r.drivers.of(SensorType::Analog).raw = 900; // über rawMax 750 von Kanal 1
  r.measureAll(60000);                        // mehrere Messungen
  TEST_ASSERT_EQUAL(1, count());              // nachts nicht bei jeder Messung
  r.drivers.of(SensorType::Analog).raw = 600; // wieder im Bereich
  r.measureAll(15000);
  r.drivers.of(SensorType::Analog).raw = 900;
  r.measureAll(15000);
  TEST_ASSERT_EQUAL(2, count()); // neuer Ausreißer wird wieder gemeldet
  log::attach(nullptr);
}

void test_autocal_moves_limits_and_persists() {
  Rig r;
  r.admin(
      hal::HttpMethod::Patch, "/api/v1/sensors",
      "{\"ANALOG\":{\"interval\":10,\"channels\":{\"1\":{\"autocal\":true,\"halfLife\":21600}}}}");
  r.drivers.of(SensorType::Analog).raw = 900; // über rawMax 750
  r.measureAll(15000);
  TEST_ASSERT_EQUAL(900, r.cfg.sensors().channels[1].rawMax); // sofort erweitert
  TEST_ASSERT_EQUAL_FLOAT(0, r.sensors.channel(1).value);     // invertiert: Maximum = 0 %
  r.drivers.of(SensorType::Analog).raw = 600;
  for (int i = 0; i < 40000; i++) { // 4000 s im 100-ms-Takt, Messung alle 10 s
    native::advance(100);
    r.sensors.tick(hal::now());
  }
  TEST_ASSERT_TRUE(r.cfg.sensors().channels[1].rawMax < 900); // vergisst langsam
  TEST_ASSERT_TRUE(r.cfg.sensors().channels[1].rawMax > 600);
  TEST_ASSERT_TRUE(r.sensors.channel(1).cal.valid());
  r.cfg.tick(hal::now(), true);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        pstest::readFile(r.fs, "/sensors.cfg").find("\"autocal\":true"));
  // Abschalten verwirft den Float-Zustand
  r.admin(hal::HttpMethod::Patch, "/api/v1/sensors",
          "{\"ANALOG\":{\"channels\":{\"1\":{\"autocal\":false}}}}");
  TEST_ASSERT_FALSE(r.sensors.channel(1).cal.valid());
}

void test_tare_takes_raw_counts() {
  Rig r;
  ValueDriver& hx = r.drivers.of(SensorType::Hx711);
  hx.rawCounts = 84210;
  hx.hasCounts = true;
  r.sensors.begin(r.drivers.ptrs, hal::now());
  const uint8_t i = static_cast<uint8_t>(board::sensorIndexOf(SensorType::Hx711));
  TEST_ASSERT_EQUAL(Err::None, r.sensors.tare(i, hal::now()));
  for (int k = 0; k < 50; k++) {
    native::advance(100);
    r.sensors.tick(hal::now());
  }
  TEST_ASSERT_EQUAL(84210, r.cfg.sensors().sensors[i].tare);
  TEST_ASSERT_EQUAL(84210, hx.lastCfg.tare);
  TEST_ASSERT_EQUAL(Err::Unsupported, r.sensors.tare(0, hal::now()));
  r.admin(hal::HttpMethod::Patch, "/api/v1/sensors", "{\"HX711\":{\"enabled\":false}}");
  TEST_ASSERT_EQUAL(Err::Disabled, r.sensors.tare(i, hal::now()));
}

namespace {
int g_gateCalls = 0;
bool gate(void*) {
  g_gateCalls++;
  return true;
}
} // namespace

void test_ds18b20_failure_requests_reboot_once() {
  Rig r;
  r.drivers.of(SensorType::Ds18b20).failBegin = true;
  g_gateCalls = 0;
  r.sensors.setRestartGate(gate, nullptr);
  r.sensors.begin(r.drivers.ptrs, hal::now());
  for (int k = 0; k < 400; k++) {
    native::advance(100);
    r.sensors.tick(hal::now());
  }
  TEST_ASSERT_EQUAL(1, g_gateCalls);
  Event e;
  bool reboot = false, failed = false;
  while (r.ev.pop(e)) {
    reboot |= e.type == EventType::RebootRequested;
    failed |= e.type == EventType::SensorFailed;
  }
  TEST_ASSERT_TRUE(reboot);
  TEST_ASSERT_TRUE(failed);
  const uint8_t slot = board::sensor(board::sensorIndexOf(SensorType::Ds18b20)).firstSlot;
  TEST_ASSERT_EQUAL(ChannelStatus::Error, r.sensors.status(slot, hal::now()));
  TEST_ASSERT_FALSE(r.sensors.allMeasuredOnce());
}

void test_invalid_values_and_warmup_status() {
  Rig r;
  r.drivers.of(SensorType::Bmp280).value = 5000; // außerhalb 300..1100 bzw. -40..85
  r.sensors.begin(r.drivers.ptrs, hal::now());
  const uint8_t mh = board::sensor(board::sensorIndexOf(SensorType::Mhz19)).firstSlot;
  TEST_ASSERT_EQUAL(ChannelStatus::Warmup, r.sensors.status(mh, hal::now()));
  for (int k = 0; k < 200; k++) {
    native::advance(100);
    r.sensors.tick(hal::now());
  }
  const uint8_t b = board::sensor(board::sensorIndexOf(SensorType::Bmp280)).firstSlot;
  TEST_ASSERT_TRUE(isnan(r.sensors.channel(b).value));
  // Unbekannter Sensorname
  TEST_ASSERT_EQUAL(-1, SensorService::sensorByName("NICHTS"));
  TEST_ASSERT_EQUAL(-1, SensorService::sensorByName(nullptr));
  uint32_t w;
  TEST_ASSERT_EQUAL(MeasureResult::NotFound, r.sensors.measureNow(200, hal::now(), false, w));
  TEST_ASSERT_EQUAL(ChannelStatus::Unknown, r.sensors.status(250, hal::now()));
}

void test_pause_aborts_and_resumes() {
  Rig r;
  r.drivers.of(SensorType::Analog).hang = true;
  r.sensors.begin(r.drivers.ptrs, hal::now());
  // Eine hängende Runde läuft nach 200 ms in ihre Rundenfrist; direkt nach
  // dem Start ist der Auftrag aber noch aktiv
  native::advance(100);
  r.sensors.tick(hal::now());
  TEST_ASSERT_TRUE(r.sensors.run(0).active);
  r.sensors.pause(true);
  TEST_ASSERT_FALSE(r.sensors.run(0).active);
  for (int k = 0; k < 100; k++) {
    native::advance(100);
    r.sensors.tick(hal::now());
  }
  TEST_ASSERT_FALSE(r.sensors.run(0).active);
  r.sensors.pause(false);
  r.drivers.of(SensorType::Analog).hang = false;
  for (int k = 0; k < 100; k++) {
    native::advance(100);
    r.sensors.tick(hal::now());
  }
  TEST_ASSERT_TRUE(r.sensors.measuredOnce(0));
}

// ---------------------------------------------------------------------------
// Netz
// ---------------------------------------------------------------------------
void test_net_rename_and_reconnect() {
  Rig r;
  r.cfg.setWifi(0, "A", "", false);
  r.wifi.reachable["A"] = true;
  r.net.begin(hal::now());
  r.net.tick(hal::now());
  TEST_ASSERT_TRUE(r.net.connected());
  char buf[40];
  r.net.ssid(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("A", buf);
  r.net.ip(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("192.168.1.23", buf);
  TEST_ASSERT_EQUAL(-60, r.net.rssi());
  // Umbenennen: mDNS neu
  r.admin(hal::HttpMethod::Patch, "/api/v1/config", "{\"general\":{\"deviceName\":\"Küche Süd\"}}");
  r.net.configChanged(hal::now(), false);
  TEST_ASSERT_EQUAL_STRING("kueche-sued", r.wifi.mdnsHost.c_str());
  TEST_ASSERT_EQUAL_STRING("kueche-sued", r.net.hostname());
  // Reconnect angefordert: beginnt von vorn
  const size_t tried = r.wifi.ssidTried.size();
  r.net.configChanged(hal::now(), true);
  TEST_ASSERT_EQUAL(tried + 1, r.wifi.ssidTried.size());
  // AP-SSID wird auf 32 Byte gekürzt
  char ap[40];
  r.admin(hal::HttpMethod::Patch, "/api/v1/config",
          "{\"general\":{\"deviceName\":\"ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\"}}");
  r.net.apSsid(ap, sizeof(ap));
  TEST_ASSERT_EQUAL(32, strlen(ap));
}

void test_net_failed_status_switches_early_and_ap_reconnect_request() {
  Rig r;
  r.cfg.setWifi(2, "", "", false); // ohne Fabmobil-Voreinstellung
  r.cfg.setWifi(0, "A", "", false);
  r.cfg.setWifi(1, "B", "", false);
  r.net.begin(hal::now());
  r.wifi.status = hal::StaStatus::Failed; // falsches Passwort
  native::advance(100);
  r.net.tick(hal::now());
  TEST_ASSERT_EQUAL(2, r.wifi.ssidTried.size()); // sofort weiter, nicht erst nach 10 s
  r.wifi.status = hal::StaStatus::Failed;
  native::advance(100);
  r.net.tick(hal::now());
  TEST_ASSERT_TRUE(r.net.apActive());
  char ip[16];
  r.net.ip(ip, sizeof(ip));
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", ip);
  // Neuer Slot im AP-Modus: Probe sofort statt nach 5 min
  r.cfg.setWifi(2, "C", "", false);
  r.wifi.reachable["C"] = true;
  r.net.configChanged(hal::now(), true);
  for (int i = 0; i < 400; i++) {
    native::advance(100);
    r.net.tick(hal::now());
  }
  TEST_ASSERT_TRUE(r.net.connected());
}

// ---------------------------------------------------------------------------
// Chronik
// ---------------------------------------------------------------------------
void test_chronik_full_fs_and_recheck() {
  Rig r;
  r.fs.total = 60 * 1024; // weniger als die Reserve
  r.chronik.begin(false);
  TEST_ASSERT_FALSE(r.chronik.enabled());
  TEST_ASSERT_EQUAL(0, r.chronik.targetSegments());
  r.chronik.record(0, 1700000000, hal::now()); // wird ignoriert
  r.fs.total = 950 * 1024;
  r.chronik.requestBudgetRecheck();
  r.chronik.tick(hal::now(), false);
  TEST_ASSERT_TRUE(r.chronik.enabled());
  // Datei-Log an: größere Reserve
  const uint8_t before = r.chronik.targetSegments();
  r.admin(hal::HttpMethod::Patch, "/api/v1/config", "{\"log\":{\"file\":true}}");
  r.chronik.requestBudgetRecheck();
  r.chronik.tick(hal::now(), false);
  TEST_ASSERT_TRUE(r.chronik.targetSegments() <= before);
}

void test_chronik_stream_skips_old_segments() {
  Rig r;
  r.chronik.begin(false);
  // Drei Segmente mit aufsteigenden Startzeiten von Hand anlegen
  for (uint32_t i = 0; i < 3; i++) {
    char name[chronik::kNameBuffer];
    chronik::nameFromIndex(i, name);
    uint8_t buf[512];
    chronik::TableBuilder tb(buf, sizeof(buf), 1700000000 + i * 10000);
    tb.add(0, false, false, "DHT_0", "T", "C", Limits{}, chronik::Encoding{});
    const size_t n = tb.finish();
    r.fs.files_[name] = std::vector<uint8_t>(buf, buf + n);
  }
  ChronikService c(r.fs, r.cfg, r.sensors);
  c.begin(false);
  TEST_ASSERT_EQUAL(3, c.segmentCount());
  pstest::StdSink all, recent;
  c.stream(0, all);
  c.stream(1700015000, recent); // nur Segmente 1 und 2 (+ aktuelle Tabelle)
  TEST_ASSERT_TRUE(recent.s.size() < all.s.size());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, all.s.find("DHT_0"));
}

void test_chronik_budget_counts_whole_blocks() {
  // Platz für genau drei Segmente: ein frisch begonnenes (kleines) Segment
  // belegt schon einen ganzen Block und darf das älteste nicht verdrängen.
  Rig r;
  hal::FsInfo i;
  r.fs.info(i);
  r.fs.total = i.used + chronik::reserveBytes(false) + 3 * chronik::kSegmentFootprint + 100;
  for (uint32_t k = 0; k < 3; k++) {
    char name[chronik::kNameBuffer];
    chronik::nameFromIndex(k, name);
    uint8_t buf[512];
    chronik::TableBuilder tb(buf, sizeof(buf), 1700000000 + k * 10000);
    tb.add(0, false, false, "DHT_0", "T", "C", Limits{}, chronik::Encoding{});
    std::vector<uint8_t> v(buf, buf + tb.finish());
    if (k < 2)
      v.resize(7900, 0); // ältere Segmente voll
    r.fs.files_[name] = v;
  }
  ChronikService c(r.fs, r.cfg, r.sensors);
  c.begin(false);
  TEST_ASSERT_EQUAL(3, c.targetSegments());
  TEST_ASSERT_EQUAL(3, c.segmentCount());
}

void test_chronik_disabled_for_space_rechecks_itself() {
  Rig r;
  r.fs.total = 60 * 1024;
  r.chronik.begin(false);
  TEST_ASSERT_FALSE(r.chronik.enabled());
  r.fs.total = 950 * 1024; // z.B. Altlasten gelöscht
  native::advance(ChronikService::kBudgetRecheckMs - 1000);
  r.chronik.tick(hal::now(), false);
  TEST_ASSERT_FALSE(r.chronik.enabled()); // noch nicht fällig
  native::advance(2000);
  r.chronik.tick(hal::now(), false);
  TEST_ASSERT_TRUE(r.chronik.enabled());
}

void test_storage_own_files() {
  TEST_ASSERT_TRUE(storage::isOwnFile("/device.cfg"));
  TEST_ASSERT_TRUE(storage::isOwnFile("/device.cfg.tmp"));
  TEST_ASSERT_TRUE(storage::isOwnFile("/log.txt.tmp"));
  TEST_ASSERT_TRUE(storage::isOwnFile("/import.tmp"));
  TEST_ASSERT_TRUE(storage::isOwnFile("/chr_0000001f.dat"));
  TEST_ASSERT_TRUE(storage::isOwnFile("mail.tpl"));
  TEST_ASSERT_FALSE(storage::isOwnFile("/chr_xyz.dat"));
  TEST_ASSERT_FALSE(storage::isOwnFile("/config.json"));
  TEST_ASSERT_FALSE(storage::isOwnFile("/foo.cfg.tmp"));
  TEST_ASSERT_FALSE(storage::isOwnFile("/.tmp"));
  TEST_ASSERT_FALSE(storage::isOwnFile("/device.cfg.bak"));
  TEST_ASSERT_FALSE(storage::isOwnFile(nullptr));
}

void test_storage_remove_foreign_legacy_layout() {
  native::MemFs fs;
  const char* own[] = {"/device.cfg", "/sensors.cfg.tmp", "/secrets.cfg",
                       "/state.bin",  "/mail.tpl",        "/boot.cnt",
                       "/log.txt",    "/import.tmp",      "/chr_0000000a.dat"};
  const char* foreign[] = {"/index.html",        "/css/app.css", "/js/a.js",
                           "/js/b.js",           "/nvs/ns/key",  "/config/a/b.json",
                           "/prefs_backup.json", "/.boot_loop",  "/chr_xyz.dat"};
  for (const char* n : own)
    fs.files_[n] = std::vector<uint8_t>(10, 1);
  for (const char* n : foreign)
    fs.files_[n] = std::vector<uint8_t>(9000, 2);
  const storage::CleanupResult r = storage::removeForeign(fs);
  TEST_ASSERT_EQUAL(8,
                    r.removed); // index.html, css/, js/, nvs/, config/, prefs, .boot_loop, chr_xyz
  TEST_ASSERT_EQUAL(0, r.failed);
  TEST_ASSERT_TRUE(r.freedBytes >= 9 * 8192);
  TEST_ASSERT_EQUAL(sizeof(own) / sizeof(own[0]), fs.files_.size());
  for (const char* n : own)
    TEST_ASSERT_TRUE(fs.exists(n));
  // zweiter Lauf: nichts mehr zu tun
  TEST_ASSERT_EQUAL(0, storage::removeForeign(fs).removed);
}

void test_boot_guard_sensor_counter_cleared() {
  native::setResetReason(hal::ResetReason::Watchdog);
  BootGuard g;
  g.begin(hal::resetReason());
  TEST_ASSERT_TRUE(g.allowSensorRestart());
  native::setMillis(700000);
  g.tick(hal::now());
  BootGuard g2;
  g2.begin(hal::resetReason());
  for (int i = 0; i < 3; i++)
    TEST_ASSERT_TRUE(g2.allowSensorRestart());
  TEST_ASSERT_FALSE(BootGuard::sensorRestartGate(&g2));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_display_absent);
  RUN_TEST(test_display_boot_and_rotation);
  RUN_TEST(test_display_rotation_pushes_in_page_steps);
  RUN_TEST(test_display_clock_skipped_without_time_and_ampm);
  RUN_TEST(test_display_qr_and_ota);
  RUN_TEST(test_ota_service_paths);
  RUN_TEST(test_time_service);
  RUN_TEST(test_task_scheduler);
  RUN_TEST(test_system_info);
  RUN_TEST(test_led_service_modes);
  RUN_TEST(test_clamp_warning_once_until_back_in_range);
  RUN_TEST(test_autocal_moves_limits_and_persists);
  RUN_TEST(test_tare_takes_raw_counts);
  RUN_TEST(test_ds18b20_failure_requests_reboot_once);
  RUN_TEST(test_invalid_values_and_warmup_status);
  RUN_TEST(test_pause_aborts_and_resumes);
  RUN_TEST(test_net_rename_and_reconnect);
  RUN_TEST(test_net_failed_status_switches_early_and_ap_reconnect_request);
  RUN_TEST(test_chronik_full_fs_and_recheck);
  RUN_TEST(test_chronik_stream_skips_old_segments);
  RUN_TEST(test_boot_guard_sensor_counter_cleared);
  RUN_TEST(test_chronik_budget_counts_whole_blocks);
  RUN_TEST(test_chronik_disabled_for_space_rechecks_itself);
  RUN_TEST(test_storage_own_files);
  RUN_TEST(test_storage_remove_foreign_legacy_layout);
  return UNITY_END();
}
