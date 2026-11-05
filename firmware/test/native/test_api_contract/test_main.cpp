/**
 * @file test_main.cpp
 * @brief Vertrag zwischen C++-API und Weboberfläche
 *
 * Erzeugt die Antworten der wichtigsten Endpunkte in einem festen Zustand und
 * vergleicht sie mit den Dateien in test/vectors/api/. Die JS-Tests laden die Seiten mit
 * genau diesen Dateien. Neu schreiben: PS_WRITE_FIXTURES=1.
 */
#include <fstream>
#include <sstream>

#include "web_rig.h"

using namespace ps;
using namespace pstest;
using M = hal::HttpMethod;

void setUp() {
  native::setMillis(5000);
  native::setEpoch(0);
}
void tearDown() {}

namespace {
std::string dir() {
  for (const char* d :
       {"test/vectors/api/", "../test/vectors/api/", "firmware/test/vectors/api/"}) {
    std::ifstream probe(std::string(d) + "../mail_template.json");
    if (probe)
      return d;
  }
  return "test/vectors/api/";
}

/// Normalisieren: JSON neu serialisieren (gleiche Reihenfolge, kein Leerraum)
std::string canon(const std::string& s) {
  JsonDocument d;
  TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, deserializeJson(d, s).code(), s.c_str());
  // Build-abhängige Felder festsetzen (Version aus VERSION, __DATE__)
  if (d["version"].is<const char*>())
    d["version"] = "3.0.0";
  if (d["buildDate"].is<const char*>())
    d["buildDate"] = "Nov  4 2026";
  if (d["system"]["version"].is<const char*>())
    d["system"]["version"] = "3.0.0";
  if (d["system"]["buildDate"].is<const char*>())
    d["system"]["buildDate"] = "Nov  4 2026";
  std::string out;
  serializeJsonPretty(d, out);
  return out + "\n";
}

void check(const char* name, const FakeRes& r) {
  TEST_ASSERT_EQUAL_MESSAGE(200, r.code, name);
  const std::string now = canon(r.body);
  const std::string path = dir() + name;
  if (getenv("PS_WRITE_FIXTURES")) {
    std::ofstream(path) << now;
    return;
  }
  std::ifstream f(path);
  TEST_ASSERT_TRUE_MESSAGE(static_cast<bool>(f),
                           (path + " fehlt - PS_WRITE_FIXTURES=1 setzen").c_str());
  std::stringstream ss;
  ss << f.rdbuf();
  TEST_ASSERT_EQUAL_STRING_MESSAGE(ss.str().c_str(), now.c_str(), name);
}
} // namespace

void test_contract() {
  Rig r;
  log::attach(&r.log);
  native::setEpoch(1762257600); // 2025-11-04 12:00 UTC
  r.admin(M::Patch, "/api/v1/config",
          "{\"general\":{\"deviceName\":\"Balkon\"},\"mail\":{\"enabled\":true,\"to\":\"ich@"
          "example.org\",\"deviceId\":\"dev1\",\"watch\":\"ANALOG_1\"}}");
  r.cfg.setWifi(0, "Heimnetz", "pw", false);
  r.wifi.reachable["Heimnetz"] = true;
  r.net.begin(hal::now());
  r.net.tick(hal::now());
  r.measureAll(30000);
  LOGI(Mod::Main, "Vertragstest");
  check("system.json", r.get("/api/v1/system"));
  check("latest.json", r.get("/getLatestValues"));
  check("config.json", r.admin(M::Get, "/api/v1/config"));
  check("sensors.json", r.admin(M::Get, "/api/v1/sensors"));
  check("mail_status.json", r.admin(M::Get, "/api/v1/mail/status"));
  check("template_boot.json", r.admin(M::Get, "/api/v1/mail/templates/boot"));
  check("template_alive.json", r.admin(M::Get, "/api/v1/mail/templates/alive"));
  check("template_style.json", r.admin(M::Get, "/api/v1/mail/templates/style"));
  check("chronik_info.json", r.get("/api/v1/chronik/info"));
  check("ota_status.json", r.admin(M::Get, "/api/v1/ota"));
  check("status.json", r.get("/status"));
  check("logs.json", r.get("/api/v1/logs"));
  check("logs_settings.json", r.get("/api/v1/logs/settings"));
  check("error_patch.json", [&] {
    FakeRes e = r.admin(M::Patch, "/api/v1/config", "{\"display\":{\"screenDuration\":0}}");
    e.code = 200; // Fehlerantwort als Vertrag festhalten
    return e;
  }());
  log::attach(nullptr);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_contract);
  return UNITY_END();
}
