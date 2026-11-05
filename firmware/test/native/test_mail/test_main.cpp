/**
 * @file test_main.cpp
 * @brief Mail: Scheduler, Template-Vektoren, Mailbot-Krypto, Versand Ende-zu-Ende
 */
#include <openssl/evp.h>

#include "core/mail/envelope.h"
#include "core/mail/scheduler.h"
#include "core/mail/template.h"
#include "hal/crypto.h"
#include "services/mail_service.h"
#include "test_support.h"

using namespace ps;

void setUp() { native::setMillis(1000); }
void tearDown() {}

namespace {
constexpr uint32_t T0 = 1700000000;

void collect(const char* t, size_t n, void* c) {
  static_cast<std::vector<std::string>*>(c)->emplace_back(t, n);
}
void block(const char* name, MailVorlage::ZeilenSenke aus, void* sctx, void*) {
  for (int i = 1; i <= 3; i++) {
    char l[64];
    snprintf(l, sizeof(l), "<tr>%s %d</tr>", name, i);
    aus(l, strlen(l), sctx);
  }
}
std::vector<uint8_t> hex(const char* h) {
  std::vector<uint8_t> v;
  for (size_t i = 0; h[i]; i += 2)
    v.push_back(static_cast<uint8_t>(strtoul(std::string(h + i, 2).c_str(), nullptr, 16)));
  return v;
}
} // namespace

void test_scheduler_rules() {
  Mail::Scheduler s;
  Mail::SchedulerConfig c;
  c.enabled = true;
  c.bootMail = true;
  c.aliveMail = true;
  s.begin(c, T0);
  TEST_ASSERT_EQUAL(Mail::Kind::None, s.due(0));        // keine Uhr
  TEST_ASSERT_EQUAL(Mail::Kind::None, s.due(T0 + 10));  // Sensoren nicht bereit
  TEST_ASSERT_EQUAL(Mail::Kind::Boot, s.due(T0 + 300)); // spätestens nach 300 s
  s.markSent(Mail::Kind::Boot, T0 + 300);
  s.reportLevel(0, Mail::Level::Yellow, true);
  TEST_ASSERT_EQUAL(Mail::Kind::Warning, s.due(T0 + 301));
  s.markSent(Mail::Kind::Warning, T0 + 301);
  TEST_ASSERT_EQUAL(Mail::Kind::None, s.due(T0 + 302));
  TEST_ASSERT_EQUAL(Mail::Kind::Warning, s.due(T0 + 301 + 4 * 3600));
  s.reportLevel(0, Mail::Level::Yellow, false); // nicht überwacht
  TEST_ASSERT_EQUAL(Mail::Kind::Alive, s.due(T0 + 24 * 3600));
  TEST_ASSERT_TRUE(Mail::isWatched("ANALOG_1,DHT_1", "DHT_1"));
  TEST_ASSERT_FALSE(Mail::isWatched("DHT_10", "DHT_1"));
  TEST_ASSERT_TRUE(Mail::isWatched("", "X"));
}

void test_template_vectors() {
  JsonDocument d;
  TEST_ASSERT_TRUE(pstest::loadVector("mail_template.json", d));
  std::vector<MailVorlage::Paar> werte;
  for (JsonPair kv : d["values"].as<JsonObject>())
    werte.push_back({kv.key().c_str(), kv.value().as<const char*>()});
  MailVorlage::Umgebung u;
  u.werte = werte.data();
  u.anzahl = werte.size();
  u.bloecke = block;
  int n = 0;
  for (JsonObject c : d["cases"].as<JsonArray>()) {
    std::vector<std::string> lines;
    MailVorlage::expandiereZeile(c["input"], u, collect, &lines);
    JsonArray exp = c["output"];
    TEST_ASSERT_EQUAL_MESSAGE(exp.size(), lines.size(), c["input"].as<const char*>());
    for (size_t i = 0; i < lines.size(); i++)
      TEST_ASSERT_EQUAL_STRING(exp[i].as<const char*>(), lines[i].c_str());
    n++;
  }
  TEST_ASSERT_TRUE(n > 20);
  for (JsonObject c : d["subjects"].as<JsonArray>()) {
    char out[121];
    MailVorlage::expandiereBetreff(c["input"], u, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(c["output"].as<const char*>(), out);
  }
}

void test_crypto_vector() {
  JsonDocument d;
  TEST_ASSERT_TRUE(pstest::loadVector("mailbot_crypto.json", d));
  auto key = hex(d["key"]);
  auto nonce = hex(d["nonce"]);
  std::string buf = d["plaintext"].as<const char*>();
  char aad[64];
  const size_t al = mailbot::buildAad(aad, sizeof(aad), d["deviceId"], d["ts"]);
  TEST_ASSERT_EQUAL_STRING("testdevice011700000000", aad);
  uint8_t tag[16];
  TEST_ASSERT_TRUE(hal::aes256gcmSeal(key.data(), nonce.data(),
                                      reinterpret_cast<const uint8_t*>(aad), al,
                                      reinterpret_cast<uint8_t*>(&buf[0]), buf.size(), tag));
  TEST_ASSERT_EQUAL_MEMORY(hex(d["ciphertext"]).data(), buf.data(), buf.size());
  TEST_ASSERT_EQUAL_MEMORY(hex(d["tag"]).data(), tag, 16);
  pstest::StdSink env;
  mailbot::writeEnvelope(env, "testdevice01", T0, nonce.data(),
                         reinterpret_cast<const uint8_t*>(buf.data()), buf.size(), tag);
  TEST_ASSERT_EQUAL_STRING(d["envelope"].as<const char*>(), env.s.c_str());
}

void test_plaintext_builder() {
  char buf[200];
  mailbot::PlaintextBuilder pb(buf, sizeof(buf));
  TEST_ASSERT_TRUE(pb.begin("a@b.de", "Betreff \"x\""));
  mailbot::EscapingBufferSink body = pb.bodySink();
  body.write("<p class=\"x\">Hallo</p>\n");
  const size_t n = pb.finish(body, "Gerät");
  buf[n] = '\0';
  TEST_ASSERT_EQUAL_STRING("{\"to\":\"a@b.de\",\"subject\":\"Betreff \\\"x\\\"\",\"body\":\"<p "
                           "class=\\\"x\\\">Hallo</p>\\n\",\"name\":\"Gerät\"}",
                           buf);
  TEST_ASSERT_EQUAL(23, body.rawBytes());
  TEST_ASSERT_TRUE(mailbot::plausibleAddress("x@y.de"));
  TEST_ASSERT_FALSE(mailbot::plausibleAddress("x@y"));
}

namespace {
/// Entschlüsselt den Umschlag (wie der Mailbot) und liefert den Klartext
std::string decrypt(const std::string& envelopeJson, const uint8_t key[32]) {
  JsonDocument d;
  deserializeJson(d, envelopeJson);
  auto b64 = [](const char* s) {
    std::vector<uint8_t> v(strlen(s));
    const int n = base64::decode(s, strlen(s), v.data(), v.size());
    v.resize(n > 0 ? static_cast<size_t>(n) : 0);
    return v;
  };
  auto nonce = b64(d["nonce"]);
  auto ct = b64(d["ciphertext"]);
  auto tag = b64(d["tag"]);
  const std::string aad =
      std::string(d["device_id"].as<const char*>()) + std::to_string(d["ts"].as<uint32_t>());
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  std::string out(ct.size(), '\0');
  int len = 0;
  EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
  EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce.data());
  EVP_DecryptUpdate(ctx, nullptr, &len, reinterpret_cast<const uint8_t*>(aad.data()),
                    static_cast<int>(aad.size()));
  EVP_DecryptUpdate(ctx, reinterpret_cast<uint8_t*>(&out[0]), &len, ct.data(),
                    static_cast<int>(ct.size()));
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data());
  const int ok = EVP_DecryptFinal_ex(ctx, nullptr, &len);
  EVP_CIPHER_CTX_free(ctx);
  return ok > 0 ? out : std::string();
}
} // namespace

void test_send_end_to_end() {
  native::MemFs fs;
  TemplateStore tpl(fs);
  StateStore st(fs);
  ConfigService cfg(fs, tpl, st);
  cfg.load();
  JsonDocument p;
  deserializeJson(p, "{\"mail\":{\"enabled\":true,\"deviceId\":\"dev1\",\"to\":\"ich@example.org\","
                     "\"alive\":true},"
                     "\"general\":{\"deviceName\":\"Balkon\"},\"wifi\":{\"ssid1\":\"Netz\"}}");
  schema::FieldErrors errs;
  uint8_t ch;
  TEST_ASSERT_EQUAL(Err::None, cfg.patchDevice(p.as<JsonObjectConst>(), errs, ch));
  cfg.setMailKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=");
  // Das Testprofil hat 19 Kanäle - mehr als in 2000 B Mailtext passen
  JsonDocument sp;
  deserializeJson(sp, "{\"SERIAL_RECEIVER\":{\"enabled\":false},\"DS18B20\":{\"enabled\":false},"
                      "\"BMP280\":{\"enabled\":false},\"SDS011\":{\"enabled\":false}}");
  uint32_t chs;
  TEST_ASSERT_EQUAL(Err::None, cfg.patchSensors(sp.as<JsonObjectConst>(), errs, chs));
  Events ev;
  SensorService sensors(cfg, st, ev);
  native::FakeWifi wifi;
  wifi.reachable["Netz"] = true;
  NetService net(wifi, cfg, ev);
  net.begin(hal::now());
  net.tick(hal::now());
  TEST_ASSERT_TRUE(net.connected());
  SystemInfo sys(fs);
  sys.begin();
  native::FakeTcp tcp;
  MailService mail(tcp, cfg, st, tpl, sensors, net, sys);
  native::setEpoch(T0);
  mail.begin(hal::now());
  mail.requestTest(hal::now());
  for (int i = 0; i < 100; i++) {
    native::advance(100);
    mail.tick(hal::now());
  }
  TEST_ASSERT_EQUAL_STRING("pflanzensensormailbot.fabmobil.org", tcp.host.c_str());
  TEST_ASSERT_EQUAL(45480, tcp.port);
  TEST_ASSERT_EQUAL(0, strncmp(tcp.sent.c_str(), "POST /send.php HTTP/1.1\r\n", 25));
  TEST_ASSERT_NOT_NULL(strstr(tcp.sent.c_str(), "X-Device-Mac: AA:BB:CC:DD:EE:FF\r\n"));
  const size_t body = tcp.sent.find("\r\n\r\n") + 4;
  const std::string env = tcp.sent.substr(body);
  char cl[40];
  snprintf(cl, sizeof(cl), "Content-Length: %u\r\n", static_cast<unsigned>(env.size()));
  TEST_ASSERT_NOT_NULL(strstr(tcp.sent.c_str(), cl));
  uint8_t key[32];
  for (int i = 0; i < 32; i++)
    key[i] = static_cast<uint8_t>(i);
  const std::string plain = decrypt(env, key);
  TEST_ASSERT_TRUE(plain.size() > 100);
  JsonDocument pd;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(pd, plain).code());
  TEST_ASSERT_EQUAL_STRING("ich@example.org", pd["to"]);
  TEST_ASSERT_EQUAL_STRING("Balkon", pd["name"]);
  TEST_ASSERT_NOT_NULL(strstr(pd["body"].as<const char*>(), "<table class=\"werte\">"));
  TEST_ASSERT_NOT_NULL(strstr(pd["subject"].as<const char*>(), "meldet sich"));
  pstest::StdSink status;
  JsonWriter w(status);
  mail.writeStatus(w);
  TEST_ASSERT_NOT_NULL(strstr(status.s.c_str(), "\"sent\":1"));
  // Abgelehnt: Fehlertext aus dem JSON
  tcp.response = "HTTP/1.1 403 Forbidden\r\n\r\n{\"error\":\"unknown_device\"}";
  mail.requestTest(hal::now());
  for (int i = 0; i < 100; i++) {
    native::advance(100);
    mail.tick(hal::now());
  }
  status.s.clear();
  JsonWriter w2(status);
  mail.writeStatus(w2);
  TEST_ASSERT_NOT_NULL(strstr(status.s.c_str(), "unknown_device"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_scheduler_rules);
  RUN_TEST(test_template_vectors);
  RUN_TEST(test_crypto_vector);
  RUN_TEST(test_plaintext_builder);
  RUN_TEST(test_send_end_to_end);
  return UNITY_END();
}
