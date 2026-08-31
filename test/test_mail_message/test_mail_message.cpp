/**
 * @file test_mail_message.cpp
 * @brief Tests für den Nachrichtenbau (utils/mail_message.h)
 *
 * Die Referenzwerte stammen aus Pythons email.utils bzw. base64 - also von
 * einer unabhängigen Umsetzung derselben Normen. Sichtbar werden Fehler hier
 * sonst erst im Postfach: ein falsches Datum sortiert die Mail ans falsche
 * Ende, ein unkodierter Umlaut im Betreff zeigt Buchstabensalat.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/mail_message.h"

using namespace Mail;

// === Datum ===

void test_datum_nach_rfc5322() {
  char out[48];

  TEST_ASSERT_GREATER_THAN_UINT32(0, formatDate(1788180876, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Mon, 31 Aug 2026 12:54:36 +0000", out);

  formatDate(1000000000, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Sun, 09 Sep 2001 01:46:40 +0000", out);

  formatDate(1767225600, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Thu, 01 Jan 2026 00:00:00 +0000", out);
}

/// Ohne gestellte Uhr lieber gar kein Datum als eines aus dem Jahr 1970 - die
/// Mail bekäme sonst beim Empfänger einen völlig falschen Platz.
void test_datum_ohne_uhrzeit() {
  char out[48];
  TEST_ASSERT_EQUAL_UINT32(0, formatDate(0, out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, formatDate(12345, out, sizeof(out)));
}

void test_datum_zu_kleiner_puffer() {
  char out[16];
  memset(out, 0x7F, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(0, formatDate(1788180876, out, sizeof(out)));
  TEST_ASSERT_EQUAL_HEX8(0x7F, out[0]);
}

// === Betreff ===

void test_betreff_ohne_umlaute_bleibt_unveraendert() {
  char out[128];
  const char* betreff = "Pflanzensensor: Bodenfeuchte kritisch";
  TEST_ASSERT_EQUAL_UINT32(strlen(betreff), encodeSubject(betreff, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(betreff, out);
}

void test_betreff_mit_umlauten_wird_kodiert() {
  char out[128];

  encodeSubject("Lichtstärke zu niedrig", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("=?UTF-8?B?TGljaHRzdMOkcmtlIHp1IG5pZWRyaWc=?=", out);

  encodeSubject("Bodenfeuchte kritisch (Gießkanne!)", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("=?UTF-8?B?Qm9kZW5mZXVjaHRlIGtyaXRpc2NoIChHaWXDn2thbm5lISk=?=", out);
}

void test_betreff_zu_kleiner_puffer() {
  char out[8];
  memset(out, 0x7F, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(0, encodeSubject("Lichtstärke zu niedrig", out, sizeof(out)));
  TEST_ASSERT_EQUAL_HEX8(0x7F, out[0]);
  TEST_ASSERT_EQUAL_UINT32(0, encodeSubject("viel zu langer ASCII-Betreff", out, sizeof(out)));
}

void test_ascii_erkennung() {
  TEST_ASSERT_TRUE(isPlainAscii("Bodenfeuchte kritisch"));
  TEST_ASSERT_TRUE(isPlainAscii(""));
  TEST_ASSERT_FALSE(isPlainAscii("Lichtstärke"));
  // Ein Zeilenumbruch im Betreff wäre eine Kopfzeilen-Einschleusung
  TEST_ASSERT_FALSE(isPlainAscii("Betreff\r\nBcc: wer@anders.de"));
}

/// 23 - RFC 2047 §2 erlaubt höchstens 75 Zeichen je encoded-word. Ein einziges
/// langes Wort zeigen manche Programme ungekodiert an, und genau das trifft
/// die Betreffzeilen mit Emojis, die jetzt im Webinterface bearbeitbar sind.
void test_23_langer_betreff_wird_gefaltet() {
  char out[256];
  const char* betreff = "\xF0\x9F\x8C\xB1 Frameclaw PS meldet: Bodenfeuchte kritisch, "
                        "bitte gie\xC3\x9F"
                        "en! \xF0\x9F\x9A\xA8";
  const size_t n = encodeSubject(betreff, out, sizeof(out));
  TEST_ASSERT_GREATER_THAN_UINT32(0, n);

  // Referenz aus Pythons base64, unabhängig gerechnet
  TEST_ASSERT_EQUAL_STRING(
      "=?UTF-8?B?8J+MsSBGcmFtZWNsYXcgUFMgbWVsZGV0OiBCb2RlbmZldWNodGUga3JpdGlz?=\r\n"
      " =?UTF-8?B?Y2gsIGJpdHRlIGdpZcOfZW4hIPCfmqg=?=",
      out);

  // Jedes Teilwort für sich unter der Grenze
  size_t wortLaenge = 0;
  for (size_t i = 0; i < n; i++) {
    if (out[i] == '\r') {
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(75, wortLaenge);
      wortLaenge = 0;
      i += 2; // CRLF und das Leerzeichen überspringen
      continue;
    }
    wortLaenge++;
  }
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(75, wortLaenge);
}

/// 24 - ein Emoji ist vier Byte lang; ein Schnitt mittendrin ergäbe beim
/// Empfänger zwei Ersatzzeichen statt des Zeichens.
void test_24_faltung_zerreisst_kein_emoji() {
  // 30 Emojis à vier Byte = 120 Byte. Die Stückgrenze liegt bei 45 Byte und
  // damit gerade NICHT auf einem Vielfachen von vier - genau der Fall, in dem
  // ein naiver Schnitt ein Emoji zerlegt.
  char betreff[121];
  for (uint8_t i = 0; i < 30; i++) {
    memcpy(betreff + i * 4, "\xF0\x9F\x8C\xB1", 4);
  }
  betreff[120] = '\0';

  char out[400];
  const size_t n = encodeSubject(betreff, out, sizeof(out));
  TEST_ASSERT_GREATER_THAN_UINT32(0, n);

  // Aus der base64-Länge samt Auffüllzeichen die Rohlänge zurückrechnen: sie
  // muss durch vier teilbar sein, dann enthält das Stück nur ganze Emojis.
  const char* p = out;
  uint8_t woerter = 0;
  while ((p = strstr(p, "=?UTF-8?B?")) != nullptr) {
    p += 10;
    const char* ende = strstr(p, "?=");
    TEST_ASSERT_NOT_NULL(ende);
    const size_t b64 = static_cast<size_t>(ende - p);
    TEST_ASSERT_EQUAL_UINT32(0, b64 % 4);

    size_t fuellzeichen = 0;
    if (b64 >= 1 && p[b64 - 1] == '=')
      fuellzeichen++;
    if (b64 >= 2 && p[b64 - 2] == '=')
      fuellzeichen++;
    const size_t roh = (b64 / 4) * 3 - fuellzeichen;
    TEST_ASSERT_EQUAL_UINT32(0, roh % 4);

    woerter++;
    p = ende + 2;
  }
  TEST_ASSERT_GREATER_THAN_UINT8(1, woerter);
}

/// 25 - Regression: reines ASCII darf nicht plötzlich kodiert werden
void test_25_ascii_bleibt_unkodiert() {
  char out[256];
  const char* betreff = "Frameclaw PS: Bodenfeuchte kritisch, bitte nachschauen und giessen";
  encodeSubject(betreff, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(betreff, out);
  TEST_ASSERT_NULL(strstr(out, "=?UTF-8?"));
}

// === Message-ID und Domäne ===

void test_message_id() {
  char out[96];
  TEST_ASSERT_GREATER_THAN_UINT32(
      0, formatMessageId("Frameclaw PS", "datenkollektiv.net", 1788180876, 7, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("<1788180876.7.Frameclaw PS@datenkollektiv.net>", out);
}

void test_domaene_aus_adresse() {
  char out[64];
  TEST_ASSERT_EQUAL_UINT32(18, domainOf("pflanzensensor@datenkollektiv.net", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("datenkollektiv.net", out);

  TEST_ASSERT_EQUAL_UINT32(0, domainOf("ohne-at-zeichen", out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, domainOf("endet-mit@", out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, domainOf(nullptr, out, sizeof(out)));
}

// === Adressprüfung ===

/// Fängt Tippfehler in der Weboberfläche ab, bevor der Server mit 550 antwortet
/// und niemand weiß, warum keine Mail ankommt.
void test_adressen_pruefung() {
  TEST_ASSERT_TRUE(looksLikeAddress("gaertner@example.org"));
  TEST_ASSERT_TRUE(looksLikeAddress("pflanzensensor@datenkollektiv.net"));
  TEST_ASSERT_TRUE(looksLikeAddress("a.b+c@sub.example.co.uk"));

  TEST_ASSERT_FALSE(looksLikeAddress(""));
  TEST_ASSERT_FALSE(looksLikeAddress(nullptr));
  TEST_ASSERT_FALSE(looksLikeAddress("ohne-at.example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("@example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("wer@ohnepunkt"));
  TEST_ASSERT_FALSE(looksLikeAddress("wer@.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("zwei@at@example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("mit leerzeichen@example.org"));
  // Kopfzeilen-Einschleusung über die Empfängeradresse
  TEST_ASSERT_FALSE(looksLikeAddress("wer@example.org\r\nBcc: dritte@example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("a@b.c,d@e.f"));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_datum_nach_rfc5322);
  RUN_TEST(test_datum_ohne_uhrzeit);
  RUN_TEST(test_datum_zu_kleiner_puffer);
  RUN_TEST(test_betreff_ohne_umlaute_bleibt_unveraendert);
  RUN_TEST(test_betreff_mit_umlauten_wird_kodiert);
  RUN_TEST(test_betreff_zu_kleiner_puffer);
  RUN_TEST(test_ascii_erkennung);
  RUN_TEST(test_23_langer_betreff_wird_gefaltet);
  RUN_TEST(test_24_faltung_zerreisst_kein_emoji);
  RUN_TEST(test_25_ascii_bleibt_unkodiert);
  RUN_TEST(test_message_id);
  RUN_TEST(test_domaene_aus_adresse);
  RUN_TEST(test_adressen_pruefung);
  return UNITY_END();
}
