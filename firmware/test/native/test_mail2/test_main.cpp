/**
 * @file test_main.cpp
 * @brief Mailversand: Fehlerpfade, Zeitplan im Dienst, Inhalt der Werttabellen
 */
#include "core/util/crc.h"
#include "web_rig.h"

using namespace ps;
using namespace pstest;
using M = hal::HttpMethod;

namespace {
constexpr uint32_t T0 = 1700000000;

struct MailRig : Rig {
  MailRig(bool wifiUp = true) {
    admin(M::Patch, "/api/v1/config",
          "{\"mail\":{\"enabled\":true,\"deviceId\":\"dev1\",\"to\":\"ich@example.org\"},\"wifi\":{"
          "\"ssid1\":\"N\"}}");
    // Weniger Kanäle, damit der Rumpf unter 2000 B bleibt
    admin(M::Patch, "/api/v1/sensors",
          "{\"SERIAL_RECEIVER\":{\"enabled\":false},\"DS18B20\":{\"enabled\":false},\"BMP280\":{"
          "\"enabled\":false},"
          "\"SDS011\":{\"enabled\":false}}");
    cfg.setMailKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=");
    if (wifiUp)
      wifi.reachable["N"] = true;
    net.begin(hal::now());
    net.tick(hal::now());
    native::setEpoch(T0);
  }
  /// Testmail anfordern und laufen lassen
  std::string test(uint32_t ms = 10000) {
    mail.requestTest(hal::now());
    return run(ms);
  }
  std::string run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 100) {
      native::advance(100);
      if (epochValid(hal::epoch()) && t % 1000 == 0)
        native::setEpoch(hal::epoch() + 1);
      mail.tick(hal::now());
    }
    return status();
  }
  std::string status() {
    StdSink s;
    JsonWriter w(s);
    mail.writeStatus(w);
    return s.s;
  }
};
bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
} // namespace

void setUp() {
  native::setMillis(1000);
  native::setEpoch(0);
}
void tearDown() {}

void test_preconditions() {
  {
    MailRig r(false); // kein WLAN
    TEST_ASSERT_TRUE(has(r.test(3000), "Keine WLAN-Verbindung"));
    TEST_ASSERT_TRUE(has(r.status(), "\"pending\":true"));     // vorübergehend: wird wiederholt
    TEST_ASSERT_FALSE(has(r.run(130000), "\"pending\":true")); // nach 120 s aufgegeben
    TEST_ASSERT_TRUE(r.tcp.sent.empty());
  }
  {
    MailRig r;
    native::setEpoch(0);
    TEST_ASSERT_TRUE(has(r.test(3000), "Keine gültige Uhrzeit"));
  }
  {
    MailRig r;
    r.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"to\":\"kaputt\"}}");
    const std::string s = r.test(3000);
    TEST_ASSERT_TRUE(has(s, "Empfängeradresse"));
    TEST_ASSERT_TRUE(has(s, "\"pending\":false")); // Einstellung falsch: nicht wiederholen
  }
  {
    MailRig r;
    r.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"deviceId\":\"\"}}");
    TEST_ASSERT_TRUE(has(r.test(3000), "unvollständig"));
  }
  {
    MailRig r;
    r.cfg.setMailKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=");
    r.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"url\":\"https://sicher.example\"}}");
    TEST_ASSERT_TRUE(has(r.test(3000), "nur http://"));
  }
  {
    MailRig r;
    r.tcp.connectOk = false;
    TEST_ASSERT_TRUE(has(r.test(3000), "Verbindungsaufbau"));
  }
  {
    MailRig r;
    r.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"enabled\":false}}");
    r.mail.requestTest(hal::now());
    TEST_ASSERT_TRUE(has(r.run(1000), "ausgeschaltet"));
  }
}

void test_response_variants() {
  {
    MailRig r;
    r.tcp.response = "HTTP/1.1 500 Internal\r\n\r\nkein json";
    TEST_ASSERT_TRUE(has(r.test(3000), "Status 500"));
  }
  {
    MailRig r;
    r.tcp.response = "Müll";
    TEST_ASSERT_TRUE(has(r.test(3000), "unlesbar"));
  }
  {
    MailRig r;
    r.tcp.response = ""; // Verbindung bleibt stumm offen
    struct Silent {};
    r.mail.requestTest(hal::now());
    // FakeTcp: connected() ist falsch, sobald alles gelesen ist → sofort "unlesbar"
    TEST_ASSERT_TRUE(has(r.run(3000), "unlesbar"));
  }
}

void test_url_with_path_and_port() {
  MailRig r;
  r.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"url\":\"http://bot.example/api/\"}}");
  r.test(3000);
  TEST_ASSERT_EQUAL_STRING("bot.example", r.tcp.host.c_str());
  TEST_ASSERT_EQUAL(80, r.tcp.port);
  TEST_ASSERT_EQUAL(0, r.tcp.sent.find("POST /api/send.php HTTP/1.1\r\nHost: bot.example:80\r\n"));
  MailRig r2;
  r2.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"url\":\"http://:80\"}}");
  TEST_ASSERT_TRUE(has(r2.test(3000), "URL ungültig"));
}

void test_body_too_long() {
  MailRig r;
  // Alle Kanäle wieder an und lange Namen: Tabelle sprengt 2000 B
  r.admin(M::Patch, "/api/v1/sensors",
          "{\"SERIAL_RECEIVER\":{\"enabled\":true},\"DS18B20\":{\"enabled\":true}}");
  // Lange Kanalnamen
  for (const char* id : {"SERIAL_RECEIVER", "DS18B20", "ANALOG"})
    for (int ch = 0; ch < 2; ch++) {
      std::string p = std::string("{\"") + id + "\":{\"channels\":{\"" + std::to_string(ch) +
                      "\":{\"name\":\"" + std::string(24, 'N') + "\"}}}}";
      r.admin(M::Patch, "/api/v1/sensors", p.c_str());
    }
  const std::string s = r.test(3000);
  TEST_ASSERT_TRUE_MESSAGE(has(s, "Mailtext zu lang"), s.c_str());
  TEST_ASSERT_TRUE(has(s, "\"pending\":false"));
}

void test_schedule_boot_warning_alive() {
  MailRig r;
  r.admin(M::Patch, "/api/v1/config",
          "{\"mail\":{\"boot\":true,\"alive\":true,\"aliveHours\":1,\"warnHours\":1}}");
  r.drivers.of(SensorType::Analog).raw = 1000; // Bodenfeuchte invertiert → 0 % → rot
  r.sensors.begin(r.drivers.ptrs, hal::now());
  r.mail.begin(hal::now());
  // Startmeldung, sobald alle aktiven Sensoren gemessen haben oder nach 300 s
  for (int i = 0; i < 3500; i++) { // 350 s
    native::advance(100);
    native::setEpoch(T0 + hal::now().millis() / 1000);
    r.sensors.tick(hal::now());
    r.mail.tick(hal::now());
  }
  TEST_ASSERT_TRUE(has(r.status(), "\"sent\":2")); // Start + Warnung
  TEST_ASSERT_EQUAL(T0 + 0, r.st.mailLastWarning() - (r.st.mailLastWarning() - T0));
  TEST_ASSERT_TRUE(r.st.mailLastWarning() > T0);
  // Die Warnung enthält die auffälligen Messwerte mit roter Ampel
  // (letzter Klartext lässt sich nicht lesen - verschlüsselt; Rendering direkt prüfen)
  StdSink body;
  r.mail.renderBody(Mail::Kind::Warning, body);
  TEST_ASSERT_TRUE(has(body.s, "\xF0\x9F\x94\xB4 Bodenfeuchte"));
  TEST_ASSERT_TRUE(has(body.s, "<style>"));
  TEST_ASSERT_FALSE(has(body.s, "Jetzt ist wieder alles im grünen Bereich"));
  // Lebenszeichen nach einer Stunde
  for (int i = 0; i < 37000; i++) {
    native::advance(100);
    native::setEpoch(T0 + hal::now().millis() / 1000);
    r.mail.tick(hal::now());
  }
  TEST_ASSERT_TRUE(r.st.mailLastAlive() > T0);
}

void test_render_values_and_all_green() {
  MailRig r;
  r.measureAll(30000);
  StdSink body;
  r.mail.renderBody(Mail::Kind::Alive, body);
  TEST_ASSERT_TRUE(has(body.s, "<table class=\"werte\">"));
  TEST_ASSERT_TRUE(has(body.s, "Lichtstärke"));
  TEST_ASSERT_TRUE(has(body.s, "frameclaw") == false);
  TEST_ASSERT_TRUE(has(body.s, "fabmobil-pflanzensensor.local"));
  // Nur grüne Werte: Entwarnung im Block {auffaellige}
  r.drivers.of(SensorType::Analog).raw = 500;
  StdSink warn;
  r.admin(M::Patch, "/api/v1/config",
          "{\"mail\":{\"watch\":\"DHT_1\"}}"); // DHT 500 → Fehler, nicht auffällig
  r.mail.renderBody(Mail::Kind::Warning, warn);
  TEST_ASSERT_TRUE(has(warn.s, "Jetzt ist wieder alles im grünen Bereich"));
  char subj[121];
  r.mail.renderSubject(Mail::Kind::Boot, subj, sizeof(subj));
  TEST_ASSERT_EQUAL_STRING("🌱 ist wieder wach!", subj);
  // Eigene Vorlage mit Platzhaltern
  schema::FieldErrors e;
  r.tpl.save(TemplateKind::Alive, "{geraet} um {uhrzeit}", "x", e);
  r.mail.renderSubject(Mail::Kind::Alive, subj, sizeof(subj));
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor um 22:13", subj);
  native::setEpoch(0);
  r.mail.renderSubject(Mail::Kind::Alive, subj, sizeof(subj));
  TEST_ASSERT_EQUAL_STRING("Fabmobil Pflanzensensor um ???", subj);
}

void test_failed_boot_mail_is_skipped_not_retried() {
  MailRig r;
  r.admin(M::Patch, "/api/v1/config", "{\"mail\":{\"boot\":true}}");
  r.tcp.response = "HTTP/1.1 403 Forbidden\r\n\r\n{\"error\":\"nein\"}";
  r.mail.begin(hal::now());
  r.run(310000);
  const size_t attempts = r.tcp.sent.empty() ? 0 : 1;
  TEST_ASSERT_EQUAL(1u, attempts);
  r.tcp.sent.clear();
  r.run(300000);
  TEST_ASSERT_TRUE(r.tcp.sent.empty()); // kein Dauerfeuer
}

// ---------------------------------------------------------------------------
void test_state_store_rejects_foreign_files() {
  native::MemFs fs;
  StateStore a(fs);
  a.channel(0).value = 1;
  TEST_ASSERT_TRUE(a.save());
  auto good = fs.files_[StateStore::kPath];
  fs.files_[StateStore::kPath].resize(good.size() - 3);
  StateStore b(fs);
  TEST_ASSERT_FALSE(b.load()); // falsche Größe
  // Richtige Prüfsumme, aber anderes Magic
  auto other = good;
  other[0] ^= 0xFF;
  uint32_t crc = crc::crc32(other.data(), other.size() - 4);
  memcpy(other.data() + other.size() - 4, &crc, 4);
  fs.files_[StateStore::kPath] = other;
  TEST_ASSERT_FALSE(b.load());
  // Nur Temp-Datei vorhanden
  fs.files_.erase(StateStore::kPath);
  fs.files_["/state.bin.tmp"] = good;
  TEST_ASSERT_TRUE(b.load());
  TEST_ASSERT_EQUAL_FLOAT(1, b.channel(0).value);
  b.clear();
  TEST_ASSERT_TRUE(isnan(b.channel(0).value));
  // Speichern scheitert (Platte voll): nicht jede Sekunde neu versuchen
  fs.total = 0;
  b.markDirty(hal::now());
  b.tick(hal::now(), true);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_preconditions);
  RUN_TEST(test_response_variants);
  RUN_TEST(test_url_with_path_and_port);
  RUN_TEST(test_body_too_long);
  RUN_TEST(test_schedule_boot_warning_alive);
  RUN_TEST(test_render_values_and_all_green);
  RUN_TEST(test_failed_boot_mail_is_skipped_not_retried);
  RUN_TEST(test_state_store_rejects_foreign_files);
  return UNITY_END();
}
