/**
 * @file test_main.cpp
 * @brief Konfiguration: Schema, Roundtrip, Validierung, .tmp-Wiederherstellung,
 *        Export ohne Geheimnisse, Import neu und alt, Werksreset (AP-2)
 */
#include "services/config_service.h"
#include "services/storage.h"
#include "test_support.h"

using namespace ps;

namespace {
native::MemFs* g_fs;
TemplateStore* g_tpl;
StateStore* g_state;
ConfigService* g_cfg;

void fresh() {
  delete g_cfg;
  delete g_state;
  delete g_tpl;
  delete g_fs;
  g_fs = new native::MemFs();
  g_tpl = new TemplateStore(*g_fs);
  g_state = new StateStore(*g_fs);
  g_cfg = new ConfigService(*g_fs, *g_tpl, *g_state);
  g_cfg->load();
}

ConfigService* reload() {
  // neue Instanz auf demselben Dateisystem
  delete g_cfg;
  g_cfg = new ConfigService(*g_fs, *g_tpl, *g_state);
  g_cfg->load();
  return g_cfg;
}

Err patch(const char* json, schema::FieldErrors& errs, uint8_t& changed) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return g_cfg->patchDevice(doc.as<JsonObjectConst>(), errs, changed);
}
} // namespace

void setUp() {
  native::setMillis(1000);
  fresh();
}
void tearDown() {}

void test_defaults() {
  const DeviceConfig& d = g_cfg->device();
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor", d.deviceName.c_str());
  TEST_ASSERT_EQUAL(5, d.screenDurationS);
  TEST_ASSERT_EQUAL(2, d.ledMode);
  TEST_ASSERT_EQUAL_STRING("ANALOG_1", d.flowerChannel.c_str());
  TEST_ASSERT_EQUAL_STRING("http://pflanzensensormailbot.fabmobil.org:45480", d.mailUrl.c_str());
  const SensorsConfig& s = g_cfg->sensors();
  TEST_ASSERT_EQUAL_STRING("Lichtstärke", s.channels[0].name.c_str());
  TEST_ASSERT_TRUE(s.channels[1].inverted);
  TEST_ASSERT_EQUAL(250, s.channels[1].rawMin);
  TEST_ASSERT_TRUE(g_cfg->checkAdminPassword("Fabmobil"));
  TEST_ASSERT_FALSE(g_cfg->checkAdminPassword("falsch"));
  const int sds = board::sensorIndexOf(SensorType::Sds011);
  TEST_ASSERT_EQUAL(600, s.sensors[sds].intervalS);
}

void test_patch_roundtrip() {
  schema::FieldErrors errs;
  uint8_t changed = 0;
  TEST_ASSERT_EQUAL(Err::None, patch("{\"display\":{\"screenDuration\":10,\"clockFormat\":\"12h\"},"
                                     "\"log\":{\"level\":\"DEBUG\"}}",
                                     errs, changed));
  TEST_ASSERT_EQUAL(SecDisplay | SecLog, changed);
  native::advance(3000);
  g_cfg->tick(hal::now(), false);
  TEST_ASSERT_TRUE(g_fs->exists("/device.cfg"));
  reload();
  TEST_ASSERT_EQUAL(10, g_cfg->device().screenDurationS);
  TEST_ASSERT_EQUAL(1, g_cfg->device().clockFormat);
  TEST_ASSERT_EQUAL(0, g_cfg->device().logLevel);
}

void test_numeric_strings_from_selects() {
  schema::FieldErrors errs;
  uint8_t changed = 0;
  TEST_ASSERT_EQUAL(
      Err::None,
      patch("{\"led\":{\"mode\":\"0\"},\"display\":{\"screenDuration\":\"7\"}}", errs, changed));
  TEST_ASSERT_EQUAL(0, g_cfg->device().ledMode);
  TEST_ASSERT_EQUAL(7, g_cfg->device().screenDurationS);
  errs = schema::FieldErrors{};
  TEST_ASSERT_EQUAL(Err::InvalidArgument, patch("{\"led\":{\"mode\":\"zwei\"}}", errs, changed));
}

void test_patch_invalid_is_atomic() {
  schema::FieldErrors errs;
  uint8_t changed = 0;
  TEST_ASSERT_EQUAL(
      Err::InvalidArgument,
      patch("{\"display\":{\"screenDuration\":99,\"showQr\":true},\"nope\":{}}", errs, changed));
  TEST_ASSERT_EQUAL(2, errs.count);
  TEST_ASSERT_FALSE(g_cfg->device().showQr); // nichts übernommen
  errs = schema::FieldErrors{};
  TEST_ASSERT_EQUAL(Err::InvalidArgument,
                    patch("{\"led\":{\"channel\":\"FOO_9\"}}", errs, changed));
  errs = schema::FieldErrors{};
  TEST_ASSERT_EQUAL(Err::InvalidArgument,
                    patch("{\"general\":{\"deviceName\":\"\"}}", errs, changed));
}

void test_invalid_file_value_keeps_default() {
  pstest::writeFile(
      *g_fs, "/device.cfg",
      "{\"schema\":1,\"general\":{\"deviceName\":\"Küche\"},\"display\":{\"screenDuration\":500}}");
  reload();
  TEST_ASSERT_EQUAL_STRING("Küche", g_cfg->device().deviceName.c_str());
  TEST_ASSERT_EQUAL(5, g_cfg->device().screenDurationS);
}

void test_tmp_recovery() {
  pstest::writeFile(*g_fs, "/device.cfg.tmp",
                    "{\"schema\":1,\"general\":{\"deviceName\":\"Aus Tmp\"}}");
  reload();
  TEST_ASSERT_EQUAL_STRING("Aus Tmp", g_cfg->device().deviceName.c_str());
  TEST_ASSERT_TRUE(g_fs->exists("/device.cfg"));
  // kaputte Temp-Datei wird verworfen
  g_fs->files_.clear();
  pstest::writeFile(*g_fs, "/device.cfg.tmp", "{\"schema\":1,\"gen");
  reload();
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor", g_cfg->device().deviceName.c_str());
  TEST_ASSERT_FALSE(g_fs->exists("/device.cfg.tmp"));
}

void test_sensor_patch_limits() {
  JsonDocument doc;
  deserializeJson(
      doc,
      "{\"ANALOG\":{\"interval\":30,\"channels\":{\"1\":{\"name\":\"Topf\",\"greenLow\":25.5}}}}");
  schema::FieldErrors errs;
  uint32_t changed = 0;
  TEST_ASSERT_EQUAL(Err::None, g_cfg->patchSensors(doc.as<JsonObjectConst>(), errs, changed));
  TEST_ASSERT_EQUAL(1u, changed);
  TEST_ASSERT_EQUAL_FLOAT(25.5f,
                          g_cfg->sensors().channels[1].limits.greenLow); // Nachkommastellen (B-30)
  // falsche Ordnung abgelehnt
  deserializeJson(doc, "{\"DHT\":{\"channels\":{\"0\":{\"greenLow\":50}}}}");
  errs = schema::FieldErrors{};
  TEST_ASSERT_EQUAL(Err::InvalidArgument,
                    g_cfg->patchSensors(doc.as<JsonObjectConst>(), errs, changed));
  // einseitig: nur gh ≤ yh (B-26)
  deserializeJson(doc, "{\"SDS011\":{\"channels\":{\"0\":{\"greenLow\":100,\"greenHigh\":20}}}}");
  errs = schema::FieldErrors{};
  TEST_ASSERT_EQUAL(Err::None, g_cfg->patchSensors(doc.as<JsonObjectConst>(), errs, changed));
  // Sensor deaktivieren wird gespeichert (B-12)
  deserializeJson(doc, "{\"DHT\":{\"enabled\":false}}");
  errs = schema::FieldErrors{};
  g_cfg->patchSensors(doc.as<JsonObjectConst>(), errs, changed);
  native::advance(3000);
  g_cfg->tick(hal::now(), false);
  reload();
  TEST_ASSERT_FALSE(g_cfg->sensors().sensors[board::sensorIndexOf(SensorType::Dht)].enabled);
  TEST_ASSERT_EQUAL_STRING("Topf", g_cfg->sensors().channels[1].name.c_str());
}

void test_secrets_and_export() {
  TEST_ASSERT_EQUAL(Err::None, g_cfg->setAdminPassword("x")); // keine Mindestlänge (E10)
  TEST_ASSERT_EQUAL(Err::InvalidArgument, g_cfg->setAdminPassword(""));
  TEST_ASSERT_EQUAL(Err::InvalidArgument, g_cfg->setAdminPassword("ä"));
  g_cfg->setWifi(0, "Netz", "wlan-geheim", false);
  TEST_ASSERT_EQUAL(Err::None, g_cfg->setMailKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="));
  TEST_ASSERT_EQUAL(Err::InvalidArgument, g_cfg->setMailKey("kurz"));
  g_cfg->tick(hal::now(), true);
  reload();
  TEST_ASSERT_TRUE(g_cfg->checkAdminPassword("x"));
  TEST_ASSERT_FALSE(g_cfg->checkAdminPassword("Fabmobil"));
  TEST_ASSERT_EQUAL_STRING("wlan-geheim", g_cfg->secrets().wifiPassword[0].c_str());
  // Passwort leer lassen = behalten
  g_cfg->setWifi(0, "Netz2", "", true);
  TEST_ASSERT_EQUAL_STRING("wlan-geheim", g_cfg->secrets().wifiPassword[0].c_str());
  pstest::StdSink out;
  JsonWriter w(out);
  g_cfg->exportTo(w);
  TEST_ASSERT_NULL(strstr(out.s.c_str(), "wlan-geheim"));
  TEST_ASSERT_NULL(strstr(out.s.c_str(), "AAECAwQF"));
  TEST_ASSERT_NOT_NULL(strstr(out.s.c_str(), "\"Netz2\""));
  JsonDocument doc;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(doc, out.s).code());
}

void test_export_import_roundtrip() {
  schema::FieldErrors errs;
  uint8_t changed;
  patch("{\"general\":{\"deviceName\":\"Export\"},\"mail\":{\"warnHours\":12}}", errs, changed);
  schema::FieldErrors te;
  g_tpl->save(TemplateKind::Boot, "Hallo {geraet}", "# Eigener Text", te);
  pstest::StdSink out;
  JsonWriter w(out);
  g_cfg->exportTo(w);
  fresh();
  pstest::writeFile(*g_fs, ConfigService::kImportPath, out.s);
  const ConfigService::ImportResult r = g_cfg->importFile();
  TEST_ASSERT_EQUAL(Err::None, r.err);
  TEST_ASSERT_FALSE(r.legacyFormat);
  TEST_ASSERT_EQUAL(0, r.report.invalid);
  reload();
  TEST_ASSERT_EQUAL_STRING("Export", g_cfg->device().deviceName.c_str());
  TEST_ASSERT_EQUAL(12, g_cfg->device().mailWarnHours);
  TEST_ASSERT_TRUE(g_tpl->isCustom(TemplateKind::Boot));
}

void test_legacy_import_measurements() {
  JsonDocument v;
  TEST_ASSERT_TRUE(pstest::loadVector("legacy_config_measurements.json", v));
  std::string js;
  serializeJson(v, js);
  pstest::writeFile(*g_fs, ConfigService::kImportPath, js);
  const ConfigService::ImportResult r = g_cfg->importFile();
  TEST_ASSERT_EQUAL(Err::None, r.err);
  TEST_ASSERT_TRUE(r.legacyFormat);
  TEST_ASSERT_TRUE(r.report.applied > 30);
  TEST_ASSERT_TRUE(r.report.ignored >= 2); // ANALOG idx 5, GIBTSNICHT
  TEST_ASSERT_TRUE(r.report.invalid >= 1); // DHT-Schwellen falsch geordnet
  reload();
  const DeviceConfig& d = g_cfg->device();
  TEST_ASSERT_EQUAL_STRING("Balkon Süd", d.deviceName.c_str());
  TEST_ASSERT_TRUE(g_cfg->checkAdminPassword("geheim-platzhalter"));
  TEST_ASSERT_TRUE(d.otaRequireMd5);
  TEST_ASSERT_TRUE(d.fileLog); // general.file_log hat Vorrang
  TEST_ASSERT_EQUAL_STRING("Heimnetz", d.wifiSsid[0].c_str());
  TEST_ASSERT_EQUAL_STRING("platzhalter1", g_cfg->secrets().wifiPassword[0].c_str());
  TEST_ASSERT_EQUAL_STRING("Gast", d.wifiSsid[2].c_str());
  TEST_ASSERT_FALSE(d.showIp);
  TEST_ASSERT_TRUE(d.showQr);
  TEST_ASSERT_EQUAL(8, d.screenDurationS);
  TEST_ASSERT_EQUAL(1, d.clockFormat);
  TEST_ASSERT_TRUE(d.debugRam);
  TEST_ASSERT_FALSE(d.debugMail);
  TEST_ASSERT_EQUAL(0, d.logLevel);
  TEST_ASSERT_EQUAL_STRING("DHT_1", d.ledChannel.c_str());
  TEST_ASSERT_TRUE(d.ledOnlyRed);
  const SensorsConfig& s = g_cfg->sensors();
  TEST_ASSERT_EQUAL(120, s.sensors[0].intervalS);
  TEST_ASSERT_EQUAL(10,
                    s.sensors[board::sensorIndexOf(SensorType::Dht)].intervalS); // 5 s → Minimum
  TEST_ASSERT_EQUAL_STRING("Licht", s.channels[0].name.c_str());
  TEST_ASSERT_EQUAL_FLOAT(15.25f, s.channels[0].limits.greenLow);
  TEST_ASSERT_EQUAL(10, s.channels[0].rawMin);
  TEST_ASSERT_TRUE(s.channels[1].autocal);
  TEST_ASSERT_EQUAL(604800u, s.channels[1].halfLifeS);
  TEST_ASSERT_TRUE(s.channels[0].display);
  TEST_ASSERT_FALSE(s.channels[1].display);
  const uint8_t dhtSlot = board::sensor(board::sensorIndexOf(SensorType::Dht)).firstSlot;
  TEST_ASSERT_FALSE(s.channels[dhtSlot + 1].enabled);
  TEST_ASSERT_EQUAL_FLOAT(
      15, s.channels[dhtSlot].limits.greenLow); // ungültige Schwellen nicht übernommen
  // Zustand
  StateStore st(*g_fs);
  TEST_ASSERT_TRUE(st.load());
  TEST_ASSERT_EQUAL(512, st.channel(0).raw);
  TEST_ASSERT_EQUAL_FLOAT(49.2f, st.channel(0).value);
  TEST_ASSERT_EQUAL(12, st.channel(0).rawMin);
  TEST_ASSERT_EQUAL_FLOAT(97.25f, st.channel(0).obsMax);
  TEST_ASSERT_FALSE(st.channel(1).hasObs());
  TEST_ASSERT_FALSE(st.channel(1).hasRawObs());
  TEST_ASSERT_TRUE(st.channel(1).cal.valid());
}

void test_legacy_import_flat() {
  JsonDocument v;
  TEST_ASSERT_TRUE(pstest::loadVector("legacy_config_flat.json", v));
  std::string s;
  serializeJson(v, s);
  pstest::writeFile(*g_fs, ConfigService::kImportPath, s);
  const ConfigService::ImportResult r = g_cfg->importFile();
  TEST_ASSERT_EQUAL(Err::None, r.err);
  reload();
  TEST_ASSERT_EQUAL_STRING("Altstruktur", g_cfg->device().deviceName.c_str());
  TEST_ASSERT_EQUAL(5, g_cfg->device().screenDurationS); // 5 = Sekunden (B-04)
  TEST_ASSERT_TRUE(g_cfg->device().fileLog);
  TEST_ASSERT_EQUAL_STRING("Flach", g_cfg->sensors().channels[1].name.c_str());
  TEST_ASSERT_EQUAL(200, g_cfg->sensors().channels[1].rawMin);
  TEST_ASSERT_EQUAL(86400u, g_cfg->sensors().channels[1].halfLifeS); // ahl 0 ungültig
}

void test_import_garbage() {
  pstest::writeFile(*g_fs, ConfigService::kImportPath, "kein json");
  TEST_ASSERT_EQUAL(Err::ParseError, g_cfg->importFile().err);
  pstest::writeFile(*g_fs, ConfigService::kImportPath, "{\"foo\":1}");
  TEST_ASSERT_EQUAL(Err::ParseError, g_cfg->importFile().err);
}

void test_factory_reset_keeps_chronik_and_log() {
  schema::FieldErrors errs;
  uint8_t changed;
  patch("{\"general\":{\"deviceName\":\"Weg\"}}", errs, changed);
  g_cfg->tick(hal::now(), true);
  pstest::writeFile(*g_fs, "/chr_00000001.dat", "x");
  pstest::writeFile(*g_fs, "/log.txt", "y");
  pstest::writeFile(*g_fs, "/state.bin", "z");
  g_cfg->factoryReset();
  TEST_ASSERT_FALSE(g_fs->exists("/device.cfg"));
  TEST_ASSERT_FALSE(g_fs->exists("/state.bin"));
  TEST_ASSERT_TRUE(g_fs->exists("/chr_00000001.dat"));
  TEST_ASSERT_TRUE(g_fs->exists("/log.txt"));
  reload();
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor", g_cfg->device().deviceName.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_patch_roundtrip);
  RUN_TEST(test_numeric_strings_from_selects);
  RUN_TEST(test_patch_invalid_is_atomic);
  RUN_TEST(test_invalid_file_value_keeps_default);
  RUN_TEST(test_tmp_recovery);
  RUN_TEST(test_sensor_patch_limits);
  RUN_TEST(test_secrets_and_export);
  RUN_TEST(test_export_import_roundtrip);
  RUN_TEST(test_legacy_import_measurements);
  RUN_TEST(test_legacy_import_flat);
  RUN_TEST(test_import_garbage);
  RUN_TEST(test_factory_reset_keeps_chronik_and_log);
  return UNITY_END();
}
