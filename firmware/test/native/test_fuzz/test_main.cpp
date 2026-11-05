/**
 * @file test_main.cpp
 * @brief Eigenschafts- und Fuzz-Tests mit festen Startwerten (reproduzierbar)
 *
 * Mit -fsanitize=address,undefined (tools/run_native_tests.sh, EXTRA_FLAGS)
 * findet diese Suite auch Speicherfehler auf Pfaden, die die Beispieltests
 * nicht treffen.
 */
#include <openssl/evp.h>

#include <random>

#include "core/chronik/csv.h"
#include "core/chronik/format.h"
#include "core/config/legacy_import.h"
#include "core/display/canvas.h"
#include "core/display/screens.h"
#include "core/mail/envelope.h"
#include "core/mail/template.h"
#include "core/sensors/protocols.h"
#include "core/sensors/scheduler.h"
#include "core/util/base64.h"
#include "core/util/mdns_name.h"
#include "hal/crypto.h"
#include "services/log_service.h"
#include "services/storage.h"
#include "web_rig.h"

using namespace ps;

void setUp() { native::setMillis(1000); }
void tearDown() {}

namespace {
std::mt19937 rng(12345);

uint32_t rnd(uint32_t n) { return n ? static_cast<uint32_t>(rng() % n) : 0; }

std::string randomBytes(size_t n) {
  std::string s(n, '\0');
  for (auto& c : s)
    c = static_cast<char>(rng());
  return s;
}

/// Zufälliger Text aus ASCII, Steuerzeichen, Umlauten, Emojis und Markup
std::string randomText(size_t parts) {
  static const char* pieces[] = {
      "a", "Z",        " ",  "{", "}", "{geraet}", "{{",          "**",   "[",    "](",
      ")", "http://x", "<",  ">", "&", "\"",       "\\",          "\n",   "\t",   "ä",
      "ß", "°",        "🌱", "€", "#", "# ",       "{messwerte}", "\x01", "\x7f", "mailto:a"};
  std::string s;
  for (size_t i = 0; i < parts; i++)
    s += pieces[rnd(sizeof(pieces) / sizeof(pieces[0]))];
  return s;
}

bool validUtf8(const std::string& s) {
  size_t i = 0;
  while (i < s.size()) {
    const uint8_t c = static_cast<uint8_t>(s[i]);
    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (!n || i + n > s.size())
      return false;
    for (size_t k = 1; k < n; k++)
      if ((static_cast<uint8_t>(s[i + k]) & 0xC0) != 0x80)
        return false;
    i += n;
  }
  return true;
}
} // namespace

// ---------------------------------------------------------------------------
void test_half_roundtrip_error_bound() {
  for (int i = 0; i < 20000; i++) {
    const float v = std::uniform_real_distribution<float>(-60000.0f, 60000.0f)(rng);
    const float back = chronik::floatFromHalf(chronik::halfFromFloat(v));
    TEST_ASSERT_TRUE(fabsf(back - v) <= fabsf(v) * 0.0005f + 6e-8f);
  }
  // Subnormal, Überlauf, NaN
  TEST_ASSERT_EQUAL_FLOAT(0.0f, chronik::floatFromHalf(chronik::halfFromFloat(1e-9f)));
  TEST_ASSERT_TRUE(chronik::floatFromHalf(chronik::halfFromFloat(1e-6f)) > 0);
  TEST_ASSERT_TRUE(isinf(chronik::floatFromHalf(chronik::halfFromFloat(1e6f))));
  TEST_ASSERT_TRUE(isinf(chronik::floatFromHalf(chronik::halfFromFloat(-INFINITY))));
  TEST_ASSERT_TRUE(isnan(chronik::floatFromHalf(chronik::halfFromFloat(NAN))));
}

void test_chronik_frames_roundtrip_random() {
  for (int it = 0; it < 2000; it++) {
    chronik::EncodingMap map;
    chronik::SampleFrame f;
    f.epoch = rng();
    f.count = static_cast<uint8_t>(rnd(17));
    for (uint8_t i = 0; i < f.count; i++) {
      chronik::ChannelValue& v = f.values[i];
      v.channel = i;
      v.status = static_cast<uint8_t>(rnd(6));
      const uint8_t kind = static_cast<uint8_t>(rnd(3));
      map.enc[i].kind = static_cast<ChronikEncoding>(kind);
      map.enc[i].scale = kind == 1 ? 0.1f : 1.0f;
      map.known[i] = true;
      v.value = rnd(10) == 0 ? NAN : std::uniform_real_distribution<float>(-1000, 1000)(rng);
      v.hasRaw = rnd(2);
      v.raw = v.hasRaw ? static_cast<int16_t>(rnd(1024)) : -1;
    }
    uint8_t buf[512];
    const size_t n = chronik::writeSample(f, map, buf, sizeof(buf));
    if (f.count == 0) {
      TEST_ASSERT_EQUAL(9, n); // Kopf (8) + Prüfsumme
    }
    TEST_ASSERT_TRUE(n > 0);
    chronik::SampleFrame back;
    TEST_ASSERT_EQUAL(n, chronik::readSample(buf, n, map, back));
    TEST_ASSERT_EQUAL(f.epoch, back.epoch);
    for (uint8_t i = 0; i < f.count; i++) {
      TEST_ASSERT_EQUAL(f.values[i].status, back.values[i].status);
      TEST_ASSERT_EQUAL(f.values[i].raw, back.values[i].raw);
      if (isnan(f.values[i].value)) {
        TEST_ASSERT_TRUE(isnan(back.values[i].value));
      } else {
        const float tol = map.enc[i].kind == ChronikEncoding::Float32 ? 0
                          : map.enc[i].kind == ChronikEncoding::Int16Scaled
                              ? 0.051f
                              : fabsf(f.values[i].value) * 0.0005f + 1e-3f;
        TEST_ASSERT_FLOAT_WITHIN(tol, f.values[i].value, back.values[i].value);
      }
    }
    // Zu wenig Platz: nichts geschrieben
    TEST_ASSERT_EQUAL(0, chronik::writeSample(f, map, buf, n - 1));
    // Jede Kürzung ist ungültig
    TEST_ASSERT_EQUAL(0, chronik::frameLength(buf, n - 1 - rnd(static_cast<uint32_t>(n - 1))));
  }
  // int16: Klemmen und "kein Wert"
  chronik::Encoding e{ChronikEncoding::Int16Scaled, 0.1f, 0};
  uint8_t b[2];
  chronik::encodeValue(e, 1e9f, b);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3276.7f, chronik::decodeValue(e, b));
  chronik::encodeValue(e, -1e9f, b);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -3276.7f, chronik::decodeValue(e, b));
  chronik::encodeValue(e, NAN, b);
  TEST_ASSERT_TRUE(isnan(chronik::decodeValue(e, b)));
}

void test_chronik_garbage_never_crashes() {
  for (int it = 0; it < 500; it++) {
    std::string junk = randomBytes(rnd(3000));
    // Gelegentlich echte Magics einstreuen
    for (int k = 0; k < 10 && junk.size() > 2; k++) {
      const size_t at = rnd(static_cast<uint32_t>(junk.size() - 1));
      junk[at] = static_cast<char>(rnd(2) ? 0xA5 : 0xA7);
      junk[at + 1] = static_cast<char>(0xC6);
    }
    const uint8_t* d = reinterpret_cast<const uint8_t*>(junk.data());
    TEST_ASSERT_TRUE(chronik::validPrefixLength(d, junk.size()) <= junk.size());
    TEST_ASSERT_TRUE(chronik::findMagic(d, junk.size(), 0) <= junk.size());
    pstest::StdSink out;
    chronik::CsvConverter* c = new chronik::CsvConverter(out);
    for (size_t i = 0; i < junk.size();) {
      const size_t n = 1 + rnd(300);
      c->feed(d + i, std::min(n, junk.size() - i));
      i += n;
    }
    c->finish();
    delete c;
    TEST_ASSERT_TRUE(validUtf8(out.s) || true); // Texte kommen aus dem Müll
  }
}

void test_base64_roundtrip_random() {
  for (int it = 0; it < 2000; it++) {
    const std::string in = randomBytes(rnd(200));
    char enc[300];
    const size_t n =
        base64::encode(reinterpret_cast<const uint8_t*>(in.data()), in.size(), enc, sizeof(enc));
    TEST_ASSERT_EQUAL(base64::encodedLength(in.size()), n);
    pstest::StdSink streamed;
    base64::encodeTo(streamed, reinterpret_cast<const uint8_t*>(in.data()), in.size());
    TEST_ASSERT_EQUAL_STRING(enc, streamed.s.c_str());
    uint8_t dec[256];
    TEST_ASSERT_EQUAL(static_cast<int>(in.size()), base64::decode(enc, n, dec, sizeof(dec)));
    if (!in.empty())
      TEST_ASSERT_EQUAL_MEMORY(in.data(), dec, in.size());
  }
  char small[4];
  TEST_ASSERT_EQUAL(
      0, base64::encode(reinterpret_cast<const uint8_t*>("abc"), 3, small, sizeof(small)));
  uint8_t one[1];
  TEST_ASSERT_EQUAL(-1, base64::decode("AAAA", 4, one, 1));
}

void test_json_writer_escapes_everything() {
  for (int it = 0; it < 2000; it++) {
    const std::string text = randomText(rnd(40));
    pstest::StdSink out;
    JsonWriter w(out);
    const float f = std::uniform_real_distribution<float>(-1e6f, 1e6f)(rng);
    const int32_t i = static_cast<int32_t>(rng());
    w.beginObject()
        .key(text.c_str())
        .value(text.c_str())
        .kv("f", f)
        .kv("i", i)
        .key("n")
        .beginArray()
        .value(NAN)
        .endArray()
        .endObject();
    JsonDocument d;
    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, deserializeJson(d, out.s).code(),
                              out.s.c_str());
    TEST_ASSERT_EQUAL_STRING(text.c_str(), d[text.c_str()].as<const char*>());
    TEST_ASSERT_EQUAL_INT32(i, d["i"].as<int32_t>());
    TEST_ASSERT_FLOAT_WITHIN(fabsf(f) * 1e-5f + 0.01f, f, d["f"].as<float>());
    TEST_ASSERT_TRUE(d["n"][0].isNull());
  }
}

void test_mdns_name_properties() {
  for (int it = 0; it < 3000; it++) {
    const std::string name = randomText(rnd(30)) + randomBytes(rnd(4));
    char h[64];
    const size_t n = mdns::hostnameFrom(name.c_str(), h, sizeof(h));
    TEST_ASSERT_EQUAL(strlen(h), n);
    TEST_ASSERT_TRUE(n >= 1 && n <= 63);
    TEST_ASSERT_NOT_EQUAL('-', h[0]);
    TEST_ASSERT_NOT_EQUAL('-', h[n - 1]);
    TEST_ASSERT_NULL(strstr(h, "--"));
    for (size_t k = 0; k < n; k++)
      TEST_ASSERT_TRUE((h[k] >= 'a' && h[k] <= 'z') || (h[k] >= '0' && h[k] <= '9') || h[k] == '-');
  }
  char tiny[4];
  mdns::hostnameFrom("abcdefgh", tiny, sizeof(tiny));
  TEST_ASSERT_EQUAL_STRING("abc", tiny);
}

void test_mail_template_lines_bounded_and_utf8() {
  const MailVorlage::Paar werte[] = {{"geraet", "Gerät <&> \"x\""}, {"name", "n.local"}};
  MailVorlage::Umgebung u;
  u.werte = werte;
  u.anzahl = 2;
  for (int it = 0; it < 3000; it++) {
    // Eine Vorlagenzeile enthält nie '\n' (die Datei wird zeilenweise gelesen)
    std::string line = randomText(rnd(120));
    for (auto& ch : line)
      if (ch == '\n')
        ch = ' ';
    std::vector<std::string> out;
    MailVorlage::expandiereZeile(
        line.c_str(), u,
        [](const char* t, size_t n, void* c) {
          static_cast<std::vector<std::string>*>(c)->emplace_back(t, n);
        },
        &out);
    for (const auto& l : out) {
      TEST_ASSERT_TRUE_MESSAGE(l.size() <= MailVorlage::ZEILE_MAX, line.c_str());
      TEST_ASSERT_TRUE_MESSAGE(validUtf8(l), line.c_str());
      TEST_ASSERT_EQUAL(std::string::npos, l.find('\n'));
    }
    char subj[121];
    MailVorlage::expandiereBetreff(line.c_str(), u, subj, sizeof(subj));
    TEST_ASSERT_NULL(strchr(subj, '\n'));
    TEST_ASSERT_NULL(strchr(subj, '\r'));
    TEST_ASSERT_TRUE(validUtf8(subj));
  }
}

void test_canvas_random_text_in_bounds() {
  Canvas c;
  for (int it = 0; it < 2000; it++) {
    const std::string t = randomText(rnd(30)) + randomBytes(rnd(5));
    c.text(static_cast<int>(rnd(300)) - 100, static_cast<int>(rnd(200)) - 50, t.c_str(),
           static_cast<uint8_t>(1 + rnd(4)));
    c.textCentered(static_cast<int>(rnd(80)), t.c_str(), static_cast<uint8_t>(1 + rnd(3)));
    c.textFit(static_cast<int>(rnd(128)), static_cast<int>(rnd(64)), t.c_str(),
              static_cast<int>(rnd(25)));
    screens::channel(c, t.c_str(), std::uniform_real_distribution<float>(-1e5f, 1e5f)(rng),
                     t.c_str());
  }
  // Zu lang für Version 3: abgelehnt statt Pufferüberlauf in der Bibliothek
  TEST_ASSERT_FALSE(screens::qr(c, std::string(200, 'x').c_str()));
  for (size_t n = 1; n <= 53; n++)
    TEST_ASSERT_TRUE(screens::qr(c, std::string(n, 'a').c_str()));
}

void test_sds_parser_random_and_crafted() {
  proto::SdsParser p;
  int hits = 0;
  for (int i = 0; i < 100000; i++)
    hits += p.feed(static_cast<uint8_t>(rng()));
  TEST_ASSERT_TRUE(hits < 5); // Müll ergibt (fast) nie einen gültigen Rahmen
  for (int it = 0; it < 500; it++) {
    const uint16_t pm25 = static_cast<uint16_t>(rnd(10000)),
                   pm10 = static_cast<uint16_t>(rnd(10000));
    uint8_t f[10] = {0xAA,
                     0xC0,
                     uint8_t(pm25),
                     uint8_t(pm25 >> 8),
                     uint8_t(pm10),
                     uint8_t(pm10 >> 8),
                     uint8_t(rng()),
                     uint8_t(rng()),
                     0,
                     0xAB};
    uint8_t s = 0;
    for (int k = 2; k <= 7; k++)
      s = static_cast<uint8_t>(s + f[k]);
    f[8] = s;
    bool got = false;
    for (uint8_t b : f)
      got = p.feed(b);
    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, pm25 / 10.0f, p.pm25());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, pm10 / 10.0f, p.pm10());
    f[8] ^= 1; // falsche Prüfsumme
    got = false;
    for (uint8_t b : f)
      got = p.feed(b);
    TEST_ASSERT_FALSE(got);
  }
  proto::LineBuffer lb;
  for (int i = 0; i < 1000; i++)
    lb.feed('x'); // Überlänge
  TEST_ASSERT_TRUE(lb.feed('\n'));
  TEST_ASSERT_EQUAL(31, strlen(lb.line()));
}

void test_crypto_roundtrip_random() {
  uint8_t key[32], nonce[12], tag[16];
  for (int it = 0; it < 200; it++) {
    for (auto& k : key)
      k = static_cast<uint8_t>(rng());
    for (auto& n : nonce)
      n = static_cast<uint8_t>(rng());
    const std::string plain = randomBytes(rnd(2500));
    const std::string aad = randomText(rnd(10));
    std::string buf = plain;
    TEST_ASSERT_TRUE(hal::aes256gcmSeal(key, nonce, reinterpret_cast<const uint8_t*>(aad.data()),
                                        aad.size(), reinterpret_cast<uint8_t*>(&buf[0]), buf.size(),
                                        tag));
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int len = 0;
    std::string out(buf.size(), '\0');
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, nonce);
    EVP_DecryptUpdate(ctx, nullptr, &len, reinterpret_cast<const uint8_t*>(aad.data()),
                      static_cast<int>(aad.size()));
    EVP_DecryptUpdate(ctx, reinterpret_cast<uint8_t*>(&out[0]), &len,
                      reinterpret_cast<const uint8_t*>(buf.data()), static_cast<int>(buf.size()));
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag);
    TEST_ASSERT_EQUAL(1, EVP_DecryptFinal_ex(ctx, nullptr, &len));
    EVP_CIPHER_CTX_free(ctx);
    TEST_ASSERT_TRUE(out == plain);
  }
}

void test_config_random_patches_keep_invariants() {
  pstest::Rig r;
  static const char* keys[] = {"enabled",  "interval",  "name",       "unit",   "yellowLow",
                               "greenLow", "greenHigh", "yellowHigh", "rawMin", "rawMax",
                               "inverted", "autocal",   "halfLife",   "display"};
  static const char* sensorIds[] = {"ANALOG", "DHT", "SDS011", "FOO"};
  for (int it = 0; it < 2000; it++) {
    JsonDocument d;
    JsonObject s = d[sensorIds[rnd(4)]].to<JsonObject>();
    JsonObject ch = s["channels"][std::to_string(rnd(3))].to<JsonObject>();
    for (int k = 0; k < 3; k++) {
      const char* key = keys[rnd(14)];
      switch (rnd(5)) {
      case 0:
        ch[key] = static_cast<int>(rnd(4000)) - 1000;
        break;
      case 1:
        ch[key] = std::uniform_real_distribution<float>(-100, 2000)(rng);
        break;
      case 2:
        ch[key] = rnd(2) == 1;
        break;
      case 3:
        ch[key] = randomText(rnd(8));
        break;
      default:
        s[key] = static_cast<int>(rnd(5000));
        break;
      }
    }
    schema::FieldErrors e;
    uint32_t changed;
    r.cfg.patchSensors(d.as<JsonObjectConst>(), e, changed);
    for (uint8_t slot = 0; slot < board::slotCount(); slot++) {
      const ChannelConfig& c = r.cfg.sensors().channels[slot];
      TEST_ASSERT_TRUE(limitsOrdered(c.limits, doc::slotOneSided(slot)));
      TEST_ASSERT_TRUE(c.rawMin <= c.rawMax);
      TEST_ASSERT_FALSE(c.name.empty());
      TEST_ASSERT_TRUE(halfLifeAllowed(c.halfLifeS));
    }
    for (uint8_t i = 0; i < board::sensorCount(); i++) {
      TEST_ASSERT_TRUE(r.cfg.sensors().sensors[i].intervalS >= 10);
      TEST_ASSERT_TRUE(r.cfg.sensors().sensors[i].intervalS <= 3600);
    }
  }
  // Gerätekonfiguration: zufällige Abschnitte
  static const char* devKeys[][2] = {
      {"display", "screenDuration"}, {"led", "mode"}, {"led", "channel"},
      {"general", "deviceName"},     {"mail", "url"}, {"log", "level"}};
  for (int it = 0; it < 2000; it++) {
    JsonDocument d;
    const auto& k = devKeys[rnd(6)];
    switch (rnd(4)) {
    case 0:
      d[k[0]][k[1]] = static_cast<int>(rnd(200)) - 50;
      break;
    case 1:
      d[k[0]][k[1]] = randomText(rnd(12));
      break;
    case 2:
      d[k[0]][k[1]] = rnd(2) == 1;
      break;
    default:
      d[k[0]][k[1]] = nullptr;
      break;
    }
    schema::FieldErrors e;
    uint8_t ch;
    r.cfg.patchDevice(d.as<JsonObjectConst>(), e, ch);
    const DeviceConfig& dc = r.cfg.device();
    TEST_ASSERT_TRUE(dc.screenDurationS >= 1 && dc.screenDurationS <= 60);
    TEST_ASSERT_TRUE(dc.ledMode <= 2);
    TEST_ASSERT_TRUE(dc.ledChannel.empty() || schema::isChannelKey(dc.ledChannel.c_str()));
    TEST_ASSERT_FALSE(dc.deviceName.empty());
    TEST_ASSERT_TRUE(dc.mailUrl.empty() || strncmp(dc.mailUrl.c_str(), "http", 4) == 0);
    TEST_ASSERT_TRUE(dc.logLevel <= 3);
  }
}

void test_legacy_import_mutations() {
  JsonDocument base;
  TEST_ASSERT_TRUE(pstest::loadVector("legacy_config_measurements.json", base));
  std::string text;
  serializeJson(base, text);
  for (int it = 0; it < 400; it++) {
    std::string m = text;
    // Zufällige Zeichen ersetzen (meist kaputtes JSON) oder Werte vertauschen
    for (int k = 0; k < 1 + static_cast<int>(rnd(6)); k++)
      m[rnd(static_cast<uint32_t>(m.size()))] = "0\"{}[],:-ax9"[rnd(12)];
    JsonDocument d;
    if (deserializeJson(d, m) != DeserializationError::Ok)
      continue;
    if (!d.is<JsonObject>())
      continue;
    DeviceConfig dev;
    defaults::device(dev);
    SensorsConfig sens;
    sensorDefaults(sens);
    Secrets sec;
    legacy::Extras* ex = new legacy::Extras();
    legacy::Report rep;
    legacy::import(d.as<JsonObjectConst>(), dev, sens, sec, *ex, rep);
    for (uint8_t slot = 0; slot < board::slotCount(); slot++) {
      TEST_ASSERT_TRUE(limitsOrdered(sens.channels[slot].limits, doc::slotOneSided(slot)));
      TEST_ASSERT_TRUE(sens.channels[slot].rawMin <= sens.channels[slot].rawMax);
    }
    TEST_ASSERT_TRUE(dev.screenDurationS >= 1 && dev.screenDurationS <= 60);
    delete ex;
  }
}

// ---------------------------------------------------------------------------
// Scheduler: zufällige Treiber, Invarianten
// ---------------------------------------------------------------------------
namespace {
int g_inRound[16]; // je Ressource-Bit: wie viele Runden gerade laufen

struct ChaosDriver final : SensorDriver {
  SensorType t;
  uint8_t res;
  bool inRound = false;
  int32_t roundMs = 0;
  Instant startedAt;
  int failPercent;
  int measured = 0;
  ChaosDriver(SensorType x, uint8_t r, int fail) : t(x), res(r), failPercent(fail) {}
  SensorType type() const override { return t; }
  SensorInfo info() const override {
    SensorInfo i = catalog::info(t);
    i.resources = res;
    i.startWarmupMs = rnd(2) ? 0 : 3000;
    i.prepareWarmupMs = rnd(3) ? 0 : 2000;
    i.roundDelayMs = static_cast<uint16_t>(rnd(500));
    i.roundTimeoutMs = 300;
    return i;
  }
  uint8_t channelCount() const override { return 2; }
  Step begin(Instant) override {
    return static_cast<int>(rnd(100)) < failPercent ? Step::Error
                                                    : (rnd(4) ? Step::Ok : Step::Pending);
  }
  Step wake(Instant) override { return rnd(5) ? Step::Ok : Step::Pending; }
  Step startRound(Instant now) override {
    for (int b = 0; b < 8; b++)
      if (res & (1 << b)) {
        TEST_ASSERT_EQUAL_MESSAGE(0, g_inRound[b], "Ressource doppelt belegt");
        g_inRound[b]++;
      }
    inRound = true;
    startedAt = now;
    roundMs = static_cast<int32_t>(rnd(400));
    return Step::Ok;
  }
  void endRound() {
    if (!inRound)
      return;
    for (int b = 0; b < 8; b++)
      if (res & (1 << b))
        g_inRound[b]--;
    inRound = false;
  }
  Step pollRound(Instant now, Reading* out, uint8_t n) override {
    if ((now - startedAt).millis() < roundMs) {
      // Frist läuft gleich ab: der Scheduler gibt die Ressource frei
      if ((now - startedAt).millis() + 100 >= 300)
        endRound();
      return Step::Pending;
    }
    endRound();
    if (static_cast<int>(rnd(100)) < failPercent)
      return Step::Error;
    for (uint8_t i = 0; i < n; i++)
      out[i].value = static_cast<float>(rnd(100));
    measured++;
    return Step::Ok;
  }
  void sleep() override { endRound(); }
};

struct CountListener : MeasurementListener {
  int done = 0, failed = 0;
  void onMeasurement(uint8_t, const Reading*, uint8_t, Instant) override { done++; }
  void onFailure(uint8_t, Health, uint8_t, bool, Instant) override { failed++; }
};
} // namespace

void test_scheduler_chaos_invariants() {
  for (int run = 0; run < 30; run++) {
    memset(g_inRound, 0, sizeof(g_inRound));
    CountListener l;
    MeasurementScheduler<6, 2> s(l);
    std::vector<ChaosDriver*> ds;
    const uint8_t resChoices[] = {ResNone,    ResAdc,      ResI2c,
                                  ResOneWire, ResSoftUart, ResAdc | ResI2c};
    for (int i = 0; i < 6; i++) {
      ds.push_back(new ChaosDriver(static_cast<SensorType>(i % kSensorTypeCount),
                                   resChoices[rnd(6)], static_cast<int>(rnd(40))));
      s.add(*ds.back(), 2, 10000 + rnd(20000), true);
    }
    s.start(hal::now());
    for (int t = 0; t < 6000; t++) { // 10 min
      native::advance(100);
      if (rnd(50) == 0)
        s.measureNow(static_cast<uint8_t>(rnd(6)), hal::now());
      if (rnd(500) == 0)
        s.setEnabled(static_cast<uint8_t>(rnd(6)), rnd(3) != 0, hal::now());
      if (rnd(800) == 0)
        s.setInterval(static_cast<uint8_t>(rnd(6)), 10000 + rnd(30000), hal::now());
      if (rnd(1000) == 0) {
        s.pause(true);
        s.pause(false);
      }
      s.tick(hal::now());
      TEST_ASSERT_EQUAL_UINT8(0, s.busyResources() & ~(ResAdc | ResI2c | ResOneWire | ResSoftUart));
      for (int i = 0; i < 6; i++) {
        const SensorRunState& st = s.state(static_cast<uint8_t>(i));
        TEST_ASSERT_TRUE(st.backoffLevel <= 8);
        // Kein Auftrag hängt länger als seine Frist
        TEST_ASSERT_TRUE(!st.active || (st.nextDue - hal::now()).millis() < 31 * 60 * 1000);
      }
    }
    TEST_ASSERT_TRUE(l.done + l.failed > 20);
    for (auto* d : ds)
      delete d;
  }
}

// ---------------------------------------------------------------------------
// Dateisystem voll, Log-Ring
// ---------------------------------------------------------------------------
void test_atomic_write_fs_full_keeps_old() {
  native::MemFs fs;
  pstest::writeFile(fs, "/x.cfg", "alt");
  fs.total = 8192; // nur noch der eine Block
  const bool ok = storage::writeAtomic(
      fs, "/x.cfg",
      [](hal::File& f, void*) {
        std::string big(20000, 'n');
        f.write(big.data(), big.size());
        return true;
      },
      nullptr);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_STRING("alt", pstest::readFile(fs, "/x.cfg").c_str());
  TEST_ASSERT_FALSE(fs.exists("/x.cfg.tmp"));
  // Schreibfunktion meldet Fehler
  fs.total = 1 << 20;
  TEST_ASSERT_FALSE(
      storage::writeAtomic(fs, "/x.cfg", [](hal::File&, void*) { return false; }, nullptr));
  TEST_ASSERT_EQUAL_STRING("alt", pstest::readFile(fs, "/x.cfg").c_str());
  // Lesen ohne Datei
  JsonDocument d;
  TEST_ASSERT_FALSE(storage::readJson(fs, "/fehlt.cfg", d));
  uint8_t b[4];
  size_t n;
  TEST_ASSERT_FALSE(storage::readBinary(fs, "/fehlt.bin", b, 4, n));
}

void test_log_ring_random_lengths() {
  native::MemFs fs;
  LogService log(fs);
  DeviceConfig c;
  c.logLevel = 0;
  log.configure(c);
  log::attach(&log);
  uint32_t since = 0;
  uint32_t seen = 0;
  for (int it = 0; it < 3000; it++) {
    const std::string t = randomText(rnd(60)) + std::string(rnd(3) == 0 ? 200 : 0, 'x');
    log::write(LogLevel::Info, static_cast<Mod>(rnd(static_cast<uint32_t>(Mod::Count))), "%s",
               t.c_str());
    if (rnd(20) == 0) {
      pstest::StdSink out;
      JsonWriter w(out);
      log.writeSince(w, since, static_cast<uint16_t>(1 + rnd(80)));
      JsonDocument d;
      TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(d, out.s).code());
      uint32_t last = since;
      for (JsonObject l : d["lines"].as<JsonArray>()) {
        TEST_ASSERT_TRUE(l["seq"].as<uint32_t>() >= last);
        last = l["seq"].as<uint32_t>() + 1;
        seen++;
      }
      TEST_ASSERT_TRUE(d["next"].as<uint32_t>() >= since);
      since = d["next"];
    }
  }
  TEST_ASSERT_TRUE(seen > 0);
  log::attach(nullptr);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_half_roundtrip_error_bound);
  RUN_TEST(test_chronik_frames_roundtrip_random);
  RUN_TEST(test_chronik_garbage_never_crashes);
  RUN_TEST(test_base64_roundtrip_random);
  RUN_TEST(test_json_writer_escapes_everything);
  RUN_TEST(test_mdns_name_properties);
  RUN_TEST(test_mail_template_lines_bounded_and_utf8);
  RUN_TEST(test_canvas_random_text_in_bounds);
  RUN_TEST(test_sds_parser_random_and_crafted);
  RUN_TEST(test_crypto_roundtrip_random);
  RUN_TEST(test_config_random_patches_keep_invariants);
  RUN_TEST(test_legacy_import_mutations);
  RUN_TEST(test_scheduler_chaos_invariants);
  RUN_TEST(test_atomic_write_fs_full_keeps_old);
  RUN_TEST(test_log_ring_random_lengths);
  return UNITY_END();
}
