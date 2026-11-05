/**
 * @file test_main.cpp
 * @brief Kern: Zeit, FixedString, Result, JSON-Writer, CRC, Base64, SHA-256,
 *        mDNS-Name, Drossel, Status-/Hostname-Vektoren
 */
#include "core/sensors/status.h"
#include "core/types.h"
#include "core/util/base64.h"
#include "core/util/crc.h"
#include "core/util/mdns_name.h"
#include "core/util/sha256.h"
#include "core/util/throttle.h"
#include "test_support.h"

using namespace ps;

void setUp() {}
void tearDown() {}

void test_instant_wrap() {
  const Instant a = Instant::fromMillis(0xFFFFFF00u);
  const Instant b = a + Duration::ms(0x200);
  TEST_ASSERT_EQUAL_UINT32(0x100, b.millis());
  TEST_ASSERT_TRUE(b.reached(a));
  TEST_ASSERT_FALSE(a.reached(b));
  TEST_ASSERT_TRUE(a.before(b));
  TEST_ASSERT_EQUAL_INT32(0x200, (b - a).millis());
}

void test_fixed_string_utf8() {
  FixedString<6> s;
  TEST_ASSERT_FALSE(s.assign("abcdä")); // ä passt nicht mehr ganz (5 Byte Platz)
  TEST_ASSERT_EQUAL_STRING("abcd", s.c_str());
  TEST_ASSERT_TRUE(s.assign("äöü") == false);
  TEST_ASSERT_EQUAL_STRING("äö", s.c_str());
  s = "x";
  TEST_ASSERT_TRUE(s.append("yz"));
  TEST_ASSERT_TRUE(s == "xyz");
}

void test_result() {
  Result<int> ok(5);
  Result<int> bad(Err::NotFound);
  TEST_ASSERT_TRUE(ok.isOk());
  TEST_ASSERT_EQUAL(5, ok.value());
  TEST_ASSERT_FALSE(bad.isOk());
  TEST_ASSERT_EQUAL(7, bad.valueOr(7));
  char buf[16];
  copyP(buf, sizeof(buf), errCode(Err::NotFound));
  TEST_ASSERT_EQUAL_STRING("not_found", buf);
}

void test_json_writer() {
  pstest::StdSink s;
  JsonWriter w(s);
  w.beginObject()
      .kv("a", 1)
      .kv("b", "x\"y\n")
      .key("c")
      .beginArray()
      .value(1.5f)
      .value(NAN)
      .value(INFINITY)
      .endArray();
  w.kv("d", -42).kv("e", true).key("f").value(0.125f, 2).key("g").value(-0.004f, 2).endObject();
  TEST_ASSERT_EQUAL_STRING(
      "{\"a\":1,\"b\":\"x\\\"y\\n\",\"c\":[1.5,null,null],\"d\":-42,\"e\":true,\"f\":0.13,\"g\":0}",
      s.s.c_str());
  JsonDocument doc;
  TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(doc, s.s).code());
}

void test_crc_base64_sha() {
  const char* t = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc::crc32(reinterpret_cast<const uint8_t*>(t), 9));
  TEST_ASSERT_EQUAL_HEX8(0xF4, crc::crc8(reinterpret_cast<const uint8_t*>(t), 9));
  char b64[16];
  base64::encode(reinterpret_cast<const uint8_t*>("foob"), 4, b64, sizeof(b64));
  TEST_ASSERT_EQUAL_STRING("Zm9vYg==", b64);
  uint8_t dec[8];
  TEST_ASSERT_EQUAL(4, base64::decode(b64, strlen(b64), dec, sizeof(dec)));
  TEST_ASSERT_EQUAL(-1, base64::decode("@@", 2, dec, sizeof(dec)));
  Sha256 h;
  h.update("abc");
  uint8_t d[32];
  h.finish(d);
  TEST_ASSERT_EQUAL_HEX8(0xba, d[0]);
  TEST_ASSERT_EQUAL_HEX8(0x78, d[1]);
  TEST_ASSERT_EQUAL_HEX8(0xad, d[31]);
}

void test_throttle() {
  RequestThrottle t(2000);
  TEST_ASSERT_TRUE(t.tryAcquire(1000));
  TEST_ASSERT_FALSE(t.tryAcquire(2500));
  TEST_ASSERT_EQUAL_UINT32(500, t.remaining(2500));
  TEST_ASSERT_TRUE(t.tryAcquire(3000));
  // Überlauf
  RequestThrottle u(2000);
  TEST_ASSERT_TRUE(u.tryAcquire(0xFFFFFFF0u));
  TEST_ASSERT_FALSE(u.tryAcquire(0x10));
}

void test_status_vectors() {
  JsonDocument doc;
  TEST_ASSERT_TRUE(pstest::loadVector("status_hostname.json", doc));
  for (JsonObject c : doc["cases"].as<JsonArray>()) {
    Limits l{c["limits"][0], c["limits"][1], c["limits"][2], c["limits"][3]};
    const ChannelStatus s = evaluate(c["value"].as<float>(), l, c["oneSided"].as<bool>());
    char name[10];
    copyP(name, sizeof(name), statusName(s));
    TEST_ASSERT_EQUAL_STRING(c["status"].as<const char*>(), name);
  }
  for (JsonObject c : doc["hostnames"].as<JsonArray>()) {
    char h[64];
    mdns::hostnameFrom(c["name"], h, sizeof(h));
    TEST_ASSERT_EQUAL_STRING(c["host"].as<const char*>(), h);
  }
  TEST_ASSERT_EQUAL(ChannelStatus::Error, evaluate(NAN, Limits{}, false));
}

void test_mdns_rules() {
  char h[64];
  mdns::hostnameFrom("Fabmobil Pflanzensensor", h, sizeof(h));
  TEST_ASSERT_EQUAL_STRING("fabmobil-pflanzensensor", h);
  mdns::hostnameFrom("Größe", h, sizeof(h));
  TEST_ASSERT_EQUAL_STRING("groesse", h);
  mdns::hostnameFrom("!!!", h, sizeof(h));
  TEST_ASSERT_EQUAL_STRING("pflanzensensor", h);
  std::string lang(100, 'a');
  TEST_ASSERT_EQUAL(63, mdns::hostnameFrom(lang.c_str(), h, sizeof(h)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_instant_wrap);
  RUN_TEST(test_fixed_string_utf8);
  RUN_TEST(test_result);
  RUN_TEST(test_json_writer);
  RUN_TEST(test_crc_base64_sha);
  RUN_TEST(test_throttle);
  RUN_TEST(test_status_vectors);
  RUN_TEST(test_mdns_rules);
  return UNITY_END();
}
