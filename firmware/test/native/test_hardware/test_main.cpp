/**
 * @file test_main.cpp
 * @brief Hardware zur Laufzeit: Prüfregeln, Kanal-Layout, Webflasher-Block
 */
#include <string>

#include "board.h"
#include "core/config/defaults.h"
#include "core/provision.h"
#include "core/util/crc.h"
#include "core/util/sha256.h"
#include "services/config_service.h"
#include "services/provision_service.h"
#include "services/state_store.h"
#include "services/template_store.h"
#include "test_support.h"

using namespace ps;

void setUp() {
  native::setMillis(1000);
  native::provisionSector().clear();
  board::setHardware(board::defaultHardware());
}
void tearDown() {}

namespace {
Platform platformOf(const char* s) {
  if (strcmp(s, "ESP8266") == 0)
    return Platform::Esp8266;
  if (strcmp(s, "ESP32") == 0)
    return Platform::Esp32;
  return Platform::Native;
}

void readFeatures(JsonObjectConst o, Features& f) {
  f = Features{};
  f.analog = o["analog"] | false;
  f.analogChannels = o["analogChannels"] | 0;
  f.multiplexer = o["multiplexer"] | false;
  f.dht = o["dht"] | false;
  f.ds18b20 = o["ds18b20"] | false;
  f.ds18b20Max = o["ds18b20Max"] | 0;
  f.sds011 = o["sds011"] | false;
  f.mhz19 = o["mhz19"] | false;
  f.mhz19Range = o["mhz19Range"] | 5000;
  f.hx711 = o["hx711"] | false;
  f.bmp280 = o["bmp280"] | false;
  f.serialReceiver = o["serialReceiver"] | false;
  f.display = o["display"] | false;
  f.ledTraffic = o["ledTraffic"] | false;
}

void readPins(JsonObjectConst o, Pins& p) {
  p = Pins{};
  // clang-format off
#define X(n) p.n = static_cast<int8_t>(o[#n] | -1);
  X(adc)
  X(muxA)
  X(muxB)
  X(muxC)
  X(dht)
  X(ledGreen)
  X(ledYellow)
  X(ledRed)
  X(sda)
  X(scl)
  X(oneWire)
  X(sdsRx)
  X(sdsTx)
  X(mhz19Pwm)
  X(hxSck)
  X(hxDout)
  X(serialRx)
  X(serialTx)
  // clang-format on
#undef X
}

Hardware pcb() {
  Hardware h;
  strcpy(h.name, "pcb_v1");
  h.features.analog = true;
  h.features.analogChannels = 2;
  h.features.multiplexer = true;
  h.features.dht = true;
  h.features.display = true;
  h.features.ledTraffic = true;
  h.pins.adc = 17;
  h.pins.muxA = 15;
  h.pins.muxB = 2;
  h.pins.muxC = 16;
  h.pins.dht = 0;
  h.pins.ledGreen = 14;
  h.pins.ledYellow = 12;
  h.pins.ledRed = 13;
  h.pins.sda = 4;
  h.pins.scl = 5;
  return h;
}
} // namespace

void test_shared_vectors() {
  JsonDocument doc;
  TEST_ASSERT_TRUE_MESSAGE(pstest::loadVector("hardware_check.json", doc),
                           "hardware_check.json fehlt");
  int n = 0;
  for (JsonObjectConst c : doc["cases"].as<JsonArrayConst>()) {
    Features f;
    Pins p;
    readFeatures(c["features"], f);
    readPins(c["pins"], p);
    const HwCheckResult r = checkHardware(platformOf(c["platform"]), f, p);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(c["problem"].as<const char*>(), hwProblemCode(r.problem),
                                     c["name"].as<const char*>());
    TEST_ASSERT_EQUAL_INT_MESSAGE(c["pin"] | -1, r.pin, c["name"].as<const char*>());
    n++;
  }
  TEST_ASSERT_TRUE(n >= 20);
}

void test_layout_follows_hardware() {
  // Voreinstellung des Testprofils: alle Sensoren
  TEST_ASSERT_EQUAL(8, board::sensorCount());
  TEST_ASSERT_EQUAL(19, board::slotCount());
  // PCB-Hardware: ANALOG (2) und DHT (2)
  Hardware h = pcb();
  TEST_ASSERT_EQUAL(HwProblem::None, board::setHardware(h).problem);
  TEST_ASSERT_EQUAL_STRING("pcb_v1", board::name());
  TEST_ASSERT_EQUAL(2, board::sensorCount());
  TEST_ASSERT_EQUAL(4, board::slotCount());
  TEST_ASSERT_EQUAL(SensorType::Dht, board::sensor(1).type);
  TEST_ASSERT_EQUAL(2, board::sensor(1).firstSlot);
  TEST_ASSERT_EQUAL(1, board::slot(3).sensor);
  TEST_ASSERT_EQUAL(1, board::slot(3).channel);
  TEST_ASSERT_EQUAL(-1, board::sensorIndexOf(SensorType::Ds18b20));
  // Ungültige Hardware ändert nichts
  Hardware bad = h;
  bad.pins.dht = bad.pins.ledGreen;
  TEST_ASSERT_EQUAL(HwProblem::DuplicatePin, board::setHardware(bad).problem);
  TEST_ASSERT_EQUAL(4, board::slotCount());
}

void test_provision_roundtrip_and_errors() {
  provision::Block b;
  b.hasHardware = true;
  b.hardware = pcb();
  b.hardware.pins.adc = 30;
  b.hardware.features.dhtType = DhtType::Dht22;
  b.hardware.staticIp.enabled = true;
  const uint8_t ip[4] = {192, 168, 1, 50}, gw[4] = {192, 168, 1, 1};
  memcpy(b.hardware.staticIp.ip, ip, 4);
  memcpy(b.hardware.staticIp.gateway, gw, 4);
  b.settings.wifi[2].set = true;
  strcpy(b.settings.wifi[2].ssid, "Garten");
  strcpy(b.settings.wifi[2].password, "geheim");
  b.settings.channelNameCount = 1;
  strcpy(b.settings.channelNames[0].key, "ANALOG_0");
  strcpy(b.settings.channelNames[0].name, "Licht Süd");
  uint8_t sector[provision::kSectorSize];
  TEST_ASSERT_TRUE(provision::encode(b, true, sector, sizeof(sector)));
  provision::Block back;
  TEST_ASSERT_EQUAL(provision::Result::Ok,
                    provision::parse(sector, sizeof(sector), Hardware{}, back));
  TEST_ASSERT_TRUE(back.hasHardware);
  TEST_ASSERT_EQUAL(30, back.hardware.pins.adc);
  TEST_ASSERT_EQUAL(DhtType::Dht22, back.hardware.features.dhtType);
  TEST_ASSERT_TRUE(back.hardware.staticIp.enabled);
  TEST_ASSERT_EQUAL(50, back.hardware.staticIp.ip[3]);
  TEST_ASSERT_FALSE(back.settings.wifi[0].set);
  TEST_ASSERT_FALSE(back.settings.wifi[1].set);
  TEST_ASSERT_TRUE(back.settings.wifi[2].set);
  TEST_ASSERT_EQUAL_STRING("Garten", back.settings.wifi[2].ssid);
  TEST_ASSERT_EQUAL_STRING("geheim", back.settings.wifi[2].password);
  TEST_ASSERT_EQUAL(1, back.settings.channelNameCount);
  TEST_ASSERT_EQUAL_STRING("ANALOG_0", back.settings.channelNames[0].key);
  TEST_ASSERT_EQUAL_STRING("Licht Süd", back.settings.channelNames[0].name);
  TEST_ASSERT_FALSE(back.settings.hasAdminPassword);
  // Ohne Einstellungen
  TEST_ASSERT_TRUE(provision::encode(b, false, sector, sizeof(sector)));
  provision::parse(sector, sizeof(sector), Hardware{}, back);
  TEST_ASSERT_FALSE(back.settings.any());
  TEST_ASSERT_EQUAL(0xFF, sector[provision::kSectorSize - 1]);
  // Beschädigt
  sector[provision::kHeaderSize + 3] ^= 1;
  TEST_ASSERT_EQUAL(provision::Result::BadCrc,
                    provision::parse(sector, sizeof(sector), Hardware{}, back));
  memset(sector, 0xFF, sizeof(sector));
  TEST_ASSERT_EQUAL(provision::Result::Empty,
                    provision::parse(sector, sizeof(sector), Hardware{}, back));
  sector[0] = 'X';
  TEST_ASSERT_EQUAL(provision::Result::BadMagic,
                    provision::parse(sector, sizeof(sector), Hardware{}, back));
}

void test_vector_block_from_webflasher_parses() {
  // Vom JavaScript des Webflashers erzeugter Block (webflasher/test)
  JsonDocument doc;
  TEST_ASSERT_TRUE_MESSAGE(pstest::loadVector("provision_block.json", doc),
                           "provision_block.json fehlt");
  const char* hex = doc["hex"];
  std::string bytes;
  for (size_t i = 0; hex[i] && hex[i + 1]; i += 2)
    bytes += static_cast<char>(std::stoi(std::string(hex + i, 2), nullptr, 16));
  bytes.resize(provision::kSectorSize, '\xFF');
  provision::Block b;
  TEST_ASSERT_EQUAL(provision::Result::Ok,
                    provision::parse(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(),
                                     Hardware{}, b));
  JsonObjectConst e = doc["expect"];
  TEST_ASSERT_EQUAL_STRING(e["name"].as<const char*>(), b.hardware.name);
  TEST_ASSERT_EQUAL(e["slots"].as<int>(),
                    b.hardware.features.analogChannels + (b.hardware.features.dht ? 2 : 0) +
                        (b.hardware.features.ds18b20 ? b.hardware.features.ds18b20Max : 0));
  TEST_ASSERT_EQUAL(e["oneWire"].as<int>(), b.hardware.pins.oneWire);
  TEST_ASSERT_EQUAL_STRING(e["deviceName"].as<const char*>(), b.settings.deviceName);
  for (uint8_t i = 0; i < provision::kWifiSlots; i++) {
    const char* ssid = e["wifi"][i] | "";
    TEST_ASSERT_EQUAL(*ssid != '\0', b.settings.wifi[i].set);
    TEST_ASSERT_EQUAL_STRING(ssid, b.settings.wifi[i].ssid);
  }
  JsonObjectConst names = e["channelNames"];
  TEST_ASSERT_EQUAL(names.size(), b.settings.channelNameCount);
  for (uint8_t i = 0; i < b.settings.channelNameCount; i++)
    TEST_ASSERT_EQUAL_STRING(names[b.settings.channelNames[i].key].as<const char*>(),
                             b.settings.channelNames[i].name);
}

namespace {
struct CfgRig {
  native::MemFs fs;
  TemplateStore tpl{fs};
  StateStore st{fs};
  ConfigService cfg{fs, tpl, st};
};
} // namespace

void test_provision_service_applies_once_and_strips_secrets() {
  provision::Block b;
  b.hasHardware = true;
  b.hardware = pcb();
  b.hardware.pins.adc = 30;
  strcpy(b.hardware.name, "eigene");
  b.settings.hasDeviceName = true;
  strcpy(b.settings.deviceName, "Balkon Süd");
  b.settings.wifi[0].set = true;
  strcpy(b.settings.wifi[0].ssid, "Garten");
  strcpy(b.settings.wifi[0].password, "geheim12");
  b.settings.wifi[1].set = true;
  strcpy(b.settings.wifi[1].ssid, "Schule");
  strcpy(b.settings.wifi[1].password, "geheim34");
  b.settings.channelNameCount = 3;
  strcpy(b.settings.channelNames[0].key, "ANALOG_0");
  strcpy(b.settings.channelNames[0].name, "Licht Balkon");
  strcpy(b.settings.channelNames[1].key, "DHT_1");
  strcpy(b.settings.channelNames[1].name, "Feuchte innen");
  strcpy(b.settings.channelNames[2].key, "MHZ19_0"); // nicht bestückt: ignoriert
  strcpy(b.settings.channelNames[2].name, "CO2");
  b.settings.hasAdminPassword = true;
  strcpy(b.settings.adminPassword, "NeuesPw1");
  uint8_t sector[provision::kSectorSize];
  provision::encode(b, true, sector, sizeof(sector));
  native::provisionSector().assign(reinterpret_cast<char*>(sector), sizeof(sector));

  ProvisionService ps1;
  ps1.loadHardware();
  TEST_ASSERT_TRUE(ps1.fromWebflasher());
  TEST_ASSERT_EQUAL_STRING("eigene", board::name());
  TEST_ASSERT_EQUAL(4, board::slotCount());
  CfgRig r;
  r.cfg.load();
  TEST_ASSERT_TRUE(ps1.applySettings(r.cfg));
  TEST_ASSERT_EQUAL_STRING("Balkon Süd", r.cfg.device().deviceName.c_str());
  TEST_ASSERT_EQUAL_STRING("Garten", r.cfg.device().wifiSsid[0].c_str());
  TEST_ASSERT_EQUAL_STRING("geheim12", r.cfg.secrets().wifiPassword[0].c_str());
  TEST_ASSERT_EQUAL_STRING("Schule", r.cfg.device().wifiSsid[1].c_str());
  TEST_ASSERT_EQUAL_STRING("geheim34", r.cfg.secrets().wifiPassword[1].c_str());
  // Slot 3 nicht im Block: Voreinstellung (Fabmobil-WLAN) bleibt
  TEST_ASSERT_EQUAL_STRING(defaults::kFabmobilWifiSsid, r.cfg.device().wifiSsid[2].c_str());
  TEST_ASSERT_TRUE(r.cfg.checkAdminPassword("NeuesPw1"));
  // Kanalnamen gespeichert (neu geladen)
  {
    ConfigService again{r.fs, r.tpl, r.st};
    again.load();
    TEST_ASSERT_EQUAL_STRING("Licht Balkon", again.sensors().channels[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("Feuchte innen", again.sensors().channels[3].name.c_str());
    TEST_ASSERT_EQUAL_STRING("Lufttemperatur", again.sensors().channels[2].name.c_str());
    TEST_ASSERT_EQUAL_STRING("Schule", again.device().wifiSsid[1].c_str());
  }
  // Sektor ohne Geheimnisse, Hardware bleibt
  const std::string& s = native::provisionSector();
  TEST_ASSERT_EQUAL(std::string::npos, s.find("geheim12"));
  TEST_ASSERT_EQUAL(std::string::npos, s.find("NeuesPw1"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, s.find("eigene"));
  // Zweiter Start: Hardware ja, Einstellungen nicht noch einmal
  ProvisionService ps2;
  ps2.loadHardware();
  TEST_ASSERT_TRUE(ps2.fromWebflasher());
  TEST_ASSERT_FALSE(ps2.applySettings(r.cfg));
}

namespace {
/// Block aus provision_config_blocks.json in den (nativen) Provisionierungssektor legen
void loadConfigBlock(const char* name) {
  JsonDocument doc;
  TEST_ASSERT_TRUE_MESSAGE(pstest::loadVector("provision_config_blocks.json", doc),
                           "provision_config_blocks.json fehlt");
  const char* hex = nullptr;
  for (JsonObjectConst c : doc["cases"].as<JsonArrayConst>())
    if (strcmp(c["name"] | "", name) == 0)
      hex = c["hex"];
  TEST_ASSERT_NOT_NULL(hex);
  std::string bytes;
  for (size_t i = 0; hex[i] && hex[i + 1]; i += 2)
    bytes += static_cast<char>(std::stoi(std::string(hex + i, 2), nullptr, 16));
  bytes.resize(provision::kSectorSize, '\xFF');
  native::provisionSector() = bytes;
}

int slotOf(const char* key) { return schema::slotOfKey(key); }
} // namespace

void test_provision_config_from_firmware2_file() {
  // webflasher/test/fixtures/config_v2.json, durch den Webflasher verteilt:
  // Name, Passwort, WLAN 1 und Kanalnamen im Formular, der Rest als "config"
  loadConfigBlock("v2");
  ProvisionService ps1;
  ps1.loadHardware();
  CfgRig r;
  r.cfg.load();
  TEST_ASSERT_TRUE(ps1.applySettings(r.cfg));
  const DeviceConfig& d = r.cfg.device();
  TEST_ASSERT_EQUAL_STRING("Gemeinschaftskueche", d.deviceName.c_str());
  TEST_ASSERT_TRUE(r.cfg.checkAdminPassword("Kuechenpasswort"));
  TEST_ASSERT_EQUAL_STRING("Kuechen-WLAN", d.wifiSsid[0].c_str());
  TEST_ASSERT_EQUAL_STRING("kuechen-wlan-pw", r.cfg.secrets().wifiPassword[0].c_str());
  TEST_ASSERT_EQUAL_STRING(defaults::kFabmobilWifiSsid, d.wifiSsid[2].c_str());
  // aus der Datei (Altformat → neu)
  TEST_ASSERT_FALSE(d.showIp);
  TEST_ASSERT_FALSE(d.showQr);
  TEST_ASSERT_FALSE(d.showFabmobil);
  TEST_ASSERT_EQUAL(3, d.screenDurationS); // 3000 ms
  TEST_ASSERT_TRUE(d.ledOnlyRed);
  TEST_ASSERT_EQUAL(static_cast<uint8_t>(LogLevel::Debug), d.logLevel);
  TEST_ASSERT_TRUE(d.fileLog); // general.file_log vor log.file_enabled
  TEST_ASSERT_TRUE(d.debugRam);
  TEST_ASSERT_FALSE(d.debugDisplay);
  const int t = slotOf("DHT_0");
  TEST_ASSERT_TRUE(t >= 0);
  TEST_ASSERT_EQUAL_FLOAT(-10, r.cfg.sensors().channels[t].limits.yellowLow);
  TEST_ASSERT_EQUAL_FLOAT(30, r.cfg.sensors().channels[t].limits.greenHigh);
  TEST_ASSERT_EQUAL_STRING("Lufttemperatur", r.cfg.sensors().channels[t].name.c_str());
  // Datei ist weg, Passwörter auch
  const std::string& s = native::provisionSector();
  TEST_ASSERT_EQUAL(std::string::npos, s.find("Kuechenpasswort"));
  TEST_ASSERT_EQUAL(std::string::npos, s.find("kuechen-wlan-pw"));
  TEST_ASSERT_EQUAL(std::string::npos, s.find("thresh"));
  ProvisionService ps2;
  ps2.loadHardware();
  TEST_ASSERT_FALSE(ps2.applySettings(r.cfg));
}

void test_provision_config_from_firmware3_export() {
  loadConfigBlock("v3");
  ProvisionService ps1;
  ps1.loadHardware();
  CfgRig r;
  r.cfg.load();
  // Vorher eingerichtetes WLAN: der Export hat keine Passwörter und darf es nicht anfassen
  TEST_ASSERT_EQUAL(Err::None, r.cfg.setWifi(0, "Werkstatt", "werkstatt-pw", false));
  TEST_ASSERT_TRUE(ps1.applySettings(r.cfg));
  const DeviceConfig& d = r.cfg.device();
  TEST_ASSERT_EQUAL_STRING("Fensterbank", d.deviceName.c_str());
  TEST_ASSERT_EQUAL_STRING("Werkstatt", d.wifiSsid[0].c_str());
  TEST_ASSERT_EQUAL_STRING("werkstatt-pw", r.cfg.secrets().wifiPassword[0].c_str());
  TEST_ASSERT_EQUAL(5, d.screenDurationS);
  TEST_ASSERT_TRUE(d.ledOnlyRed);
  TEST_ASSERT_EQUAL_STRING("kueche@example.org", d.mailTo.c_str());
  TEST_ASSERT_EQUAL(static_cast<uint8_t>(LogLevel::Debug), d.logLevel);
  const int a1 = slotOf("ANALOG_1"), a0 = slotOf("ANALOG_0"), t = slotOf("DHT_0");
  TEST_ASSERT_TRUE(a1 >= 0 && a0 >= 0 && t >= 0);
  const SensorsConfig& sc = r.cfg.sensors();
  TEST_ASSERT_EQUAL(300, sc.channels[a1].rawMin);
  TEST_ASSERT_EQUAL(700, sc.channels[a1].rawMax);
  TEST_ASSERT_TRUE(sc.channels[a1].inverted);
  TEST_ASSERT_TRUE(sc.channels[a1].autocal);
  TEST_ASSERT_EQUAL(259200, sc.channels[a1].halfLifeS);
  TEST_ASSERT_EQUAL_STRING("Licht Fenster", sc.channels[a0].name.c_str());
  TEST_ASSERT_EQUAL_FLOAT(5, sc.channels[t].limits.yellowLow);
  TEST_ASSERT_EQUAL_FLOAT(35, sc.channels[t].limits.yellowHigh);
  TEST_ASSERT_EQUAL(120, sc.sensors[board::sensorIndexOf(SensorType::Analog)].intervalS);
  TEST_ASSERT_TRUE(r.tpl.isCustom(TemplateKind::Boot));
  TEST_ASSERT_FALSE(r.tpl.isCustom(TemplateKind::Warning));
}

void test_provision_config_broken_is_skipped() {
  // "config" ohne gültigen Inhalt: Rest der Einstellungen gilt trotzdem
  const std::string json =
      "{\"settings\":{\"deviceName\":\"Nur Name\",\"config\":{\"schema\":7,\"device\":{}}}}";
  uint8_t sector[provision::kSectorSize];
  memset(sector, 0xFF, sizeof(sector));
  memcpy(sector, provision::kMagic, sizeof(provision::kMagic));
  const uint32_t n = static_cast<uint32_t>(json.size());
  for (int i = 0; i < 4; i++)
    sector[8 + i] = static_cast<uint8_t>(n >> (8 * i));
  memcpy(sector + provision::kHeaderSize, json.data(), n);
  const uint32_t c = crc::crc32(sector + provision::kHeaderSize, n);
  for (int i = 0; i < 4; i++)
    sector[12 + i] = static_cast<uint8_t>(c >> (8 * i));
  native::provisionSector().assign(reinterpret_cast<char*>(sector), sizeof(sector));
  provision::Block b;
  TEST_ASSERT_EQUAL(provision::Result::Ok, provision::parse(sector, sizeof(sector), Hardware{}, b));
  TEST_ASSERT_TRUE(b.settings.hasConfig);
  TEST_ASSERT_FALSE(b.hasHardware);
  ProvisionService ps1;
  ps1.loadHardware();
  CfgRig r;
  r.cfg.load();
  TEST_ASSERT_TRUE(ps1.applySettings(r.cfg));
  TEST_ASSERT_EQUAL_STRING("Nur Name", r.cfg.device().deviceName.c_str());
  TEST_ASSERT_EQUAL(std::string::npos, native::provisionSector().find("schema"));
}

void test_default_wifi_slot3_is_fabmobil() {
  DeviceConfig d;
  defaults::device(d);
  Secrets sec;
  defaults::secrets(sec);
  TEST_ASSERT_EQUAL_STRING("Fabmobil Gäste", d.wifiSsid[2].c_str());
  TEST_ASSERT_TRUE(d.wifiSsid[0].empty());
  // Klartext steht nicht im Quelltext; geprüft wird über den SHA-256
  const char* pw = sec.wifiPassword[2].c_str();
  uint8_t h[32];
  Sha256 sha;
  sha.update(pw);
  sha.finish(h);
  char hex[65];
  for (int i = 0; i < 32; i++)
    snprintf(hex + 2 * i, 3, "%02x", h[i]);
  TEST_ASSERT_EQUAL_STRING("f148993e88fd73168c12863e9b35f2441c61b172dd2a822285fcb8caa6a3b266", hex);
  TEST_ASSERT_TRUE(sec.wifiPassword[0].empty());
}

void test_provision_service_invalid_hardware_keeps_default() {
  provision::Block b;
  b.hasHardware = true;
  b.hardware = pcb();
  b.hardware.pins.adc = 30;
  b.hardware.pins.dht = b.hardware.pins.ledRed; // doppelt
  uint8_t sector[provision::kSectorSize];
  provision::encode(b, false, sector, sizeof(sector));
  native::provisionSector().assign(reinterpret_cast<char*>(sector), sizeof(sector));
  ProvisionService p;
  p.loadHardware();
  TEST_ASSERT_FALSE(p.fromWebflasher());
  TEST_ASSERT_EQUAL(HwProblem::DuplicatePin, p.hardwareProblem().problem);
  TEST_ASSERT_EQUAL_STRING("native", board::name());
  TEST_ASSERT_EQUAL(19, board::slotCount());
  // Ohne applySettings(): zweiter Aufruf und Destruktor geben den Block frei
  // (der Sanitizer-Lauf meldet sonst ein Leck)
  p.loadHardware();
  TEST_ASSERT_EQUAL(HwProblem::DuplicatePin, p.hardwareProblem().problem);
  // Leerer Sektor: Voreinstellung, keine Meldung
  native::provisionSector().clear();
  ProvisionService q;
  q.loadHardware();
  TEST_ASSERT_EQUAL(provision::Result::Empty, q.readResult());
  TEST_ASSERT_FALSE(q.fromWebflasher());
}

void test_state_store_discards_on_layout_change() {
  native::MemFs fs;
  StateStore st(fs);
  st.channel(0).obsMax = 42.0f;
  st.channel(0).obsMin = 1.0f;
  TEST_ASSERT_TRUE(st.save());
  StateStore same(fs);
  TEST_ASSERT_TRUE(same.load());
  TEST_ASSERT_EQUAL_FLOAT(42.0f, same.channel(0).obsMax);
  Hardware h = pcb();
  h.pins.adc = 30;
  board::setHardware(h);
  StateStore other(fs);
  TEST_ASSERT_FALSE(other.load());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_shared_vectors);
  RUN_TEST(test_layout_follows_hardware);
  RUN_TEST(test_provision_roundtrip_and_errors);
  RUN_TEST(test_vector_block_from_webflasher_parses);
  RUN_TEST(test_provision_service_applies_once_and_strips_secrets);
  RUN_TEST(test_provision_config_from_firmware2_file);
  RUN_TEST(test_provision_config_from_firmware3_export);
  RUN_TEST(test_provision_config_broken_is_skipped);
  RUN_TEST(test_default_wifi_slot3_is_fabmobil);
  RUN_TEST(test_provision_service_invalid_hardware_keeps_default);
  RUN_TEST(test_state_store_discards_on_layout_change);
  return UNITY_END();
}
