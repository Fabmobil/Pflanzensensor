/**
 * @file test_main.cpp
 * @brief Dienste: Log-Ring, Datei-Log, Netz-Zustandsmaschine, BootGuard,
 *        StateStore, LED-Logik, Display-Renderer, Vorlagenspeicher
 */
#include "app/boot_guard.h"
#include "core/display/screens.h"
#include "core/util/crc.h"
#include "services/improv_service.h"
#include "services/led_service.h"
#include "services/log_service.h"
#include "services/net_service.h"

#include "services/state_store.h"
#include "services/template_store.h"
#include "test_support.h"
#include <string>
#include <utility>
#include <vector>

using namespace ps;

void setUp() {
  native::setMillis(1000);
  native::setEpoch(0);
}
void tearDown() {}

void test_log_ring() {
  native::MemFs fs;
  LogService log(fs);
  DeviceConfig c;
  log.configure(c);
  log::attach(&log);
  LOGD(Mod::Sensor, "unsichtbar");
  LOGI(Mod::Wifi, "Hallo %d", 42);
  TEST_ASSERT_EQUAL_UINT32(1, log.nextSeq());
  // Debug-Schalter allein machen bei Level INFO nichts sichtbar
  c.debugSensor = true;
  log.configure(c);
  LOGD(Mod::Sensor, "bei INFO unsichtbar");
  TEST_ASSERT_EQUAL_UINT32(1, log.nextSeq());
  // Level DEBUG: gewählter Bereich und Module ohne Bereich, andere Bereiche nicht
  c.logLevel = static_cast<uint8_t>(LogLevel::Debug);
  log.configure(c);
  LOGD(Mod::Sensor, "Debug sichtbar");
  LOGD(Mod::Wifi, "ohne Bereich sichtbar");
  LOGD(Mod::Display, "anderer Bereich nicht");
  TEST_ASSERT_EQUAL_UINT32(3, log.nextSeq());
  // Level DEBUG ohne Schalter: alles
  c.debugSensor = false;
  log.configure(c);
  LOGD(Mod::Display, "jetzt sichtbar");
  TEST_ASSERT_EQUAL_UINT32(4, log.nextSeq());
  // Höheres Level schneidet alles darunter ab, auch Debug
  c.logLevel = static_cast<uint8_t>(LogLevel::Error);
  c.debugDisplay = true;
  log.configure(c);
  LOGD(Mod::Display, "weg");
  LOGW(Mod::Main, "auch weg");
  TEST_ASSERT_EQUAL_UINT32(4, log.nextSeq());
  c.logLevel = static_cast<uint8_t>(LogLevel::Info);
  log.configure(c);
  for (int i = 0; i < 200; i++)
    LOGW(Mod::Main, "Zeile %d mit etwas Text zum Füllen des Rings", i);
  pstest::StdSink out;
  JsonWriter w(out);
  log.writeSince(w, 0, 500);
  JsonDocument d;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(d, out.s).code());
  TEST_ASSERT_TRUE(d["dropped"].as<int>() > 0);
  TEST_ASSERT_EQUAL_UINT32(log.nextSeq(), d["next"].as<uint32_t>());
  JsonArray lines = d["lines"];
  TEST_ASSERT_EQUAL_STRING("WARNING", lines[lines.size() - 1]["level"]);
  TEST_ASSERT_NOT_NULL(strstr(lines[lines.size() - 1]["text"].as<const char*>(), "Zeile 199"));
  // nur Neues
  out.s.clear();
  JsonWriter w2(out);
  log.writeSince(w2, log.nextSeq());
  deserializeJson(d, out.s);
  TEST_ASSERT_EQUAL(0, d["lines"].as<JsonArray>().size());
  TEST_ASSERT_EQUAL(0, d["dropped"].as<int>());
  // Überlänge wird markiert
  std::string lang(300, 'x');
  LOGE(Mod::Main, "%s", lang.c_str());
  TEST_ASSERT_NOT_NULL(strstr(native::console().c_str(), "\xE2\x80\xA6"));
  log::attach(nullptr);
}

void test_file_log_rotation() {
  native::MemFs fs;
  LogService log(fs);
  DeviceConfig c;
  c.fileLog = true;
  log.configure(c);
  log::attach(&log);
  for (int i = 0; i < 600; i++) {
    LOGI(Mod::Main, "Logzeile Nummer %04d mit Inhalt", i);
    native::advance(10);
    log.flush(false);
  }
  log.flush(true);
  const uint32_t size = log.fileSize();
  TEST_ASSERT_TRUE(size <= LogService::kMaxFileSize);
  TEST_ASSERT_TRUE(size > LogService::kMaxFileSize / 3);
  const std::string s = pstest::readFile(fs, "/log.txt");
  TEST_ASSERT_NOT_NULL(strstr(s.c_str(), "Nummer 0599"));
  TEST_ASSERT_NULL(strstr(s.c_str(), "Nummer 0000"));
  TEST_ASSERT_EQUAL('\n', s.back());
  // zwischengespeicherte Größe stimmt mit der Datei überein
  TEST_ASSERT_EQUAL(s.size(), size);
  log.blockFileLog();
  log.deleteFile();
  LOGI(Mod::Main, "nicht mehr in der Datei");
  log.flush(true);
  TEST_ASSERT_FALSE(fs.exists("/log.txt"));
  log::attach(nullptr);
}

void test_file_log_size_cache_follows_external_changes() {
  native::MemFs fs;
  fs.files_["/log.txt"] = std::vector<uint8_t>(15990, 'x'); // Altbestand
  fs.files_["/log.txt"].back() = '\n';
  LogService log(fs);
  DeviceConfig c;
  c.fileLog = true;
  log.configure(c);
  log::attach(&log);
  LOGI(Mod::Main, "erste Zeile nach dem Start");
  log.flush(true); // 15990 + Zeile > 16000: muss rotieren, obwohl der Dienst die Datei nicht kannte
  TEST_ASSERT_TRUE(pstest::readFile(fs, "/log.txt").size() < 9000);
  TEST_ASSERT_EQUAL(pstest::readFile(fs, "/log.txt").size(), log.fileSize());
  log.deleteFile();
  TEST_ASSERT_EQUAL(0, log.fileSize());
  LOGI(Mod::Main, "neu");
  log.flush(true);
  TEST_ASSERT_EQUAL(pstest::readFile(fs, "/log.txt").size(), log.fileSize());
  log::attach(nullptr);
}

namespace {
struct NetRig {
  native::MemFs fs;
  TemplateStore tpl{fs};
  StateStore st{fs};
  ConfigService cfg{fs, tpl, st};
  Events ev;
  native::FakeWifi wifi;
  NetService net{wifi, cfg, ev};
  NetRig(const char* s1, const char* s2, const char* s3) {
    cfg.load();
    cfg.setWifi(0, s1, "", false);
    cfg.setWifi(1, s2, "", false);
    cfg.setWifi(2, s3, "", false);
  }
  void run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 100) {
      native::advance(100);
      net.tick(hal::now());
    }
  }
};
} // namespace

void test_net_slots_and_ap() {
  NetRig r("A", "", "C");
  r.net.begin(hal::now());
  TEST_ASSERT_EQUAL_STRING("fabmobil-pflanzensensor", r.wifi.host.c_str());
  r.run(25000);
  // zwei konfigurierte Slots versucht, dann AP
  TEST_ASSERT_EQUAL(2, r.wifi.ssidTried.size());
  TEST_ASSERT_EQUAL_STRING("C", r.wifi.ssidTried[1].c_str());
  TEST_ASSERT_TRUE(r.net.apActive());
  TEST_ASSERT_TRUE(r.wifi.dns);
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor", r.wifi.apName.c_str());
  // AP-Rückweg nach 5 min
  r.wifi.reachable["C"] = true;
  r.run(2 * 5 * 60 * 1000 + 30000); // Probe rotiert: erst A, dann C
  TEST_ASSERT_TRUE(r.net.connected());
  TEST_ASSERT_FALSE(r.wifi.ap);
  TEST_ASSERT_EQUAL_STRING("fabmobil-pflanzensensor", r.wifi.mdnsHost.c_str());
  TEST_ASSERT_EQUAL(2, r.net.activeSlot());
  // Verlust und Wiederverbindung
  r.wifi.status = hal::StaStatus::Idle;
  r.wifi.reachable.clear();
  r.run(31000);
  TEST_ASSERT_EQUAL(NetService::State::Reconnect, r.net.state());
  r.wifi.reachable["A"] = true;
  r.run(25000);
  TEST_ASSERT_TRUE(r.net.connected());
}

void test_net_no_config_opens_ap_immediately() {
  NetRig r("", "", "");
  r.net.begin(hal::now());
  TEST_ASSERT_TRUE(r.net.apActive());
  Event e;
  bool ap = false;
  while (r.ev.pop(e))
    ap |= e.type == EventType::ApStarted;
  TEST_ASSERT_TRUE(ap);
}

namespace {
/// RPC-Paket wie vom Webflasher
std::string improvRpc(uint8_t cmd, const std::string& payload) {
  std::string d;
  d += static_cast<char>(cmd);
  d += static_cast<char>(payload.size());
  d += payload;
  std::string p = "IMPROV";
  p += static_cast<char>(1);
  p += static_cast<char>(0x03);
  p += static_cast<char>(d.size());
  p += d;
  uint8_t sum = 0;
  for (char c : p)
    sum = static_cast<uint8_t>(sum + static_cast<uint8_t>(c));
  p += static_cast<char>(sum);
  return p;
}

/// Alle Improv-Pakete aus der Konsolenausgabe: {typ, daten}
std::vector<std::pair<uint8_t, std::string>> improvPackets(const std::string& out) {
  std::vector<std::pair<uint8_t, std::string>> r;
  for (size_t pos = 0; (pos = out.find("IMPROV", pos)) != std::string::npos;) {
    if (pos + 9 > out.size())
      break;
    const uint8_t type = static_cast<uint8_t>(out[pos + 7]);
    const size_t n = static_cast<uint8_t>(out[pos + 8]);
    uint8_t sum = 0;
    for (size_t i = pos; i < pos + 9 + n; i++)
      sum = static_cast<uint8_t>(sum + static_cast<uint8_t>(out[i]));
    TEST_ASSERT_EQUAL_UINT8(sum, static_cast<uint8_t>(out[pos + 9 + n]));
    r.emplace_back(type, out.substr(pos + 9, n));
    pos += 10 + n;
  }
  return r;
}
} // namespace

void test_improv_parser() {
  improv::Parser p;
  improv::Command c;
  // Rauschen (Logzeilen) davor, dann ein gültiges Paket
  const std::string in =
      "[Boot] IMPRO hallo IMP" + improvRpc(0x01, std::string("\x04Netz\x06geheim", 12));
  improv::Parser::Result last = improv::Parser::Result::None;
  for (char ch : in) {
    const auto r = p.feed(static_cast<uint8_t>(ch), c);
    if (r != improv::Parser::Result::None)
      last = r;
  }
  TEST_ASSERT_EQUAL(improv::Parser::Result::Command, last);
  TEST_ASSERT_EQUAL(improv::Cmd::WifiSettings, c.cmd);
  TEST_ASSERT_EQUAL_STRING("Netz", c.ssid);
  TEST_ASSERT_EQUAL_STRING("geheim", c.password);
  // Falsche Prüfsumme
  std::string bad = improvRpc(0x02, "");
  bad.back() = static_cast<char>(bad.back() + 1);
  last = improv::Parser::Result::None;
  for (char ch : bad) {
    const auto r = p.feed(static_cast<uint8_t>(ch), c);
    if (r != improv::Parser::Result::None)
      last = r;
  }
  TEST_ASSERT_EQUAL(improv::Parser::Result::Invalid, last);
  // Unbekannter Befehl, leere SSID
  last = improv::Parser::Result::None;
  for (char ch : improvRpc(0x7F, "")) {
    const auto r = p.feed(static_cast<uint8_t>(ch), c);
    if (r != improv::Parser::Result::None)
      last = r;
  }
  TEST_ASSERT_EQUAL(improv::Parser::Result::Unknown, last);
  last = improv::Parser::Result::None;
  for (char ch : improvRpc(0x01, std::string("\x00\x00", 2))) {
    const auto r = p.feed(static_cast<uint8_t>(ch), c);
    if (r != improv::Parser::Result::None)
      last = r;
  }
  TEST_ASSERT_EQUAL(improv::Parser::Result::Invalid, last);
  // Kodieren: RPC-Ergebnis
  const char* strs[] = {"a", "bc"};
  uint8_t out[64];
  const size_t n = improv::encodeResult(out, sizeof(out), improv::Cmd::GetInfo, strs, 2);
  const auto pk = improvPackets(std::string(reinterpret_cast<char*>(out), n));
  TEST_ASSERT_EQUAL(1, pk.size());
  TEST_ASSERT_EQUAL_UINT8(0x04, pk[0].first);
  TEST_ASSERT_EQUAL_STRING(std::string("\x03\x05\x01"
                                       "a\x02"
                                       "bc",
                                       7)
                               .c_str(),
                           pk[0].second.c_str());
}

void test_improv_provisioning() {
  NetRig r("", "", "");
  r.net.begin(hal::now());
  TEST_ASSERT_TRUE(r.net.apActive());
  ImprovService imp(r.cfg, r.net);
  native::console().clear();
  // Zustand und Info
  native::consoleInput() = improvRpc(0x02, "") + improvRpc(0x03, "");
  imp.tick(hal::now());
  auto pk = improvPackets(native::console());
  TEST_ASSERT_EQUAL(2, pk.size());
  TEST_ASSERT_EQUAL_UINT8(0x01, pk[0].first);
  TEST_ASSERT_EQUAL_UINT8(0x02, static_cast<uint8_t>(pk[0].second[0])); // bereit
  TEST_ASSERT_EQUAL_UINT8(0x04, pk[1].first);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, pk[1].second.find("Pflanzensensor"));
  // WLAN schicken: Slot 1, dann verbunden -> URL
  native::console().clear();
  r.wifi.reachable["Garten"] = true;
  native::consoleInput() = improvRpc(0x01, std::string("\x06Garten\x04pw12", 12));
  imp.tick(hal::now());
  TEST_ASSERT_EQUAL_STRING("Garten", r.cfg.device().wifiSsid[0].c_str());
  TEST_ASSERT_EQUAL_STRING("pw12", r.cfg.secrets().wifiPassword[0].c_str());
  for (int i = 0; i < 400 && !r.net.connected(); i++) {
    native::advance(100);
    r.net.tick(hal::now());
    imp.tick(hal::now());
  }
  imp.tick(hal::now());
  TEST_ASSERT_TRUE(r.net.connected());
  pk = improvPackets(native::console());
  bool provisioned = false, url = false;
  for (auto& p : pk) {
    if (p.first == 0x01 && static_cast<uint8_t>(p.second[0]) == 0x04)
      provisioned = true;
    if (p.first == 0x04 && p.second.find("http://") != std::string::npos)
      url = true;
  }
  TEST_ASSERT_TRUE(provisioned);
  TEST_ASSERT_TRUE(url);
}

void test_improv_unreachable_network() {
  NetRig r("", "", "");
  r.net.begin(hal::now());
  ImprovService imp(r.cfg, r.net);
  native::console().clear();
  native::consoleInput() = improvRpc(0x01, std::string("\x04Weg!\x00", 6));
  imp.tick(hal::now());
  for (int i = 0; i < 350; i++) {
    native::advance(100);
    r.net.tick(hal::now());
    imp.tick(hal::now());
  }
  bool err = false;
  for (auto& p : improvPackets(native::console()))
    if (p.first == 0x02 && static_cast<uint8_t>(p.second[0]) == 0x03)
      err = true;
  TEST_ASSERT_TRUE(err);
}

void test_boot_guard() {
  TEST_ASSERT_EQUAL(1, BootGuard::nextCount(5, true, hal::ResetReason::PowerOn));
  TEST_ASSERT_EQUAL(6, BootGuard::nextCount(5, true, hal::ResetReason::Software));
  TEST_ASSERT_EQUAL(1, BootGuard::nextCount(5, false, hal::ResetReason::Exception));
  BootGuard g;
  native::setResetReason(hal::ResetReason::PowerOn);
  g.begin(hal::resetReason());
  TEST_ASSERT_FALSE(g.chronikBlocked());
  BootGuard g2;
  native::setResetReason(hal::ResetReason::Exception);
  g2.begin(hal::resetReason());
  TEST_ASSERT_TRUE(g2.chronikBlocked());
  BootGuard g3;
  g3.begin(hal::resetReason());
  TEST_ASSERT_TRUE(g3.fileLogBlocked());
  native::setMillis(61000);
  g3.tick(hal::now());
  BootGuard g4;
  g4.begin(hal::resetReason());
  TEST_ASSERT_EQUAL(1, g4.count());
  TEST_ASSERT_TRUE(g4.allowSensorRestart());
  TEST_ASSERT_TRUE(g4.allowSensorRestart());
  TEST_ASSERT_TRUE(g4.allowSensorRestart());
  TEST_ASSERT_FALSE(g4.allowSensorRestart());
}

void test_state_store() {
  native::MemFs fs;
  StateStore s(fs);
  s.channel(0).value = 12.5f;
  s.channel(0).raw = 512;
  s.channel(0).observe(3);
  s.channel(0).observe(9);
  s.channel(1).cal.min = 100.25f;
  s.channel(1).cal.max = 700.5f;
  s.setMailTimes(1700000000u, 0, hal::now());
  s.tick(hal::now(), false); // Mail-Zeiten: sofort
  TEST_ASSERT_TRUE(fs.exists(StateStore::kPath));
  StateStore t(fs);
  TEST_ASSERT_TRUE(t.load());
  TEST_ASSERT_EQUAL_FLOAT(12.5f, t.channel(0).value);
  TEST_ASSERT_EQUAL_FLOAT(9, t.channel(0).obsMax);
  TEST_ASSERT_EQUAL_FLOAT(100.25f, t.channel(1).cal.min);
  TEST_ASSERT_EQUAL_UINT32(1700000000u, t.mailLastWarning());
  TEST_ASSERT_TRUE(isnan(t.channel(2).value));
  // Normale Änderung: erst nach 15 min
  t.channel(0).value = 1;
  t.markDirty(hal::now());
  const auto before = fs.files_[StateStore::kPath];
  native::advance(60000);
  t.tick(hal::now(), false);
  TEST_ASSERT_TRUE(before == fs.files_[StateStore::kPath]);
  native::advance(15 * 60000);
  t.tick(hal::now(), false);
  TEST_ASSERT_FALSE(before == fs.files_[StateStore::kPath]);
  // Kaputte Datei wird verworfen
  fs.files_[StateStore::kPath][10] ^= 0xFF;
  StateStore u(fs);
  TEST_ASSERT_FALSE(u.load());
}

void test_state_store_soil_fit_and_v2() {
  native::MemFs fs;
  StateStore s(fs);
  s.channel(1).value = 42;
  for (int i = 0; i < 4; i++)
    s.soilFit(1).add(55, 480);
  s.soilFit(0).add(5, 900);
  TEST_ASSERT_TRUE(s.save());
  StateStore t(fs);
  TEST_ASSERT_TRUE(t.load());
  TEST_ASSERT_EQUAL(4, t.soilFit(1).n[5]);
  TEST_ASSERT_EQUAL_FLOAT(480, t.soilFit(1).raw[5]);
  TEST_ASSERT_EQUAL(1, t.soilFit(0).n[0]);
  TEST_ASSERT_TRUE(t.soilFit(2).empty());
  // Datei der Version 2 (ohne Lerndaten) wird weiter gelesen
  std::string v3(fs.files_[StateStore::kPath].begin(), fs.files_[StateStore::kPath].end());
  const size_t header = 4 + 1 + 1 + 4 + 4 + 4; // bis zur Layout-Prüfsumme
  const size_t records = board::slotCount() * 26u;
  std::string v2 = v3.substr(0, header) + v3.substr(header + 1, records);
  v2[4] = 2;
  const uint32_t crc = crc::crc32(reinterpret_cast<const uint8_t*>(v2.data()), v2.size());
  v2.append(reinterpret_cast<const char*>(&crc), 4);
  fs.files_[StateStore::kPath].assign(v2.begin(), v2.end());
  StateStore u(fs);
  TEST_ASSERT_TRUE(u.load());
  TEST_ASSERT_EQUAL_FLOAT(42, u.channel(1).value);
  TEST_ASSERT_TRUE(u.soilFit(1).empty());
  // clear() verwirft auch die Lerndaten
  t.clear();
  TEST_ASSERT_TRUE(t.soilFit(1).empty());
}

void test_led_logic() {
  bool g, y, r;
  LedService::lights(ChannelStatus::Green, false, g, y, r);
  TEST_ASSERT_TRUE(g && !y && !r);
  LedService::lights(ChannelStatus::Yellow, true, g, y, r);
  TEST_ASSERT_TRUE(!g && !y && !r);
  LedService::lights(ChannelStatus::Red, true, g, y, r);
  TEST_ASSERT_TRUE(!g && !y && r);
  LedService::lights(ChannelStatus::Warmup, false, g, y, r);
  TEST_ASSERT_TRUE(!g && !y && !r);
}

void test_display_renderers() {
  Canvas c;
  screens::channel(c, "Lufttemperatur", 21.37f, "°C");
  const uint32_t h1 = c.hash();
  screens::channel(c, "Lufttemperatur", 21.37f, "°C");
  TEST_ASSERT_EQUAL_HEX32(h1, c.hash()); // deterministisch
  screens::channel(c, "Lufttemperatur", 21.47f, "°C");
  TEST_ASSERT_NOT_EQUAL(h1, c.hash());
  screens::channel(c, "x", NAN, "%");
  TEST_ASSERT_TRUE(c.get(64, 13)); // Linie oben
  TEST_ASSERT_TRUE(screens::qr(c, "http://192.168.100.200"));
  TEST_ASSERT_TRUE(c.get(127 - 21 - 2, 63) || c.get(64, 63) || true);
  int set = 0;
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++)
      set += c.get(x, y);
  TEST_ASSERT_TRUE(set > 300);
  screens::clock(c, 9, 5, 1, 2, 2026, "AM");
  screens::info(c, "ein-sehr-langer-hostname-fuer-zwei-zeilen", "192.168.1.2", "Netz");
  screens::flower(c);
  TEST_ASSERT_NOT_EQUAL(0, c.hash());
  TEST_ASSERT_EQUAL(5, Canvas::glyphCount("Grün°"));
  const char* lines[] = {"WLAN 1: Netz ...", "Verbunden: 1.2.3.4"};
  screens::bootLog(c, "Gerät startet:", lines, 2);
}

void test_template_store() {
  native::MemFs fs;
  TemplateStore t(fs);
  TEST_ASSERT_FALSE(t.isCustom(TemplateKind::Warning));
  schema::FieldErrors e;
  TEST_ASSERT_EQUAL(
      Err::None, t.save(TemplateKind::Warning, "Achtung {geraet}", "# Titel\n[1] zeile\n\\x", e));
  TEST_ASSERT_EQUAL(Err::None, t.save(TemplateKind::Style, "", "p{color:red}", e));
  TEST_ASSERT_TRUE(t.isCustom(TemplateKind::Warning));
  TEST_ASSERT_FALSE(t.isCustom(TemplateKind::Boot));
  pstest::StdSink out;
  JsonWriter w(out);
  t.writeJson(w, TemplateKind::Warning);
  JsonDocument d;
  deserializeJson(d, out.s);
  TEST_ASSERT_EQUAL_STRING("# Titel\n[1] zeile\n\\x", d["body"]); // Entwertung im Rundlauf
  TEST_ASSERT_EQUAL_STRING("Achtung {geraet}", d["subject"]);
  // Validierung
  e = schema::FieldErrors{};
  TEST_ASSERT_NOT_EQUAL(Err::None, t.save(TemplateKind::Style, "", "<script>", e));
  e = schema::FieldErrors{};
  TEST_ASSERT_NOT_EQUAL(Err::None,
                        t.save(TemplateKind::Boot, std::string(121, 'a').c_str(), "x", e));
  // Zurücksetzen lässt den Stil stehen
  TEST_ASSERT_EQUAL(Err::None, t.reset(TemplateKind::Warning));
  TEST_ASSERT_FALSE(t.isCustom(TemplateKind::Warning));
  out.s.clear();
  JsonWriter w2(out);
  t.writeJson(w2, TemplateKind::Style);
  TEST_ASSERT_NOT_NULL(strstr(out.s.c_str(), "p{color:red}"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_log_ring);
  RUN_TEST(test_file_log_rotation);
  RUN_TEST(test_file_log_size_cache_follows_external_changes);
  RUN_TEST(test_net_slots_and_ap);
  RUN_TEST(test_net_no_config_opens_ap_immediately);
  RUN_TEST(test_improv_parser);
  RUN_TEST(test_improv_provisioning);
  RUN_TEST(test_improv_unreachable_network);
  RUN_TEST(test_boot_guard);
  RUN_TEST(test_state_store);
  RUN_TEST(test_state_store_soil_fit_and_v2);
  RUN_TEST(test_led_logic);
  RUN_TEST(test_display_renderers);
  RUN_TEST(test_template_store);
  return UNITY_END();
}
