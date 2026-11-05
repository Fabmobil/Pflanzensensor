/**
 * @file gen_vectors.cpp
 * @brief Erzeugt die gemeinsamen Testvektoren C++/JS (test/vectors/*.json)
 * @details Quelle der Wahrheit ist die C++-Implementierung; die Vektoren
 *          werden einmal erzeugt, versioniert und danach von beiden Seiten
 *          geprüft. Neu erzeugen nur bei bewusster Formatänderung:
 *            tools/run_native_tests.sh --vectors
 */
#include <stdio.h>

#include <string>
#include <vector>

#include "core/chronik/format.h"
#include "core/json_writer.h"
#include "core/mail/envelope.h"
#include "core/mail/template.h"
#include "core/sensors/status.h"
#include "core/util/mdns_name.h"
#include "hal/crypto.h"

using namespace ps;

namespace {

struct StdSink : Sink {
  std::string s;
  void write(const char* d, size_t n) override { s.append(d, n); }
  using Sink::write;
};

const MailVorlage::Paar kWerte[] = {
    {"geraet", "Frameclaw PS"}, {"ip", "172.17.1.44"}, {"ssid", "Magrathea"},
    {"neustarts", "7"},         {"laufzeit", "2d 5h"}, {"datum", "31.08.2026"},
    {"uhrzeit", "18:24"},       {"leer", ""},          {"name", "frameclaw-ps.local"}};

void block(const char* name, MailVorlage::ZeilenSenke aus, void* sctx, void*) {
  for (int i = 1; i <= 3; i++) {
    char l[64];
    snprintf(l, sizeof(l), "<tr>%s %d</tr>", name, i);
    aus(l, strlen(l), sctx);
  }
}

void collect(const char* t, size_t n, void* c) {
  static_cast<std::vector<std::string>*>(c)->emplace_back(t, n);
}

void mailTemplate(FILE* f) {
  const std::vector<std::string> inputs = {
      "Hallo Welt",
      "{geraet} auf {ip}",
      "Wert: {quatsch}!",
      "{{geraet} bleibt",
      "a {{ b",
      "Ende: {geraet",
      "[{leer}]",
      "{messwerte}",
      "Werte: {messwerte} soweit",
      "   {auffaellige}  ",
      std::string(300, 'x'),
      "Wort " + std::string(270, 'y'),
      std::string("🟢") +
          std::string("🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢"
                      "🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢"),
      "# 🌻 Hallo {geraet}",
      "Nr. # 1",
      "#kein Leerzeichen",
      "Bei **{geraet}** klemmt es",
      "3 * 4 = 12",
      "**ohne Ende",
      "[Jetzt nachschauen](http://{ip})",
      "[Klick](javascript:alert(1))",
      "[Schreib](mailto:tommy@fabmobil.org)",
      "5 < 7 & \"eins\"",
      "",
      "   ",
      std::string(119, '&'),
      "🔗 [{name}](http://{name}) · {ip}"};
  StdSink out;
  JsonWriter w(out);
  w.beginObject();
  w.key("values").beginObject();
  for (const auto& p : kWerte)
    w.kv(p.name, p.wert);
  w.endObject();
  w.key("blockRows").value("<tr>{name} {n}</tr> für n = 1..3");
  w.key("cases").beginArray();
  MailVorlage::Umgebung u;
  u.werte = kWerte;
  u.anzahl = sizeof(kWerte) / sizeof(kWerte[0]);
  u.bloecke = block;
  for (const auto& in : inputs) {
    std::vector<std::string> lines;
    MailVorlage::expandiereZeile(in.c_str(), u, collect, &lines);
    w.beginObject().kv("input", in.c_str()).key("output").beginArray();
    for (const auto& l : lines)
      w.value(l.c_str(), l.size());
    w.endArray().endObject();
  }
  w.endArray();
  w.key("subjects").beginArray();
  // 118 Zeichen + Emoji: wird vor dem Emoji abgeschnitten, nie mittendrin
  const std::string longSubject = std::string(118, 'a') + "🌱🌱";
  const std::string longValue =
      "{geraet}{geraet}{geraet}{geraet}{geraet}{geraet}{geraet}{geraet}{geraet}🌱";
  for (const char* in : {"🌱 {geraet} ist wach", "Zeile\r\nBcc: x", "{messwerte} & <b>",
                         longSubject.c_str(), longValue.c_str()}) {
    char out2[121];
    MailVorlage::expandiereBetreff(in, u, out2, sizeof(out2));
    w.beginObject().kv("input", in).kv("output", static_cast<const char*>(out2)).endObject();
  }
  w.endArray();
  w.endObject();
  fprintf(f, "%s\n", out.s.c_str());
}

void hex(JsonWriter& w, const uint8_t* d, size_t n) {
  std::string s;
  char b[3];
  for (size_t i = 0; i < n; i++) {
    snprintf(b, 3, "%02x", d[i]);
    s += b;
  }
  w.value(s.c_str());
}

void chronikVectors(FILE* f) {
  using namespace chronik;
  StdSink out;
  JsonWriter w(out);
  uint8_t buf[512];
  EncodingMap map;
  w.beginObject();
  // Tabelle
  TableBuilder tb(buf, sizeof(buf), 1700000000);
  Encoding half;
  Encoding i16{ChronikEncoding::Int16Scaled, 0.1f, 0.0f};
  Encoding f32{ChronikEncoding::Float32, 1.0f, 0.0f};
  tb.add(0, true, false, "ANALOG_0", "Lichtstärke", "%", Limits{10, 20, 80, 90}, half);
  tb.add(1, false, false, "BMP280_1", "Luftdruck", "hPa", Limits{950, 980, 1030, 1060}, i16);
  tb.add(2, false, true, "SDS011_0", "PM10", "µg/m³", Limits{0, 0, 25, 50}, f32);
  const size_t tl = tb.finish();
  readTable(buf, tl, nullptr, nullptr, &map);
  w.key("table");
  hex(w, buf, tl);
  w.key("tableEntries").beginArray();
  w.beginObject()
      .kv("channel", 0)
      .kv("key", "ANALOG_0")
      .kv("name", "Lichtstärke")
      .kv("unit", "%")
      .kv("analog", true)
      .kv("oneSided", false)
      .kv("encoding", 0)
      .endObject();
  w.beginObject()
      .kv("channel", 1)
      .kv("key", "BMP280_1")
      .kv("name", "Luftdruck")
      .kv("unit", "hPa")
      .kv("analog", false)
      .kv("oneSided", false)
      .kv("encoding", 1)
      .endObject();
  w.beginObject()
      .kv("channel", 2)
      .kv("key", "SDS011_0")
      .kv("name", "PM10")
      .kv("unit", "µg/m³")
      .kv("analog", false)
      .kv("oneSided", true)
      .kv("encoding", 2)
      .endObject();
  w.endArray();
  // Messrahmen
  SampleFrame sf;
  sf.epoch = 1700000060;
  sf.count = 3;
  sf.values[0] = ChannelValue{0, kGreen, 42.5f, true, 612};
  sf.values[1] = ChannelValue{1, kYellow, 1013.2f, false, -1};
  sf.values[2] = ChannelValue{2, kRed, NAN, false, -1};
  const size_t sl = writeSample(sf, map, buf, sizeof(buf));
  w.key("sample");
  hex(w, buf, sl);
  SampleFrame back;
  readSample(buf, sl, map, back);
  w.key("sampleDecoded").beginObject().kv("epoch", back.epoch).key("values").beginArray();
  for (uint8_t i = 0; i < back.count; i++) {
    w.beginObject().kv("channel", back.values[i].channel).kv("status", back.values[i].status);
    w.key("value").value(back.values[i].value, 4);
    w.kv("raw", static_cast<int32_t>(back.values[i].raw)).endObject();
  }
  w.endArray().endObject();
  w.key("half").beginArray();
  for (float v : {0.0f, 1.0f, -2.5f, 25.37f, 100.0f, 65504.0f, 70000.0f, 0.0001f}) {
    w.beginObject().key("value").value(v, 6).kv("bits", static_cast<uint32_t>(halfFromFloat(v)));
    w.key("back").value(floatFromHalf(halfFromFloat(v)), 6);
    w.endObject();
  }
  w.endArray();
  w.endObject();
  fprintf(f, "%s\n", out.s.c_str());
}

void crypto(FILE* f) {
  // Testvektor des Altcodes (mail_crypto.h), gegen PHP openssl geprüft
  uint8_t key[32], nonce[12];
  for (int i = 0; i < 32; i++)
    key[i] = static_cast<uint8_t>(i);
  for (int i = 0; i < 12; i++)
    nonce[i] = static_cast<uint8_t>(i);
  const char* plain = "{\"to\":\"test@example.com\",\"subject\":\"Test\",\"body\":\"Hallo Welt\"}";
  std::string buf = plain;
  char aad[64];
  const size_t al = mailbot::buildAad(aad, sizeof(aad), "testdevice01", 1700000000);
  uint8_t tag[16];
  hal::aes256gcmSeal(key, nonce, reinterpret_cast<const uint8_t*>(aad), al,
                     reinterpret_cast<uint8_t*>(&buf[0]), buf.size(), tag);
  StdSink out;
  JsonWriter w(out);
  w.beginObject();
  w.key("key");
  hex(w, key, 32);
  w.key("nonce");
  hex(w, nonce, 12);
  w.kv("deviceId", "testdevice01").kv("ts", 1700000000u).kv("aad", static_cast<const char*>(aad));
  w.kv("plaintext", plain);
  w.key("ciphertext");
  hex(w, reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
  w.key("tag");
  hex(w, tag, 16);
  StdSink env;
  mailbot::writeEnvelope(env, "testdevice01", 1700000000, nonce,
                         reinterpret_cast<const uint8_t*>(buf.data()), buf.size(), tag);
  w.kv("envelope", env.s.c_str());
  w.endObject();
  fprintf(f, "%s\n", out.s.c_str());
}

void status(FILE* f) {
  StdSink out;
  JsonWriter w(out);
  w.beginObject().key("cases").beginArray();
  const Limits l{10, 20, 80, 90};
  const Limits pm{0, 0, 25, 50};
  for (float v : {5.0f, 10.0f, 15.0f, 20.0f, 50.0f, 80.0f, 85.0f, 90.0f, 95.0f}) {
    w.beginObject().key("value").value(v, 2).kv("oneSided", false);
    w.key("limits")
        .beginArray()
        .value(l.yellowLow, 1)
        .value(l.greenLow, 1)
        .value(l.greenHigh, 1)
        .value(l.yellowHigh, 1)
        .endArray();
    w.key("status").value(statusName(evaluate(v, l, false))).endObject();
  }
  for (float v : {0.0f, 25.0f, 26.0f, 50.0f, 51.0f}) {
    w.beginObject().key("value").value(v, 2).kv("oneSided", true);
    w.key("limits")
        .beginArray()
        .value(pm.yellowLow, 1)
        .value(pm.greenLow, 1)
        .value(pm.greenHigh, 1)
        .value(pm.yellowHigh, 1)
        .endArray();
    w.key("status").value(statusName(evaluate(v, pm, true))).endObject();
  }
  w.endArray();
  w.key("hostnames").beginArray();
  for (const char* n : {"Fabmobil Pflanzensensor", "Größe Übung", "  --Hallo--  ", "", "€uro",
                        "ÄÖÜ äöü ß", "Sensor_42 (Küche)"}) {
    char h[64];
    mdns::hostnameFrom(n, h, sizeof(h));
    w.beginObject().kv("name", n).kv("host", static_cast<const char*>(h)).endObject();
  }
  w.endArray().endObject();
  fprintf(f, "%s\n", out.s.c_str());
}

} // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "test/vectors";
  struct {
    const char* name;
    void (*fn)(FILE*);
  } outs[] = {{"mail_template.json", mailTemplate},
              {"chronik_frames.json", chronikVectors},
              {"mailbot_crypto.json", crypto},
              {"status_hostname.json", status}};
  for (auto& o : outs) {
    FILE* f = fopen((dir + "/" + o.name).c_str(), "w");
    if (!f)
      return 1;
    o.fn(f);
    fclose(f);
    printf("%s geschrieben\n", o.name);
  }
  return 0;
}
