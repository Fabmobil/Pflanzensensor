/**
 * @file test_main.cpp
 * @brief Alle HTTP-Endpunkte mit Erfolgs- und Fehlerpfaden, Assets, Uploads
 */
#include "web/assets.h"
#include "web/respond.h"
#include "web_rig.h"

using namespace ps;
using namespace pstest;
using M = hal::HttpMethod;

void setUp() {
  native::setMillis(5000);
  native::setEpoch(0);
}
void tearDown() {}

// ---------------------------------------------------------------------------
// Seiten und Assets
// ---------------------------------------------------------------------------
void test_pages_public_and_admin() {
  Rig r;
  FakeRes home = r.get("/");
  TEST_ASSERT_EQUAL(200, home.code);
  TEST_ASSERT_EQUAL_STRING("gzip", home.headers["Content-Encoding"].c_str());
  TEST_ASSERT_EQUAL_STRING("no-cache", home.headers["Cache-Control"].c_str());
  TEST_ASSERT_EQUAL_STRING("text/html; charset=utf-8", home.type.c_str());
  TEST_ASSERT_EQUAL(0x1f, static_cast<uint8_t>(home.body[0])); // gzip-Magic
  TEST_ASSERT_EQUAL(static_cast<int32_t>(home.body.size()), home.length);
  TEST_ASSERT_EQUAL(200, r.get("/chronik").code);
  const char* adminPages[] = {"/admin",      "/admin/sensors",        "/admin/display",
                              "/admin/mail", "/admin/mail/templates", "/admin/update"};
  for (const char* p : adminPages) {
    FakeRes x = r.get(p);
    // ohne Anmeldung zur Anmeldeseite, kein Basic-Auth-Dialog
    TEST_ASSERT_EQUAL_MESSAGE(302, x.code, p);
    TEST_ASSERT_EQUAL_STRING((std::string("/login?next=") + p).c_str(),
                             x.headers["Location"].c_str());
    TEST_ASSERT_EQUAL(0, x.headers.count("WWW-Authenticate"));
    TEST_ASSERT_EQUAL_MESSAGE(200, r.get(p, "Fabmobil").code, p);
  }
  TEST_ASSERT_EQUAL(200, r.get("/login").code);
  TEST_ASSERT_EQUAL(404, r.call(M::Post, "/").code);
  // Logs sind öffentlich
  TEST_ASSERT_EQUAL(200, r.get("/logs").code);
}

void test_log_settings_public_but_limited() {
  Rig r;
  JsonDocument d = r.call(M::Get, "/api/v1/logs/settings").json();
  TEST_ASSERT_TRUE(d["ok"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("INFO", d["log"]["level"].as<const char*>());
  TEST_ASSERT_TRUE(d["log"]["file"].isNull());
  // ohne Login: Level und Debug-Schalter ja
  FakeRes ok = r.call(M::Patch, "/api/v1/logs/settings",
                      "{\"log\":{\"level\":\"DEBUG\",\"debugRam\":true}}");
  TEST_ASSERT_EQUAL(200, ok.code);
  JsonDocument after = r.call(M::Get, "/api/v1/logs/settings").json();
  TEST_ASSERT_EQUAL_STRING("DEBUG", after["log"]["level"].as<const char*>());
  TEST_ASSERT_TRUE(after["log"]["debugRam"].as<bool>());
  // Datei-Log und andere Bereiche nicht
  TEST_ASSERT_EQUAL(400,
                    r.call(M::Patch, "/api/v1/logs/settings", "{\"log\":{\"file\":true}}").code);
  TEST_ASSERT_EQUAL(400, r.call(M::Patch, "/api/v1/logs/settings",
                                "{\"log\":{\"level\":\"INFO\"},\"mail\":{\"enabled\":true}}")
                             .code);
  TEST_ASSERT_EQUAL(
      400, r.call(M::Patch, "/api/v1/logs/settings", "{\"general\":{\"deviceName\":\"x\"}}").code);
  TEST_ASSERT_EQUAL(
      400, r.call(M::Patch, "/api/v1/logs/settings", "{\"log\":{\"level\":\"LAUT\"}}").code);
  // Admin-Konfiguration bleibt geschützt
  TEST_ASSERT_EQUAL(401,
                    r.call(M::Patch, "/api/v1/config", "{\"log\":{\"level\":\"DEBUG\"}}").code);
}

void test_login_with_password_only() {
  Rig r;
  // API ohne Anmeldung: 401 ohne Browser-Dialog
  FakeRes no = r.call(M::Get, "/api/v1/config");
  TEST_ASSERT_EQUAL(401, no.code);
  TEST_ASSERT_EQUAL(0, no.headers.count("WWW-Authenticate"));
  // Basic-Auth: Benutzername egal
  FakeReq q;
  q.m = M::Get;
  q.p = "/api/v1/config";
  q.auth("irgendwer", "Fabmobil");
  FakeRes any;
  r.router.handle(q, any);
  TEST_ASSERT_EQUAL(200, any.code);
  // Falsches Passwort
  TEST_ASSERT_EQUAL(401, r.call(M::Post, "/api/v1/login", "{\"password\":\"falsch\"}").code);
  // Richtiges Passwort: Cookie
  FakeRes in = r.call(M::Post, "/api/v1/login", "{\"password\":\"Fabmobil\"}");
  TEST_ASSERT_EQUAL(200, in.code);
  const std::string sc = in.headers["Set-Cookie"];
  TEST_ASSERT_EQUAL(0, sc.find("ps_session="));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, sc.find("HttpOnly"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, sc.find("SameSite=Strict"));
  const std::string cookie = "theme=x; " + sc.substr(0, sc.find(';'));
  FakeRes withCookie = r.call(M::Get, "/api/v1/config", "", nullptr, {}, {{"Cookie", cookie}});
  TEST_ASSERT_EQUAL(200, withCookie.code);
  TEST_ASSERT_EQUAL(200, r.call(M::Get, "/admin", "", nullptr, {}, {{"Cookie", cookie}}).code);
  // Gefälschtes Cookie
  TEST_ASSERT_EQUAL(401, r.call(M::Get, "/api/v1/config", "", nullptr, {},
                                {{"Cookie", "ps_session=00000000000000000000000000000000"}})
                             .code);
  // Zweiter Browser, dann Passwortwechsel aus dem ersten: der zweite fliegt raus
  FakeRes in2 = r.call(M::Post, "/api/v1/login", "{\"password\":\"Fabmobil\"}");
  const std::string cookie2 =
      in2.headers["Set-Cookie"].substr(0, in2.headers["Set-Cookie"].find(';'));
  TEST_ASSERT_EQUAL(200, r.call(M::Put, "/api/v1/secrets/admin-password",
                                "{\"password\":\"Neu123\"}", nullptr, {}, {{"Cookie", cookie}})
                             .code);
  TEST_ASSERT_EQUAL(200,
                    r.call(M::Get, "/api/v1/config", "", nullptr, {}, {{"Cookie", cookie}}).code);
  TEST_ASSERT_EQUAL(401,
                    r.call(M::Get, "/api/v1/config", "", nullptr, {}, {{"Cookie", cookie2}}).code);
  // Abmelden
  FakeRes out = r.call(M::Post, "/api/v1/logout", "", nullptr, {}, {{"Cookie", cookie}});
  TEST_ASSERT_EQUAL(200, out.code);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, out.headers["Set-Cookie"].find("Max-Age=0"));
  TEST_ASSERT_EQUAL(401,
                    r.call(M::Get, "/api/v1/config", "", nullptr, {}, {{"Cookie", cookie}}).code);
  // Sperre nach Fehlversuchen gilt auch für die Anmeldeseite
  for (int i = 0; i < 5; i++)
    r.call(M::Post, "/api/v1/login", "{\"password\":\"x\"}");
  FakeRes locked = r.call(M::Post, "/api/v1/login", "{\"password\":\"Neu123\"}");
  TEST_ASSERT_EQUAL(429, locked.code);
  TEST_ASSERT_TRUE(locked.headers.count("Retry-After"));
  native::advance(31000);
  TEST_ASSERT_EQUAL(200, r.call(M::Post, "/api/v1/login", "{\"password\":\"Neu123\"}").code);
}

void test_assets_etag_and_304() {
  Rig r;
  FakeRes css = r.get("/css/style.css");
  TEST_ASSERT_EQUAL(200, css.code);
  TEST_ASSERT_EQUAL_STRING("max-age=86400", css.headers["Cache-Control"].c_str());
  const std::string etag = css.headers["ETag"];
  TEST_ASSERT_EQUAL(12, etag.size()); // "…" mit 10 Zeichen
  FakeRes again = r.call(M::Get, "/css/style.css", "", nullptr, {}, {{"If-None-Match", etag}});
  TEST_ASSERT_EQUAL(304, again.code);
  TEST_ASSERT_EQUAL(0, again.body.size());
  TEST_ASSERT_EQUAL(
      200,
      r.call(M::Get, "/css/style.css", "", nullptr, {}, {{"If-None-Match", "\"falsch\""}}).code);
  TEST_ASSERT_EQUAL(200, r.get("/js/api.js").code);
  TEST_ASSERT_EQUAL(200, r.get("/img/flower_big.gif").code);
  TEST_ASSERT_EQUAL(200, r.get("/favicon.ico").code);
  TEST_ASSERT_EQUAL(404, r.get("/js/gibtsnicht.js").code);
  TEST_ASSERT_EQUAL(404, r.get("/index.html.bak").code);
  // Jede Seite und jedes eingebettete Skript ist ausgeliefert
  for (uint8_t i = 0; i < assets::kCount; i++) {
    assets::Asset a;
    memcpy(&a, &assets::kAssets[i], sizeof(a));
    if (a.html)
      continue;
    TEST_ASSERT_EQUAL_MESSAGE(200, r.get(a.path).code, a.path);
  }
  TEST_ASSERT_TRUE(assets::kTotalBytes < 200 * 1024);
}

// ---------------------------------------------------------------------------
// Öffentliche Endpunkte
// ---------------------------------------------------------------------------
void test_system_endpoint() {
  Rig r;
  r.measureAll();
  native::setEpoch(1700000000);
  JsonDocument d = r.get("/api/v1/system").json();
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor", d["deviceName"]);
  TEST_ASSERT_TRUE(d["timeValid"]);
  TEST_ASSERT_EQUAL_STRING("14.11.2023 22:13", d["time"]); // nativ in UTC
  TEST_ASSERT_TRUE(d["features"]["display"]);
  TEST_ASSERT_TRUE(d["heap"]["freeHeap"].as<int>() > 0);
  JsonArray ch = d["channels"];
  TEST_ASSERT_EQUAL(board::kSlotCount, ch.size());
  TEST_ASSERT_EQUAL_STRING("ANALOG_0", ch[0]["key"]);
  TEST_ASSERT_EQUAL_STRING("ANALOG", ch[0]["sensor"]);
  TEST_ASSERT_EQUAL_STRING("Lichtstärke", ch[0]["name"]);
  // Deaktivierter Kanal fehlt
  r.admin(M::Patch, "/api/v1/sensors", "{\"ANALOG\":{\"channels\":{\"0\":{\"enabled\":false}}}}");
  d = r.get("/api/v1/system").json();
  TEST_ASSERT_EQUAL(board::kSlotCount - 1, d["channels"].as<JsonArray>().size());
  native::setEpoch(0);
  d = r.get("/api/v1/system").json();
  TEST_ASSERT_TRUE(d["time"].isNull());
}

void test_get_latest_values_details() {
  Rig r;
  // Vor der ersten Messung: keine Kanäle (Altverhalten)
  JsonDocument d = r.get("/getLatestValues").json();
  TEST_ASSERT_EQUAL(0, d["sensors"].as<JsonObject>().size());
  r.drivers.of(SensorType::Mhz19).value = 0; // 0 ppm wird übersprungen
  r.measureAll(200000);
  d = r.get("/getLatestValues").json();
  JsonObject s = d["sensors"];
  TEST_ASSERT_FALSE(s["ANALOG_1"].isNull());
  TEST_ASSERT_TRUE(s["MHZ19_0"].isNull());
  TEST_ASSERT_EQUAL_STRING("%", s["ANALOG_1"]["unit"]);
  TEST_ASSERT_TRUE(s["ANALOG_1"]["lastMeasurement"].as<uint32_t>() > 0);
  TEST_ASSERT_EQUAL(500, s["ANALOG_1"]["absoluteRawMin"].as<int>());
  TEST_ASSERT_TRUE(s["DHT_0"]["raw"].isNull()); // nur Analog hat Rohwerte
  TEST_ASSERT_EQUAL(600000, s["SDS011_0"]["measurementInterval"].as<int>());
  TEST_ASSERT_TRUE(d["system"]["processedSensors"].as<int>() >= 2);
  // Deaktivierter Sensor fehlt
  r.admin(M::Patch, "/api/v1/sensors", "{\"DHT\":{\"enabled\":false}}");
  d = r.get("/getLatestValues").json();
  TEST_ASSERT_TRUE(d["sensors"]["DHT_0"].isNull());
  // Gerätename wird JSON-escaped (B-28)
  r.admin(M::Patch, "/api/v1/config", "{\"general\":{\"deviceName\":\"A \\\"B\\\" \\\\ C\"}}");
  d = r.get("/getLatestValues").json();
  TEST_ASSERT_EQUAL_STRING("A \"B\" \\ C", d["deviceName"]);
}

void test_measure_variants() {
  Rig r;
  r.drivers.of(SensorType::Ds18b20).hang = true;
  r.sensors.begin(r.drivers.ptrs, hal::now());
  // Formular-Parameter statt JSON
  FakeRes a = r.call(M::Post, "/api/v1/measure", "", nullptr, {{"sensor", "ANALOG"}});
  TEST_ASSERT_EQUAL(200, a.code);
  native::advance(2100);
  // Kanalschlüssel wird akzeptiert
  TEST_ASSERT_EQUAL(200, r.call(M::Post, "/api/v1/measure", "{\"sensor\":\"DS18B20_0\"}").code);
  native::advance(100);
  r.sensors.tick(hal::now());
  native::advance(2100);
  FakeRes busy = r.call(M::Post, "/api/v1/measure", "{\"sensor\":\"DS18B20\"}");
  TEST_ASSERT_EQUAL(409, busy.code);
  TEST_ASSERT_EQUAL_STRING("busy", busy.json()["error"]);
  native::advance(2100);
  FakeRes warm = r.call(M::Post, "/api/v1/measure", "{\"sensor\":\"MHZ19\"}");
  TEST_ASSERT_EQUAL(409, warm.code);
  TEST_ASSERT_EQUAL_STRING("not_ready", warm.json()["error"]);
  TEST_ASSERT_TRUE(atoi(warm.headers["Retry-After"].c_str()) > 100);
  native::advance(2100);
  r.admin(M::Patch, "/api/v1/sensors", "{\"BMP280\":{\"enabled\":false}}");
  FakeRes off = r.call(M::Post, "/api/v1/measure", "{\"sensor\":\"BMP280\"}");
  TEST_ASSERT_EQUAL(409, off.code);
  TEST_ASSERT_EQUAL_STRING("disabled", off.json()["error"]);
  native::advance(2100);
  TEST_ASSERT_EQUAL(400, r.call(M::Post, "/api/v1/measure", "{kaputt").code);
  TEST_ASSERT_EQUAL(404, r.call(M::Post, "/api/v1/measure").code); // ohne Sensor
  TEST_ASSERT_EQUAL(405, r.call(M::Get, "/api/v1/measure").code);
}

void test_chronik_endpoints() {
  Rig r;
  r.chronik.begin(false);
  native::setEpoch(1700000000);
  r.measureAll(5000);
  Event e;
  while (r.ev.pop(e))
    if (e.type == EventType::MeasurementDone)
      r.chronik.record(e.a, hal::epoch(), hal::now());
  r.chronik.tick(hal::now(), true);
  FakeRes data = r.get("/api/v1/chronik/data");
  TEST_ASSERT_EQUAL(200, data.code);
  TEST_ASSERT_EQUAL_STRING("application/octet-stream", data.type.c_str());
  TEST_ASSERT_EQUAL(-1, data.length); // chunked
  TEST_ASSERT_EQUAL_HEX8(0xA7, static_cast<uint8_t>(data.body[0]));
  TEST_ASSERT_EQUAL_HEX8(0xC6, static_cast<uint8_t>(data.body[1]));
  FakeRes since = r.call(M::Get, "/api/v1/chronik/data", "", nullptr, {{"since", "2000000000"}});
  // Filter wirkt segmentweise; das jüngste Segment kommt immer mit
  TEST_ASSERT_TRUE(since.body.size() <= data.body.size());
  JsonDocument info = r.get("/api/v1/chronik/info").json();
  TEST_ASSERT_EQUAL(1, info["segments"].as<int>());
  TEST_ASSERT_TRUE(info["enabled"]);
  FakeRes csv = r.get("/api/v1/chronik/export.csv");
  TEST_ASSERT_EQUAL(200, csv.code);
  TEST_ASSERT_EQUAL_STRING("attachment; filename=chronik.csv",
                           csv.headers["Content-Disposition"].c_str());
  TEST_ASSERT_EQUAL(0, csv.body.find("zeit,kanal,schluessel,wert,rohwert,status\n"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, csv.body.find(",ANALOG_0,"));
  // Drossel 10 s
  FakeRes again = r.get("/api/v1/chronik/export.csv");
  TEST_ASSERT_EQUAL(429, again.code);
  TEST_ASSERT_EQUAL_STRING("10", again.headers["Retry-After"].c_str());
  native::advance(10001);
  TEST_ASSERT_EQUAL(200, r.get("/api/v1/chronik/export.csv").code);
}

// ---------------------------------------------------------------------------
// Konfiguration und Geheimnisse
// ---------------------------------------------------------------------------
void test_config_get_shape() {
  Rig r;
  r.cfg.setMailKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=");
  JsonDocument d = r.admin(M::Get, "/api/v1/config").json();
  TEST_ASSERT_TRUE(d["ok"]);
  TEST_ASSERT_EQUAL(1, d["config"]["schema"].as<int>());
  TEST_ASSERT_EQUAL_STRING("INFO", d["config"]["log"]["level"]);
  TEST_ASSERT_EQUAL_STRING("24h", d["config"]["display"]["clockFormat"]);
  TEST_ASSERT_FALSE(d["secrets"]["admin"]["set"]);
  TEST_ASSERT_TRUE(d["secrets"]["mailKey"]["set"]);
  TEST_ASSERT_EQUAL(3, d["secrets"]["wifi"].as<JsonArray>().size());
  TEST_ASSERT_EQUAL(-1, d["activeWifiSlot"].as<int>());
  TEST_ASSERT_EQUAL(board::kSlotCount, d["channels"].as<JsonArray>().size());
  TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", d["system"]["mac"]);
  TEST_ASSERT_EQUAL(16000, d["system"]["logFileMax"].as<int>());
  TEST_ASSERT_EQUAL(std::string::npos, r.admin(M::Get, "/api/v1/config").body.find("AAECAwQF"));
}

void test_config_patch_events_and_errors() {
  Rig r;
  FakeRes ok = r.admin(M::Patch, "/api/v1/config",
                       "{\"display\":{\"showQr\":true},\"mail\":{\"warnHours\":8}}");
  TEST_ASSERT_EQUAL(200, ok.code);
  TEST_ASSERT_EQUAL(SecDisplay | SecMail, ok.json()["changed"].as<int>());
  Event e;
  TEST_ASSERT_TRUE(r.popEvent(EventType::ConfigChanged, &e));
  TEST_ASSERT_EQUAL(SecDisplay | SecMail, e.a);
  // Keine Änderung, kein Ereignis
  TEST_ASSERT_EQUAL(0, r.admin(M::Patch, "/api/v1/config", "{\"display\":{\"showQr\":true}}")
                           .json()["changed"]
                           .as<int>());
  TEST_ASSERT_FALSE(r.popEvent(EventType::ConfigChanged));
  // Fehler je Feld
  FakeRes bad =
      r.admin(M::Patch, "/api/v1/config",
              "{\"mail\":{\"url\":\"ftp://x\",\"warnHours\":0},\"log\":{\"level\":\"LAUT\"}}");
  TEST_ASSERT_EQUAL(400, bad.code);
  JsonDocument d = bad.json();
  TEST_ASSERT_EQUAL_STRING("invalid", d["fields"]["mail.url"]);
  TEST_ASSERT_EQUAL_STRING("range", d["fields"]["mail.warnHours"]);
  TEST_ASSERT_EQUAL_STRING("range", d["fields"]["log.level"]);
  TEST_ASSERT_EQUAL(400, r.admin(M::Patch, "/api/v1/config", "").code);
  TEST_ASSERT_EQUAL(400, r.admin(M::Patch, "/api/v1/config", "[1,2]").code);
  std::string huge = "{\"x\":\"" + std::string(7000, 'a') + "\"}";
  TEST_ASSERT_EQUAL(413, r.admin(M::Patch, "/api/v1/config", huge.c_str()).code);
  // LED "folgt Display" ist im Testprofil erlaubt
  TEST_ASSERT_EQUAL(200, r.admin(M::Patch, "/api/v1/config", "{\"led\":{\"mode\":1}}").code);
}

void test_active_wifi_slot_locked() {
  Rig r;
  r.cfg.setWifi(0, "Netz", "", false);
  r.wifi.reachable["Netz"] = true;
  r.net.begin(hal::now());
  r.net.tick(hal::now());
  TEST_ASSERT_EQUAL(0, r.net.activeSlot());
  FakeRes p = r.admin(M::Patch, "/api/v1/config", "{\"wifi\":{\"ssid1\":\"Anders\"}}");
  TEST_ASSERT_EQUAL(409, p.code);
  TEST_ASSERT_EQUAL_STRING("busy", p.json()["fields"]["wifi.ssid1"]);
  TEST_ASSERT_EQUAL(409, r.admin(M::Put, "/api/v1/secrets/wifi/1", "{\"ssid\":\"x\"}").code);
  TEST_ASSERT_EQUAL(200, r.admin(M::Put, "/api/v1/secrets/wifi/3",
                                 "{\"ssid\":\"Drei\",\"password\":\"pw\",\"reconnect\":true}")
                             .code);
  Event e;
  TEST_ASSERT_TRUE(r.popEvent(EventType::ConfigChanged, &e));
  TEST_ASSERT_EQUAL(SecWifi, e.a);
  TEST_ASSERT_EQUAL(1u, e.v); // Reconnect gewünscht
  TEST_ASSERT_EQUAL(404, r.admin(M::Put, "/api/v1/secrets/wifi/4", "{}").code);
  TEST_ASSERT_EQUAL(404, r.admin(M::Put, "/api/v1/secrets/wifi/12", "{}").code);
  TEST_ASSERT_EQUAL(400, r.admin(M::Put, "/api/v1/secrets/wifi/2",
                                 ("{\"ssid\":\"" + std::string(40, 'x') + "\"}").c_str())
                             .code);
  // clear: Passwort löschen
  r.admin(M::Put, "/api/v1/secrets/wifi/2", "{\"ssid\":\"Offen\",\"password\":\"x\"}");
  r.admin(M::Put, "/api/v1/secrets/wifi/2", "{\"ssid\":\"Offen\",\"clear\":true}");
  TEST_ASSERT_TRUE(r.cfg.secrets().wifiPassword[1].empty());
}

void test_secret_validation() {
  Rig r;
  TEST_ASSERT_EQUAL(400,
                    r.admin(M::Put, "/api/v1/secrets/admin-password", "{\"password\":\"\"}").code);
  TEST_ASSERT_EQUAL(
      400, r.admin(M::Put, "/api/v1/secrets/admin-password", "{\"password\":\"Grün\"}").code);
  TEST_ASSERT_EQUAL(400, r.admin(M::Put, "/api/v1/secrets/mail-key", "{\"key\":\"abc\"}").code);
  TEST_ASSERT_EQUAL(200, r.admin(M::Put, "/api/v1/secrets/mail-key",
                                 "{\"key\":\"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=\"}")
                             .code);
  TEST_ASSERT_EQUAL(401, r.call(M::Put, "/api/v1/secrets/mail-key", "{}").code);
}

void test_export_and_import_via_upload() {
  Rig r;
  r.admin(M::Patch, "/api/v1/config", "{\"general\":{\"deviceName\":\"Exportiert\"}}");
  FakeRes ex = r.admin(M::Get, "/api/v1/config/export");
  TEST_ASSERT_EQUAL_STRING("attachment; filename=pflanzensensor-config.json",
                           ex.headers["Content-Disposition"].c_str());
  TEST_ASSERT_EQUAL_STRING("Exportiert", ex.json()["device"]["general"]["deviceName"]);
  // Upload ohne Anmeldung wird verworfen
  FakeReq anon;
  TEST_ASSERT_FALSE(r.importUpload.uploadStart(anon, "config", "x.json"));
  // Mit Anmeldung, in Stücken
  r.admin(M::Patch, "/api/v1/config", "{\"general\":{\"deviceName\":\"Anders\"}}");
  FakeReq q;
  q.auth("admin", "Fabmobil");
  TEST_ASSERT_TRUE(r.importUpload.uploadStart(q, "config", "x.json"));
  for (size_t i = 0; i < ex.body.size(); i += 100)
    r.importUpload.uploadData(reinterpret_cast<const uint8_t*>(ex.body.data() + i),
                              std::min<size_t>(100, ex.body.size() - i));
  r.importUpload.uploadEnd(static_cast<uint32_t>(ex.body.size()));
  FakeRes im = r.admin(M::Post, "/api/v1/config/import");
  TEST_ASSERT_EQUAL(200, im.code);
  JsonDocument d = im.json();
  TEST_ASSERT_FALSE(d["legacy"]);
  TEST_ASSERT_TRUE(d["needsReboot"]);
  TEST_ASSERT_EQUAL_STRING("Exportiert", r.cfg.device().deviceName.c_str());
  TEST_ASSERT_TRUE(r.popEvent(EventType::RebootRequested));
  // Ohne Datei
  TEST_ASSERT_EQUAL(404, r.admin(M::Post, "/api/v1/config/import").code);
  // Zu groß
  TEST_ASSERT_TRUE(r.importUpload.uploadStart(q, "config", "big.json"));
  std::string big(20000, ' ');
  r.importUpload.uploadData(reinterpret_cast<const uint8_t*>(big.data()), big.size());
  r.importUpload.uploadEnd(20000);
  TEST_ASSERT_FALSE(r.fs.exists(ConfigService::kImportPath));
  // Abgebrochen
  TEST_ASSERT_TRUE(r.importUpload.uploadStart(q, "config", "x.json"));
  r.importUpload.uploadData(reinterpret_cast<const uint8_t*>("{"), 1);
  r.importUpload.uploadAborted();
  TEST_ASSERT_FALSE(r.fs.exists(ConfigService::kImportPath));
  // Unlesbar
  pstest::writeFile(r.fs, ConfigService::kImportPath, "nicht json");
  TEST_ASSERT_EQUAL(400, r.admin(M::Post, "/api/v1/config/import").code);
}

void test_system_actions_and_logs() {
  Rig r;
  log::attach(&r.log);
  TEST_ASSERT_EQUAL(200, r.admin(M::Post, "/api/v1/system/factory-reset").code);
  TEST_ASSERT_TRUE(r.popEvent(EventType::FactoryResetRequested));
  LOGI(Mod::Main, "Hallo Log");
  JsonDocument d = r.admin(M::Get, "/api/v1/logs").json();
  TEST_ASSERT_TRUE(d["lines"].as<JsonArray>().size() >= 1);
  const uint32_t next = d["next"];
  FakeRes since = r.call(M::Get, "/api/v1/logs", "", "Fabmobil", {{"since", std::to_string(next)}});
  TEST_ASSERT_EQUAL(0, since.json()["lines"].as<JsonArray>().size());
  TEST_ASSERT_EQUAL(404, r.admin(M::Get, "/api/v1/logs/file").code);
  pstest::writeFile(r.fs, "/log.txt", "Zeile 1\nZeile 2\n");
  FakeRes f = r.admin(M::Get, "/api/v1/logs/file");
  TEST_ASSERT_EQUAL(200, f.code);
  TEST_ASSERT_EQUAL(16, f.length);
  TEST_ASSERT_EQUAL_STRING("Zeile 1\nZeile 2\n", f.body.c_str());
  log::attach(nullptr);
}

// ---------------------------------------------------------------------------
// Sensoren
// ---------------------------------------------------------------------------
void test_sensor_actions() {
  Rig r;
  r.sensors.begin(r.drivers.ptrs, hal::now());
  TEST_ASSERT_EQUAL(200, r.admin(M::Post, "/api/v1/sensors/DHT/measure").code);
  TEST_ASSERT_EQUAL(409, r.admin(M::Post, "/api/v1/sensors/MHZ19/measure").code); // Warmup
  TEST_ASSERT_EQUAL(404, r.admin(M::Post, "/api/v1/sensors/FOO/measure").code);
  TEST_ASSERT_EQUAL(404, r.admin(M::Post, "/api/v1/sensors/DHT").code);
  TEST_ASSERT_EQUAL(404, r.admin(M::Post, "/api/v1/sensors/DHT/tanzen").code);
  TEST_ASSERT_EQUAL(200, r.admin(M::Post, "/api/v1/sensors/HX711/tare").code);
  TEST_ASSERT_EQUAL(400,
                    r.admin(M::Post, "/api/v1/sensors/ANALOG/0/reset-extrema", "{kaputt").code);
  TEST_ASSERT_EQUAL(200, r.admin(M::Post, "/api/v1/sensors/ANALOG/0/reset-extrema").code);
  // Status der Sensorliste
  native::advance(100);
  r.sensors.tick(hal::now());
  JsonDocument d = r.admin(M::Get, "/api/v1/sensors").json();
  for (JsonObject s : d["sensors"].as<JsonArray>()) {
    if (strcmp(s["id"], "MHZ19") == 0)
      TEST_ASSERT_TRUE(s["warmupMs"].as<int>() > 0);
    if (strcmp(s["id"], "HX711") == 0)
      TEST_ASSERT_TRUE(s["tare"]);
    if (strcmp(s["id"], "SDS011") == 0)
      TEST_ASSERT_TRUE(s["channels"][0]["oneSided"]);
  }
  TEST_ASSERT_TRUE(d["led"]["available"]);
  // Fehlerhafte Patches
  TEST_ASSERT_EQUAL(400, r.admin(M::Patch, "/api/v1/sensors", "{\"FOO\":{}}").code);
  TEST_ASSERT_EQUAL(400, r.admin(M::Patch, "/api/v1/sensors", "{\"DHT\":{\"interval\":5}}").code);
  TEST_ASSERT_EQUAL(
      400, r.admin(M::Patch, "/api/v1/sensors", "{\"DHT\":{\"channels\":{\"7\":{}}}}").code);
  TEST_ASSERT_EQUAL(400, r.admin(M::Patch, "/api/v1/sensors",
                                 "{\"ANALOG\":{\"channels\":{\"0\":{\"halfLife\":1234}}}}")
                             .code);
  TEST_ASSERT_EQUAL(400, r.admin(M::Patch, "/api/v1/sensors",
                                 "{\"ANALOG\":{\"channels\":{\"0\":{\"name\":\"\"}}}}")
                             .code);
  TEST_ASSERT_EQUAL(400,
                    r.admin(M::Patch, "/api/v1/sensors",
                            "{\"ANALOG\":{\"channels\":{\"0\":{\"rawMin\":900,\"rawMax\":100}}}}")
                        .code);
  FakeRes ok = r.admin(M::Patch, "/api/v1/sensors", "{\"DHT\":{\"interval\":30}}");
  TEST_ASSERT_EQUAL(200, ok.code);
  TEST_ASSERT_TRUE(r.popEvent(EventType::SensorConfigChanged));
  TEST_ASSERT_EQUAL(
      30000u,
      r.sensors.run(static_cast<uint8_t>(board::sensorIndexOf(SensorType::Dht))).intervalMs);
}

// ---------------------------------------------------------------------------
// Mail
// ---------------------------------------------------------------------------
void test_mail_endpoints() {
  Rig r;
  TEST_ASSERT_EQUAL(409, r.admin(M::Post, "/api/v1/mail/test").code); // aus
  r.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"enabled\":true}}");
  TEST_ASSERT_EQUAL(200, r.admin(M::Post, "/api/v1/mail/test").code);
  JsonDocument d = r.admin(M::Get, "/api/v1/mail/status").json();
  TEST_ASSERT_TRUE(d["pending"]);
  TEST_ASSERT_TRUE(d["enabled"]);
  TEST_ASSERT_FALSE(d["keySet"]);
  // alle Vorlagen lesbar, Stil ohne Betreff
  for (const char* k : {"boot", "warning", "alive", "style"}) {
    std::string p = std::string("/api/v1/mail/templates/") + k;
    JsonDocument t = r.admin(M::Get, p.c_str()).json();
    TEST_ASSERT_EQUAL_STRING(k, t["kind"]);
    TEST_ASSERT_FALSE(t["custom"]);
    TEST_ASSERT_EQUAL(strcmp(k, "style") != 0, !t["subject"].isNull());
  }
  FakeRes bad = r.admin(M::Put, "/api/v1/mail/templates/boot", "{\"subject\":\"\",\"body\":\"x\"}");
  TEST_ASSERT_EQUAL(400, bad.code);
  TEST_ASSERT_EQUAL_STRING("invalid", bad.json()["fields"]["subject"]);
  std::string longLine = "{\"subject\":\"S\",\"body\":\"" + std::string(300, 'x') + "\"}";
  TEST_ASSERT_EQUAL(400, r.admin(M::Put, "/api/v1/mail/templates/alive", longLine.c_str()).code);
}

// ---------------------------------------------------------------------------
// OTA über den Upload-Handler
// ---------------------------------------------------------------------------
void test_ota_upload_flow() {
  Rig r;
  r.sensors.begin(r.drivers.ptrs, hal::now());
  FakeReq q;
  q.auth("admin", "Fabmobil");
  q.args["size"] = "4";
  TEST_ASSERT_TRUE(r.otaUpload.uploadStart(q, "firmware", "firmware.bin"));
  TEST_ASSERT_TRUE(r.ota.active());
  const uint8_t img[] = {0xE9, 1, 2, 3};
  r.otaUpload.uploadData(img, 2);
  r.otaUpload.uploadData(img + 2, 2);
  r.otaUpload.uploadEnd(4);
  TEST_ASSERT_TRUE(r.otaPort.ended);
  TEST_ASSERT_EQUAL(4, r.otaPort.written.size());
  JsonDocument st = r.admin(M::Get, "/api/v1/ota").json();
  TEST_ASSERT_TRUE(st["success"]);
  FakeRes done = r.admin(M::Post, "/api/v1/ota");
  TEST_ASSERT_EQUAL(200, done.code);
  TEST_ASSERT_TRUE(done.json()["needsReboot"]);
  TEST_ASSERT_TRUE(r.popEvent(EventType::RebootRequested));
}

void test_ota_upload_rejections() {
  Rig r;
  FakeReq anon;
  TEST_ASSERT_FALSE(r.otaUpload.uploadStart(anon, "firmware", "f.bin"));
  FakeReq q;
  q.auth("admin", "Fabmobil");
  // Falsches Magic-Byte
  TEST_ASSERT_TRUE(r.otaUpload.uploadStart(q, "firmware", "f.bin"));
  const uint8_t junk[] = {'P', 'K', 3, 4};
  r.otaUpload.uploadData(junk, 4);
  r.otaUpload.uploadEnd(4);
  TEST_ASSERT_TRUE(r.otaPort.aborted);
  FakeRes done = r.admin(M::Post, "/api/v1/ota");
  TEST_ASSERT_EQUAL(400, done.code);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, done.body.find("Magic"));
  // MD5 verlangt
  r.admin(M::Patch, "/api/v1/config", "{\"general\":{\"otaRequireMd5\":true}}");
  TEST_ASSERT_FALSE(r.otaUpload.uploadStart(q, "firmware", "f.bin"));
  q.args["md5"] = "0123456789abcdef0123456789abcdef";
  TEST_ASSERT_TRUE(r.otaUpload.uploadStart(q, "firmware", "f.bin"));
  TEST_ASSERT_EQUAL_STRING("0123456789abcdef0123456789abcdef", r.otaPort.md5.c_str());
  r.otaUpload.uploadAborted();
  TEST_ASSERT_FALSE(r.ota.active());
  // Zu groß
  q.args["size"] = "99999999";
  TEST_ASSERT_FALSE(r.otaUpload.uploadStart(q, "firmware", "f.bin"));
  // POST ohne vorherigen Upload
  Rig r2;
  TEST_ASSERT_EQUAL(400, r2.admin(M::Post, "/api/v1/ota").code);
}

void test_respond_helpers() {
  TEST_ASSERT_EQUAL(200, web::statusFor(Err::None));
  TEST_ASSERT_EQUAL(404, web::statusFor(Err::NotFound));
  TEST_ASSERT_EQUAL(409, web::statusFor(Err::Busy));
  TEST_ASSERT_EQUAL(401, web::statusFor(Err::Auth));
  TEST_ASSERT_EQUAL(429, web::statusFor(Err::Throttled));
  TEST_ASSERT_EQUAL(507, web::statusFor(Err::NoSpace));
  TEST_ASSERT_EQUAL(500, web::statusFor(Err::Io));
  TEST_ASSERT_EQUAL(501, web::statusFor(Err::Unsupported));
  TEST_ASSERT_EQUAL(400, web::statusFor(Err::ParseError));
  FakeReq q;
  q.args["n"] = "42";
  q.args["x"] = "4x";
  uint32_t v = 0;
  TEST_ASSERT_TRUE(web::argU32(q, "n", v));
  TEST_ASSERT_EQUAL(42u, v);
  TEST_ASSERT_FALSE(web::argU32(q, "x", v));
  TEST_ASSERT_FALSE(web::argU32(q, "fehlt", v));
  FakeRes res;
  web::sendError(res, 418, Err::Unknown);
  TEST_ASSERT_EQUAL(418, res.code);
  TEST_ASSERT_EQUAL_STRING("Unbekannter Fehler", res.json()["message"]);
  TEST_ASSERT_TRUE(res.json()["fields"].isNull());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pages_public_and_admin);
  RUN_TEST(test_log_settings_public_but_limited);
  RUN_TEST(test_login_with_password_only);
  RUN_TEST(test_assets_etag_and_304);
  RUN_TEST(test_system_endpoint);
  RUN_TEST(test_get_latest_values_details);
  RUN_TEST(test_measure_variants);
  RUN_TEST(test_chronik_endpoints);
  RUN_TEST(test_config_get_shape);
  RUN_TEST(test_config_patch_events_and_errors);
  RUN_TEST(test_active_wifi_slot_locked);
  RUN_TEST(test_secret_validation);
  RUN_TEST(test_export_and_import_via_upload);
  RUN_TEST(test_system_actions_and_logs);
  RUN_TEST(test_sensor_actions);
  RUN_TEST(test_mail_endpoints);
  RUN_TEST(test_ota_upload_flow);
  RUN_TEST(test_ota_upload_rejections);
  RUN_TEST(test_respond_helpers);
  return UNITY_END();
}
