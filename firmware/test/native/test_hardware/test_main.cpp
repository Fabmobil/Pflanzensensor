/**
 * @file test_main.cpp
 * @brief Hardware zur Laufzeit: Prüfregeln, Kanal-Layout, Webflasher-Block
 */
#include <string>

#include "board.h"
#include "core/provision.h"
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
  b.settings.hasWifi = true;
  strcpy(b.settings.wifiSsid, "Garten");
  strcpy(b.settings.wifiPassword, "geheim");
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
  TEST_ASSERT_TRUE(back.settings.hasWifi);
  TEST_ASSERT_EQUAL_STRING("geheim", back.settings.wifiPassword);
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
  TEST_ASSERT_EQUAL_STRING(e["wifiSsid"].as<const char*>(), b.settings.wifiSsid);
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
  b.settings.hasWifi = true;
  strcpy(b.settings.wifiSsid, "Garten");
  strcpy(b.settings.wifiPassword, "geheim12");
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
  TEST_ASSERT_TRUE(r.cfg.checkAdminPassword("NeuesPw1"));
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
  RUN_TEST(test_provision_service_invalid_hardware_keeps_default);
  RUN_TEST(test_state_store_discards_on_layout_change);
  return UNITY_END();
}
