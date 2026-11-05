/**
 * @file test_main.cpp
 * @brief Web-Schicht: Routing, Auth, kompatible Endpunkte (refactor.md 3.1, 3.2, 5.9)
 */
#include <map>

#include "core/util/base64.h"
#include "web_rig.h"

using namespace ps;

using pstest::FakeReq;
using pstest::FakeRes;
using pstest::Rig;

void setUp() { native::setMillis(5000); }
void tearDown() {}

void test_public_and_admin() {
  Rig r;
  FakeRes s = r.call(hal::HttpMethod::Get, "/status");
  TEST_ASSERT_EQUAL(200, s.code);
  JsonDocument d;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(d, s.body).code());
  TEST_ASSERT_TRUE(d["uptime"].is<uint32_t>());
  TEST_ASSERT_TRUE(d["version"].is<const char*>());
  TEST_ASSERT_EQUAL(401, r.call(hal::HttpMethod::Get, "/api/v1/config").code);
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Get, "/api/v1/config", "", "Fabmobil").code);
  TEST_ASSERT_EQUAL(
      200, r.call(hal::HttpMethod::Get, "/api/v1/config", "", "FabmobilNotfallpasswort!11").code);
  TEST_ASSERT_EQUAL(405, r.call(hal::HttpMethod::Delete, "/api/v1/config", "", "Fabmobil").code);
  TEST_ASSERT_EQUAL(404, r.call(hal::HttpMethod::Get, "/gibtsnicht").code);
  // Debug-Schalter und Log-Level nur mit Login (B-31)
  TEST_ASSERT_EQUAL(
      401,
      r.call(hal::HttpMethod::Patch, "/api/v1/config", "{\"log\":{\"level\":\"DEBUG\"}}").code);
  // Logs lesen darf jeder
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Get, "/api/v1/logs").code);
}

void test_auth_lockout() {
  Rig r;
  for (int i = 0; i < 5; i++)
    TEST_ASSERT_EQUAL(401, r.call(hal::HttpMethod::Get, "/api/v1/sensors", "", "falsch").code);
  FakeRes locked = r.call(hal::HttpMethod::Get, "/api/v1/sensors", "", "Fabmobil");
  TEST_ASSERT_EQUAL(429, locked.code);
  TEST_ASSERT_TRUE(locked.headers.count("Retry-After"));
  native::advance(31000);
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Get, "/api/v1/sensors", "", "Fabmobil").code);
}

void test_config_patch_and_secrets_hidden() {
  Rig r;
  FakeRes p = r.call(hal::HttpMethod::Patch, "/api/v1/config",
                     "{\"display\":{\"screenDuration\":0}}", "Fabmobil");
  TEST_ASSERT_EQUAL(400, p.code);
  JsonDocument d;
  deserializeJson(d, p.body);
  TEST_ASSERT_FALSE(d["ok"]);
  TEST_ASSERT_EQUAL_STRING("range", d["fields"]["display.screenDuration"]);
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Put, "/api/v1/secrets/wifi/2",
                                "{\"ssid\":\"N\",\"password\":\"geheim123\"}", "Fabmobil")
                             .code);
  FakeRes g = r.call(hal::HttpMethod::Get, "/api/v1/config", "", "Fabmobil");
  TEST_ASSERT_NULL(strstr(g.body.c_str(), "geheim123"));
  deserializeJson(d, g.body);
  TEST_ASSERT_TRUE(d["secrets"]["wifi"][1]["set"]);
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Put, "/api/v1/secrets/admin-password",
                                "{\"password\":\"neu\"}", "Fabmobil")
                             .code);
  TEST_ASSERT_EQUAL(401, r.call(hal::HttpMethod::Get, "/api/v1/config", "", "Fabmobil").code);
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Get, "/api/v1/config", "", "neu").code);
}

void test_get_latest_values_compat() {
  Rig r;
  struct D final : SensorDriver {
    SensorType t;
    explicit D(SensorType x) : t(x) {}
    SensorType type() const override { return t; }
    uint8_t channelCount() const override { return 8; }
    Step begin(Instant) override { return Step::Ok; }
    Step pollRound(Instant, Reading* out, uint8_t n) override {
      for (uint8_t i = 0; i < n; i++) {
        out[i].value = 500;
        out[i].raw = 500;
      }
      return Step::Ok;
    }
  };
  std::vector<D*> ds;
  SensorDriver* drivers[board::kSensorCount];
  for (uint8_t i = 0; i < board::kSensorCount; i++) {
    ds.push_back(new D(board::kSensors[i].type));
    drivers[i] = ds.back();
  }
  r.sensors.begin(drivers, hal::now());
  for (int i = 0; i < 40; i++) {
    native::advance(500);
    r.sensors.tick(hal::now());
  }
  r.cfg.setWifi(0, "x", "", false);
  FakeRes res = r.call(hal::HttpMethod::Get, "/getLatestValues");
  JsonDocument d;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(d, res.body).code());
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor", d["deviceName"]);
  TEST_ASSERT_EQUAL_STRING("ANALOG_1", d["flowerStatusSensor"]);
  JsonObject a0 = d["sensors"]["ANALOG_0"];
  TEST_ASSERT_FALSE(a0.isNull());
  TEST_ASSERT_EQUAL(500, a0["raw"].as<int>());
  TEST_ASSERT_EQUAL(60000, a0["measurementInterval"].as<int>());
  TEST_ASSERT_TRUE(a0["minmax"]["min"].is<float>());
  TEST_ASSERT_FALSE(a0["calibrationMode"]);
  TEST_ASSERT_TRUE(d["system"]["rebootCount"].is<int>());
  TEST_ASSERT_TRUE(d["sensors"]["DHT_0"]["value"].isNull()); // 500 °C ungültig → null, nie 0
  // ±Inf (Extremwerte zurückgesetzt) ergibt null statt "inf" (B-28)
  r.sensors.resetExtrema(0, false, hal::now());
  res = r.call(hal::HttpMethod::Get, "/getLatestValues");
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(d, res.body).code());
  TEST_ASSERT_TRUE(d["sensors"]["ANALOG_0"]["absoluteMin"].isNull());
  // Metriken
  res = r.call(hal::HttpMethod::Get, "/metrics");
  TEST_ASSERT_NOT_NULL(strstr(res.body.c_str(), "# TYPE pflanzensensor_memory_free_bytes gauge"));
  TEST_ASSERT_NOT_NULL(
      strstr(res.body.c_str(),
             "pflanzensensor_sensor_value{device=\"Fabmobil "
             "Pflanzensensor\",sensor_id=\"ANALOG\",field=\"lichtstaerke\",unit=\"%\"}"));
  TEST_ASSERT_NOT_NULL(strstr(res.body.c_str(), "platform=\"native\",board=\"native\""));
  TEST_ASSERT_NULL(strstr(res.body.c_str(), "pflanzenserver"));
  // Sofortmessung: Drossel 429
  TEST_ASSERT_EQUAL(200,
                    r.call(hal::HttpMethod::Post, "/api/v1/measure", "{\"sensor\":\"DHT\"}").code);
  TEST_ASSERT_EQUAL(
      429, r.call(hal::HttpMethod::Post, "/api/v1/measure", "{\"sensor\":\"ANALOG\"}").code);
  native::advance(2500);
  TEST_ASSERT_EQUAL(404,
                    r.call(hal::HttpMethod::Post, "/api/v1/measure", "{\"sensor\":\"FOO\"}").code);
  for (auto* x : ds)
    delete x;
}

void test_sensors_api_and_patch() {
  Rig r;
  FakeRes g = r.call(hal::HttpMethod::Get, "/api/v1/sensors", "", "Fabmobil");
  JsonDocument d;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(d, g.body).code());
  TEST_ASSERT_EQUAL_STRING("ANALOG", d["sensors"][0]["id"]);
  TEST_ASSERT_TRUE(d["sensors"][0]["channels"][1]["inverted"]);
  FakeRes p = r.call(hal::HttpMethod::Patch, "/api/v1/sensors",
                     "{\"ANALOG\":{\"channels\":{\"1\":{\"inverted\":false}}}}", "Fabmobil");
  TEST_ASSERT_EQUAL(200, p.code);
  TEST_ASSERT_FALSE(r.cfg.sensors().channels[1].inverted); // wirkt (B-25, B-56)
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Post, "/api/v1/sensors/ANALOG/1/reset-extrema",
                                "{\"raw\":true}", "Fabmobil")
                             .code);
  TEST_ASSERT_EQUAL(
      404,
      r.call(hal::HttpMethod::Post, "/api/v1/sensors/ANALOG/9/reset-extrema", "", "Fabmobil").code);
  TEST_ASSERT_EQUAL(501,
                    r.call(hal::HttpMethod::Post, "/api/v1/sensors/DHT/tare", "", "Fabmobil").code);
}

void test_templates_api() {
  Rig r;
  FakeRes g = r.call(hal::HttpMethod::Get, "/api/v1/mail/templates/boot", "", "Fabmobil");
  JsonDocument d;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(d, g.body).code());
  TEST_ASSERT_NOT_NULL(strstr(d["body"].as<const char*>(), "{messwerte}"));
  TEST_ASSERT_EQUAL(200, r.call(hal::HttpMethod::Put, "/api/v1/mail/templates/boot",
                                "{\"subject\":\"S\",\"body\":\"B\"}", "Fabmobil")
                             .code);
  TEST_ASSERT_EQUAL(400, r.call(hal::HttpMethod::Put, "/api/v1/mail/templates/style",
                                "{\"body\":\"<x>\"}", "Fabmobil")
                             .code);
  TEST_ASSERT_EQUAL(
      200, r.call(hal::HttpMethod::Delete, "/api/v1/mail/templates/boot", "", "Fabmobil").code);
  TEST_ASSERT_EQUAL(
      404, r.call(hal::HttpMethod::Get, "/api/v1/mail/templates/foo", "", "Fabmobil").code);
}

void test_reboot_event_after_response() {
  Rig r;
  FakeRes res = r.call(hal::HttpMethod::Post, "/api/v1/system/reboot", "", "Fabmobil");
  TEST_ASSERT_EQUAL(200, res.code);
  Event e;
  TEST_ASSERT_TRUE(r.ev.pop(e));
  TEST_ASSERT_EQUAL(EventType::RebootRequested, e.type);
}

void test_captive_redirect() {
  Rig r;
  r.net.begin(hal::now()); // ohne WLAN → AP
  FakeRes res = r.call(hal::HttpMethod::Get, "/generate_204");
  TEST_ASSERT_EQUAL(302, res.code);
  TEST_ASSERT_EQUAL_STRING("http://192.168.4.1/admin", res.headers["Location"].c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_public_and_admin);
  RUN_TEST(test_auth_lockout);
  RUN_TEST(test_config_patch_and_secrets_hidden);
  RUN_TEST(test_get_latest_values_compat);
  RUN_TEST(test_sensors_api_and_patch);
  RUN_TEST(test_templates_api);
  RUN_TEST(test_reboot_event_after_response);
  RUN_TEST(test_captive_redirect);
  return UNITY_END();
}
